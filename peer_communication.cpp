
#include "peer_communication.h"
#include "pk_mgr.h"
#include "network.h"
#include "network_udp.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "io_accept.h"
#include "io_connect.h"
#include "io_connect_udp.h"
#include "io_connect_udp_ctrl.h"
#include "logger_client.h"
#include "io_nonblocking.h"
#include "io_nonblocking_udp.h"

#include "udt_lib/udt.h"

using namespace UDT;
using namespace std;

peer_communication::peer_communication(network *net_ptr,network_udp *net_udp_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, logger_client * logger_client_ptr){
	_net_ptr = net_ptr;
	_net_udp_ptr = net_udp_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_logger_client_ptr = logger_client_ptr;
	total_manifest = 0;
	session_id_count = 1;
	self_info =NULL;
	self_info = new struct level_info_t;
	if(!(self_info) ){
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::self_info  new error", __FUNCTION__, __LINE__);
	}
	_io_accept_ptr =NULL;
	_io_connect_ptr =NULL;
	_io_connect_udp_ptr =NULL;
	_io_connect_udp_ctrl_ptr = NULL;
	_io_nonblocking_ptr=NULL;
	_io_nonblocking_udp_ptr=NULL;
	_io_nonblocking_ptr = new io_nonblocking(_net_ptr,log_ptr ,this,logger_client_ptr);
	_io_nonblocking_udp_ptr = new io_nonblocking_udp(_net_udp_ptr,log_ptr ,this,logger_client_ptr);
	_io_accept_ptr = new io_accept(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	_io_connect_ptr = new io_connect(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	_io_connect_udp_ptr = new io_connect_udp(net_udp_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	_io_connect_udp_ctrl_ptr = new io_connect_udp_ctrl(net_udp_ptr, log_ptr, prep_ptr, peer_mgr_ptr, peer_ptr, pk_mgr_ptr, this, logger_client_ptr);
	if (!(_io_nonblocking_ptr) || !(_io_accept_ptr) || !(_io_connect_ptr) || !(_io_connect_udp_ptr) || !(_io_connect_udp_ctrl_ptr)){
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] !(_io_nonblocking_ptr) || !(_io_accept_ptr) || !(_io_connect_ptr) || !(_io_connect_ptr)  new error", __FUNCTION__, __LINE__);
	}
	fd_list_ptr =NULL;
	fd_list_ptr = pk_mgr_ptr->fd_list_ptr ;
	//peer_com_log = fopen("./peer_com_log.txt","wb");
}

peer_communication::~peer_communication(){

	if(self_info)
		delete self_info;

	if(_io_accept_ptr)
		delete _io_accept_ptr;
	if(_io_connect_ptr)
		delete _io_connect_ptr;
	if(_io_connect_udp_ptr)
		delete _io_connect_udp_ptr;
	if(_io_nonblocking_ptr)
		delete _io_nonblocking_ptr;
	_io_accept_ptr =NULL;
	_io_connect_ptr =NULL;
	_io_nonblocking_ptr=NULL;

	for (map<unsigned long, struct mysession_candidates *>::iterator iter = map_mysession_candidates.begin(); iter != map_mysession_candidates.end(); iter++) {
		for (int i = 0; i < iter->second->candidates_num; i++) {
			delete iter->second->p_candidates_info;
			delete iter->second->n_candidates_info;
		}
		delete iter->second;
	}
	map_mysession_candidates.clear();


	for(map_fd_info_iter=map_fd_info.begin() ; map_fd_info_iter!=map_fd_info.end();map_fd_info_iter++){
		delete map_fd_info_iter->second;
	}
	map_fd_info.clear();
	/*
	for (map_udpfd_info_iter = map_udpfd_info.begin(); map_udpfd_info_iter != map_udpfd_info.end(); map_udpfd_info_iter++) {
		delete map_udpfd_info_iter->second;
	}
	map_udpfd_info.clear();
	*/

	for(map_fd_NonBlockIO_iter=map_fd_NonBlockIO.begin() ; map_fd_NonBlockIO_iter!=map_fd_NonBlockIO.end();map_fd_NonBlockIO_iter++){
		delete map_fd_NonBlockIO_iter->second;
	}
	map_fd_NonBlockIO.clear();
	
	for (map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.begin(); map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end(); map_udpfd_NonBlockIO_iter++) {
		delete map_udpfd_NonBlockIO_iter->second;
	}
	map_udpfd_NonBlockIO.clear();

	debug_printf("Have deleted peer_communication \n");
}

void peer_communication::set_self_info(unsigned long public_ip){
	self_info->public_ip = public_ip;
	//self_info->private_ip = _net_ptr->getLocalIpv4();
	self_info->private_ip = _pk_mgr_ptr->my_private_ip;
}

//flag 0 rescue peer(caller is child), flag 1 candidate's peer(caller is parent)
void peer_communication::set_candidates_handler(struct chunk_level_msg_t *testing_info, int candidates_num, int caller, UINT32 my_session, UINT32 peercomm_session)
{	
	UINT32 manifest = testing_info->manifest;

	if (candidates_num < 1) {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Candidates_num cannot less than 1", __FUNCTION__, __LINE__);
	}

	_log_ptr->write_log_format("s(u) s u s u s u s d s d \n", __FUNCTION__, __LINE__,
															"my_session", my_session,
															"peercomm_session", peercomm_session,
															"manifest", manifest,
															"caller", caller, 
															"candidates_num", candidates_num);
	//for (unsigned int i = 0; i < candidates_num; i++) {
	//	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "list pid", testing_info->level_info[i]->pid);
	//}
	
	
	if (caller == CHILD_PEER) {
		
		if (map_mysession_candidates.find(my_session) != map_mysession_candidates.end()) {
			_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] duplicate mysession in map_mysession_candidates", __FUNCTION__, __LINE__);
		}
		else {
			int size = candidates_num * sizeof(struct peer_info_t);
			bool all_behind_nat = true;

			// Create map_mysession_candidates table
			map_mysession_candidates[my_session] = new struct mysession_candidates;
			if (!map_mysession_candidates[my_session]) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id] new failed", __FUNCTION__, __LINE__);
			}
			map_mysession_candidates[my_session]->mypid = _pk_mgr_ptr->my_pid;
			map_mysession_candidates[my_session]->candidates_num = candidates_num;
			map_mysession_candidates[my_session]->manifest = manifest;
			map_mysession_candidates[my_session]->myrole = caller;
			map_mysession_candidates[my_session]->session_state = SESSION_CONNECTING;
			map_mysession_candidates[my_session]->all_behind_nat = TRUE;
			map_mysession_candidates[my_session]->p_candidates_info = (struct peer_info_t *) new unsigned char[size];
			map_mysession_candidates[my_session]->n_candidates_info = (struct peer_info_t *) new unsigned char[size];
			if (!map_mysession_candidates[my_session]->p_candidates_info || !map_mysession_candidates[my_session]->n_candidates_info) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id]->list_info new failed", __FUNCTION__, __LINE__);
			}
			memset(map_mysession_candidates[my_session]->p_candidates_info, 0, size);
			memset(map_mysession_candidates[my_session]->n_candidates_info, 0, size);
			_log_ptr->timerGet(&(map_mysession_candidates[my_session]->timer));

			for (unsigned int i = 0; i < candidates_num; i++) {
				// 先初始化 map_mysession_candidates table
				memcpy(&map_mysession_candidates[my_session]->p_candidates_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
				map_mysession_candidates[my_session]->p_candidates_info[i].manifest = manifest;
				map_mysession_candidates[my_session]->p_candidates_info[i].peercomm_session = peercomm_session;
				map_mysession_candidates[my_session]->p_candidates_info[i].priority = my_session;
				map_mysession_candidates[my_session]->p_candidates_info[i].connection_state = PEER_CONNECTING;
				//_log_ptr->timerGet(&(map_mysession_candidates[my_session]->p_candidates_info[i].time_start));

				memcpy(&map_mysession_candidates[my_session]->n_candidates_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
				map_mysession_candidates[my_session]->n_candidates_info[i].manifest = manifest;
				map_mysession_candidates[my_session]->n_candidates_info[i].peercomm_session = peercomm_session;
				map_mysession_candidates[my_session]->n_candidates_info[i].priority = my_session;
				map_mysession_candidates[my_session]->n_candidates_info[i].connection_state = PEER_CONNECTING;

				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u s u s u s u \n", "my_pid", _pk_mgr_ptr->my_pid, "[PEER_LIST] parent", testing_info->level_info[i]->pid,
					"manifest", manifest,
					"peercomm_session", peercomm_session,
					"my_session", my_session);
				_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "[PEER_LIST] parent", testing_info->level_info[i]->pid,
					"peercomm_session", peercomm_session,
					"my_session", my_session);


				// 檢查 peer 是否已經有連線存在
				struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(testing_info->level_info[i]->pid);
				if (parent_info != NULL) {
					if (parent_info->peerInfo.connection_state != PEER_CONNECTING) {
						map_mysession_candidates[my_session]->p_candidates_info[i].connection_state = PEER_CONNECTED;
						_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Has a connection with parent", testing_info->level_info[i]->pid);
					}
				}
				
				// 抓 NAT 穿透成功率 (session 裡全部的 peer 都在 NAT 底下才去記錄)
				if (self_info->private_ip == self_info->public_ip || testing_info->level_info[i]->private_ip == testing_info->level_info[i]->public_ip) {
					if (self_info->public_ip == testing_info->level_info[i]->public_ip) {
						map_mysession_candidates[my_session]->all_behind_nat = FALSE;
					}
				}
			}
			if (map_mysession_candidates[my_session]->all_behind_nat == TRUE) {
				_logger_client_ptr->add_nat_total_times();
			}
			
			for (unsigned int i = 0; i < candidates_num; i++) {
				char public_IP[16] = {0};
				char private_IP[16] = {0};
				memcpy(public_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip)));
				memcpy(private_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip)));
				
				for (map<unsigned long, struct peer_connect_down_t *>::iterator iter = _pk_mgr_ptr->map_pid_parent.begin(); iter != _pk_mgr_ptr->map_pid_parent.end(); iter++) {
					_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "parent", iter->first, "in map_pid_parent");
				}
				
				// Build connection if the peer doesn't exist in map_pid_parent
				if (_pk_mgr_ptr->map_pid_parent.find(testing_info->level_info[i]->pid) == _pk_mgr_ptr->map_pid_parent.end()) {
					_peer_ptr->substream_first_reply_peer[my_session]->allSkipConnFlag = FALSE;
					//_pk_mgr_ptr->parents_table[testing_info->level_info[i]->pid] = PEER_CONNECTING;

					// 防止同一台電腦互連
					if (self_info->private_ip == testing_info->level_info[i]->private_ip && self_info->public_ip == testing_info->level_info[i]->public_ip) {
						//continue;
					}
					if (testing_info->level_info[i]->pid != 0) {
						//continue;
					}
					
					struct peer_connect_down_t *parent_info_ptr = new struct peer_connect_down_t;
					if (!parent_info_ptr) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::peerDownInfoPtr  new error", __FUNCTION__, __LINE__);
					}
					memset(parent_info_ptr, 0, sizeof(struct peer_connect_down_t));
					_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
					memcpy(parent_info_ptr, testing_info->level_info[i], sizeof(struct level_info_t));
					_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
					parent_info_ptr->peerInfo.connection_state = PEER_CONNECTING;
					parent_info_ptr->peerInfo.manifest = 0;

					_pk_mgr_ptr->map_pid_parent.insert(pair<unsigned long, struct peer_connect_down_t *>(parent_info_ptr->peerInfo.pid, parent_info_ptr));
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[INSERT PARENT]", parent_info_ptr->peerInfo.pid);
					_pk_mgr_ptr->SetParentManifest(parent_info_ptr, testing_info->manifest);

					// Self not behind NAT, candidate-peer not behind NAT
					if (self_info->private_ip == self_info->public_ip && testing_info->level_info[i]->private_ip == testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Both are not behind NAT, active connect");
						non_blocking_build_connection_udp(&map_mysession_candidates[my_session]->p_candidates_info[i], caller, manifest, testing_info->level_info[i]->pid, 1, my_session, peercomm_session);
					}
					// Self not behind NAT, candidate-peer is behind NAT
					else if (self_info->private_ip == self_info->public_ip && testing_info->level_info[i]->private_ip != testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Candidate-peer is behind NAT, passive connect");
						non_blocking_build_connection_udp(&map_mysession_candidates[my_session]->p_candidates_info[i], caller, manifest, testing_info->level_info[i]->pid, 1, my_session, peercomm_session);
					}
					// Self is behind NAT, candidate-peer not behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[i]->private_ip == testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Rescue-peer is behind NAT, active connect");
						non_blocking_build_connection_udp(&map_mysession_candidates[my_session]->p_candidates_info[i], caller, manifest, testing_info->level_info[i]->pid, 1, my_session, peercomm_session);
					}
					// Self is behind NAT, candidate-peer is behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[i]->private_ip != testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__,"candidate-peer IP =", public_IP, private_IP, "Both are behind NAT");
						
						// if both are in the same NAT
						if (self_info->public_ip == testing_info->level_info[i]->public_ip) {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__,"candidate-peer IP =", public_IP, private_IP, "Both are behind NAT(identical private IP)");
							non_blocking_build_connection_udp(&map_mysession_candidates[my_session]->p_candidates_info[i], caller, manifest, testing_info->level_info[i]->pid, 0, my_session, peercomm_session);
						}
						else {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Both are behind NAT(different private IP)");
							non_blocking_build_connection_udp(&map_mysession_candidates[my_session]->p_candidates_info[i], caller, manifest, testing_info->level_info[i]->pid, 1, my_session, peercomm_session);
						}
					}
				}
				else {
					_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
					map<unsigned long, struct peer_connect_down_t *>::iterator iter = _pk_mgr_ptr->map_pid_parent.find(testing_info->level_info[i]->pid);
					_pk_mgr_ptr->SetParentManifest(iter->second, iter->second->peerInfo.manifest | testing_info->manifest);
					_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "parent", testing_info->level_info[i]->pid, "has existed in map_pid_parent");
				}
			}
			// 如果名單中所有 peer 皆已建立連線，直接跳過 PEER_CON 階段，直接認第一個 peer 為 the first connected peer
			if (_peer_ptr->substream_first_reply_peer[my_session]->allSkipConnFlag == TRUE) {
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "All connected, select pid", testing_info->level_info[0]->pid);
				_peer_ptr->substream_first_reply_peer[my_session]->session_state = FIRST_CONNECTED_OK;
			}
		}
		
		//}
	}
	// Caller is the parent of the candidate-peer
	else if (caller == PARENT_PEER) {
		if (candidates_num != 1) {
			_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] candidates_num is not equal to 1 when caller is candidate-peer", __FUNCTION__, __LINE__);
		}
		else {
			if (map_mysession_candidates.find(my_session) != map_mysession_candidates.end()) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] session id in the record in set_candidates_test");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] session id in the record in set_candidates_test \n");
				_logger_client_ptr->log_exit();
			}
			else {
				int size = candidates_num * sizeof(struct peer_info_t);

				map_mysession_candidates[my_session] = new struct mysession_candidates;;
				if (!map_mysession_candidates[my_session]) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id] new failed", __FUNCTION__, __LINE__);
				}  
				map_mysession_candidates[my_session]->mypid = _pk_mgr_ptr->my_pid;
				map_mysession_candidates[my_session]->candidates_num = candidates_num;
				map_mysession_candidates[my_session]->manifest = manifest;
				map_mysession_candidates[my_session]->myrole = caller;
				map_mysession_candidates[my_session]->session_state = SESSION_CONNECTING;
				map_mysession_candidates[my_session]->all_behind_nat = FALSE;
				map_mysession_candidates[my_session]->p_candidates_info = (struct peer_info_t *) new unsigned char[size];
				map_mysession_candidates[my_session]->n_candidates_info = (struct peer_info_t *) new unsigned char[size];
				if (!map_mysession_candidates[my_session]->p_candidates_info || !map_mysession_candidates[my_session]->n_candidates_info) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id]->list_info new failed", __FUNCTION__, __LINE__);
				}
				memset(map_mysession_candidates[my_session]->p_candidates_info, 0, size);
				memset(map_mysession_candidates[my_session]->n_candidates_info, 0, size);
				_log_ptr->timerGet(&(map_mysession_candidates[my_session]->timer));

				for (unsigned int i = 0; i < candidates_num; i++) {
					memcpy(&map_mysession_candidates[my_session]->p_candidates_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
					map_mysession_candidates[my_session]->p_candidates_info[i].manifest = manifest;
					map_mysession_candidates[my_session]->p_candidates_info[i].peercomm_session = peercomm_session;
					map_mysession_candidates[my_session]->p_candidates_info[i].priority = my_session;
					map_mysession_candidates[my_session]->p_candidates_info[i].connection_state = PEER_CONNECTING;
					//_log_ptr->timerGet(&(map_mysession_candidates[my_session]->p_candidates_info[i].time_start));

					memcpy(&map_mysession_candidates[my_session]->n_candidates_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
					map_mysession_candidates[my_session]->n_candidates_info[i].manifest = manifest;
					map_mysession_candidates[my_session]->n_candidates_info[i].peercomm_session = peercomm_session;
					map_mysession_candidates[my_session]->n_candidates_info[i].priority = my_session;
					map_mysession_candidates[my_session]->n_candidates_info[i].connection_state = PEER_CONNECTING;

					
					_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "[PEER_LIST] child", testing_info->level_info[i]->pid,
						"peercomm_session", peercomm_session,
						"my_session", my_session);


					// 檢查 peer 是否已經有連線存在
					struct peer_info_t *child_info = _pk_mgr_ptr->GetChildFromPid(testing_info->level_info[i]->pid);
					if (child_info != NULL) {
						if (child_info->connection_state != PEER_CONNECTING) {
							//map_mysession_candidates[my_session]->p_candidates_info[i].sock = _pk_mgr_ptr->map_pid_child.find(testing_info->level_info[i]->pid)->second->sock;
							map_mysession_candidates[my_session]->p_candidates_info[i].connection_state = PEER_CONNECTED;
							_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Has a connection with child", testing_info->level_info[i]->pid);
						}
					}
				}
				
				char public_IP[16] = {0};
				char private_IP[16] = {0};
				memcpy(public_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip)));
				memcpy(private_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip)));
				
				for (map<unsigned long, int>::iterator iter = _pk_mgr_ptr->children_table.begin(); iter != _pk_mgr_ptr->children_table.end(); iter++) {
					_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "children_table pid", iter->first, "state", iter->second);
				}
				for (map<unsigned long, struct peer_info_t *>::iterator iter = _pk_mgr_ptr->map_pid_child.begin(); iter != _pk_mgr_ptr->map_pid_child.end(); iter++) {
					_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "child", iter->first, "in map_pid_child");
				}

				if (_pk_mgr_ptr->GetChildFromPid(testing_info->level_info[0]->pid) == NULL) {
					
					struct peer_info_t *child_info_ptr = new struct peer_info_t;
					if (!child_info_ptr) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::peerDownInfoPtr  new error", __FUNCTION__, __LINE__);
					}

					memset(child_info_ptr, 0, sizeof(struct peer_info_t));
					memcpy(child_info_ptr, testing_info->level_info[0], sizeof(struct level_info_t));
					//child_info_ptr->manifest = manifest;
					child_info_ptr->manifest = 0;
					child_info_ptr->peercomm_session = peercomm_session;
					child_info_ptr->priority = my_session;
					child_info_ptr->connection_state = PEER_CONNECTING;

					
					_pk_mgr_ptr->map_pid_child.insert(pair<unsigned long, struct peer_info_t *>(child_info_ptr->pid, child_info_ptr));
					//_peer_ptr->priority_children.push_back(child_info_ptr->pid);
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[INSERT CHILD]", child_info_ptr->pid);

					// Self not behind NAT, rescue-peer not behind NAT
					if (self_info->private_ip == self_info->public_ip && testing_info->level_info[0]->private_ip == testing_info->level_info[0]->public_ip) {
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__, "rescue-peer IP =", public_IP, private_IP, "Both are not behind NAT, passive connect");
						non_blocking_build_connection_udp(&(map_mysession_candidates[my_session]->p_candidates_info[0]), caller, manifest, testing_info->level_info[0]->pid, 1, my_session, peercomm_session);
					}
					// Self not behind NAT, rescue-peer is behind NAT
					else if (self_info->private_ip == self_info->public_ip && testing_info->level_info[0]->private_ip != testing_info->level_info[0]->public_ip) {
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__, "rescue-peer IP =", public_IP, private_IP, "Rescue-peer is behind NAT, passive connect");
						non_blocking_build_connection_udp(&(map_mysession_candidates[my_session]->p_candidates_info[0]), caller, manifest, testing_info->level_info[0]->pid, 1, my_session, peercomm_session);
					}
					// Self is behind NAT, rescue-peer not behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[0]->private_ip == testing_info->level_info[0]->public_ip) {
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__, "rescue-peer IP =", public_IP, private_IP, "Rescue-peer is behind NAT, active connect");
						non_blocking_build_connection_udp(&(map_mysession_candidates[my_session]->p_candidates_info[0]), caller, manifest, testing_info->level_info[0]->pid, 1, my_session, peercomm_session);
					}
					// Self is behind NAT, rescue-peer is behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[0]->private_ip != testing_info->level_info[0]->public_ip) {
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__, "Rescue-peer IP =", public_IP, private_IP, "Both are behind NAT");
						//non_blocking_build_connectionNAT_udp(testing_info->level_info[0], caller, rescue_manifest, testing_info->level_info[0]->pid, 0, session_id);

						// if both are in the same NAT
						if (self_info->public_ip == testing_info->level_info[0]->public_ip) {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__, "rescue-peer IP =", public_IP, private_IP, "Both are behind NAT(identical private IP)");
							non_blocking_build_connection_udp(&(map_mysession_candidates[my_session]->p_candidates_info[0]), caller, manifest, testing_info->level_info[0]->pid, 0, my_session, peercomm_session);
						}
						else {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__, "rescue-peer IP =", public_IP, private_IP, "Both are behind NAT(different private IP)");
							non_blocking_build_connection_udp(&(map_mysession_candidates[my_session]->p_candidates_info[0]), caller, manifest, testing_info->level_info[0]->pid, 1, my_session, peercomm_session);
						}
					}
				}
				else {
					//map<unsigned long, struct peer_info_t *>::iterator iter = _pk_mgr_ptr->map_pid_child.find(testing_info->level_info[0]->pid);
					//iter->second->manifest |= testing_info->manifest;
					_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "child", testing_info->level_info[0]->pid, "has existed in map_pid_child");
				}
			}
		}
	}
	
	return ;
}

