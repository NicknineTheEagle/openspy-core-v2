#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>

#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>

namespace GP {
	void Peer::m_delete_profile_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;

		switch (error_details.response_code) {
			case TaskShared::WebErrorCode_Success:
				break;
			case TaskShared::WebErrorCode_CannotDeleteLastProfile:
				((GP::Peer *)peer)->send_error(GPShared::GP_DELPROFILE_LAST_PROFILE);
				return;
				break;
			default:
				((GP::Peer *)peer)->send_error(GPShared::GP_DELPROFILE);
				return;
				break;
		}
		
		s << "\\dpr\\" << (int)(error_details.response_code == TaskShared::WebErrorCode_Success);
		s << "\\id\\" << (ptrdiff_t)extra;
		((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		if ((error_details.response_code == TaskShared::WebErrorCode_Success)) {
			peer->Delete();
		} 
	}
	void Peer::handle_delprofile(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		request.profile_search_details.id = m_profile.id;
		request.extra = (void *)(ptrdiff_t)data_parser.GetValueInt("id");
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_DeleteProfile;
		request.callback = Peer::m_delete_profile_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}