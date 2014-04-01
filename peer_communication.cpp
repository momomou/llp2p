
#include "peer_communication.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "io_accept.h"
#include "io_connect.h"
#include "logger_client.h"
#include "io_nonblocking.h"

using namespace std;

peer_communication::peer_communication(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, logger_client * logger_client_ptr){
	_net_ptr = net_ptr;
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
	_io_nonblocking_ptr=NULL;
	_io_nonblocking_ptr = new io_nonblocking(net_ptr,log_ptr ,this,logger_client_ptr);
	_io_accept_ptr = new io_accept(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	_io_connect_ptr = new io_connect(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	if(!(_io_nonblocking_ptr) || !(_io_accept_ptr) || !(_io_connect_ptr)){
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] !(_io_nonblocking_ptr) || !(_io_accept_ptr) || !(_io_connect_ptr)   new error", __FUNCTION__, __LINE__);
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
	if(_io_nonblocking_ptr)
		delete _io_nonblocking_ptr;
	_io_accept_ptr =NULL;
	_io_connect_ptr =NULL;
	_io_nonblocking_ptr=NULL;

	for(session_id_candidates_set_iter = session_id_candidates_set.begin() ;session_id_candidates_set_iter !=session_id_candidates_set.end();session_id_candidates_set_iter++){
		
		for(int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}
		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
	}
	session_id_candidates_set.clear();


	for(map_fd_info_iter=map_fd_info.begin() ; map_fd_info_iter!=map_fd_info.end();map_fd_info_iter++){
		delete map_fd_info_iter->second;
	}
	map_fd_info.clear();



	for(map_fd_NonBlockIO_iter=map_fd_NonBlockIO.begin() ; map_fd_NonBlockIO_iter!=map_fd_NonBlockIO.end();map_fd_NonBlockIO_iter++){
		delete map_fd_NonBlockIO_iter->second;
	}
	map_fd_NonBlockIO.clear();

	printf("==============deldet peer_communication success==========\n");
}

void peer_communication::set_self_info(unsigned long public_ip){
	self_info->public_ip = public_ip;
	self_info->private_ip = _net_ptr->getLocalIpv4();
}

//flag 0 rescue peer(caller is child), flag 1 candidate's peer(caller is parent)
int peer_communication::set_candidates_handler(unsigned long rescue_manifest, struct chunk_level_msg_t *testing_info, unsigned int candidates_num, int caller)
{	
	if (candidates_num < 1) {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Candidates_num cannot less than 1", __FUNCTION__, __LINE__);
	}

	_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
															"session_id =", session_id_count,
															"manifest =", rescue_manifest,
															"caller =", caller, 
															"candidates_num =", candidates_num);
	for (int i = 0; i < candidates_num; i++) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "list pid", testing_info->level_info[i]->pid);
	}
	
	// caller is the child of the candidate-peer, 0
	if (caller == RESCUE_PEER) {
		if ((total_manifest & rescue_manifest) == 1) {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d d \n","error : re-rescue for some sub stream : %d %d in set_candidates_test\n",total_manifest,rescue_manifest);
			_logger_client_ptr->log_exit();
		}
		else {
			debug_printf2("rescue manifest: %d already rescue manifest: %d \n", rescue_manifest, total_manifest);
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "rescue-peer calls peer_communication");
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "rescue manifest", rescue_manifest, "already rescue manifest", total_manifest);
			total_manifest = total_manifest | rescue_manifest;	//total_manifest has to be erased in stop_attempt_connect
		
			session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
			if (session_id_candidates_set_iter != session_id_candidates_set.end()) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] session id already in the record in set_candidates_test");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] session id already in the record in set_candidates_test \n");
				_logger_client_ptr->log_exit();
			}
			else {
				session_id_candidates_set[session_id_count] = new struct peer_com_info;
				if (!session_id_candidates_set[session_id_count]) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id_count] new failed", __FUNCTION__, __LINE__);
				}
				/*	redundant code
				session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
				if (session_id_candidates_set_iter == session_id_candidates_set.end()) {
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : session id cannot find in the record in set_candidates_test\n");
					_logger_client_ptr->log_exit();
				}
				*/
				int level_msg_size;
				int offset = 0;
				level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + candidates_num * sizeof(struct level_info_t *);

				session_id_candidates_set[session_id_count]->peer_num = candidates_num;
				session_id_candidates_set[session_id_count]->manifest = rescue_manifest;
				session_id_candidates_set[session_id_count]->role = caller;
				session_id_candidates_set[session_id_count]->list_info = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
				if (!session_id_candidates_set[session_id_count]->list_info) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id_count]->list_info new failed", __FUNCTION__, __LINE__);
				}
				memset(session_id_candidates_set[session_id_count]->list_info, 0, level_msg_size);
				memcpy(session_id_candidates_set[session_id_count]->list_info, testing_info, (level_msg_size - candidates_num * sizeof(struct level_info_t *)));

				offset += (level_msg_size - candidates_num * sizeof(struct level_info_t *));

				for (int i = 0; i < candidates_num; i++) {
					debug_printf("candidates_num: %d, i: %d \n", candidates_num, i);
					session_id_candidates_set[session_id_count]->list_info->level_info[i] = new struct level_info_t;
					if (!session_id_candidates_set[session_id_count]->list_info->level_info[i]) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id_count]->list_info->level_info[i] new failed", __FUNCTION__, __LINE__);
					}
					memset(session_id_candidates_set[session_id_count]->list_info->level_info[i], 0, sizeof(struct level_info_t));
					memcpy(session_id_candidates_set[session_id_count]->list_info->level_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
					
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] pid =", session_id_candidates_set[session_id_count]->list_info->level_info[i]->pid);
					offset += sizeof(struct level_info_t);
				}

				for (int i = 0; i < candidates_num; i++) {
					struct in_addr privateIP;
					struct in_addr publicIP;
					memcpy(&privateIP, &testing_info->level_info[i]->private_ip, sizeof(struct in_addr));
					memcpy(&publicIP, &testing_info->level_info[i]->public_ip, sizeof(struct in_addr));
					
					// Self not behind NAT, candidate-peer not behind NAT
					if (self_info->private_ip == self_info->public_ip && testing_info->level_info[i]->private_ip == testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are not behind NAT, active connect");
						non_blocking_build_connection(testing_info->level_info[i], caller, rescue_manifest,testing_info->level_info[i]->pid, 0, session_id_count);
					}
					// Self not behind NAT, candidate-peer is behind NAT
					else if (self_info->private_ip == self_info->public_ip && testing_info->level_info[i]->private_ip != testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Candidate-peer is behind NAT, passive connect");
						accept_check(testing_info->level_info[i],0,rescue_manifest,testing_info->level_info[i]->pid,session_id_count);
					}
					// Self is behind NAT, candidate-peer not behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[i]->private_ip == testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Rescue-peer is behind NAT, active connect");
						non_blocking_build_connection(testing_info->level_info[i], caller, rescue_manifest,testing_info->level_info[i]->pid, 0, session_id_count);
					}
					// Self is behind NAT, candidate-peer is behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[i]->private_ip != testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__,"candidate-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are behind NAT");
						non_blocking_build_connection(testing_info->level_info[i], caller, rescue_manifest,testing_info->level_info[i]->pid, 1, session_id_count);
						/*
						// if both are in the same NAT
						if (self_info->public_ip == testing_info->level_info[i]->public_ip) {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__,"candidate-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are behind NAT(identical private IP)");
							non_blocking_build_connection(testing_info->level_info[i], caller, rescue_manifest,testing_info->level_info[i]->pid, 1, session_id_count);
						}
						else {
							debug_printf2("---------------TCP-Punch connect (actually listener)----------------- \n");
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are behind NAT(different private IP)");
							_stunt_mgr_ptr->tcpPunch_connection(testing_info->level_info[i], 0, rescue_manifest, testing_info->level_info[i]->pid, 0, session_id_count);
							//accept_check(testing_info->level_info[i], 0, rescue_manifest, testing_info->level_info[0]->pid, session_id_count);
						}
						*/
					}
				}
			}
			session_id_count++;
		}
	}
	// Caller is the parent of the candidate-peer
	else if (caller == CANDIDATE_PEER) {
		if (candidates_num != 1) {
			_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] candidates_num is not equal to 1 when caller is candidate-peer", __FUNCTION__, __LINE__);
		}
		else {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "candidate-peer calls peer_communication");
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "rescue manifest =", rescue_manifest);
		
			session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
			if (session_id_candidates_set_iter != session_id_candidates_set.end()) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] session id in the record in set_candidates_test");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] session id in the record in set_candidates_test \n");
				_logger_client_ptr->log_exit();
			}
			else {
				session_id_candidates_set[session_id_count] = new struct peer_com_info;
				if (!session_id_candidates_set[session_id_count]) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id_count] new failed", __FUNCTION__, __LINE__);
				}  
				
				int level_msg_size;
				int offset = 0;
				level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + candidates_num * sizeof(struct level_info_t *);

				session_id_candidates_set[session_id_count]->peer_num = candidates_num;
				session_id_candidates_set[session_id_count]->manifest = rescue_manifest;
				session_id_candidates_set[session_id_count]->role = caller;
				session_id_candidates_set[session_id_count]->list_info = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
				if (!session_id_candidates_set[session_id_count]->list_info) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id_count]->list_info new failed", __FUNCTION__, __LINE__);
				}
				memset(session_id_candidates_set[session_id_count]->list_info, 0, level_msg_size);
				memcpy(session_id_candidates_set[session_id_count]->list_info, testing_info, (level_msg_size - candidates_num * sizeof(struct level_info_t *)));

				offset += (level_msg_size - candidates_num * sizeof(struct level_info_t *));

				for (int i = 0; i < candidates_num; i++) {
					session_id_candidates_set[session_id_count]->list_info->level_info[i] = new struct level_info_t;
					if (!session_id_candidates_set[session_id_count]->list_info->level_info[i]) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id_count]->list_info->level_info[i] new failed", __FUNCTION__, __LINE__);
					}
					memset(session_id_candidates_set[session_id_count]->list_info->level_info[i], 0, sizeof(struct level_info_t));
					memcpy(session_id_candidates_set[session_id_count]->list_info->level_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
					
					offset += sizeof(struct level_info_t);
				}
				
				struct in_addr privateIP;
				struct in_addr publicIP;
				memcpy(&privateIP, &testing_info->level_info[0]->private_ip, sizeof(struct in_addr));
				memcpy(&publicIP, &testing_info->level_info[0]->public_ip, sizeof(struct in_addr));
				
				// Self not behind NAT, rescue-peer not behind NAT
				if (self_info->private_ip == self_info->public_ip && testing_info->level_info[0]->private_ip == testing_info->level_info[0]->public_ip) {	
					_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are not behind NAT, passive connect");
					accept_check(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,session_id_count);
				}
				// Self not behind NAT, rescue-peer is behind NAT
				else if (self_info->private_ip == self_info->public_ip && testing_info->level_info[0]->private_ip != testing_info->level_info[0]->public_ip) {	
					_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Rescue-peer is behind NAT, passive connect");
					accept_check(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,session_id_count);
				}
				// Self is behind NAT, rescue-peer not behind NAT
				else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[0]->private_ip == testing_info->level_info[0]->public_ip) {	
					_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Rescue-peer is behind NAT, active connect");
					non_blocking_build_connection(testing_info->level_info[0], caller, rescue_manifest,testing_info->level_info[0]->pid, 0, session_id_count);
				}
				// Self is behind NAT, rescue-peer is behind NAT
				else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[0]->private_ip != testing_info->level_info[0]->public_ip) {	
					_log_ptr->write_log_format("s(u) s u(u) \n", __FUNCTION__, __LINE__,"rescue-peer IP =", (testing_info->level_info[0]->public_ip), testing_info->level_info[0]->private_ip);
					_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__,"Rescue-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are behind NAT");
					accept_check(testing_info->level_info[0], 1, rescue_manifest,testing_info->level_info[0]->pid, session_id_count);
					/*
					// if both are in the same NAT
					if (self_info->public_ip == testing_info->level_info[0]->public_ip) {
						_log_ptr->write_log_format("s(u) s u(u) \n", __FUNCTION__, __LINE__,"my IP =", (self_info->public_ip), (self_info->private_ip));
						_log_ptr->write_log_format("s(u) s u(u) \n", __FUNCTION__, __LINE__,"rescue-peer IP =", (testing_info->level_info[0]->public_ip), testing_info->level_info[0]->private_ip);
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__,"rescue-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are behind NAT(identical private IP)");
						accept_check(testing_info->level_info[0], 1, rescue_manifest,testing_info->level_info[0]->pid, session_id_count);
					}
					else {
						debug_printf2("---------------TCP-Punch listen (actually connecter)----------------- \n");
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", inet_ntoa(publicIP), inet_ntoa(privateIP), "Both are behind NAT(different private IP)");
						_stunt_mgr_ptr->accept_check_nat(testing_info->level_info[0], 1, rescue_manifest,testing_info->level_info[0]->pid, session_id_count);
					}
					*/
				}
			}
			session_id_count++;
		}
	}
	else {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] unknow flag in set_candidates_test");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] unknow flag in set_candidates_test \n");
		_logger_client_ptr->log_exit();
	}

	return (session_id_count-1);
}