void peer_communication::clear_fd_in_peer_com(int fd)
{
	map<int ,  struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
	map<int, struct fd_information *>::iterator map_fd_info_iter;
	
	debug_printf("Close fd %d \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", fd);
	debug_printf("Before close fd %d. Table information: \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close fd", fd);
	for (map<int, struct fd_information*>::iterator iter = map_fd_info.begin(); iter != map_fd_info.end(); iter++) {
		debug_printf("map_fd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_fd_NonBlockIO.begin(); iter != map_fd_NonBlockIO.end(); iter++) {
		debug_printf("map_fd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_NonBlockIO  fd", iter->first);
	}
	for (list<int>::iterator iter = _io_accept_ptr->map_fd_unknown.begin(); iter != _io_accept_ptr->map_fd_unknown.end(); iter++) {
		debug_printf("map_fd_unknown  fd : %d \n", *iter);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_unknown  fd", *iter);
	}
	
	// Remove the fd in map_fd_info
	map_fd_info_iter = map_fd_info.find(fd);
	if (map_fd_info_iter != map_fd_info.end()) {
		_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__,
															"fd ", fd,
															"session id ",map_fd_info_iter->second->session_id,
															"pid", map_fd_info_iter->second->pid);
		delete map_fd_info_iter->second;
		map_fd_info.erase(map_fd_info_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_info", __FUNCTION__, __LINE__);
	}
	
	// Remove the fd in map_fd_NonBlockIO
	map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(fd);
	if (map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()) {
		delete map_fd_NonBlockIO_iter->second;
		map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	// Remove the fd in map_fd_unknown
	for (list<int>::iterator iter = _io_accept_ptr->map_fd_unknown.begin(); iter != _io_accept_ptr->map_fd_unknown.end(); iter++) {
		if (*iter == fd) {
			_io_accept_ptr->map_fd_unknown.erase(iter);
			break;
		}
	}
	
	debug_printf("Close fd %d \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", fd);
	debug_printf("After close fd %d. Table information: \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close fd", fd);
	for (map<int, struct fd_information*>::iterator iter = map_fd_info.begin(); iter != map_fd_info.end(); iter++) {
		debug_printf("map_fd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_fd_NonBlockIO.begin(); iter != map_fd_NonBlockIO.end(); iter++) {
		debug_printf("map_fd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_NonBlockIO  fd", iter->first);
	}
	for (list<int>::iterator iter = _io_accept_ptr->map_fd_unknown.begin(); iter != _io_accept_ptr->map_fd_unknown.end(); iter++) {
		debug_printf("map_fd_unknown  fd : %d \n", *iter);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_unknown  fd", *iter);
	}
	/*
	_log_ptr->write_log_format("s =>u \n", __FUNCTION__,__LINE__);
	_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"start close in peer_communication::clear_fd_in_peer_com fd : ",sock);

	map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(sock);
	if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
		delete map_fd_NonBlockIO_iter ->second;
		map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);
	}

	list<int>::iterator map_fd_unknown_iter;

	for(map_fd_unknown_iter = _io_accept_ptr->map_fd_unknown.begin();map_fd_unknown_iter != _io_accept_ptr->map_fd_unknown.end();map_fd_unknown_iter++){
		if( sock == *map_fd_unknown_iter){
			_io_accept_ptr->map_fd_unknown.erase(map_fd_unknown_iter);
			break;
		}
	}



	map_fd_info_iter = map_fd_info.find(sock);
	if(map_fd_info_iter == map_fd_info.end()){
		_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"close fail (clear_fd_in_peer_com) fd : ",sock);
	}
	else{
		_log_ptr->write_log_format("s =>u s d s d s d s\n", __FUNCTION__,__LINE__,"fd : ",sock," session id : ",map_fd_info_iter->second->session_id," pid : ",map_fd_info_iter->second->pid," close succeed (clear_fd_in_peer_com)\n");
		
		delete map_fd_info_iter->second;
		map_fd_info.erase(map_fd_info_iter);
	}
	_log_ptr->write_log_format("s =>u \n", __FUNCTION__,__LINE__);
	*/
}

void peer_communication::clear_udpfd_in_peer_com(int udpfd)
{
	/*
	map<int ,  struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
	map<int, struct fd_information *>::iterator map_udpfd_info_iter;
	
	debug_printf("Close fd %d \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", udpfd);
	debug_printf("Before close fd %d. Table information: \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close fd", udpfd);
	for (map<int, struct fd_information*>::iterator iter = map_udpfd_info.begin(); iter != map_udpfd_info.end(); iter++) {
		debug_printf("map_udpfd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_udpfd_NonBlockIO.begin(); iter != map_udpfd_NonBlockIO.end(); iter++) {
		debug_printf("map_udpfd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_NonBlockIO  fd", iter->first);
	}
	
	// Remove the fd in map_udpfd_info
	map_udpfd_info_iter = map_udpfd_info.find(udpfd);
	if (map_udpfd_info_iter != map_udpfd_info.end()) {
		_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__,
															"fd ", udpfd,
															"session id ",map_udpfd_info_iter->second->session_id,
															"pid", map_udpfd_info_iter->second->pid);
		delete map_udpfd_info_iter->second;
		map_udpfd_info.erase(map_udpfd_info_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_info", __FUNCTION__, __LINE__);
	}
	
	// Remove the fd in map_fd_NonBlockIO
	map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(udpfd);
	if (map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end()) {
		delete map_udpfd_NonBlockIO_iter->second;
		map_udpfd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	debug_printf("Close fd %d \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", udpfd);
	debug_printf("After close fd %d. Table information: \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close fd", udpfd);
	for (map<int, struct fd_information*>::iterator iter = map_udpfd_info.begin(); iter != map_udpfd_info.end(); iter++) {
		debug_printf("map_udpfd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_udpfd_NonBlockIO.begin(); iter != map_udpfd_NonBlockIO.end(); iter++) {
		debug_printf("map_udpfd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_NonBlockIO  fd", iter->first);
	}
	*/
}

void peer_communication::accept_check(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id){
	//map<int, int>::iterator map_fd_unknown_iter;
	list<int>::iterator map_fd_unknown_iter;
	/*map<int, unsigned long>::iterator map_fd_session_id_iter;
	map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
	map<int, unsigned long>::iterator map_fd_manifest_iter;*/
	
	for(map_fd_unknown_iter = _io_accept_ptr->map_fd_unknown.begin();map_fd_unknown_iter != _io_accept_ptr->map_fd_unknown.end();map_fd_unknown_iter++){
		//if(*map_fd_unknown_iter == 1){
			
			
			map_fd_info_iter = map_fd_info.find(*map_fd_unknown_iter);
			if(map_fd_info_iter == map_fd_info.end()){
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
				_logger_client_ptr->log_exit();
			}

			if((manifest == map_fd_info_iter->second->manifest)&&(fd_role == map_fd_info_iter->second->role)&&(fd_pid == map_fd_info_iter->second->pid)){
				_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"update session of fd in peer_communication::accept_check fd : ",*map_fd_unknown_iter);
				printf("fd : %d update session of fd in peer_communication::accept_check\n",*map_fd_unknown_iter);

				map_fd_info_iter->second->session_id = session_id;

				/*
				bind to peer_com~ object
				*/
				_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"bind to peer_com in peer_communication::accept_check fd : ",*map_fd_unknown_iter);
				_net_ptr->set_nonblocking(map_fd_info_iter->first);

				_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(map_fd_info_iter->first, dynamic_cast<basic_class *> (this));
				_peer_mgr_ptr->fd_list_ptr->push_back(map_fd_info_iter->first);

				_io_accept_ptr->map_fd_unknown.erase(map_fd_unknown_iter);
				break;
			}
		
	}

	_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"accept_check end ");


	/*
	call nat accept check
	*/
}

