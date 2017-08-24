#include "SBDriver.h"
#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>

#include "SBServer.h"
#include <OS/legacy/buffwriter.h>

#include "SBPeer.h"
#include "V1Peer.h"
#include "V2Peer.h"
#include <OS/socketlib/socketlib.h>

#include "MMQuery.h"
namespace SB {
	Driver::Driver(INetServer *server, const char *host, uint16_t port, int version) : INetDriver(server) {
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = Socket::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			//signal error
		}
		int on = 1;
		if (Socket::setsockopt(m_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
			< 0) {
			//signal error
		}

		m_local_addr.sin_port = Socket::htons(port);
		m_local_addr.sin_addr.s_addr = Socket::htonl(bind_ip);
		m_local_addr.sin_family = AF_INET;
		int n = Socket::bind(m_sd, (struct sockaddr *)&m_local_addr, sizeof m_local_addr);
		if (n < 0) {
			//signal error
		}
		if (Socket::listen(m_sd, SOMAXCONN)
			< 0) {
			//signal error
		}

		gettimeofday(&m_server_start, NULL);

		m_version = version;

		mp_mutex = OS::CreateMutex();

	}
	Driver::~Driver() {
		//end all MMQuery tasks first, otherwise can crash here
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			delete peer;
			it++;
		}

		delete mp_mutex;
	}
	void Driver::think(bool listen_waiting) {
		mp_mutex->lock();
		if (listen_waiting) {
			socklen_t psz = sizeof(struct sockaddr_in);
			struct sockaddr_in peer;
			int sda = Socket::accept(m_sd, (struct sockaddr *)&peer, &psz);
			if (sda <= 0) return;
			Peer *mp_peer = NULL;
			switch (m_version) {
			case 1:
				mp_peer = new V1Peer(this, &peer, sda);
				break;
			case 2:
				mp_peer = new V2Peer(this, &peer, sda);
				break;
			}
			m_connections.push_back(mp_peer);
			mp_peer->think(true);
		}
		else {
			std::vector<Peer *>::iterator it = m_connections.begin();
			while (it != m_connections.end()) {
				Peer *peer = *it;
				if (peer->ShouldDelete() && std::find(m_peers_to_delete.begin(), m_peers_to_delete.end(), peer) == m_peers_to_delete.end()) {
					//marked for delection, dec reference and delete when zero
					it = m_connections.erase(it);
					peer->DecRef();
					m_peers_to_delete.push_back(peer);
					continue;
				}
				it++;
			}

			it = m_peers_to_delete.begin();
			while (it != m_peers_to_delete.end()) {
				SB::Peer *p = *it;
				if (p->GetRefCount() == 0) {
					delete p;
					it = m_peers_to_delete.erase(it);
					continue;
				}
				it++;
			}

			MM::Server serv;
			while (!m_server_delete_queue.empty()) {
				serv = m_server_delete_queue.front();
				m_server_delete_queue.pop();
				SendDeleteServer(&serv);
			}

			while (!m_server_new_queue.empty()) {
				serv = m_server_new_queue.front();
				m_server_new_queue.pop();
				SendNewServer(&serv);
			}

			while (!m_server_update_queue.empty()) {
				serv = m_server_update_queue.front();
				m_server_update_queue.pop();
				SendUpdateServer(&serv);
			}
		}
		mp_mutex->unlock();
	}

	Peer *Driver::find_client(struct sockaddr_in *address) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			const struct sockaddr_in *peer_address = peer->getAddress();
			if (address->sin_port == peer_address->sin_port && address->sin_addr.s_addr == peer_address->sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		return NULL;
	}
	bool Driver::HasPeer(SB::Peer * peer) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			if (*it == peer) {
				return true;
			}
			it++;
		}
		return false;
	}


	int Driver::getListenerSocket() {
		return m_sd;
	}

	uint32_t Driver::getBindIP() {
		return htonl(m_local_addr.sin_addr.s_addr);
	}
	uint32_t Driver::getDeltaTime() {
		struct timeval now;
		gettimeofday(&now, NULL);
		uint32_t t = (now.tv_usec / 1000.0) - (m_server_start.tv_usec / 1000.0);
		return t;
	}


	int Driver::GetNumConnections() {
		return m_connections.size();
	}

	const std::vector<int> Driver::getSockets() {
		std::vector<int> sockets;
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			sockets.push_back(p->GetSocket());
			it++;
		}
		return sockets;
	}

	void Driver::TickConnections() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(false);
			it++;
		}
	}
	void Driver::SendDeleteServer(MM::Server *server) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->informDeleteServers(server);
			it++;
		}
	}
	void Driver::SendNewServer(MM::Server *server) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->informNewServers(server);
			it++;
		}
	}
	void Driver::SendUpdateServer(MM::Server *server) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->informUpdateServers(server);
			it++;
		}
	}
	void Driver::AddDeleteServer(MM::Server serv) {
		mp_mutex->lock();
		m_server_delete_queue.push(serv);
		mp_mutex->unlock();
	}
	void Driver::AddNewServer(MM::Server serv) {
		mp_mutex->lock();
		m_server_new_queue.push(serv);
		mp_mutex->unlock();
	}
	void Driver::AddUpdateServer(MM::Server serv) {
		mp_mutex->lock();
		m_server_update_queue.push(serv);
		mp_mutex->unlock();
	}
}