void peer_communication::clear_fd_in_peer_com(int sock){
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

			if((manifest == map_fd_info_iter->second->manifest)&&(fd_role == map_fd_info_iter->second->flag)&&(fd_pid == map_fd_info_iter->second->pid)){
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
int peer_communication::non_blocking_build_connection(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long fd_pid, int flag, unsigned long session_id)
{	
	struct sockaddr_in peer_saddr;
	int retVal;
	struct in_addr ip;
	int _sock;
	
	_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "fd_pid =", fd_pid, "flag =", flag);
	
	// If the caller's parent already exists, we don't create it again
	if (caller == RESCUE_PEER) {
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

		// 之前已經建立過連線的 在map_in_pid_fd裡面 則不再建立(保證對同個parent不再建立第二條線)
		// map_in_pid_fd: parent-peer which alreay established connection, including temp parent-peer
		for(map_pid_fd_iter = _peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == level_info_ptr->pid ){
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_in_pid_fd in non_blocking_build_connection (rescue peer)");
				return 1;
			}
		}

		/*
		this may have problem****************************************************************
		*/
		// map_pid_peer_info: temp parent-peer
		pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(level_info_ptr ->pid);
		if(pid_peer_info_iter !=  _pk_mgr_ptr ->map_pid_peer_info.end() ){
			//兩個以上就沿用第一個的連線
			if(_pk_mgr_ptr ->map_pid_peer_info.count(level_info_ptr ->pid) >= 2 ){
				printf("pid =%d already in connect find in map_pid_peer_info  testing",level_info_ptr ->pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peer_info testing");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_peer_info in non_blocking_build_connection (rescue peer)");	
				return 1;
			}
		}

		// 若在map_pid_peerDown_info 則不再次建立連線
		// map_pid_peerDown_info: real parent-peer
		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(level_info_ptr ->pid);
		if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
			printf("pid =%d already in connect find in map_pid_peerDown_info",level_info_ptr ->pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peerDown_info");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_peerdown_info in non_blocking_build_connection (rescue peer)");
			return 1;
		}
	}
	// If the caller's child already exists, we don't create it again.
	else if (caller == CANDIDATE_PEER) {
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		multimap<unsigned long, struct peer_info_t *>::iterator map_pid_child_peer_info_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;

		//之前已經建立過連線的 在map_out_pid_fd裡面 則不再建立(保證對同個child不再建立第二條線)
		for(map_pid_fd_iter = _peer_ptr->map_out_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_out_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == level_info_ptr->pid ){
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_out_pid_fd in non_blocking_build_connection (candidate peer)");
				return 1;
			}
		}

		//若在map_pid_rescue_peer_info 則不再次建立連線
		map_pid_rescue_peer_info_iter = _pk_mgr_ptr ->map_pid_rescue_peer_info.find(level_info_ptr ->pid);
		if(map_pid_rescue_peer_info_iter != _pk_mgr_ptr ->map_pid_rescue_peer_info.end()){
			printf("pid =%d already in connect find in map_pid_rescue_peer_info",level_info_ptr ->pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_rescue_peer_info");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_rescue_peer_info in non_blocking_build_connection (candidate peer)");\
			return 1;
		}


		map_pid_child_peer_info_iter = _pk_mgr_ptr ->map_pid_child_peer_info.find(level_info_ptr ->pid);
		if(map_pid_child_peer_info_iter !=  _pk_mgr_ptr ->map_pid_child_peer_info.end()){
			if(_pk_mgr_ptr ->map_pid_child_peer_info.count(level_info_ptr ->pid) >=2){
				printf("pid =%d  already in connect find in map_pid_child_peer_info  testing",level_info_ptr ->pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_child_peer_info ");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_child_peer_info in non_blocking_build_connection (candidate peer)");	
				return 1;
			}
		}
	}

	if ((_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		int socketErr = WSAGetLastError();
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);
		
		_net_ptr->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
	}

	map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(_sock);
	if (map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	map_fd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_fd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_fd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_fd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY ;
	_net_ptr ->set_nonblocking(_sock);		//non-blocking connect
	memset((struct sockaddr_in*)&peer_saddr, 0, sizeof(struct sockaddr_in));

    if (flag == 0) {	
	    peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
		ip.s_addr = level_info_ptr->public_ip;
		debug_printf("connect to public_ip %s port= %d \n" ,inet_ntoa (ip),level_info_ptr->tcp_port );
		_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"public_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	else if (flag == 1) {	//in the same NAT
		peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
		ip.s_addr = level_info_ptr->private_ip;
		//selfip.s_addr = self_public_ip ;
		debug_printf("connect to private_ip %s  port= %d \n", inet_ntoa(ip),level_info_ptr->tcp_port);	
		_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"private_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknown flag in non_blocking_build_connection\n");
		_logger_client_ptr->log_exit();
	}
	
	peer_saddr.sin_port = htons(level_info_ptr->tcp_port);
	peer_saddr.sin_family = AF_INET;
	
	_log_ptr->write_log_format("s(u) s d s s(s) d \n", __FUNCTION__, __LINE__,
												"Connecting to pid", level_info_ptr->pid, 
												"IP", inet_ntoa(*(struct in_addr *)&level_info_ptr->public_ip), inet_ntoa(*(struct in_addr *)&level_info_ptr->private_ip), 
												level_info_ptr->tcp_port);
	
	if ((retVal = connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) < 0) {
		int socketErr = WSAGetLastError();
		if (socketErr == WSAEWOULDBLOCK) {
			_net_ptr->set_nonblocking(_sock);
			_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_ptr));
			_peer_mgr_ptr->fd_list_ptr->push_back(_sock);	
			//_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"build_ connection failure : WSAEWOULDBLOCK");
		}
		else {
#ifdef _WIN32
			::closesocket(_sock);
			::WSACleanup();
			
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "[ERROR] Build connection failed", retVal, socketErr);
			debug_printf("[ERROR] Build connection failed %d %d \n", retVal, socketErr);
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed", retVal, socketErr);
			_logger_client_ptr->log_exit();