//flag 0 public ip flag 1 private ip //caller 0 rescue peer caller 1 
// pid: 對方的pid
int peer_communication::non_blocking_build_connection(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long pid, int flag, unsigned long session_id)
{	
	struct sockaddr_in peer_saddr;
	int retVal;
	int _sock;
	
	_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid, "flag =", flag);

	// Check is there any connection already built
	if (CheckConnectionExist(caller, level_info_ptr->pid) == 1) {
		return 1;
	}
	
	if ((_sock = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);
		
		_net_ptr->set_nonblocking(_sock);

	}
	
	_net_udp_ptr->set_nonblocking(_sock);
	
	//map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(_sock);
	if (map_fd_NonBlockIO.find(_sock) != map_fd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	map_fd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_fd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_fd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_fd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
	map_fd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingSendCtrl.recv_ctl_info.ctl_state = READY;
	_net_ptr ->set_nonblocking(_sock);		//non-blocking connect
	memset((struct sockaddr_in*)&peer_saddr, 0, sizeof(struct sockaddr_in));

    if (flag == 1) {	
	    peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
		//_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"public_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	else if (flag == 0) {	//in the same NAT
		peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
		//debug_printf("connect to private_ip %s  port= %d \n", inet_ntoa(ip),level_info_ptr->tcp_port);	
		//_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"private_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	
	
	peer_saddr.sin_port = htons(level_info_ptr->tcp_port);
	peer_saddr.sin_family = AF_INET;
	
	_log_ptr->write_log_format("s(u) s d s s(s) d \n", __FUNCTION__, __LINE__,
												"Connecting to pid", level_info_ptr->pid, 
												"IP", inet_ntoa(*(struct in_addr *)&level_info_ptr->public_ip), inet_ntoa(*(struct in_addr *)&level_info_ptr->private_ip), 
												level_info_ptr->tcp_port);
	
	if ((retVal = ::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
		if (socketErr == WSAEWOULDBLOCK) {
			//_net_ptr->set_nonblocking(_sock);
			_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_ptr));
			_peer_mgr_ptr->fd_list_ptr->push_back(_sock);	
			//_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"build_ connection failure : WSAEWOULDBLOCK");
		}
		else {

			::closesocket(_sock);
			
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "[ERROR] Build connection failed", retVal, socketErr);
			debug_printf("[ERROR] Build connection failed %d %d \n", retVal, socketErr);
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed", retVal, socketErr);
			_logger_client_ptr->log_exit();
		}	
#else
		int socketErr = errno;
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "[ERROR] Build connection failed", retVal, socketErr);
		debug_printf("[ERROR] Build connection failed %d %d \n", retVal, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed", retVal, socketErr);
		_logger_client_ptr->log_exit();
		::close(_sock);
#endif
		

	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[DEBUG] Build connection too fast", retVal);
		debug_printf("[DEBUG] Build connection too fast %d \n", retVal);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Build connection too fast", retVal);
		_logger_client_ptr->log_exit();
	}
	
	_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", pid, inet_ntoa(peer_saddr.sin_addr), ntohs(peer_saddr.sin_port));
	/*
	this part stores the info in each table.
	*/
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect (before) fd =", _sock,
																"manifest =", manifest,
																"session_id =", session_id,
																"role =", caller,
																"pid =", pid,
																"non_blocking_build_connection (candidate peer)");

	map_fd_info_iter = map_fd_info.find(_sock);
	if(map_fd_info_iter != map_fd_info.end()){
		
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : fd %d already in map_fd_info in non_blocking_build_connection\n",_sock);
		_logger_client_ptr->log_exit();
		PAUSE
	}
	
	map_fd_info[_sock] = new struct fd_information;
	if (!map_fd_info[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] fd_information new error", __FUNCTION__, __LINE__);
	}

	map_fd_info_iter = map_fd_info.find(_sock);
	if (map_fd_info_iter == map_fd_info.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_info", __FUNCTION__, __LINE__);
	}

	memset(map_fd_info_iter->second, 0, sizeof(struct fd_information));
	map_fd_info_iter->second->role = caller;
	map_fd_info_iter->second->manifest = manifest;
	map_fd_info_iter->second->pid = pid;
	map_fd_info_iter->second->session_id = session_id;
	
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect fd =", map_fd_info_iter->first,
																"manifest =", map_fd_info_iter->second->manifest,
																"session_id =", map_fd_info_iter->second->session_id,
																"role =", map_fd_info_iter->second->role,
																"pid =", map_fd_info_iter->second->pid,
																"non_blocking_build_connection (candidate peer)");
	
	return RET_OK;
}

//flag 0 public ip flag 1 private ip //caller 0 rescue peer caller 1 
// pid: 對方的pid
int peer_communication::non_blocking_build_connection_udp(struct peer_info_t *candidates_info, INT32 caller, UINT32 manifest, UINT32 peer_pid, INT32 flag, UINT32 my_session, UINT32 peercomm_session)
{	
	_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__, "peer_pid", peer_pid, "manifest", manifest, "my_session", my_session, "peercomm_session", peercomm_session);
	
	// Put into queue, and start building connection until timeout
	struct delay_build_connection build_udp_conn_temp;
	memset(&build_udp_conn_temp, 0, sizeof(struct delay_build_connection));
	build_udp_conn_temp.candidates_info = candidates_info;
	
	build_udp_conn_temp.caller = caller;
	build_udp_conn_temp.manifest = manifest;
	build_udp_conn_temp.peer_pid = peer_pid;
	build_udp_conn_temp.flag = flag;
	build_udp_conn_temp.my_session = my_session;
	build_udp_conn_temp.peercomm_session = peercomm_session;
	_log_ptr->timerGet(&(build_udp_conn_temp.timer));

	list_build_connection.push_back(build_udp_conn_temp);

	//mmap_build_udp_conn.insert((pair<int, build_udp_conn>(session_id, build_udp_conn_temp)));
	
	return RET_OK;
}
 
//int peer_communication::non_blocking_build_connection_udp_now(struct build_udp_conn build_udp_conn_temp)
int peer_communication::non_blocking_build_connection_udp_now(struct peer_info_t *candidates_info, INT32 caller, UINT32 manifest, UINT32 peer_pid, INT32 flag, UINT32 my_session, UINT32 peercomm_session, INT32 bctype)
{
	int retVal;
	int _sock;		// UDP socket
	char public_IP[16] = { 0 };
	char private_IP[16] = { 0 };

	//int caller = build_udp_conn_temp.caller;
	//unsigned long manifest = level_info_ptr->;
	//unsigned long pid = build_udp_conn_temp.pid;
	//int flag = build_udp_conn_temp.flag;
	//unsigned long session_id = build_udp_conn_temp.session_id;
	struct sockaddr_in peer_saddr;
	peer_saddr.sin_family = AF_INET;
	if (flag == 1) {
		peer_saddr.sin_addr.s_addr = candidates_info->public_ip;
		peer_saddr.sin_port = htons(candidates_info->public_port);
	}
	else {
		// LAN connection
		peer_saddr.sin_addr.s_addr = candidates_info->private_ip;
		peer_saddr.sin_port = htons(candidates_info->private_port);
	}
	//memcpy(&peer_saddr, &build_udp_conn_temp.peer_saddr, sizeof(struct sockaddr_in));

	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", peer_pid);

	
	if ((_sock = UDT::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);
		return RET_OK;
	}

	_net_udp_ptr->set_nonblocking(_sock);

	_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", peer_pid, inet_ntoa(peer_saddr.sin_addr), ntohs(peer_saddr.sin_port));

	string svc_udp_port;
	_prep->read_key("svc_udp_port", svc_udp_port);
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_udp_port.c_str()));

	if (UDT::bind(_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == UDT::ERROR) {
		debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Bind socket failed", __FUNCTION__, __LINE__);
		//log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "ErrCode:", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		PAUSE
	}

	_log_ptr->timerGet(&(candidates_info->time_start));
	if (UDT::ERROR == UDT::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) {
		cout << "connect: " << UDT::getlasterror().getErrorMessage() << "  " << UDT::getlasterror().getErrorCode();
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Connect failed", __FUNCTION__, __LINE__);
		PAUSE
	}

	struct timerStruct new_timer;
	_log_ptr->timerGet(&new_timer);
	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "sock", _sock, "clocktime", new_timer.clockTime, "ticktime", new_timer.tickTime, new_timer.tickTime.QuadPart);

	if (UDT::getsockstate(_sock) == CONNECTING || UDT::getsockstate(_sock) == CONNECTED) {
		_net_udp_ptr->epoll_control(_sock, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
		if (bctype == 0) {
			_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_udp_ptr));
		}
		else {
			_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_udp_ctrl_ptr));
		}
		//_peer_mgr_ptr->fd_udp_list_ptr->push_back(_sock);	
		_net_udp_ptr->fd_list_ptr->push_back(_sock);
	}
	else {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed. state =", UDT::getsockstate(_sock));
		debug_printf("[ERROR] Build connection failed. state = %d \n", UDT::getsockstate(_sock));
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] Build connection failed. state", UDT::getsockstate(_sock));
		_logger_client_ptr->log_exit();
		PAUSE
	}

	
	// This part stores the info in each table.
	
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s d \n", __FUNCTION__, __LINE__,
		"fd", _sock,
		"manifest", manifest,
		"my_session", my_session,
		"peercomm_session", peercomm_session,
		"my role", caller,
		"peer_pid", peer_pid);

	// Create map_udpfd_NonBlockIO table
	if (map_udpfd_NonBlockIO.find(_sock) != map_udpfd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	map_udpfd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_udpfd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_udpfd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingSendCtrl.recv_ctl_info.ctl_state = READY;

	// Add socket to map_mysession_candidates table
	candidates_info->sock = _sock;

	/*
	// Create map_udpfd_info table
	if (map_udpfd_info.find(_sock) != map_udpfd_info.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] found map_udpfd_info", __FUNCTION__, __LINE__);
	}
	map_udpfd_info[_sock] = new struct fd_information;
	if (!map_udpfd_info[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] fd_information new error", __FUNCTION__, __LINE__);
	}
	map_udpfd_info_iter = map_udpfd_info.find(_sock);
	memset(map_udpfd_info[_sock], 0, sizeof(struct fd_information));

	map_udpfd_info[_sock]->role = caller == CHILD_PEER ? PARENT_PEER : CHILD_PEER;
	map_udpfd_info[_sock]->manifest = manifest;
	map_udpfd_info[_sock]->pid = peer_pid;
	map_udpfd_info[_sock]->session_id = my_session;
	map_udpfd_info[_sock]->peercomm_session = peercomm_session;

	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
		"non blocking connect fd =", _sock,
		"manifest =", map_udpfd_info[_sock]->manifest,
		"session_id =", map_udpfd_info[_sock]->session_id,
		"role =", map_udpfd_info[_sock]->role,
		"pid =", map_udpfd_info[_sock]->pid,
		"non_blocking_build_connection (candidate peer)");
	*/
	return RET_OK;
}


int peer_communication::fake_conn_udp(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long pid, int flag, unsigned long session_id)
{
	int retVal;
	int _sock;		// UDP socket
	char public_IP[16] = { 0 };
	char private_IP[16] = { 0 };
	struct sockaddr_in peer_saddr;

	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid);


	peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
	peer_saddr.sin_port = htons(level_info_ptr->public_port);
	peer_saddr.sin_family = AF_INET;

	if ((_sock = UDT::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);

		_net_ptr->set_nonblocking(_sock);

		PAUSE
	}

	_net_udp_ptr->set_nonblocking(_sock);

	string svc_udp_port;
	_prep->read_key("svc_udp_port", svc_udp_port);
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_udp_port.c_str()));
	if (UDT::bind(_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == UDT::ERROR) {
		debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
		//log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "ErrCode:", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		PAUSE
	}

	if (UDT::ERROR == UDT::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) {
		cout << "connect: " << UDT::getlasterror().getErrorMessage() << "  " << UDT::getlasterror().getErrorCode();
		PAUSE
	}

	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", _sock, "state =", UDT::getsockstate(_sock));
	debug_printf("sock = %d  state = %d \n", _sock, UDT::getsockstate(_sock));
	_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", pid, inet_ntoa(peer_saddr.sin_addr), ntohs(peer_saddr.sin_port));

	UDT::close(_sock);
	return RET_OK;
}

