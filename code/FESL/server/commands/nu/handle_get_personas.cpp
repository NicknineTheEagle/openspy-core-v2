#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::send_personas() {
		std::ostringstream s;
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		s << "TXN=NuGetPersonas\n";
		if(m_last_profile_lookup_tid != -1)
			s << "TID=" << m_last_profile_lookup_tid << "\n";
		s << "personas.[]=" << m_profiles.size() << "\n";
		int i = 0;
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			s << "personas." << i++ << "=\"" << profile.uniquenick << "\"\n";
			it++;
		}
		mp_mutex->unlock();
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
	}
	bool Peer::m_acct_get_personas(OS::KVReader kv_list) {
		int tid = -1;
		if(kv_list.HasKey("TID")) {
			tid = kv_list.GetValueInt("TID");
		}
		m_last_profile_lookup_tid = tid;
		if (!m_got_profiles) {
			m_pending_nuget_personas= true;
		}
		else {
			
			send_personas();
		}
		return true;
	}
}