#else
			::close(_sock);
#endif
		}

	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[DEBUG] Build connection too fast", retVal);
		debug_printf("[DEBUG] Build connection too fast %d \n", retVal);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Build connection too fast", retVal);
		_logger_client_ptr->log_exit();
	}

	/*
	this part stores the info in each table.
	*/
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect (before) fd =", _sock,
																"manifest =", manifest,
																"session_id =", session_id,
																"role =", caller,
																"pid =", fd_pid,
																"non_blocking_build_connection (candidate peer)");

	map_fd_info_iter = map_fd_info.find(_sock);
	if(map_fd_info_iter != map_fd_info.end()){
		
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : fd %d already in map_fd_info in non_blocking_build_connection\n",_sock);
		_logger_client_ptr->log_exit();
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
	map_fd_info_iter->second->flag = caller;
	map_fd_info_iter->second->manifest = manifest;
	map_fd_info_iter->second->pid = fd_pid;
	map_fd_info_iter->second->session_id = session_id;
	
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect fd =", map_fd_info_iter->first,
																"manifest =", map_fd_info_iter->second->manifest,
																"session_id =", map_fd_info_iter->second->session_id,
																"role =", map_fd_info_iter->second->flag,
																"pid =", map_fd_info_iter->second->pid,
																"non_blocking_build_connection (candidate peer)");
	
	return RET_OK;
}