// This function is for RENDEZVOUS connection for both peers behind NAT
// Both peers must connect to each other. Wait until the sockstate is "CONNECTED"
int peer_communication::non_blocking_build_connectionNAT_udp(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long pid, int flag, unsigned long session_id)
{
	/*
	struct sockaddr_in peer_saddr;
	int retVal;
	int _sock;		// UDP socket
	char public_IP[16] = { 0 };
	char private_IP[16] = { 0 };


	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid);

	// Check is there any connection already built
	if (CheckConnectionExist(caller, level_info_ptr->pid) == 1) {
		return 1;
	}

	if ((_sock = UDT::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);

		_net_ptr->set_nonblocking(_sock);

		PAUSE
	}

	_net_udp_ptr->set_nonblocking(_sock);
	_net_udp_ptr->set_rendezvous(_sock);

	//map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(_sock);
	if (map_udpfd_NonBlockIO.find(_sock) != map_udpfd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}

	map_udpfd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_udpfd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_udpfd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingSendCtrl.recv_ctl_info.ctl_state = READY;
	
	memset((struct sockaddr_in*)&peer_saddr, 0, sizeof(struct sockaddr_in));
	peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
	peer_saddr.sin_port = htons(level_info_ptr->udp_port);
	peer_saddr.sin_family = AF_INET;

	_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", level_info_ptr->pid, inet_ntoa(peer_saddr.sin_addr), level_info_ptr->udp_port);

	string svc_udp_port;
	_prep->read_key("svc_udp_port", svc_udp_port);
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_udp_port.c_str()));
	debug_printf("----- port : %d \n", (unsigned short)atoi(svc_udp_port.c_str()));
	if (UDT::bind(_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == UDT::ERROR) {
		debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
		//log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "ErrCode:", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		PAUSE
	}

	if (UDT::ERROR == UDT::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) {
		cout << "connect: " << UDT::getlasterror().getErrorMessage() << "  " << UDT::getlasterror().getErrorCode();
		PAUSE
	}

	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", _sock, "state =", UDT::getsockstate(_sock));
	debug_printf("sock = %d  state = %d \n", _sock, UDT::getsockstate(_sock));


	if (caller == CHILD_PEER) {
		if (UDT::getsockstate(_sock) == CONNECTING || UDT::getsockstate(_sock) == CONNECTED) {
			_net_udp_ptr->epoll_control(_sock, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
			_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_udp_ptr));
			//_peer_mgr_ptr->fd_udp_list_ptr->push_back(_sock);	
			_net_udp_ptr->fd_list_ptr->push_back(_sock);
		}
		else {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed. state =", UDT::getsockstate(_sock));
			debug_printf("[ERROR] Build connection failed. state = %d \n", UDT::getsockstate(_sock));
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] Build connection failed. state", UDT::getsockstate(_sock));
			_logger_client_ptr->log_exit();
			PAUSE
		}

		
		// Stores the info in each table.
		_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
			"non blocking connect (before) fd =", _sock,
			"manifest =", manifest,
			"session_id =", session_id,
			"my role =", caller,
			"pid =", pid,
			"non_blocking_build_connection (candidate peer)");

		if (map_udpfd_info.find(_sock) != map_udpfd_info.end()) {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "error : fd %d already in map_udpfd_info in non_blocking_build_connection\n", _sock);
			_logger_client_ptr->log_exit();
			PAUSE
		}

		map_udpfd_info[_sock] = new struct fd_information;
		if (!map_udpfd_info[_sock]) {
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] fd_information new error", __FUNCTION__, __LINE__);
		}

		map_udpfd_info_iter = map_udpfd_info.find(_sock);

		memset(map_udpfd_info_iter->second, 0, sizeof(struct fd_information));
		map_udpfd_info_iter->second->role = caller == CHILD_PEER ? PARENT_PEER : CHILD_PEER;
		map_udpfd_info_iter->second->manifest = manifest;
		map_udpfd_info_iter->second->pid = pid;
		map_udpfd_info_iter->second->session_id = session_id;

		_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
			"non blocking connect fd =", map_udpfd_info_iter->first,
			"manifest =", map_udpfd_info_iter->second->manifest,
			"session_id =", map_udpfd_info_iter->second->session_id,
			"role =", map_udpfd_info_iter->second->role,
			"pid =", map_udpfd_info_iter->second->pid,
			"non_blocking_build_connection (candidate peer)");
	}
	else {
		if (UDT::getsockstate(_sock) == CONNECTING || UDT::getsockstate(_sock) == CONNECTED) {
			_net_udp_ptr->epoll_control(_sock, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
			_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_nonblocking_udp_ptr));
			//_peer_mgr_ptr->fd_udp_list_ptr->push_back(_sock);	
			_net_udp_ptr->fd_list_ptr->push_back(_sock);
		}
		else {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed. state =", UDT::getsockstate(_sock));
			debug_printf("[ERROR] Build connection failed. state = %d \n", UDT::getsockstate(_sock));
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] Build connection failed. state", UDT::getsockstate(_sock));
			_logger_client_ptr->log_exit();
			PAUSE
		}

		// Check wheather new_fd is already in the map_udpfd_NonBlockIO or not
		
		struct ioNonBlocking* ioNonBlocking_ptr = new struct ioNonBlocking;
		if (!ioNonBlocking_ptr) {
			debug_printf("ioNonBlocking_ptr new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "ioNonBlocking_ptr new error");
			PAUSE
		}

		_log_ptr->write_log_format("s(u) s d s \n", __FUNCTION__, __LINE__, "Add fd", _sock, "into map_udpfd_NonBlockIO");

		memset(ioNonBlocking_ptr, 0, sizeof(struct ioNonBlocking));
		ioNonBlocking_ptr->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
		map_udpfd_NonBlockIO[_sock] = ioNonBlocking_ptr;
		
	}
	*/
	return RET_OK;
}

// Called by childen when the connection is build from parent
void peer_communication::WaitForParentConn(unsigned long parent_pid, unsigned long manifest, unsigned long session_id)
{
	struct fd_information *temp = new struct fd_information;
	temp->role = PARENT_PEER;
	temp->manifest = manifest;
	temp->pid = parent_pid;
	temp->session_id = session_id;
	conn_from_parent_list.push_back(temp);
	_log_ptr->write_log_format("s(u) d d d \n", __FUNCTION__, __LINE__, session_id, parent_pid, manifest);
}

// If it is able to not build connection, return 1
int peer_communication::CheckConnectionExist(int caller, unsigned long pid)
{
	if (caller == CHILD_PEER) {
		map<unsigned long, int>::iterator map_pid_fd_iter;
		
		if (_pk_mgr_ptr->map_pid_parent.find(pid) != _pk_mgr_ptr->map_pid_parent.end()) {
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "parent pid", pid, "is already in map_pid_parent");
			return 1;
		}
		
		if (_pk_mgr_ptr->map_pid_parent_temp.find(pid) != _pk_mgr_ptr->map_pid_parent_temp.end()) {
			if (_pk_mgr_ptr->map_pid_parent_temp.count(pid) > 1) {
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "parent pid", pid, "is already in map_pid_parent_temp");
				return 1;
			}
		}
		
		/*
		// 之前已經建立過連線的 在map_in_pid_fd裡面 則不再建立(保證對同個parent不再建立第二條線)
		// map_in_pid_fd: parent-peer which alreay established connection, including temp parent-peer
		if (_peer_ptr->map_in_pid_fd.find(pid) != _peer_ptr->map_in_pid_fd.end()) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "fd already in map_in_pid_fd in non_blocking_build_connection (rescue peer)");
			return 1;
		}
		//for(map_pid_fd_iter = _peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
		//	if(map_pid_fd_iter->first == pid ){
		//		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "fd already in map_in_pid_fd in non_blocking_build_connection (rescue peer)");
		//		return 1;
		//	}
		//}
		
		// this may have problem
		// map_pid_parent_temp: temp parent-peer
		if (_pk_mgr_ptr->map_pid_parent_temp.find(pid) != _pk_mgr_ptr->map_pid_parent_temp.end()) {
			//兩個以上就沿用第一個的連線
			if(_pk_mgr_ptr ->map_pid_parent_temp.count(pid) >= 2 ){
				return 1;
			}
		}

		// 若在map_pid_parent 則不再次建立連線
		// map_pid_parent: real parent-peer
		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_parent.find(pid);
		if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_parent.end()){
			printf("pid =%d already in connect find in map_pid_parent",pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",pid,"already in connect find in map_pid_parent");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_parent in non_blocking_build_connection (rescue peer)");
			return 1;
		}
		*/
	}
	else if (caller == PARENT_PEER) {
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		multimap<unsigned long, struct peer_info_t *>::iterator map_pid_child_temp_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_info_t *>::iterator map_pid_child1_iter;

		
		
		if(_pk_mgr_ptr->map_pid_child_temp.find(pid) != _pk_mgr_ptr->map_pid_child_temp.end()){
			if (_pk_mgr_ptr->map_pid_child_temp.count(pid) > 1) {
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "child pid", pid, "is already in map_pid_child_temp");
				return 1;
			}
		}
		
		/*
		//之前已經建立過連線的 在map_out_pid_fd裡面 則不再建立(保證對同個child不再建立第二條線)
		for(map_pid_fd_iter = _peer_ptr->map_out_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_out_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == pid ){
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_out_pid_fd in non_blocking_build_connection (candidate peer)");
				return 1;
			}
		}

		//若在map_pid_child1 則不再次建立連線
		map_pid_child1_iter = _pk_mgr_ptr ->map_pid_child1.find(pid);
		if(map_pid_child1_iter != _pk_mgr_ptr ->map_pid_child1.end()){
			printf("pid =%d already in connect find in map_pid_child1",pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",pid,"already in connect find in map_pid_child1");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_child1 in non_blocking_build_connection (candidate peer)");\
			return 1;
		}


		map_pid_child_temp_iter = _pk_mgr_ptr ->map_pid_child_temp.find(pid);
		if(map_pid_child_temp_iter !=  _pk_mgr_ptr ->map_pid_child_temp.end()){
			if(_pk_mgr_ptr ->map_pid_child_temp.count(pid) >=2){
				printf("pid =%d  already in connect find in map_pid_child_temp  testing",pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",pid,"already in connect find in map_pid_child_temp ");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_child_temp in non_blocking_build_connection (candidate peer)");	
				return 1;
			}
		}
		*/
	}
	return 0;
}

io_accept * peer_communication::get_io_accept_handler()
{
	return _io_accept_ptr;
}

void peer_communication::fd_close(int sock){
	_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"close in peer_communication::fd_close fd ",sock);
	_net_ptr->close(sock);

	list<int>::iterator fd_iter;
	for(fd_iter = _peer_ptr->fd_list_ptr->begin(); fd_iter != _peer_ptr->fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == sock) {
			_peer_ptr->fd_list_ptr->erase(fd_iter);
			break;
		}
	}

	map_fd_info_iter = map_fd_info.find(sock);
	if(map_fd_info_iter == map_fd_info.end()){
		//_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"error : close cannot find table in peer_communication::fd_close fd ",sock);
		//_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : close cannot find table in peer_communication::fd_close fd ",sock);
		//_logger_client_ptr->log_exit();
		
	}
	else{
		delete map_fd_info_iter->second;
		map_fd_info.erase(sock);
	}

	map_fd_NonBlockIO_iter= map_fd_NonBlockIO.find(sock);
	if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
		delete map_fd_NonBlockIO_iter->second;
		map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);
	
	}



	list<int>::iterator map_fd_unknown_iter;

	for(map_fd_unknown_iter = _io_accept_ptr->map_fd_unknown.begin();map_fd_unknown_iter != _io_accept_ptr->map_fd_unknown.end();map_fd_unknown_iter++){
		if( sock == *map_fd_unknown_iter){
			_io_accept_ptr->map_fd_unknown.erase(map_fd_unknown_iter);
			break;
		}
	}
}

// Remove certain iterator in "session_id_candidates_set", "map_fd_info", "and map_fd_NonBlockIO"
// When connect time triggered, this function will be called
void peer_communication::stop_attempt_connect(unsigned long stop_session_id)
{
	/*
	int delete_fd_flag = 0;
	map<int, struct fd_information *>::iterator map_fd_info_iter;
	map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
	
	session_id_candidates_set_iter = session_id_candidates_set.find(stop_session_id);
	if(session_id_candidates_set_iter == session_id_candidates_set.end()){
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in session_id_candidates_set", __FUNCTION__, __LINE__);
		return ;
	}
	
	if (session_id_candidates_set_iter->second->role == CHILD_PEER) {	//caller is rescue peer
		//total_manifest = total_manifest & (~session_id_candidates_set_iter->second->manifest);
		_log_ptr->write_log_format("s =>u s d s d s d s d\n", __FUNCTION__,__LINE__,"session_id : ",stop_session_id,", manifest : ",session_id_candidates_set_iter->second->manifest,", role: ",session_id_candidates_set_iter->second->role,", list_number: ",session_id_candidates_set_iter->second->peer_num);
		
		// Remove the session id in session_id_candidates_set
		for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "pid", session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}
		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
		session_id_candidates_set.erase(session_id_candidates_set_iter);

		

		map_fd_info_iter = map_fd_info.begin();
		while(map_fd_info_iter != map_fd_info.end()){
			if(map_fd_info_iter->second->session_id == stop_session_id){

				delete_fd_flag = 1;

				_log_ptr->write_log_format("s =>u s d s u \n", __FUNCTION__, __LINE__,
					"connect faild delete table and close fd", map_fd_info_iter->first,
					"pid", map_fd_info_iter->second->pid);

				if(_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()){
					
					//connect faild delete table and close fd
					


					_log_ptr->write_log_format("s =>u s d s u \n", __FUNCTION__, __LINE__,
																"connect faild delete table and close fd", map_fd_info_iter->first,
																"pid", map_fd_info_iter->second->pid);
					
					//close fd
					
					list<int>::iterator fd_iter;

					//_log_ptr->write_log_format("s => s \n", (char*)__PRETTY_FUNCTION__, "peer_com");
					cout << "peer_com close fd since timeout " << map_fd_info_iter->first <<  endl;
//						_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_DEL, 0);
					_net_ptr->close(map_fd_info_iter->first);

					for(fd_iter = _peer_mgr_ptr->fd_list_ptr->begin(); fd_iter != _peer_mgr_ptr->fd_list_ptr->end(); fd_iter++) {
						if(*fd_iter == map_fd_info_iter->first) {
							_peer_mgr_ptr->fd_list_ptr->erase(fd_iter);
							break;
						}
					}
				}
				else{
					
					//connect succeed just delete table
					
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"connect succeed just delete table");
					
				}
				
				map_fd_NonBlockIO_iter= map_fd_NonBlockIO.find(map_fd_info_iter->first);
				if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
					delete map_fd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);

				}


				delete [] map_fd_info_iter ->second;
				map_fd_info.erase(map_fd_info_iter);
				map_fd_info_iter = map_fd_info.begin();
			}
			else{
				map_fd_info_iter++;
			}
		}

	}
	else if (session_id_candidates_set_iter->second->role == PARENT_PEER){	//caller is candidate
		_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
															"Stop session_id", stop_session_id,
															"my role", session_id_candidates_set_iter->second->role,
															"manifest", session_id_candidates_set_iter->second->manifest,
															"list_number", session_id_candidates_set_iter->second->peer_num);
		for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "list pid : ", session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}

		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
		session_id_candidates_set.erase(session_id_candidates_set_iter);

		// Remove certain iterator in map_fd_info and map_fd_NonBlockIO
		map_fd_info_iter = map_fd_info.begin();
		while (map_fd_info_iter != map_fd_info.end()) {
			if (map_fd_info_iter->second->session_id == stop_session_id) {

				delete_fd_flag = 1;

				if (_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()) {
					
					//connect faild delete table and close fd
					
					

					_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"connect faild delete table and close fd ",map_fd_info_iter->first);
					
					//close fd
					
					list<int>::iterator fd_iter;

					_log_ptr->write_log_format("s => s \n", (char*)__PRETTY_FUNCTION__, "peer_com");
					cout << "peer_com close fd since timeout " << map_fd_info_iter->first <<  endl;
//						_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_DEL, 0);
					_net_ptr->close(map_fd_info_iter->first);

					for(fd_iter = _peer_mgr_ptr->fd_list_ptr->begin(); fd_iter != _peer_mgr_ptr->fd_list_ptr->end(); fd_iter++) {
						if(*fd_iter == map_fd_info_iter->first) {
							_peer_mgr_ptr->fd_list_ptr->erase(fd_iter);
							break;
						}
					}
				}
				else{
					
					//connect succeed just delete table
					
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"connect succeed just delete table ");
					

				}

				map_fd_NonBlockIO_iter= map_fd_NonBlockIO.find(map_fd_info_iter->first);
				if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
					delete map_fd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);

				}

				delete map_fd_info_iter->second;
				map_fd_info.erase(map_fd_info_iter);
				map_fd_info_iter = map_fd_info.begin();
			}
			else{
				map_fd_info_iter++;
			}
		}
	}
	else {
	
	}

	//map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
	if(delete_fd_flag==0){
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"cannot find fd info table ");
	}
	else{
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"delete fd info table");
	}

	for(map_fd_info_iter = map_fd_info.begin();map_fd_info_iter != map_fd_info.end();map_fd_info_iter++){
		_log_ptr->write_log_format("s =>u s d s d \n", __FUNCTION__,__LINE__,"fd : ",map_fd_info_iter->first,", pid: ",map_fd_info_iter->second->pid);
	}
	*/
}

// 限定 child 呼叫
// 挑選順序:
// 1. 從 stable_list 挑選
// 2. 從已建立連線之 parent 挑選
// 3. 從 warning_list 挑選
// 4. 從 dangerous_list 挑選
void peer_communication::SelectStrategy(UINT32 my_session)
{
	bool selection_done = false;	// 讓被選中的 parent 只有一個
	UINT32 selected_pid = -1;
	UINT32 selected_est_delay = 100000;
	list<UINT32> stable_list;
	list<UINT32> warning_list;
	list<UINT32> dangerous_list;
	list<UINT32> overloading_list;
	map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter;
	map_mysession_candidates_iter = map_mysession_candidates.find(my_session);
	if (map_mysession_candidates_iter == map_mysession_candidates.end()){
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_mysession_candidates", __FUNCTION__, __LINE__);
		return;
	}

	
	// 新的PS找法
	for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
		int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
		int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
		int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
		int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

		_log_ptr->write_log_format("s(u) s u s u s u s u(u)(u)(u) s u(u)(u)(u) \n", __FUNCTION__, __LINE__,
			"parent", peer_pid,
			"my_session", my_session,
			"peercomm_session", map_mysession_candidates_iter->second->p_candidates_info[i].peercomm_session,
			"sock1", peer_fd1, map_mysession_candidates_iter->second->p_candidates_info[i].connection_state, map_mysession_candidates_iter->second->p_candidates_info[i].estimated_delay, map_mysession_candidates_iter->second->p_candidates_info[i].PS_class,
			"sock2", peer_fd2, map_mysession_candidates_iter->second->n_candidates_info[i].connection_state, map_mysession_candidates_iter->second->n_candidates_info[i].estimated_delay, map_mysession_candidates_iter->second->n_candidates_info[i].PS_class);
		

		if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_CONNECTED || map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_CONNECTED) {
			struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
			if (parent_info != NULL) {
				// 必須確定有在 map_pid_parent/map_pid_child 找到，因為 table 隨時可能因為收到 SEED 而被清除
				if (map_mysession_candidates_iter->second->p_candidates_info[i].PS_class == PS_STABLE || map_mysession_candidates_iter->second->n_candidates_info[i].PS_class == PS_STABLE) {
					_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "PS_STABLE");
					stable_list.push_back(peer_pid);
				}
				else if (map_mysession_candidates_iter->second->p_candidates_info[i].PS_class == PS_WARNING || map_mysession_candidates_iter->second->n_candidates_info[i].PS_class == PS_WARNING) {
					_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "PS_WARNING");
					warning_list.push_back(peer_pid);
				}
				else if (map_mysession_candidates_iter->second->p_candidates_info[i].PS_class == PS_DANGEROUS || map_mysession_candidates_iter->second->n_candidates_info[i].PS_class == PS_DANGEROUS) {
					_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "PS_DANGEROUS");
					dangerous_list.push_back(peer_pid);
				}
				else if (map_mysession_candidates_iter->second->p_candidates_info[i].PS_class == PS_OVERLOADING || map_mysession_candidates_iter->second->n_candidates_info[i].PS_class == PS_OVERLOADING) {
					_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "PS_OVERLOADING");
					overloading_list.push_back(peer_pid);
				}
			}
		}
	}
	
	// 1. 先從 stable_list 中挑選 parent (找 estimated_delay 最低的)
	for (list<UINT32>::iterator iter = stable_list.begin(); iter != stable_list.end(); iter++) {
		for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
			int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
			int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
			int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
			int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

			if (peer_pid == *iter) {
				if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_CONNECTED) {
					if (selected_est_delay > map_mysession_candidates_iter->second->p_candidates_info[i].estimated_delay) {
						selected_pid = peer_pid;
						selection_done = true;
					}
				}
				else if (map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_CONNECTED) {
					if (selected_est_delay > map_mysession_candidates_iter->second->n_candidates_info[i].estimated_delay) {
						selected_pid = peer_pid;
						selection_done = true;
					}
				}
			}
		}
	}

	// 2. 都沒選到再從已建立連線之 parent 中挑選 parent (找 estimated_delay 最低的)
	if (selection_done == false) {
		for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
			int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
			int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
			int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
			int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

			struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
			if (parent_info != NULL) {
				if (parent_info->peerInfo.connection_state == PEER_SELECTED) {
					selected_pid = peer_pid;
					selection_done = true;
				}
			}
		}
	}

	// 都沒選到再從 warning_list 中挑選 parent (找 estimated_delay 最低的)
	if (selection_done == false) {
		for (list<UINT32>::iterator iter = warning_list.begin(); iter != warning_list.end(); iter++) {
			for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
				int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
				int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
				int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
				int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

				if (peer_pid == *iter) {
					if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_CONNECTED) {
						if (selected_est_delay > map_mysession_candidates_iter->second->p_candidates_info[i].estimated_delay) {
							selected_pid = peer_pid;
							selection_done = true;
							selected_est_delay = map_mysession_candidates_iter->second->p_candidates_info[i].estimated_delay;
						}
					}
					else if (map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_CONNECTED) {
						if (selected_est_delay > map_mysession_candidates_iter->second->n_candidates_info[i].estimated_delay) {
							selected_pid = peer_pid;
							selection_done = true;
							selected_est_delay = map_mysession_candidates_iter->second->n_candidates_info[i].estimated_delay;
						}
					}
				}
			}
		}
	}


	// 都沒選到再從 dangerous_list 中挑選 parent (找 estimated_delay 最低的)
	if (selection_done == false) {
		for (list<UINT32>::iterator iter = dangerous_list.begin(); iter != dangerous_list.end(); iter++) {
			for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
				int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
				int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
				int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
				int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

				if (peer_pid == *iter) {
					if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_CONNECTED) {
						if (selected_est_delay > map_mysession_candidates_iter->second->p_candidates_info[i].estimated_delay) {
							selected_pid = peer_pid;
							selection_done = true;
							selected_est_delay = map_mysession_candidates_iter->second->p_candidates_info[i].estimated_delay;
						}
					}
					else if (map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_CONNECTED) {
						if (selected_est_delay > map_mysession_candidates_iter->second->n_candidates_info[i].estimated_delay) {
							selected_pid = peer_pid;
							selection_done = true;
							selected_est_delay = map_mysession_candidates_iter->second->n_candidates_info[i].estimated_delay;
						}
					}
				}
			}
		}
	}

	_log_ptr->write_log_format("s(u) s d s u \n", __FUNCTION__, __LINE__, "select parent", selected_pid, "in my_session", my_session);
	for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
		int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
		int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
		int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
		int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

		if (selected_pid == peer_pid) {
			if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_CONNECTED) {
				struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
				if (parent_info != NULL) {
					// 必須確定有在 map_pid_parent/map_pid_child 找到，因為 table 隨時可能因為收到 SEED 而被清除
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "select parent", peer_pid, "in my_session", my_session);
					if (peer_fd1 != 0) {
						parent_info->peerInfo.sock = peer_fd1;
					}
					parent_info->peerInfo.connection_state = PEER_SELECTED;
					map_mysession_candidates_iter->second->p_candidates_info[i].connection_state = PEER_SELECTED;
				}
			}
			else if (map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_CONNECTED) {
				struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
				if (parent_info != NULL) {
					// 必須確定有在 map_pid_parent/map_pid_child 找到，因為 table 隨時可能因為收到 SEED 而被清除
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "select parent", peer_pid, "in my_session", my_session);
					if (peer_fd2 != 0) {
						parent_info->peerInfo.sock = peer_fd2;
					}
					parent_info->peerInfo.connection_state = PEER_SELECTED;
					map_mysession_candidates_iter->second->n_candidates_info[i].connection_state = PEER_SELECTED;
				}
			}
		}

		struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
		if (parent_info != NULL) {
			if (parent_info->peerInfo.connection_state != PEER_SELECTED) {
				_pk_mgr_ptr->SetParentManifest(parent_info, parent_info->peerInfo.manifest & ~peer_manifest);
			}
		}
		else {
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] Not Found parent", peer_pid);
		}
	}
	
	/*
	// 最簡單找法
	// 變成 PEER_SELECTED 的 peer 接下來將成為真正 parent，其他的 peer 在這次 peercomm_session 落選
	// 修正落選的 parent manifest 
	for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
		int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
		int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
		int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
		int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

		if (selection_done == false) {
			if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_CONNECTED) {
				_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "parent", peer_pid, "my_session", my_session, "peercomm_session", map_mysession_candidates_iter->second->p_candidates_info[i].peercomm_session);

				if (map_mysession_candidates_iter->second->myrole == CHILD_PEER) {
					struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
					if (parent_info != NULL) {
						// 必須確定有在 map_pid_parent/map_pid_child 找到，因為 table 隨時可能因為收到 SEED 而被清除
						_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "select parent", peer_pid, "in my_session", my_session);
						if (peer_fd1 != 0) {
							parent_info->peerInfo.sock = peer_fd1;
						}
						parent_info->peerInfo.connection_state = PEER_SELECTED;
						map_mysession_candidates_iter->second->p_candidates_info[i].connection_state = PEER_SELECTED;
						selection_done = true;
					}
				}
				else if (map_mysession_candidates_iter->second->myrole == PARENT_PEER) {
					struct peer_info_t *child_info = _pk_mgr_ptr->GetChildFromPid(peer_pid);
					if (child_info != NULL) {
						// 必須確定有在 map_pid_parent/map_pid_child 找到，因為 table 隨時可能因為收到 SEED 而被清除
						_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "select child", peer_pid, "in my_session", my_session);
						if (peer_fd1 != 0) {
							child_info->sock = peer_fd1;
						}
						child_info->connection_state = PEER_SELECTED;
						map_mysession_candidates_iter->second->p_candidates_info[i].connection_state = PEER_SELECTED;
						selection_done = true;
					}
				}
			}
			else if (map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_CONNECTED) {
				_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "Select pid", map_mysession_candidates_iter->second->n_candidates_info[i].pid, "in peercomm_session", map_mysession_candidates_iter->second->n_candidates_info[i].peercomm_session);

				if (map_mysession_candidates_iter->second->myrole == CHILD_PEER) {
					struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
					if (parent_info != NULL) {
						// 必須確定有在 map_pid_parent/map_pid_child 找到，因為 table 隨時可能因為收到 SEED 而被清除
						_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "select parent", peer_pid, "in my_session", my_session);
						if (peer_fd2 != 0) {
							parent_info->peerInfo.sock = peer_fd2;
						}
						parent_info->peerInfo.connection_state = PEER_SELECTED;
						map_mysession_candidates_iter->second->n_candidates_info[i].connection_state = PEER_SELECTED;
						selection_done = true;
					}
				}
				else if (map_mysession_candidates_iter->second->myrole == PARENT_PEER) {
					struct peer_info_t *child_info = _pk_mgr_ptr->GetChildFromPid(peer_pid);
					if (child_info != NULL) {
						// 必須確定有在 map_pid_parent/map_pid_child 找到，因為 table 隨時可能因為收到 SEED 而被清除
						_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "select child", peer_pid, "in my_session", my_session);
						if (peer_fd2 != 0) {
							child_info->sock = peer_fd2;
						}
						child_info->connection_state = PEER_SELECTED;
						map_mysession_candidates_iter->second->n_candidates_info[i].connection_state = PEER_SELECTED;
						selection_done = true;
					}
				}
			}
		}

		struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(peer_pid);
		if (parent_info != NULL) {
			if (parent_info->peerInfo.connection_state != PEER_SELECTED) {
				_pk_mgr_ptr->SetParentManifest(parent_info, parent_info->peerInfo.manifest & ~peer_manifest);
			}
		}
		else {
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] Not Found parent", peer_pid);
		}
	}
	*/
}

// Child 發送這個 CMD 給 parent 表示已認定這個 parent
// 借由此 CMD 使雙方達成共識決定使用哪一條連線
int peer_communication::SendPeerCon(UINT32 my_session)
{
	map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter;
	map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
	Nonblocking_Ctl *Nonblocking_Send_Ctrl_ptr = NULL;
	struct peer_connect_down_t *parent_info = NULL;		// 指向被選中的 parent
	
	map_mysession_candidates_iter = map_mysession_candidates.find(my_session);
	if (map_mysession_candidates_iter == map_mysession_candidates.end()) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Not found in map_mysession_candidates", my_session);
		debug_printf("[ERROR] Not found in map_mysession_candidates %d \n", my_session);
		return 0;		// 放棄這次的 session
	}

	// 找出被選中的 parent
	for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
		if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_SELECTED) {
			parent_info = _pk_mgr_ptr->GetParentFromPid(map_mysession_candidates_iter->second->p_candidates_info[i].pid);
			if (parent_info != NULL) {
				break;
			}
		}
		else if (map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_SELECTED) {
			parent_info = _pk_mgr_ptr->GetParentFromPid(map_mysession_candidates_iter->second->n_candidates_info[i].pid);
			if (parent_info != NULL) {
				break;
			}
		}
	}

	if (parent_info == NULL) {
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "No suitable parents in my_session", my_session);
		return 0;
	}
	
	map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(parent_info->peerInfo.sock);
	if (map_udpfd_NonBlockIO_iter == map_udpfd_NonBlockIO.end()) {
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] target fd", parent_info->peerInfo.sock);
		for (map<int, struct ioNonBlocking*>::iterator iter = map_udpfd_NonBlockIO.begin(); iter != map_udpfd_NonBlockIO.end(); iter++) {
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "udpfd", iter->first);
		}
		//_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_NonBlockIO", __FUNCTION__, __LINE__);
		return 0;		// 放棄這次的 session
	}

	Nonblocking_Send_Ctrl_ptr = &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingRecv);

	if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {

		struct chunk_request_peer_t *chunk_request_ptr = NULL;
		int send_byte;
		unsigned long channel_id;
		string svc_tcp_port("");
		string svc_udp_port("");

		_prep->read_key("channel_id", channel_id);
		_prep->read_key("svc_tcp_port", svc_tcp_port);
		_prep->read_key("svc_udp_port", svc_udp_port);

		chunk_request_ptr = (struct chunk_request_peer_t *)new unsigned char[sizeof(struct chunk_request_peer_t)];
		if (!chunk_request_ptr) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_request_peer_t new error", my_session);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_request_peer_t new error", my_session);
			debug_printf("[ERROR] chunk_request_peer_t new error %d \n", my_session);
			return 0;		// 放棄這次的 session
		}
		memset(chunk_request_ptr, 0, sizeof(struct chunk_request_peer_t));

		chunk_request_ptr->header.cmd = CHNK_CMD_PEER_CON;
		chunk_request_ptr->header.rsv_1 = REQUEST;
		chunk_request_ptr->header.length = sizeof(struct chunk_request_peer_t) - sizeof(struct chunk_header_t);
		chunk_request_ptr->info.pid = _pk_mgr_ptr->my_pid;
		chunk_request_ptr->info.channel_id = channel_id;
		chunk_request_ptr->info.private_ip = _pk_mgr_ptr->my_private_ip;
		chunk_request_ptr->info.tcp_port = (unsigned short)atoi(svc_tcp_port.c_str());
		chunk_request_ptr->info.public_udp_port = (unsigned short)atoi(svc_udp_port.c_str());		// 先暫時隨便給個值
		chunk_request_ptr->info.private_udp_port = (unsigned short)atoi(svc_udp_port.c_str());		// 先暫時隨便給個值

		_net_udp_ptr->set_nonblocking(parent_info->peerInfo.sock);

		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = sizeof(struct chunk_request_peer_t);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = sizeof(struct chunk_request_peer_t);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = chunk_request_ptr->header.sequence_number;

		send_byte = _net_udp_ptr->nonblock_send(parent_info->peerInfo.sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));
		_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__, "Send CHNK_CMD_PEER_CON to pid", parent_info->peerInfo.pid, "sock", parent_info->peerInfo.sock, "in my_session", my_session, "bytes", send_byte);

		if (send_byte < 0) {
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] Send bytes", send_byte, parent_info->peerInfo.sock);
		}
		else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {
			_net_udp_ptr->set_fd_bcptr_map(parent_info->peerInfo.sock, dynamic_cast<basic_class *> (_peer_ptr));
		}

		delete chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
	}
	else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == RUNNING) {
		int send_byte;
		send_byte = _net_udp_ptr->nonblock_send(parent_info->peerInfo.sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));
		/*
		if (_send_byte < 0) {
		if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr) {
		delete Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr;
		}
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
		//data_close(sock, "PEER　COM error",CLOSE_PARENT);
		CloseParent(parent_pid, true, "PEER　COM error");
		return RET_SOCK_ERROR;
		}
		else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY){

		if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr) {
		delete Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr;
		}
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
		return 0;
		}
		*/
	}

	_peer_ptr->InitPeerInfo(parent_info->peerInfo.sock, parent_info->peerInfo.pid, parent_info->peerInfo.manifest, map_mysession_candidates_iter->second->myrole);
	_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_info->peerInfo.manifest, ADD_OP);
}