io_accept * peer_communication::get_io_accept_handler(){
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

// When connect time triggered, this function will be called
void peer_communication::stop_attempt_connect(unsigned long stop_session_id){
	/*
	erase the manifest structure, and close and take out the fd if it is not in fd_pid table.
	*/
	//_log_ptr->write_log_format("s =>u \n", __FUNCTION__,__LINE__);
	session_id_candidates_set_iter = session_id_candidates_set.find(stop_session_id);
	if(session_id_candidates_set_iter == session_id_candidates_set.end()){
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"cannot find stop_session_id in structure in stop_attempt_connect");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find stop_session_id in structure in stop_attempt_connect\n");
		_logger_client_ptr->log_exit();
	}
	else{
		int delete_fd_flag = 0;

		if(session_id_candidates_set_iter->second->role == 0){	//caller is rescue peer
			total_manifest = total_manifest & (~session_id_candidates_set_iter->second->manifest);
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"find candidates in structure in stop_attempt_connect may be rescue peer");
			_log_ptr->write_log_format("s =>u s d s d s d s d\n", __FUNCTION__,__LINE__,"session_id : ",stop_session_id,", manifest : ",session_id_candidates_set_iter->second->manifest,", role: ",session_id_candidates_set_iter->second->role,", list_number: ",session_id_candidates_set_iter->second->peer_num);
			for(int i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
				//fprintf(peer_com_log,"list pid : %d, public_ip : %s, private_ip: %s\n",session_id_candidates_set_iter->second->list_info->level_info[i]->pid,session_id_candidates_set_iter->second->list_info->level_info[i]->public_ip,session_id_candidates_set_iter->second->list_info->level_info[i]->private_ip);
				_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"list pid : ",session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
				delete session_id_candidates_set_iter->second->list_info->level_info[i];

			}

			delete session_id_candidates_set_iter->second->list_info;
			delete session_id_candidates_set_iter->second;
			session_id_candidates_set.erase(session_id_candidates_set_iter);

			/*map<int, unsigned long>::iterator map_fd_session_id_iter;
			map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
			map<int, unsigned long>::iterator map_fd_manifest_iter;*/

			map_fd_info_iter = map_fd_info.begin();
			while(map_fd_info_iter != map_fd_info.end()){
				if(map_fd_info_iter->second->session_id == stop_session_id){

					delete_fd_flag = 1;

					if(_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()){
						/*
						connect faild delete table and close fd
						*/


						_log_ptr->write_log_format("s =>u s d s u \n", __FUNCTION__, __LINE__,
																	"connect faild delete table and close fd", map_fd_info_iter->first,
																	"pid", map_fd_info_iter->second->pid);
						/*
						close fd
						*/
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
						/*
						connect succeed just delete table
						*/
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
		else{	//caller is candidate
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"find candidates in structure in stop_attempt_connect may be candidate peer");
			_log_ptr->write_log_format("s =>u s d s d s d s d\n", __FUNCTION__,__LINE__,"session_id : ",stop_session_id,", manifest : ",session_id_candidates_set_iter->second->manifest,", role: ",session_id_candidates_set_iter->second->role,", list_number: ",session_id_candidates_set_iter->second->peer_num);
			for(int i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
				//fprintf(peer_com_log,"list pid : %d, public_ip : %s, private_ip: %s\n",session_id_candidates_set_iter->second->list_info->level_info[i]->pid,session_id_candidates_set_iter->second->list_info->level_info[i]->public_ip,session_id_candidates_set_iter->second->list_info->level_info[i]->private_ip);
				_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"list pid : ",session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
				delete session_id_candidates_set_iter->second->list_info->level_info[i];

			}

			delete session_id_candidates_set_iter->second->list_info;
			delete session_id_candidates_set_iter->second;
			session_id_candidates_set.erase(session_id_candidates_set_iter);

			/*map<int, unsigned long>::iterator map_fd_session_id_iter;
			map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
			map<int, unsigned long>::iterator map_fd_manifest_iter;*/

			map_fd_info_iter = map_fd_info.begin();
			while(map_fd_info_iter != map_fd_info.end()){
				if(map_fd_info_iter->second->session_id == stop_session_id){

					delete_fd_flag = 1;

					if(_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()){
						/*
						connect faild delete table and close fd
						*/
						

						_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"connect faild delete table and close fd ",map_fd_info_iter->first);
						/*
						close fd
						*/
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
						/*
						connect succeed just delete table
						*/
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
	}
}



int peer_communication::handle_pkt_in(int sock)
{	
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only receive PEER_CON protocol sent by join/rescue peer (the peer is candidate's peer).
	And handle P2P structure.
	*/
	printf("peer_communication::handle_pkt_in \n");
	
	map_fd_info_iter = map_fd_info.find(sock);
	
	if(map_fd_info_iter == map_fd_info.end()){
		
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info structure in peer_communication::handle_pkt_in\n");
		_logger_client_ptr->log_exit();
	}
	else{
		printf("map_fd_info_iter->second->flag: %d \n", map_fd_info_iter->second->flag);
		if(map_fd_info_iter->second->flag == 0) {	//this fd is rescue peer
			//do nothing rebind to event out only
			_net_ptr->set_nonblocking(sock);
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLOUT);
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"this fd is rescue peer do nothing, jsut rebind to event out only");
		}
		else if(map_fd_info_iter->second->flag == 1){	//this fd is candidate peer
			
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
				/*
				use fake cin info
				*/
				struct sockaddr_in fake_cin;
				memset(&fake_cin,0x00,sizeof(struct sockaddr_in));

				_peer_ptr->handle_connect(sock, chunk_ptr,fake_cin);

				/*
				bind to peer_com~ object
				*/
				_net_ptr->set_nonblocking(sock);
				_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
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

int peer_communication::handle_pkt_out(int sock)	//first write, then set fd to readable & excecption only
{
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only send PEER_CON protocol to candidates, if the fd is in the list. (the peer is join/rescue peer)
	And handle P2P structure.
	*/
	
	printf("peer_communication::handle_pkt_out \n");
	
	map_fd_info_iter = map_fd_info.find(sock);
	if (map_fd_info_iter == map_fd_info.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
		_logger_client_ptr->log_exit();
	}
	else {
		printf("map_fd_info_iter->second->flag: %d \n", map_fd_info_iter->second->flag);
		if(map_fd_info_iter->second->flag == 0){	//this fd is rescue peer
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
			
				_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				
			}else if (map_fd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == READY){
				
				_net_ptr->set_nonblocking(sock);
				_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
				return RET_OK;
			}
		}
		else if(map_fd_info_iter->second->flag == 1){	//this fd is candidate
			//do nothing rebind to event in only
			_net_ptr->set_nonblocking(sock);
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"this fd is candidate do nothing rebind to event in only");
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
		}
		else{	
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknow flag in peer_communication::handle_pkt_out\n");
			_logger_client_ptr->log_exit();
		}
	}

	return RET_OK;
}

void peer_communication::handle_pkt_error(int sock)
{
	
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in peer_communication error number : ",WSAGetLastError());
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