// Remove certain iterator in "session_id_candidates_set", "map_(udp)fd_info", "map_(udp)fd_NonBlockIO", "(udp)fd_list", and "conn_from_parent_list"
// When connect time triggered, this function will be called
void peer_communication::StopSession(UINT32 my_session)
{
	int delete_fd_flag = 0;
	map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter;

	map_mysession_candidates_iter = map_mysession_candidates.find(my_session);
	if (map_mysession_candidates_iter == map_mysession_candidates.end()){
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_mysession_candidates", __FUNCTION__, __LINE__);
		return;
	}

	_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
		"Stop my_session", my_session,
		"my role", map_mysession_candidates_iter->second->myrole,
		"manifest", map_mysession_candidates_iter->second->manifest,
		"list_number", map_mysession_candidates_iter->second->candidates_num);


	if (map_mysession_candidates_iter->second->myrole == CHILD_PEER) {
		// 刪除此次 my_session 的 map_udp_NonBlockIO(除了被選中的parent) 和 map_mysession_candidates(全部)
		// peer 在 map_pid_parent 中的 manifest 等於 0的話，關閉它的 socket 連線，否則繼續保持連線
		
		// Remove the session id in map_mysession_candidates
		for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
			
			int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
			int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
			int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
			int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

			_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__, "pid", peer_pid, "fd1", peer_fd1, "fd2", peer_fd2, "peercomm_session", map_mysession_candidates_iter->second->p_candidates_info[i].peercomm_session);

			struct peer_connect_down_t *parent_info_ptr = _pk_mgr_ptr->GetParentFromPid(peer_pid);
			if (parent_info_ptr != NULL) {
				if (parent_info_ptr->peerInfo.connection_state != PEER_SELECTED) {

					//_pk_mgr_ptr->set_parent_manifest(parent_info_ptr, parent_info_ptr->peerInfo.manifest & ~peer_manifest);
					if (parent_info_ptr->peerInfo.manifest == 0) {

						// Remove the fd in map_(udp)fd_NonBlockIO，因為雙向建立連線，所以可能需要刪除2個fd
						map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
						for (map<int, struct ioNonBlocking*>::iterator iter = map_udpfd_NonBlockIO.begin(); iter != map_udpfd_NonBlockIO.end(); iter++) {
							_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "fd", iter->first, "size", map_udpfd_NonBlockIO.size());
						}
						map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(peer_fd1);
						if (map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end()){
							delete map_udpfd_NonBlockIO_iter->second;
							map_udpfd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);
						}

						for (map<int, struct ioNonBlocking*>::iterator iter = map_udpfd_NonBlockIO.begin(); iter != map_udpfd_NonBlockIO.end(); iter++) {
							_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "fd", iter->first, "size", map_udpfd_NonBlockIO.size());
						}
						map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(peer_fd2);
						if (map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end()){
							delete map_udpfd_NonBlockIO_iter->second;
							map_udpfd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);
						}

						_peer_ptr->CloseSocketUDP(peer_fd1, false, "Closed by session stopped with manifest = 0");
						_peer_ptr->CloseSocketUDP(peer_fd2, false, "Closed by session stopped with manifest = 0");
						_peer_ptr->CloseParent(parent_info_ptr->peerInfo.pid, parent_info_ptr->peerInfo.sock, false, "Closed by session stopped with manifest = 0");
					}
				}
				else {
					// 如果兩邊都成功建立連線，刪除較慢建成的那條連線
					// 這個 Parent 是被選中的
					_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__, "parent", peer_pid, "fd", parent_info_ptr->peerInfo.sock, "fd1", peer_fd1, "fd2", peer_fd2);
					if (peer_fd1 != parent_info_ptr->peerInfo.sock) {
						_peer_ptr->CloseSocketUDP(peer_fd1, false, "Closed by duplicate connection");
					}
					if (peer_fd2 != parent_info_ptr->peerInfo.sock) {
						_peer_ptr->CloseSocketUDP(peer_fd2, false, "Closed by duplicate connection");
					}
				}
			}
			else {
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] Not Found parent", peer_pid);
			}
		}
		delete map_mysession_candidates_iter->second->p_candidates_info;
		delete map_mysession_candidates_iter->second->n_candidates_info;
		delete map_mysession_candidates_iter->second;
		map_mysession_candidates.erase(map_mysession_candidates_iter);

		// Remove the conn_from_parent_list of that session id
		for (list<struct fd_information *>::iterator iter = conn_from_parent_list.begin(); iter != conn_from_parent_list.end();) {
			if ((*iter)->session_id == my_session) {
				delete (*iter);
				conn_from_parent_list.erase(iter);
				iter = conn_from_parent_list.begin();
			}
			else {
				iter++;
			}
		}
	}
	else if (map_mysession_candidates_iter->second->myrole == PARENT_PEER) {
		
		// 刪除此次 my_session 的 map_udp_NonBlockIO(全部，因為 fd 已經轉移至 peer class) 和 map_mysession_candidates(全部)
		// peer 在 map_pid_parent 中的 manifest 等於 0的話，關閉它的 socket 連線，否則繼續保持連線

		// Remove the session id in map_mysession_candidates
		for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
			
			int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
			int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
			int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
			int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

			_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "peer_pid", peer_pid, "fd1", peer_fd1, "fd2", peer_fd2);

			// Remove the fd in map_(udp)fd_NonBlockIO，因為雙向建立連線，所以可能需要刪除2個fd
			map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
			map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(peer_fd1);
			if (map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end()){
				delete map_udpfd_NonBlockIO_iter->second;
				map_udpfd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);
			}
			map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(peer_fd2);
			if (map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end()){
				delete map_udpfd_NonBlockIO_iter->second;
				map_udpfd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);
			}

			struct peer_info_t *child_info = _pk_mgr_ptr->GetChildFromPid(peer_pid);
			if (child_info != NULL) {
				if (child_info->connection_state != PEER_SELECTED) {
					//child_info_ptr->manifest &= ~peer_manifest;
					if (child_info->manifest == 0) {
						// Close socket
						_peer_ptr->CloseSocketUDP(peer_fd1, false, "Closed by session stopped with manifest = 0");
						_peer_ptr->CloseSocketUDP(peer_fd2, false, "Closed by session stopped with manifest = 0");
						_peer_ptr->CloseChild(child_info->pid, child_info->sock, false, "Closed by session stopped with manifest = 0");
					}
				}
				else {
					// 如果兩邊都成功建立連線，刪除較慢建成的那條連線
					_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__, "parent", peer_pid, "fd", child_info->sock, "fd1", peer_fd1, "fd2", peer_fd2);
					if (peer_fd1 != child_info->sock) {
						_peer_ptr->CloseSocketUDP(peer_fd1, false, "Closed by duplicate connection");
					}
					if (peer_fd2 != child_info->sock) {
						_peer_ptr->CloseSocketUDP(peer_fd2, false, "Closed by duplicate connection");
					}
				}
			}
			else {
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] Not Found child", peer_pid);
			}

		}
		delete map_mysession_candidates_iter->second->p_candidates_info;
		delete map_mysession_candidates_iter->second->n_candidates_info;
		delete map_mysession_candidates_iter->second;
		map_mysession_candidates.erase(map_mysession_candidates_iter);
	}

	for (map<int, struct fd_information *>::iterator map_fd_info_iter = map_fd_info.begin(); map_fd_info_iter != map_fd_info.end(); map_fd_info_iter++){
		_log_ptr->write_log_format("s =>u s d s d \n", __FUNCTION__, __LINE__, "fd : ", map_fd_info_iter->first, ", pid: ", map_fd_info_iter->second->pid);
	}
	for (map<int, struct fd_information *>::iterator map_udpfd_info_iter = map_fd_info.begin(); map_udpfd_info_iter != map_fd_info.end(); map_udpfd_info_iter++){
		_log_ptr->write_log_format("s =>u s d s d \n", __FUNCTION__, __LINE__, "fd : ", map_udpfd_info_iter->first, ", pid: ", map_udpfd_info_iter->second->pid);
	}
	/*
	int delete_fd_flag = 0;
	map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;

	session_id_candidates_set_iter = session_id_candidates_set.find(session_id);
	if (session_id_candidates_set_iter == session_id_candidates_set.end()){
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in session_id_candidates_set", __FUNCTION__, __LINE__);
		return;
	}

	_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
														"Stop session", session_id,
														"my role", session_id_candidates_set_iter->second->role,
														"manifest", session_id_candidates_set_iter->second->manifest,
														"list_number", session_id_candidates_set_iter->second->peer_num);


	if (session_id_candidates_set_iter->second->role == CHILD_PEER) {
		
		// Remove the session id in session_id_candidates_set
		for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "pid", session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}
		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
		session_id_candidates_set.erase(session_id_candidates_set_iter);

		// TCP part
		for (map<int, struct fd_information *>::iterator map_fd_info_iter = map_fd_info.begin(); map_fd_info_iter != map_fd_info.end();) {
			if (map_fd_info_iter->second->session_id == session_id) {

				_log_ptr->write_log_format("s(u) s d s u \n", __FUNCTION__, __LINE__,
															"connect faild delete table and close fd", map_fd_info_iter->first,
															"pid", map_fd_info_iter->second->pid);

				if (_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()) {
				
					_net_ptr->close(map_fd_info_iter->first);

					list<int>::iterator iter = std::find(_net_ptr->fd_list_ptr->begin(), _net_ptr->fd_list_ptr->end(), map_fd_info_iter->first);
					if (iter != _net_ptr->fd_list_ptr->end()) {
						_net_ptr->fd_list_ptr->erase(iter);
					}
					else {
						_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Not found fd", map_fd_info_iter->first);
					}
				}
				else {
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "connect succeed just delete table");
				}

				map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(map_fd_info_iter->first);
				if (map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
					delete map_fd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);
				}

				delete[] map_fd_info_iter->second;
				map_fd_info.erase(map_fd_info_iter);
				map_fd_info_iter = map_fd_info.begin();
			}
			else{
				map_fd_info_iter++;
			}
		}

		// UDP part
		for (map<int, struct fd_information *>::iterator map_udpfd_info_iter = map_fd_info.begin(); map_udpfd_info_iter != map_fd_info.end();) {
			_log_ptr->write_log_format("s(u) s d s u s u \n", __FUNCTION__, __LINE__,
				"fd", map_udpfd_info_iter->first,
				"pid", map_udpfd_info_iter->second->pid,
				"session", map_udpfd_info_iter->second->session_id);

			if (map_udpfd_info_iter->second->session_id == session_id) {

				_log_ptr->write_log_format("s(u) s d s u \n", __FUNCTION__, __LINE__,
					"connect faild delete table and close fd", map_udpfd_info_iter->first,
					"pid", map_udpfd_info_iter->second->pid);

				if (_peer_ptr->map_udpfd_pid.find(map_udpfd_info_iter->first) == _peer_ptr->map_udpfd_pid.end()) {

					_peer_ptr->CloseSocketUDP(map_udpfd_info_iter->first, true, "Closed by session stopped");

					list<int>::iterator iter = std::find(_net_udp_ptr->fd_list_ptr->begin(), _net_udp_ptr->fd_list_ptr->end(), map_udpfd_info_iter->first);
					if (iter != _net_udp_ptr->fd_list_ptr->end()) {
						_net_udp_ptr->fd_list_ptr->erase(iter);
					}
					else {
						_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Not found fd", map_udpfd_info_iter->first);
					}
				}
				else {
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "connect succeed just delete table");
				}

				map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(map_udpfd_info_iter->first);
				if (map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end()){
					delete map_udpfd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);
				}

				delete[] map_udpfd_info_iter->second;
				map_udpfd_info.erase(map_udpfd_info_iter);
				map_udpfd_info_iter = map_fd_info.begin();
			}
			else{
				map_udpfd_info_iter++;
			}
		}

		// Remove the conn_from_parent_list of that session id
		for (list<struct fd_information *>::iterator iter = conn_from_parent_list.begin(); iter != conn_from_parent_list.end(); ) {
			if ((*iter)->session_id == session_id) {
				delete (*iter);
				conn_from_parent_list.erase(iter);
				iter = conn_from_parent_list.begin();
			}
			else {
				iter++;
			}
		}
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "conn_from_parent_list.size()", conn_from_parent_list.size());
		debug_printf("conn_from_parent_list.size() = %d \n", conn_from_parent_list.size());
	}
	else if (session_id_candidates_set_iter->second->role == PARENT_PEER) {
		
		for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "list pid : ", session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}
		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
		session_id_candidates_set.erase(session_id_candidates_set_iter);

		// Remove certain iterator in map_fd_info and map_fd_NonBlockIO
		// TCP part
		for (map<int, struct fd_information *>::iterator map_fd_info_iter = map_fd_info.begin(); map_fd_info_iter != map_fd_info.end();) {
			if (map_fd_info_iter->second->session_id == session_id) {

				if (_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()) {
					
					_net_ptr->close(map_fd_info_iter->first);

					list<int>::iterator iter = std::find(_net_ptr->fd_list_ptr->begin(), _net_ptr->fd_list_ptr->end(), map_fd_info_iter->first);
					if (iter != _net_ptr->fd_list_ptr->end()) {
						_net_ptr->fd_list_ptr->erase(iter);
					}
					else {
						_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Not found fd", map_fd_info_iter->first);
					}
					
				}
				else {
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "connect succeed just delete table ");
				}

				map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(map_fd_info_iter->first);
				if (map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
					delete map_fd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);

				}

				delete map_fd_info_iter->second;
				map_fd_info.erase(map_fd_info_iter);
				map_fd_info_iter = map_fd_info.begin();
			}
			else{
				map_fd_info_iter++;
			}
		}

		// UDP part
		for (map<int, struct fd_information *>::iterator map_udpfd_info_iter = map_fd_info.begin(); map_udpfd_info_iter != map_fd_info.end();) {
			if (map_udpfd_info_iter->second->session_id == session_id) {

				if (_peer_ptr->map_udpfd_pid.find(map_udpfd_info_iter->first) == _peer_ptr->map_udpfd_pid.end()) {
					// The socket isn't found in map_fd_pid means that not receive PEER_CON

					//_net_ptr->close(map_udpfd_info_iter->first);
					_peer_ptr->CloseSocketUDP(map_udpfd_info_iter->first, true, "Closed by session stopped");

					list<int>::iterator iter = std::find(_net_udp_ptr->fd_list_ptr->begin(), _net_udp_ptr->fd_list_ptr->end(), map_udpfd_info_iter->first);
					if (iter != _net_udp_ptr->fd_list_ptr->end()) {
						_net_udp_ptr->fd_list_ptr->erase(iter);
					}
					else {
						_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Not found fd", map_udpfd_info_iter->first);
					}

				}
				else {
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "connect succeed just delete table ");
				}

				map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter = map_fd_NonBlockIO.find(map_udpfd_info_iter->first);
				if (map_udpfd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
					delete map_udpfd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);

				}

				delete map_udpfd_info_iter->second;
				map_udpfd_info.erase(map_udpfd_info_iter);
				map_udpfd_info_iter = map_fd_info.begin();
			}
			else{
				map_udpfd_info_iter++;
			}
		}
	}
	
	for (map<int, struct fd_information *>::iterator map_fd_info_iter = map_fd_info.begin(); map_fd_info_iter != map_fd_info.end(); map_fd_info_iter++){
		_log_ptr->write_log_format("s =>u s d s d \n", __FUNCTION__, __LINE__, "fd : ", map_fd_info_iter->first, ", pid: ", map_fd_info_iter->second->pid);
	}
	for (map<int, struct fd_information *>::iterator map_udpfd_info_iter = map_fd_info.begin(); map_udpfd_info_iter != map_fd_info.end(); map_udpfd_info_iter++){
		_log_ptr->write_log_format("s =>u s d s d \n", __FUNCTION__, __LINE__, "fd : ", map_udpfd_info_iter->first, ", pid: ", map_udpfd_info_iter->second->pid);
	}
	*/
}


int peer_communication::handle_pkt_in(int sock)
{	
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only receive PEER_CON protocol sent by join/rescue peer (the peer is candidate's peer).
	And handle P2P structure.
	*/
	debug_printf("peer_communication::handle_pkt_in \n");
	
	map_fd_info_iter = map_fd_info.find(sock);
	
	if(map_fd_info_iter == map_fd_info.end()){
		
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info structure in peer_communication::handle_pkt_in\n");
		_logger_client_ptr->log_exit();
	}
	else{
		printf("map_fd_info_iter->second->flag: %d \n", map_fd_info_iter->second->role);
		if(map_fd_info_iter->second->role == CHILD_PEER) {	//this fd is rescue peer
			//do nothing rebind to event out only
			_net_ptr->set_nonblocking(sock);
			//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLOUT);
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"this fd is rescue peer do nothing, jsut rebind to event out only");
		}
		else if(map_fd_info_iter->second->role == PARENT_PEER){	//this fd is candidate peer
			
			map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(sock);
			if(map_fd_NonBlockIO_iter == map_fd_NonBlockIO.end()){
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] can't  find map_fd_NonBlockIO_iter in peer_commiication", __FUNCTION__, __LINE__);
			}

			Nonblocking_Ctl * Nonblocking_Recv_Ctl_ptr =NULL;
			struct chunk_header_t* chunk_header_ptr = NULL;
			struct chunk_t* chunk_ptr = NULL;
			unsigned long buf_len=0;
			int recv_byte=0;

			Nonblocking_Recv_Ctl_ptr = &(map_fd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingRecv) ;


			for(int i =0;i<5;i++){
				if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY){
					chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
					if(!(chunk_header_ptr ) ){
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::chunk_header_ptr  new error", __FUNCTION__, __LINE__);
					}
					memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =0 ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_header_ptr ;
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_RUNNING){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_OK){
					buf_len = sizeof(chunk_header_t)+ ((chunk_t *)(Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)) ->header.length ;
					chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];
					if (!chunk_ptr) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::chunk_ptr  new error", __FUNCTION__, __LINE__);
					}
					
					memset(chunk_ptr, 0x0, buf_len);
					memcpy(chunk_ptr,Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer,sizeof(chunk_header_t));

					if (Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)
						delete [] (unsigned char*)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer ;

					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;

					//			printf("chunk_ptr->header.length = %d  seq = %d\n",chunk_ptr->header.length,chunk_ptr->header.sequence_number);
					Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_PAYLOAD_READY ;

				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){
					//			chunk_ptr =(chunk_t *)Recv_nonblocking_ctl_ptr ->recv_ctl_info.buffer;

					//			Recv_nonblocking_ctl_ptr->recv_packet_state = READ_HEADER_READY ;
					break;
				}

				recv_byte =_net_ptr->nonblock_recv(sock,Nonblocking_Recv_Ctl_ptr);
				printf("peer_communication::handle_pkt_in  recv_byte: %d \n", recv_byte);

				if(recv_byte < 0) {
					printf("error occ in nonblocking \n");
					fd_close(sock);

					//PAUSE
					return RET_SOCK_ERROR;
				}

			}

			if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){

				chunk_ptr =(chunk_t *)Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer;

				Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY ;

				buf_len =  sizeof(struct chunk_header_t) +  chunk_ptr->header.length ;

			}else{
				//other stats
				return RET_OK;
			}

			//determine stream direction
			if (chunk_ptr->header.cmd == CHNK_CMD_PEER_CON) {
				cout << "CHNK_CMD_PEER_CON" << endl;
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_CON");
				
				//use fake cin info
				
				struct sockaddr_in fake_cin;
				memset(&fake_cin,0x00,sizeof(struct sockaddr_in));

				_peer_ptr->handle_connect(sock, chunk_ptr,fake_cin);

				
				//bind to peer_com~ object
				
				_net_ptr->set_nonblocking(sock);
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
			} else{
				
				printf("error : unknow or cannot handle cmd : in peer_communication::handle_pkt_in  cmd =%d \n",chunk_ptr->header.cmd);
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : unknow or cannot handle cmd : in peer_communication::handle_pkt_in ",chunk_ptr->header.cmd);
				_logger_client_ptr->log_exit();
			}

			if(chunk_ptr)
				delete chunk_ptr;
		}
		else{
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknow flag in peer_communication::handle_pkt_in\n");
			_logger_client_ptr->log_exit();
		}
	}
	
	return RET_OK;
}

// CHNK_CMD_ROLE: Child 會收到 parent 回覆的 ACK
// CHNK_CMD_PEER_CON: Parent 會收到這個訊息，表示被 child 選中
int peer_communication::handle_pkt_in_udp(int sock)
{	
	map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter = map_mysession_candidates.end();
	struct peer_info_t *peer_info_ptr = NULL;

	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "state", UDT::getsockstate(sock));

	// Get iter of map_mysession_candidates
	for (map<unsigned long, struct mysession_candidates *>::iterator iter = map_mysession_candidates.begin(); iter != map_mysession_candidates.end(); iter++) {
		for (int i = 0; i != iter->second->candidates_num; i++) {
			if (iter->second->p_candidates_info[i].sock == sock) {
				map_mysession_candidates_iter = iter;
				peer_info_ptr = &iter->second->p_candidates_info[i];
			}
			else if (iter->second->n_candidates_info[i].sock == sock) {
				map_mysession_candidates_iter = iter;
				peer_info_ptr = &iter->second->n_candidates_info[i];
			}
		}
	}
	if (map_mysession_candidates_iter == map_mysession_candidates.end()) {
		if (UDT::getsockstate(sock) != UDTSTATUS::CONNECTED) {
			_peer_ptr->CloseSocketUDP(sock, false, "Not Found in map_mysession_candidates");
		}
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Not found in map_mysession_candidates", sock);
		debug_printf("[ERROR] Not found in map_mysession_candidates %d \n", sock);
		return 0;
	}

	if (map_mysession_candidates_iter->second->myrole == CHILD_PEER) {
		map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;

		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Recv from", sock);

		map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(sock);
		if (map_udpfd_NonBlockIO_iter == map_udpfd_NonBlockIO.end()) {
			debug_printf("[DEBUG] Not found map_udpfd_NonBlockIO %d \n", sock);
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Not found map_udpfd_NonBlockIO", sock);
			return RET_SOCK_ERROR;		// 關閉這條 socket
		}

		Nonblocking_Ctl * Nonblocking_Recv_Ctl_ptr = NULL;
		struct chunk_header_t* chunk_header_ptr = NULL;
		struct chunk_t* chunk_ptr = NULL;
		unsigned long buf_len = 0;
		int recv_byte = 0;

		Nonblocking_Recv_Ctl_ptr = &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingRecv);

		for (int i = 0; i < 5; i++) {
			if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY) {
				chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
				if (!chunk_header_ptr) {
					_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_header_t new error", sock);
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] chunk_header_t  new error", __FUNCTION__, __LINE__);
					return RET_OK;
				}
				memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.offset = 0;
				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.total_len = sizeof(chunk_header_t);
				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.expect_len = sizeof(chunk_header_t);
				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer = (char *)chunk_header_ptr;
			}
			else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_RUNNING) {
				//do nothing
			}
			else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_OK) {
				buf_len = sizeof(chunk_header_t)+((chunk_t *)(Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer))->header.length;
				chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];
				if (!chunk_ptr) {
					_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_t new error", sock);
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] chunk_t new error", __FUNCTION__, __LINE__);
					return RET_OK;
				}

				memset(chunk_ptr, 0x0, buf_len);
				memcpy(chunk_ptr, Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer, sizeof(chunk_header_t));

				if (Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer) {
					delete[](unsigned char*)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer;
				}


				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.offset = sizeof(chunk_header_t);
				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.total_len = chunk_ptr->header.length;
				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.expect_len = chunk_ptr->header.length;
				Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer = (char *)chunk_ptr;
				Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_PAYLOAD_READY;

			}
			else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY){
				//do nothing
			}
			else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING){
				//do nothing
			}
			else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){
				break;
			}

			recv_byte = _net_udp_ptr->nonblock_recv(sock, Nonblocking_Recv_Ctl_ptr);

			if (recv_byte < 0) {
				_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] Recv bytes", recv_byte, sock);
				return RET_SOCK_ERROR;		// 關閉這條 socket
			}
		}

		if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK) {
			chunk_ptr = (chunk_t *)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer;
			Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY;
			buf_len = sizeof(struct chunk_header_t) + chunk_ptr->header.length;
		}
		else {
			//other stats
			_log_ptr->write_log_format("s(u) u \n", __FUNCTION__, __LINE__, Nonblocking_Recv_Ctl_ptr->recv_packet_state);
			return RET_OK;
		}

		// Get response from parent
		if (chunk_ptr->header.cmd == CHNK_CMD_ROLE) {
			struct role_struct *role_protocol_ptr = (struct role_struct *)chunk_ptr;
			map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter = map_mysession_candidates.end();
			struct peer_info_t *peer_info_ptr = NULL;

			// Get iter of map_mysession_candidates
			for (map<unsigned long, struct mysession_candidates *>::iterator iter = map_mysession_candidates.begin(); iter != map_mysession_candidates.end(); iter++) {
				for (int i = 0; i != iter->second->candidates_num; i++) {
					if (iter->second->p_candidates_info[i].sock == sock) {
						map_mysession_candidates_iter = iter;
						peer_info_ptr = &(iter->second->p_candidates_info[i]);
						peer_info_ptr->estimated_delay = role_protocol_ptr->parent_src_delay + role_protocol_ptr->queueing_time + role_protocol_ptr->transmission_time;
						peer_info_ptr->PS_class = role_protocol_ptr->PS_class;
						_log_ptr->write_log_format("s(u) s d(d) u u u \n", __FUNCTION__, __LINE__, "peer", peer_info_ptr->pid, sock, role_protocol_ptr->parent_src_delay, role_protocol_ptr->queueing_time, role_protocol_ptr->transmission_time);
					}
				}
			}
			if (map_mysession_candidates_iter == map_mysession_candidates.end()) {
				// 可能是 peer-list 還沒收到，對方就先連進來
				//return RET_OK;
			}

			_log_ptr->write_log_format("s(u) s u s u s d s u s u s u s u \n", __FUNCTION__, __LINE__, "Recv CHNK_CMD_ROLE from sock", sock,
				"pid", role_protocol_ptr->send_pid,
				"role", role_protocol_ptr->flag,
				"manifest", role_protocol_ptr->manifest,
				"parent_src_delay", role_protocol_ptr->parent_src_delay,
				"queueint_time", role_protocol_ptr->queueing_time,
				"transmission_time", role_protocol_ptr->transmission_time);
		}
		else {
			debug_printf("[ERROR] Recv error CMD %d from sock %d \n", chunk_ptr->header.cmd, sock);
			_log_ptr->write_log_format("s(u) s u u \n", __FUNCTION__, __LINE__, "[ERROR] Recv error CMD from sock", sock, chunk_ptr->header.cmd);
			//_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Receive unknown header command", __FUNCTION__, __LINE__);
		}

		if (chunk_ptr) {
			delete chunk_ptr;
		}
	}
	else if (map_mysession_candidates_iter->second->myrole == PARENT_PEER) {
		map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;

		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Recv CHNK_CMD_PEER_CON from", sock);

		map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(sock);
		if (map_udpfd_NonBlockIO_iter == map_udpfd_NonBlockIO.end()) {
			debug_printf("[DEBUG] Not found map_udpfd_NonBlockIO %d \n", sock);
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Not found map_udpfd_NonBlockIO", sock);
			return RET_SOCK_ERROR;		// 關閉這條 socket
		}

		Nonblocking_Ctl * Nonblocking_Recv_Ctl_ptr =NULL;
		struct chunk_header_t* chunk_header_ptr = NULL;
		struct chunk_t* chunk_ptr = NULL;
		unsigned long buf_len=0;
		int recv_byte=0;

		Nonblocking_Recv_Ctl_ptr = &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingRecv) ;

		for (int i = 0; i < 5; i++) {
			if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY) {
				chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
				if (!chunk_header_ptr) {
					_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_header_t new error", sock);
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] chunk_header_t  new error", __FUNCTION__, __LINE__);
					return RET_OK;
				}
				memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =0 ;
				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = sizeof(chunk_header_t) ;
				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = sizeof(chunk_header_t) ;
				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_header_ptr ;
			}
			else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_RUNNING) {
				//do nothing
			}
			else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_OK) {
				buf_len = sizeof(chunk_header_t)+ ((chunk_t *)(Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)) ->header.length ;
				chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];
				if (!chunk_ptr) {
					_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_t new error", sock);
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] chunk_t new error", __FUNCTION__, __LINE__);
					return RET_OK;
				}
					
				memset(chunk_ptr, 0x0, buf_len);
				memcpy(chunk_ptr,Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer,sizeof(chunk_header_t));

				if (Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer) {
					delete [] (unsigned char*)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer;
				}
					

				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =sizeof(chunk_header_t);
				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length;
				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length;
				Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr;
				Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_PAYLOAD_READY ;

			}
			else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY){
				//do nothing
			}
			else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING){
				//do nothing
			}
			else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){
				break;
			}

			recv_byte =_net_udp_ptr->nonblock_recv(sock,Nonblocking_Recv_Ctl_ptr);
		
			if (recv_byte < 0) {
				_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] Recv bytes", recv_byte, sock);
				return RET_SOCK_ERROR;		// 關閉這條 socket
			}
		}

		if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK) {
			chunk_ptr =(chunk_t *)Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer;
			Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY ;
			buf_len =  sizeof(struct chunk_header_t) +  chunk_ptr->header.length ;
		}
		else {
			//other stats
			_log_ptr->write_log_format("s(u) u \n", __FUNCTION__, __LINE__, Nonblocking_Recv_Ctl_ptr->recv_packet_state);
			return RET_OK;
		}

		//determine stream direction
		if (chunk_ptr->header.cmd == CHNK_CMD_PEER_CON) {
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Recv CHNK_CMD_PEER_CON from pid", peer_info_ptr->pid);
				
			struct chunk_request_peer_t *chunk_request_ptr = (struct chunk_request_peer_t *)chunk_ptr;
			struct peer_info_t *peer_info_ptr = NULL;
			UINT32 my_role = 0;

			for (map<unsigned long, struct mysession_candidates *>::iterator iter = map_mysession_candidates.begin(); iter != map_mysession_candidates.end(); iter++) {

				for (int i = 0; i != iter->second->candidates_num; i++) {
					int peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
					int peer_fd1 = map_mysession_candidates_iter->second->p_candidates_info[i].sock;
					int peer_fd2 = map_mysession_candidates_iter->second->n_candidates_info[i].sock;
					int peer_manifest = map_mysession_candidates_iter->second->n_candidates_info[i].manifest;

					_log_ptr->write_log_format("s(u) s u s u s u s u(u)(u)(u) s u(u)(u)(u) \n", __FUNCTION__, __LINE__,
						"parent", peer_pid,
						"my_session", iter->first,
						"peercomm_session", map_mysession_candidates_iter->second->p_candidates_info[i].peercomm_session,
						"sock1", peer_fd1, map_mysession_candidates_iter->second->p_candidates_info[i].connection_state, map_mysession_candidates_iter->second->p_candidates_info[i].estimated_delay, map_mysession_candidates_iter->second->p_candidates_info[i].PS_class,
						"sock2", peer_fd2, map_mysession_candidates_iter->second->n_candidates_info[i].connection_state, map_mysession_candidates_iter->second->n_candidates_info[i].estimated_delay, map_mysession_candidates_iter->second->n_candidates_info[i].PS_class);

					if (iter->second->p_candidates_info[i].sock == sock) {
						iter->second->p_candidates_info[i].connection_state = PEER_SELECTED;
						peer_info_ptr = &(iter->second->p_candidates_info[i]);
						my_role = iter->second->myrole;
						if (std::find(std::begin(_peer_ptr->priority_children), std::end(_peer_ptr->priority_children), iter->second->p_candidates_info[i].pid) == std::end(_peer_ptr->priority_children)) {
							_peer_ptr->priority_children.push_back(iter->second->p_candidates_info[i].pid);
						}
					}
					else if (iter->second->n_candidates_info[i].sock == sock) {
						iter->second->n_candidates_info[i].connection_state = PEER_SELECTED;
						peer_info_ptr = &(iter->second->n_candidates_info[i]);
						my_role = iter->second->myrole;
						if (std::find(std::begin(_peer_ptr->priority_children), std::end(_peer_ptr->priority_children), iter->second->n_candidates_info[i].pid) == std::end(_peer_ptr->priority_children)) {
							_peer_ptr->priority_children.push_back(iter->second->p_candidates_info[i].pid);
						}
					}
				}
			}
			if (peer_info_ptr == NULL) {
				_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] sock", sock);
				return RET_SOCK_ERROR;		// 關閉這條 socket
			}

			struct peer_info_t *child_info = _pk_mgr_ptr->GetChildFromPid(peer_info_ptr->pid);
			if (child_info != NULL) {
				child_info->connection_state = PEER_SELECTED;
				child_info->sock = sock;
			}
			else {
				// 不做任何事，由 Child 等待 timeout 關閉連線
				_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "[DEBUG] sock", sock, "pid", peer_info_ptr->pid);
			}

			_peer_ptr->InitPeerInfo(sock, peer_info_ptr->pid, peer_info_ptr->manifest, my_role);
			_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
		}
		else {
			debug_printf("[ERROR] Recv error CMD %d from sock %d \n", chunk_ptr->header.cmd, sock);
			_log_ptr->write_log_format("s(u) s u u \n", __FUNCTION__, __LINE__, "[ERROR] Recv error CMD from sock", sock, chunk_ptr->header.cmd);
			//_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Receive unknown header command", __FUNCTION__, __LINE__);
		}

		if (chunk_ptr) {
			delete chunk_ptr;
		}
	}
	else {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Receive unknown header command", __FUNCTION__, __LINE__);
	}
	
	return RET_OK;
}

//first write, then set fd to readable & excecption only
int peer_communication::handle_pkt_out(int sock)	
{
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only send PEER_CON protocol to candidates, if the fd is in the list. (the peer is join/rescue peer)
	And handle P2P structure.
	*/
	/*
	map_fd_info_iter = map_fd_info.find(sock);
	if (map_fd_info_iter == map_fd_info.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
		_logger_client_ptr->log_exit();
	}
	else {
		if(map_fd_info_iter->second->role == 0){	//this fd is rescue peer
			//send peer con
			int ret,
				send_flag = 0;
			int i;
			
			
			session_id_candidates_set_iter = session_id_candidates_set.find(map_fd_info_iter->second->session_id);
			if(session_id_candidates_set_iter == session_id_candidates_set.end()){
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find session_id_candidates_set structure in peer_communication::handle_pkt_out\n");
				_logger_client_ptr->log_exit();
			}

			


			map_fd_NonBlockIO_iter =map_fd_NonBlockIO.find(sock);
			if(map_fd_NonBlockIO_iter==map_fd_NonBlockIO.end()){
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] can't  find map_fd_NonBlockIO_iter in peer_commiication", __FUNCTION__, __LINE__);
			}

			for(i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
				if(session_id_candidates_set_iter->second->list_info->level_info[i]->pid == map_fd_info_iter->second->pid){
					ret = _peer_ptr->handle_connect_request(sock, session_id_candidates_set_iter->second->list_info->level_info[i], session_id_candidates_set_iter->second->list_info->pid,&(map_fd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl));
					send_flag =1;
				}
			}

			if(send_flag == 0){
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find level info structure in peer_communication::handle_pkt_out\n");
				_logger_client_ptr->log_exit();
			}

			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"send CHNK_CMD_PEER_CON");
			printf("ret: %d \n", ret);
			
			if(ret < 0) {
				cout << "handle_connect_request error!!!" << endl;
				fd_close(sock);
				return RET_ERROR;
				
			} else if(map_fd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == RUNNING){
			
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				
			}else if (map_fd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == READY){
				
				_net_ptr->set_nonblocking(sock);
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
				return RET_OK;
			}
		}
		else if(map_fd_info_iter->second->role == 1){	//this fd is candidate
			//do nothing rebind to event in only
			_net_ptr->set_nonblocking(sock);
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"this fd is candidate do nothing rebind to event in only");
			//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
		}
		else{	
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknow flag in peer_communication::handle_pkt_out\n");
			_logger_client_ptr->log_exit();
		}
	}
	*/
	return RET_OK;
}

// Socket is already built, send "CHNK_CMD_PEER_CON" to build virtual connection again
int peer_communication::handle_pkt_out_udp(int sock)	
{
	//_log_ptr->write_log_format("s(u) d \n", __FUNCTION__, __LINE__, sock);
	/*
	map<int, struct fd_information *>::iterator map_udpfd_info_iter = map_udpfd_info.find(sock);
	if (map_udpfd_info_iter == map_udpfd_info.end()) {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Not found in map_udpfd_info", __FUNCTION__, __LINE__);
		PAUSE
	}

	if (map_udpfd_info_iter->second->role == PARENT_PEER) {
		UINT32 peercomm_session = map_udpfd_info_iter->second->peercomm_session;
		map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter;

		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Send CHNK_CMD_PEER_CON to", sock);

		if (map_udpfd_NonBlockIO.find(sock) == map_udpfd_NonBlockIO.end()) {
			_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_NonBlockIO", __FUNCTION__, __LINE__);
		}

		for (map<unsigned long, struct mysession_candidates *>::iterator iter = map_mysession_candidates.begin(); iter != map_mysession_candidates.end(); iter++) {
			for (int i = 0; i != iter->second->candidates_num; i++) {
				// 比對 peercomm_session
				if (iter->second->candidates_info[i].pid == map_udpfd_info_iter->second->pid && iter->second->candidates_info[i].peercomm_session == peercomm_session) {
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "send to pid", map_udpfd_info_iter->second->pid, "in session", map_udpfd_info_iter->second->session_id);
					_peer_ptr->SendPeerCon(sock, map_udpfd_info_iter->second->pid, &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl), peercomm_session);

					iter->second->candidates_info[i].connection_state = PEER_CONNECTED;
					if (_pk_mgr_ptr->map_pid_parent.find(map_udpfd_info_iter->second->pid) != _pk_mgr_ptr->map_pid_parent.end()) {
						_pk_mgr_ptr->map_pid_parent.find(map_udpfd_info_iter->second->pid)->second->peerInfo.connection_state = PEER_CONNECTED;
					}
					_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "session", map_udpfd_info_iter->second->session_id, "state", _peer_ptr->substream_first_reply_peer[map_udpfd_info_iter->second->session_id]->session_state);
				}
			}
		}

		if (map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == RUNNING) {

		}
		else if (map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == READY) {
			_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
			return RET_OK;
		}

	}
	else if (map_udpfd_info_iter->second->role == CHILD_PEER) {
		//_log_ptr->write_log_format("s(u) d \n", __FUNCTION__, __LINE__, sock);
		// Do nothing, just wait for socket readable
	}
	else {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Unknown command", __FUNCTION__, __LINE__);
	}
	*/
	/*
	if (map_udpfd_info_iter->second->role == PARENT_PEER) {
		int ret;
		UINT32 peercomm_session = map_udpfd_info_iter->second->peercomm_session;
		map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
		map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;

		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Send CHNK_CMD_PEER_CON to", sock);

		// 先檢查 session ID 存不存在，不存在的話就認為是因為 Parent 主動建立連線，若再次找不到就放棄該次 session
		session_id_candidates_set_iter = session_id_candidates_set.find(map_udpfd_info_iter->second->session_id);
		if (session_id_candidates_set_iter == session_id_candidates_set.end()) {
			bool conn_from_parent_flag = false;

			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "conn_from_parent_list.size()", conn_from_parent_list.size());
			for (list<struct fd_information *>::iterator iter = conn_from_parent_list.begin(); iter != conn_from_parent_list.end(); iter++) {
				_log_ptr->write_log_format("s(u) d d d \n", __FUNCTION__, __LINE__, (*iter)->session_id, (*iter)->pid, (*iter)->manifest);
				if ((*iter)->pid == map_udpfd_info_iter->second->pid && (*iter)->manifest == map_udpfd_info_iter->second->manifest) {
					// 有找到代表這次的 session 是由 parent 主動建連線的，並重新補上 map_udpfd_info_iter 的 session ID
					map_udpfd_info_iter->second->session_id = (*iter)->session_id;
					session_id_candidates_set_iter = session_id_candidates_set.find((*iter)->session_id);
					delete (*iter);
					conn_from_parent_list.erase(iter);
					conn_from_parent_flag = true;
					break;
				}
			}

			if (session_id_candidates_set_iter == session_id_candidates_set.end()) {
				// 曾經發生 20140815
				debug_printf("[ERROR] Not found fd %d in session %d \n", sock, map_udpfd_info_iter->second->session_id);
				//_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in session_id_candidates_set", __FUNCTION__, __LINE__);
				_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "[DEBUG] Not found fd", sock, "in session", map_udpfd_info_iter->second->session_id);
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s u s u \n", __FUNCTION__, __LINE__, "[DEBUG] Not found fd", sock, "in session", map_udpfd_info_iter->second->session_id);
				return RET_SOCK_ERROR;
			}
		}

		map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(sock);
		if (map_udpfd_NonBlockIO_iter == map_udpfd_NonBlockIO.end()) {
			debug_printf("[ERROR] Not found in map_udpfd_NonBlockIO  %d \n", sock);
			_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_NonBlockIO", __FUNCTION__, __LINE__);
		}


		for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			if (session_id_candidates_set_iter->second->list_info->level_info[i]->pid == map_udpfd_info_iter->second->pid) {
				// Only send "CHNK_CMD_PEER_CON" to the first 
				if (_peer_ptr->substream_first_reply_peer[map_udpfd_info_iter->second->session_id]->session_state == FIRST_CONNECTED_RUNNING) {
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "send to pid", map_udpfd_info_iter->second->pid, "in session", map_udpfd_info_iter->second->session_id);
					ret = _peer_ptr->SendPeerCon(sock, session_id_candidates_set_iter->second->list_info->level_info[i], session_id_candidates_set_iter->second->list_info->pid, &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl), peercomm_session);
					_peer_ptr->substream_first_reply_peer[map_udpfd_info_iter->second->session_id]->session_state = FIRST_CONNECTED_OK;
					if (_pk_mgr_ptr->map_pid_parent.find(map_udpfd_info_iter->second->pid) != _pk_mgr_ptr->map_pid_parent.end()) {
						_pk_mgr_ptr->map_pid_parent.find(map_udpfd_info_iter->second->pid)->second->peerInfo.state = PEER_CONNECTED;
					}
					_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "session", map_udpfd_info_iter->second->session_id, "state", _peer_ptr->substream_first_reply_peer[map_udpfd_info_iter->second->session_id]->session_state);
					break;
				}
			}
		}

		if (map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == RUNNING) {

		}
		else if (map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == READY) {
			_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
			return RET_OK;
		}

	}
	else if (map_udpfd_info_iter->second->role == CHILD_PEER) {
		//_log_ptr->write_log_format("s(u) d \n", __FUNCTION__, __LINE__, sock);
		// Do nothing, just wait for socket readable
	}
	else {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Unknown command", __FUNCTION__, __LINE__);
	}
	*/
	return RET_OK;
}


void peer_communication::handle_pkt_error(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in peer_communication error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void peer_communication::handle_pkt_error_udp(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in peer_communication error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void peer_communication::handle_job_realtime()
{

}


void peer_communication::handle_job_timer()
{

}

void peer_communication::handle_sock_error(int sock, basic_class *bcptr){
}