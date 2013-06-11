
#include "peer_communication.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "io_accept.h"
#include "io_connect.h"

using namespace std;

peer_communication::peer_communication(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr){
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	total_manifest = 0;
	session_id_count = 0;
	self_info = new struct level_info_t;
	_io_accept_ptr = new io_accept(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this);
	_io_connect_ptr = new io_connect(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this);

	peer_com_log = fopen("./peer_com_log.txt","wb");
}

void peer_communication::set_self_info(unsigned long public_ip){
	self_info->public_ip = public_ip;
	self_info->private_ip = _net_ptr->getLocalIpv4();
}

int peer_communication::set_candidates_handler(unsigned long rescue_manifest,struct chunk_level_msg_t *testing_info,unsigned int candidates_num,int flag){	//flag 0 rescue peer, flag 1 candidate's peer
	
	fprintf(peer_com_log,"\n");
	fprintf(peer_com_log,"set_candidates_handler\n");
	fprintf(peer_com_log,"session_id : %d, manifest : %d, role: %d, list_number: %d\n",session_id_count,rescue_manifest,flag,candidates_num);
	for(int i=0;i<candidates_num;i++){
		//fprintf(peer_com_log,"list pid : %d, public_ip : %d, private_ip: %d\n",testing_info->level_info[i]->pid,testing_info->level_info[i]->public_ip,testing_info->level_info[i]->private_ip);
		fprintf(peer_com_log,"list pid : %d\n",testing_info->level_info[i]->pid);
	}
	fflush(peer_com_log);
	
	if((candidates_num==0)&&(flag==0)){
		fprintf(peer_com_log,"rescue peer call peer API, but list is empty\n");
		if((total_manifest & rescue_manifest)==1){
			printf("error : re-rescue for some sub stream : %d %d in set_candidates_test\n",total_manifest,rescue_manifest);
			PAUSE
			exit(1);
		}
		else{
			printf("rescue manifest: %d already rescue manifest: %d\n",rescue_manifest,total_manifest);
			fprintf(peer_com_log,"rescue manifest: %d already rescue manifest: %d\n",rescue_manifest,total_manifest);
			total_manifest = total_manifest | rescue_manifest;	//total_manifest has to be erased in stop_attempt_connect
		
			session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
			if(session_id_candidates_set_iter != session_id_candidates_set.end()){
				printf("error : session id already in the record in set_candidates_test\n");
				PAUSE
				exit(1);
			}
			else{
				session_id_candidates_set[session_id_count] = new struct peer_com_info;

				session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
				if(session_id_candidates_set_iter == session_id_candidates_set.end()){
					printf("error : session id cannot find in the record in set_candidates_test\n");
					PAUSE
					exit(1);
				}

				int level_msg_size,offset;
				offset = 0;
				level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + candidates_num * sizeof(struct level_info_t *);

				session_id_candidates_set_iter->second->peer_num = candidates_num;
				session_id_candidates_set_iter->second->manifest = rescue_manifest;
				session_id_candidates_set_iter->second->role = 0;
				session_id_candidates_set_iter->second->list_info = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
				memset(session_id_candidates_set_iter->second->list_info, 0x0, level_msg_size);
				memcpy(session_id_candidates_set_iter->second->list_info, testing_info, (level_msg_size - candidates_num * sizeof(struct level_info_t *)));

				offset += (level_msg_size - candidates_num * sizeof(struct level_info_t *));

				for(int i=0;i<candidates_num;i++){
					session_id_candidates_set_iter->second->list_info->level_info[i] = new struct level_info_t;
					memset(session_id_candidates_set_iter->second->list_info->level_info[i], 0x0 , sizeof(struct level_info_t));
					memcpy(session_id_candidates_set_iter->second->list_info->level_info[i], testing_info->level_info[i] , sizeof(struct level_info_t));
					offset += sizeof(struct level_info_t);
				}
			}
			session_id_count++;
		}
		fflush(peer_com_log);
		return (session_id_count-1);
	}
	else if((candidates_num==0)&&(flag==1)){
		printf("unknow state in set_candidates_handler\n");
		PAUSE
		exit(1);
	}

	if(flag == 0){
		fprintf(peer_com_log,"rescue peer call peer API\n");
		if((total_manifest & rescue_manifest)==1){
			printf("error : re-rescue for some sub stream : %d %d in set_candidates_test\n",total_manifest,rescue_manifest);
			PAUSE
			exit(1);
		}
		else{
			printf("rescue manifest: %d already rescue manifest: %d\n",rescue_manifest,total_manifest);
			fprintf(peer_com_log,"rescue manifest: %d already rescue manifest: %d\n",rescue_manifest,total_manifest);
			total_manifest = total_manifest | rescue_manifest;	//total_manifest has to be erased in stop_attempt_connect
		
			session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
			if(session_id_candidates_set_iter != session_id_candidates_set.end()){
				printf("error : session id already in the record in set_candidates_test\n");
				PAUSE
				exit(1);
			}
			else{
				session_id_candidates_set[session_id_count] = new struct peer_com_info;

				session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
				if(session_id_candidates_set_iter == session_id_candidates_set.end()){
					printf("error : session id cannot find in the record in set_candidates_test\n");
					PAUSE
					exit(1);
				}

				int level_msg_size,offset;
				offset = 0;
				level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + candidates_num * sizeof(struct level_info_t *);

				session_id_candidates_set_iter->second->peer_num = candidates_num;
				session_id_candidates_set_iter->second->manifest = rescue_manifest;
				session_id_candidates_set_iter->second->role = 0;
				session_id_candidates_set_iter->second->list_info = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
				memset(session_id_candidates_set_iter->second->list_info, 0x0, level_msg_size);
				memcpy(session_id_candidates_set_iter->second->list_info, testing_info, (level_msg_size - candidates_num * sizeof(struct level_info_t *)));

				offset += (level_msg_size - candidates_num * sizeof(struct level_info_t *));

				for(int i=0;i<candidates_num;i++){
					session_id_candidates_set_iter->second->list_info->level_info[i] = new struct level_info_t;
					memset(session_id_candidates_set_iter->second->list_info->level_info[i], 0x0 , sizeof(struct level_info_t));
					memcpy(session_id_candidates_set_iter->second->list_info->level_info[i], testing_info->level_info[i] , sizeof(struct level_info_t));
					offset += sizeof(struct level_info_t);
				}

				for(int i=0;i<candidates_num;i++){
					if((self_info->private_ip == self_info->public_ip)&&(testing_info->level_info[i]->private_ip ==testing_info->level_info[i]->public_ip)){	//self public ip , candidate public ip
						printf("all public ip active connect\n");
						fprintf(peer_com_log,"all public ip active connect\n");
						non_blocking_build_connection(testing_info->level_info[i],0,rescue_manifest,testing_info->level_info[i]->pid,0,session_id_count);
					}
					else if((self_info->private_ip == self_info->public_ip)&&(testing_info->level_info[i]->private_ip !=testing_info->level_info[i]->public_ip)){	//self public ip , candidate private ip
						printf("candidate is private ip passive connect\n");
						fprintf(peer_com_log,"candidate is private ip passive connect\n");
						accept_check(testing_info->level_info[i],0,rescue_manifest,testing_info->level_info[i]->pid,session_id_count);
					}
					else if((self_info->private_ip != self_info->public_ip)&&(testing_info->level_info[i]->private_ip ==testing_info->level_info[i]->public_ip)){	//self private ip , candidate public ip
						printf("rescue peer is public ip active connect\n");
						fprintf(peer_com_log,"rescue peer is public ip active connect\n");
						non_blocking_build_connection(testing_info->level_info[i],0,rescue_manifest,testing_info->level_info[i]->pid,0,session_id_count);
					}
					else if((self_info->private_ip != self_info->public_ip)&&(testing_info->level_info[i]->private_ip !=testing_info->level_info[i]->public_ip)){	//self private ip , candidate private ip
						printf("all private ip use NAT module\n");
						fprintf(peer_com_log,"all private ip use NAT module\n");
						if(self_info->public_ip == testing_info->level_info[i]->public_ip){
							printf("same NAT device active connect\n");
							fprintf(peer_com_log,"same NAT device active connect\n");
							non_blocking_build_connection(testing_info->level_info[i],0,rescue_manifest,testing_info->level_info[i]->pid,1,session_id_count);
						}
					}
					else{
						printf("error : unknown state in rescue peer\n");
						PAUSE
						exit(1);
					}
				}
			}
			session_id_count++;
		}
	}
	else if(flag == 1){
		fprintf(peer_com_log,"candidate calls peer_com API\n");
		if((candidates_num>1)||((candidates_num==0))){
			printf("error : candidate num must be 1\n");
			PAUSE
			exit(1);
		}
		else{
			printf("candidate manifest: %d \n",rescue_manifest);
			fprintf(peer_com_log,"candidate manifest: %d \n",rescue_manifest);
		
			session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
			if(session_id_candidates_set_iter != session_id_candidates_set.end()){
				printf("error : session id in the record in set_candidates_test\n");
				PAUSE
				exit(1);
			}
			else{
				session_id_candidates_set[session_id_count] = new struct peer_com_info;

				session_id_candidates_set_iter = session_id_candidates_set.find(session_id_count);	//manifest_candidates_set has to be erased in stop_attempt_connect
				if(session_id_candidates_set_iter == session_id_candidates_set.end()){
					printf("error : session id cannot find in the record in set_candidates_test\n");
					PAUSE
					exit(1);
				}

				int level_msg_size,offset;
				offset = 0;
				level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + candidates_num * sizeof(struct level_info_t *);

				session_id_candidates_set_iter->second->peer_num = candidates_num;
				session_id_candidates_set_iter->second->manifest = rescue_manifest;
				session_id_candidates_set_iter->second->role = 1;
				session_id_candidates_set_iter->second->list_info = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
				memset(session_id_candidates_set_iter->second->list_info, 0x0, level_msg_size);
				memcpy(session_id_candidates_set_iter->second->list_info, testing_info, (level_msg_size - candidates_num * sizeof(struct level_info_t *)));

				offset += (level_msg_size - candidates_num * sizeof(struct level_info_t *));

				for(int i=0;i<candidates_num;i++){
					session_id_candidates_set_iter->second->list_info->level_info[i] = new struct level_info_t;
					memset(session_id_candidates_set_iter->second->list_info->level_info[i], 0x0 , sizeof(struct level_info_t));
					memcpy(session_id_candidates_set_iter->second->list_info->level_info[i], testing_info->level_info[i] , sizeof(struct level_info_t));
					offset += sizeof(struct level_info_t);
				}
				if((self_info->private_ip == self_info->public_ip)&&(testing_info->level_info[0]->private_ip ==testing_info->level_info[0]->public_ip)){	//self public ip , rescue peer public ip
					printf("all public ip passive connect in cnadidate\n");
					fprintf(peer_com_log,"all public ip passive connect in cnadidate\n");
					accept_check(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,session_id_count);
				}
				else if((self_info->private_ip == self_info->public_ip)&&(testing_info->level_info[0]->private_ip !=testing_info->level_info[0]->public_ip)){	//self public ip , rescue peer private ip
					printf("rescue peer is private ip passive connect in cnadidate\n");
					fprintf(peer_com_log,"rescue peer is private ip passive connect in cnadidate\n");
					accept_check(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,session_id_count);
				}
				else if((self_info->private_ip != self_info->public_ip)&&(testing_info->level_info[0]->private_ip ==testing_info->level_info[0]->public_ip)){	//self private ip , rescue peer public ip
					printf("candidate is private ip active connect in cnadidate\n");
					fprintf(peer_com_log,"candidate is private ip active connect in cnadidate\n");
					non_blocking_build_connection(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,0,session_id_count);
				}
				else if((self_info->private_ip != self_info->public_ip)&&(testing_info->level_info[0]->private_ip !=testing_info->level_info[0]->public_ip)){	//self private ip , rescue peer private ip
					printf("all private ip use NAT module in cnadidate\n");
					fprintf(peer_com_log,"all private ip use NAT module in cnadidate\n");
					if(self_info->public_ip == testing_info->level_info[0]->public_ip){
							printf("same NAT device passive connect in cnadidate\n");
							fprintf(peer_com_log,"same NAT device passive connect in cnadidate\n");
							accept_check(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,session_id_count);
					}
				}
				else{
					printf("error : unknown state in candidate's peer\n");
					PAUSE
					exit(1);
				}
			}
			session_id_count++;
		}
	}
	else{
		printf("error : unknow flag in set_candidates_test\n");
		PAUSE
		exit(1);
	}

	fflush(peer_com_log);
	return (session_id_count-1);
}

void peer_communication::clear_fd_in_peer_com(int sock){
	fprintf(peer_com_log,"\n");
	fprintf(peer_com_log,"fd : %d start close in peer_communication::clear_fd_in_peer_com\n",sock);
	fflush(peer_com_log);

	map_fd_info_iter = map_fd_info.find(sock);
	if(map_fd_info_iter == map_fd_info.end()){
		fprintf(peer_com_log,"fd : %d close fail (clear_fd_in_peer_com)\n",sock);
		fflush(peer_com_log);
	}
	else{
		fprintf(peer_com_log,"fd : %d session id : %d pid : %d close succeed (clear_fd_in_peer_com)\n",sock,map_fd_info_iter->second->session_id,map_fd_info_iter->second->pid);
		fflush(peer_com_log);

		delete map_fd_info_iter->second;
		map_fd_info.erase(map_fd_info_iter);
	}
	fprintf(peer_com_log,"\n");
	fflush(peer_com_log);
}

void peer_communication::accept_check(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id){
	//map<int, int>::iterator map_fd_unknown_iter;
	list<int>::iterator map_fd_unknown_iter;
	/*map<int, unsigned long>::iterator map_fd_session_id_iter;
	map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
	map<int, unsigned long>::iterator map_fd_manifest_iter;*/

	for(map_fd_unknown_iter = _io_accept_ptr->map_fd_unknown.begin();map_fd_unknown_iter != _io_accept_ptr->map_fd_unknown.end();map_fd_unknown_iter++){
		//if(*map_fd_unknown_iter == 1){
			
			/*map_fd_flag_iter = map_fd_flag.find(map_fd_unknown_iter->first);
			if(map_fd_flag_iter == map_fd_flag.end()){
				printf("cannot find map_fd_flag structure in peer_communication::accept_check\n");
				exit(1);
			}

			map_peer_com_fd_pid_iter = map_peer_com_fd_pid.find(map_fd_unknown_iter->first);
			if(map_peer_com_fd_pid_iter == map_peer_com_fd_pid.end()){
				printf("cannot find map_peer_com_fd_pid structure in peer_communication::accept_check\n");
				exit(1);
			}

			map_fd_manifest_iter = map_fd_manifest.find(map_fd_unknown_iter->first);
			if(map_fd_manifest_iter == map_fd_manifest.end()){
				printf("cannot find map_fd_manifest structure in peer_communication::accept_check\n");
				exit(1);
			}*/
			map_fd_info_iter = map_fd_info.find(*map_fd_unknown_iter);
			if(map_fd_info_iter == map_fd_info.end()){
				printf("cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
				PAUSE
				exit(1);
			}

			if((manifest == map_fd_info_iter->second->manifest)&&(fd_role == map_fd_info_iter->second->flag)&&(fd_pid == map_fd_info_iter->second->pid)){
				fprintf(peer_com_log,"fd : %d update session of fd in peer_communication::accept_check\n",*map_fd_unknown_iter);
				printf("fd : %d update session of fd in peer_communication::accept_check\n",*map_fd_unknown_iter);
				/*map_fd_session_id_iter = map_fd_session_id.find(map_fd_unknown_iter->first);
				if(map_fd_session_id_iter != map_fd_session_id.end()){
					printf("fd already has session id in peer_communication::accept_check\n");
					exit(1);
				}
				else{
					map_fd_session_id[map_fd_unknown_iter->first] = session_id;
				}*/
				map_fd_info_iter->second->session_id = session_id;

				/*
				bind to peer_com~ object
				*/
				fprintf(peer_com_log,"fd : %d bind to peer_com in peer_communication::accept_check\n",*map_fd_unknown_iter);
				_net_ptr->set_nonblocking(map_fd_info_iter->first);

				_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(map_fd_info_iter->first, dynamic_cast<basic_class *> (this));
				_peer_mgr_ptr->fd_list_ptr->push_back(map_fd_info_iter->first);

				_io_accept_ptr->map_fd_unknown.erase(map_fd_unknown_iter);
				break;
			}
		/*}
		else{
			printf("error : unknown error in peer_communication::accept_check\n");
			exit(1);
		}*/
	}

	/*
	call nat accept check
	*/
}

int peer_communication::non_blocking_build_connection(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, int flag, unsigned long session_id){	//flag 0 public ip flag 1 private ip //fd_role 0 rescue peer fd_role 1 
	struct sockaddr_in peer_saddr;
	int ret;
	struct in_addr ip;
	int _sock;
	/*
	this part means that if the parent is exist, we don't create it again.
	*/
	//fprintf(peer_com_log,"call non_blocking_build_connection ip : %s , role :%d , manifest : %d , fd_pid : %d , flag : %d\n",level_info_ptr->public_ip,fd_role, manifest, fd_pid, flag);
	fprintf(peer_com_log,"call non_blocking_build_connection role :%d , manifest : %d , fd_pid : %d , flag : %d\n",fd_role, manifest, fd_pid, flag);
	if(fd_role == 0){
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

		//之前已經建立過連線的 在map_in_pid_fd裡面 則不再建立(保證對同個parent不再建立第二條線)
		for(map_pid_fd_iter = _peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == level_info_ptr->pid ){
				fprintf(peer_com_log,"fd already in map_in_pid_fd in non_blocking_build_connection (rescue peer)\n");
				fflush(peer_com_log);
				return 1;
			}
		}

		/*
		this may have problem****************************************************************
		*/
		pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(level_info_ptr ->pid);
		if(pid_peer_info_iter !=  _pk_mgr_ptr ->map_pid_peer_info.end() ){
			//兩個以上就沿用第一個的連線
			if(_pk_mgr_ptr ->map_pid_peer_info.count(level_info_ptr ->pid) >= 2 ){
				printf("pid =%d already in connect find in map_pid_peer_info  testing",level_info_ptr ->pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peer_info testing");
				fprintf(peer_com_log,"fd already in map_pid_peer_info in non_blocking_build_connection (rescue peer)\n");	
				fflush(peer_com_log);
				return 1;
			}
		}

		//若在map_pid_peerDown_info 則不再次建立連線
		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(level_info_ptr ->pid);
		if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
			printf("pid =%d already in connect find in map_pid_peerDown_info",level_info_ptr ->pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peerDown_info");
			fprintf(peer_com_log,"fd already in map_pid_peerdown_info in non_blocking_build_connection (rescue peer)\n");
			fflush(peer_com_log);
			return 1;
		}
	}
	else{
	/*
	this part means that if the child is exist, we don't create it again.
	*/
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;

		//之前已經建立過連線的 在map_out_pid_fd裡面 則不再建立(保證對同個child不再建立第二條線)
		for(map_pid_fd_iter = _peer_ptr->map_out_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_out_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == level_info_ptr->pid ){
				fprintf(peer_com_log,"fd already in map_out_pid_fd in non_blocking_build_connection (candidate peer)\n");
				fflush(peer_com_log);
				return 1;
			}
		}

		//若在map_pid_rescue_peer_info 則不再次建立連線
		map_pid_rescue_peer_info_iter = _pk_mgr_ptr ->map_pid_rescue_peer_info.find(level_info_ptr ->pid);
		if(map_pid_rescue_peer_info_iter != _pk_mgr_ptr ->map_pid_rescue_peer_info.end()){
			printf("pid =%d already in connect find in map_pid_rescue_peer_info",level_info_ptr ->pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_rescue_peer_info");
			fprintf(peer_com_log,"fd already in map_pid_rescue_peer_info in non_blocking_build_connection (candidate peer)\n");
			fflush(peer_com_log);
			return 1;
		}
	}

	if((_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		cout << "init create socket failure" << endl;

		_net_ptr ->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
		PAUSE
		exit(1);
	}

	_net_ptr ->set_nonblocking(_sock);	//non-blocking connect
	memset((struct sockaddr_in*)&peer_saddr, 0x0, sizeof(struct sockaddr_in));

    if(flag == 0){	
	    peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
		ip.s_addr = level_info_ptr->public_ip;
		printf("connect to public_ip %s port= %d \n" ,inet_ntoa (ip),level_info_ptr->tcp_port );
		_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"public_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	else if(flag == 1){	//in the same NAT
		peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
		ip.s_addr = level_info_ptr->private_ip;
//		selfip.s_addr = self_public_ip ;
		printf("connect to private_ip %s  port= %d \n", inet_ntoa(ip),level_info_ptr->tcp_port);	
		_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"private_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	else{
		printf("error : unknown flag in non_blocking_build_connection\n");
		PAUSE
		exit(1);
	}
	peer_saddr.sin_port = htons(level_info_ptr->tcp_port);
	peer_saddr.sin_family = AF_INET;
	
	if(connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr)) < 0) {
		if(WSAGetLastError() == WSAEWOULDBLOCK){
			_net_ptr->set_nonblocking(_sock);
			_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_ptr));
			_peer_mgr_ptr->fd_list_ptr->push_back(_sock);	
			fprintf(peer_com_log,"build_ connection failure : WSAEWOULDBLOCK\n");
		}
		else{
			cout << "build_ connection failure : "<<WSAGetLastError()<< endl;
	#ifdef _WIN32
			::closesocket(_sock);
			::WSACleanup();
			PAUSE
			exit(1);
	#else
			::close(_sock);
	#endif
		}

	} else {
		cout << "build_ connection cannot too fast "<< endl;
		PAUSE
		exit(1);
		/*
		_net_ptr->set_nonblocking(_sock);
		_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);	
		_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *>(this));*/
	}

	/*
	this part stores the info in each table.
	*/
	fprintf(peer_com_log,"non blocking connect (before) fd : %d manifest : %d session_id : %d role : %d pid : %d non_blocking_build_connection (candidate peer)\n",_sock,manifest,session_id,fd_role,fd_pid);
	fflush(peer_com_log);

	map_fd_info_iter = map_fd_info.find(_sock);
	if(map_fd_info_iter != map_fd_info.end()){
		printf("error : fd %d already in map_fd_info in non_blocking_build_connection\n",_sock);
		PAUSE
		exit(1);
	}
	map_fd_info[_sock] = new struct fd_information;

	map_fd_info_iter = map_fd_info.find(_sock);
	if(map_fd_info_iter == map_fd_info.end()){
		printf("error : %d cannot new non_blocking_build_connection\n",_sock);
		PAUSE
		exit(1);
	}

	memset(map_fd_info_iter->second,0x00,sizeof(struct fd_information));
	map_fd_info_iter->second->flag = fd_role;
	map_fd_info_iter->second->manifest = manifest;
	map_fd_info_iter->second->pid = fd_pid;
	map_fd_info_iter->second->session_id = session_id;
	fprintf(peer_com_log,"non blocking connect fd : %d manifest : %d session_id : %d role : %d pid : %d non_blocking_build_connection (candidate peer)\n",map_fd_info_iter->first,map_fd_info_iter->second->manifest,map_fd_info_iter->second->session_id,map_fd_info_iter->second->flag,map_fd_info_iter->second->pid);
	fflush(peer_com_log);
	/*map<int, unsigned long>::iterator map_fd_session_id_iter;	//must be store before connect, and delete in stop
	map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;	//must be store before connect, and delete in stop
	map<int, unsigned long>::iterator map_fd_manifest_iter;	//must be store before connect, and delete in stop

	map_fd_flag_iter = map_fd_flag.find(_sock);
	if(map_fd_flag_iter != map_fd_flag.end()){
		printf("error : fd already in map_fd_flag in non_blocking_build_connection\n");
		exit(1);
	}
	map_fd_flag[_sock] = fd_role;

	map_fd_session_id_iter = map_fd_session_id.find(_sock);
	if(map_fd_session_id_iter != map_fd_session_id.end()){
		printf("error : fd already in map_fd_session_id in non_blocking_build_connection\n");
		exit(1);
	}
	map_fd_session_id[_sock] = session_id;

	map_fd_manifest_iter = map_fd_manifest.find(_sock);
	if(map_fd_manifest_iter != map_fd_manifest.end()){
		printf("error : fd already in map_fd_manifest in non_blocking_build_connection\n");
		exit(1);
	}
	map_fd_manifest[_sock] = manifest;
	
	map_peer_com_fd_pid_iter = map_peer_com_fd_pid.find(_sock);
	if(map_peer_com_fd_pid_iter != map_peer_com_fd_pid.end()){
		printf("error : fd already in map_peer_com_fd_pid in non_blocking_build_connection\n");
		exit(1);
	}
	map_peer_com_fd_pid[_sock] = fd_pid;*/

	return RET_OK;
}

io_accept * peer_communication::get_io_accept_handler(){
	return _io_accept_ptr;
}

void peer_communication::fd_close(int sock){
	fprintf(peer_com_log,"fd %d close in peer_communication::fd_close\n",sock);
	fflush(peer_com_log);
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
		fprintf(peer_com_log,"error : fd %d close cannot find table in peer_communication::fd_close\n",sock);
		fflush(peer_com_log);
		exit(1);
	}
	else{
		map_fd_info.erase(sock);
	}
}

void peer_communication::stop_attempt_connect(unsigned long stop_session_id){
	/*
	erase the manifest structure, and close and take out the fd if it is not in fd_pid table.
	*/
	fprintf(peer_com_log,"\n");
	session_id_candidates_set_iter = session_id_candidates_set.find(stop_session_id);
	if(session_id_candidates_set_iter == session_id_candidates_set.end()){
		printf("cannot find stop_session_id in structure in stop_attempt_connect\n");
		fprintf(peer_com_log,"cannot find stop_session_id in structure in stop_attempt_connect\n");
		PAUSE
		exit(1);
	}
	else{
		int delete_fd_flag = 0;

		if(session_id_candidates_set_iter->second->role == 0){	//caller is rescue peer
			total_manifest = total_manifest & (~session_id_candidates_set_iter->second->manifest);
			fprintf(peer_com_log,"find candidates in structure in stop_attempt_connect may be rescue peer\n");
			fprintf(peer_com_log,"session_id : %d, manifest : %d, role: %d, list_number: %d\n",stop_session_id,session_id_candidates_set_iter->second->manifest,session_id_candidates_set_iter->second->role,session_id_candidates_set_iter->second->peer_num);
			for(int i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
				//fprintf(peer_com_log,"list pid : %d, public_ip : %s, private_ip: %s\n",session_id_candidates_set_iter->second->list_info->level_info[i]->pid,session_id_candidates_set_iter->second->list_info->level_info[i]->public_ip,session_id_candidates_set_iter->second->list_info->level_info[i]->private_ip);
				fprintf(peer_com_log,"list pid : %d\n",session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			}
			fflush(peer_com_log);

			delete session_id_candidates_set_iter->second->list_info;
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
						/*map_peer_com_fd_pid_iter = map_peer_com_fd_pid.find(map_fd_session_id_iter->first);
						if(map_peer_com_fd_pid_iter == map_peer_com_fd_pid.end()){
							printf("error : cannot find map_peer_com_fd_pid in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_peer_com_fd_pid.erase(map_peer_com_fd_pid_iter);
						}

						map_fd_flag_iter = map_fd_flag.find(map_fd_session_id_iter->first);
						if(map_fd_flag_iter == map_fd_flag.end()){
							printf("error : cannot find map_fd_flag in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_flag.erase(map_fd_flag_iter);
						}

						map_fd_manifest_iter = map_fd_manifest.find(map_fd_session_id_iter->first);
						if(map_fd_manifest_iter == map_fd_manifest.end()){
							printf("error : cannot find map_fd_manifest in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_manifest.erase(map_fd_manifest_iter);
						}*/

						fprintf(peer_com_log,"connect faild delete table and close fd %d \n",map_fd_info_iter->first);
						/*
						close fd
						*/
						list<int>::iterator fd_iter;
	
						_log_ptr->write_log_format("s => s \n", (char*)__PRETTY_FUNCTION__, "peer_com");
						cout << "peer_com close fd since timeout " << map_fd_info_iter->first <<  endl;
						_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_DEL, 0);
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
						fprintf(peer_com_log,"connect succeed just delete table \n");
						/*map_peer_com_fd_pid_iter = map_peer_com_fd_pid.find(map_fd_session_id_iter->first);
						if(map_peer_com_fd_pid_iter == map_peer_com_fd_pid.end()){
							printf("error : cannot find map_peer_com_fd_pid in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_peer_com_fd_pid.erase(map_peer_com_fd_pid_iter);
						}

						map_fd_flag_iter = map_fd_flag.find(map_fd_session_id_iter->first);
						if(map_fd_flag_iter == map_fd_flag.end()){
							printf("error : cannot find map_fd_flag in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_flag.erase(map_fd_flag_iter);
						}

						map_fd_manifest_iter = map_fd_manifest.find(map_fd_session_id_iter->first);
						if(map_fd_manifest_iter == map_fd_manifest.end()){
							printf("error : cannot find map_fd_manifest in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_manifest.erase(map_fd_manifest_iter);
						}*/
					}

					map_fd_info.erase(map_fd_info_iter);
					map_fd_info_iter = map_fd_info.begin();
				}
				else{
					map_fd_info_iter++;
				}
			}
		}
		else{	//caller is candidate
			fprintf(peer_com_log,"find candidates in structure in stop_attempt_connect may be candidate peer\n");
			fprintf(peer_com_log,"session_id : %d, manifest : %d, role: %d, list_number: %d\n",stop_session_id,session_id_candidates_set_iter->second->manifest,session_id_candidates_set_iter->second->role,session_id_candidates_set_iter->second->peer_num);
			for(int i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
				//fprintf(peer_com_log,"list pid : %d, public_ip : %s, private_ip: %s\n",session_id_candidates_set_iter->second->list_info->level_info[i]->pid,session_id_candidates_set_iter->second->list_info->level_info[i]->public_ip,session_id_candidates_set_iter->second->list_info->level_info[i]->private_ip);
				fprintf(peer_com_log,"list pid : %d\n",session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			}
			fflush(peer_com_log);

			delete session_id_candidates_set_iter->second->list_info;
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
						/*map_peer_com_fd_pid_iter = map_peer_com_fd_pid.find(map_fd_session_id_iter->first);
						if(map_peer_com_fd_pid_iter == map_peer_com_fd_pid.end()){
							printf("error : cannot find map_peer_com_fd_pid in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_peer_com_fd_pid.erase(map_peer_com_fd_pid_iter);
						}

						map_fd_flag_iter = map_fd_flag.find(map_fd_session_id_iter->first);
						if(map_fd_flag_iter == map_fd_flag.end()){
							printf("error : cannot find map_fd_flag in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_flag.erase(map_fd_flag_iter);
						}

						map_fd_manifest_iter = map_fd_manifest.find(map_fd_session_id_iter->first);
						if(map_fd_manifest_iter == map_fd_manifest.end()){
							printf("error : cannot find map_fd_manifest in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_manifest.erase(map_fd_manifest_iter);
						}*/

						fprintf(peer_com_log,"connect faild delete table and close fd %d \n",map_fd_info_iter->first);
						/*
						close fd
						*/
						list<int>::iterator fd_iter;
	
						_log_ptr->write_log_format("s => s \n", (char*)__PRETTY_FUNCTION__, "peer_com");
						cout << "peer_com close fd since timeout " << map_fd_info_iter->first <<  endl;
						_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_DEL, 0);
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
						fprintf(peer_com_log,"connect succeed just delete table \n");
						/*map_peer_com_fd_pid_iter = map_peer_com_fd_pid.find(map_fd_session_id_iter->first);
						if(map_peer_com_fd_pid_iter == map_peer_com_fd_pid.end()){
							printf("error : cannot find map_peer_com_fd_pid in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_peer_com_fd_pid.erase(map_peer_com_fd_pid_iter);
						}

						map_fd_flag_iter = map_fd_flag.find(map_fd_session_id_iter->first);
						if(map_fd_flag_iter == map_fd_flag.end()){
							printf("error : cannot find map_fd_flag in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_flag.erase(map_fd_flag_iter);
						}

						map_fd_manifest_iter = map_fd_manifest.find(map_fd_session_id_iter->first);
						if(map_fd_manifest_iter == map_fd_manifest.end()){
							printf("error : cannot find map_fd_manifest in stop_attempt_connect\n");
							exit(1);
						}
						else{
							map_fd_manifest.erase(map_fd_manifest_iter);
						}*/

					}

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
			fprintf(peer_com_log,"cannot find fd info table \n");
		}
		else{
			fprintf(peer_com_log,"delete fd info table\n");
		}
		fprintf(peer_com_log,"io handler : ");
		for(map_fd_info_iter = map_fd_info.begin();map_fd_info_iter != map_fd_info.end();map_fd_info_iter++){
			fprintf(peer_com_log,"fd : %d, pid: %d\n",map_fd_info_iter->first,map_fd_info_iter->second->pid);
		}
		fprintf(peer_com_log,"\n");
		fprintf(peer_com_log,"\n");
		fprintf(peer_com_log,"\n");
		fflush(peer_com_log);
	}
}

peer_communication::~peer_communication(){
}

int peer_communication::handle_pkt_in(int sock)
{	
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only receive PEER_CON protocol sent by join/rescue peer (the peer is candidate's peer).
	And handle P2P structure.
	*/
	map_fd_info_iter = map_fd_info.find(sock);
	if(map_fd_info_iter == map_fd_info.end()){
		printf("cannot find map_fd_info structure in peer_communication::handle_pkt_in\n");
		PAUSE
		exit(1);
	}
	else{
		if(map_fd_info_iter->second->flag == 0){	//this fd is rescue peer
			//do nothing rebind to event out only
			_net_ptr->set_nonblocking(sock);
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLOUT);
			fprintf(peer_com_log,"this fd is rescue peer do nothing rebind to event out only\n");
		}
		else if(map_fd_info_iter->second->flag == 1){	//this fd is candidate
			/*
			read peer con
			*/
			int recv_byte;	
			int expect_len;
			int offset = 0;
			unsigned long buf_len;
			struct chunk_t *chunk_ptr = NULL;
			struct chunk_header_t *chunk_header_ptr = NULL;
	
			chunk_header_ptr = new struct chunk_header_t;
			memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));
			expect_len = sizeof(struct chunk_header_t) ;
	
			while (1) {
				recv_byte = recv(sock, (char *)chunk_header_ptr + offset, expect_len, 0);
				if (recv_byte < 0) {
		#ifdef _WIN32 
					if (WSAGetLastError() == WSAEWOULDBLOCK) {
		#else
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
		#endif
						continue;
					} else {
						DBG_PRINTF("here\n");
						//continue;
						cout << "error in peer_communication::handle_pkt_in (recv -1) error number : "<<WSAGetLastError()<< endl;
						PAUSE
						exit(1);
						return RET_SOCK_ERROR;
						//PAUSE
						//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
					}
			
				}
				else if(recv_byte == 0){
					printf("sock closed\n");
					cout << "error in peer_communication::handle_pkt_in (recv 0) error number : "<<WSAGetLastError()<< endl;
					fprintf(peer_com_log,"error in peer_communication::handle_pkt_in (recv 0) error number : %d\n",WSAGetLastError());
					fflush(peer_com_log);
					fd_close(sock);
					//PAUSE
					//exit(1);
						//PAUSE
					return RET_SOCK_ERROR;
				}
				expect_len -= recv_byte;
				offset += recv_byte;
		
				if (!expect_len)
					break;
			}

			expect_len = chunk_header_ptr->length;
	
			buf_len = sizeof(struct chunk_header_t) + expect_len;
			cout << "buf_len = " << buf_len << endl;

			chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

			if (!chunk_ptr) {
				cout << "error in peer_communication::handle_pkt_in (new chunk failed) error number : "<<WSAGetLastError()<< endl;
				PAUSE
				exit(1);
				_log_ptr->exit(0, "memory not enough");
				return RET_SOCK_ERROR;
			}

			memset(chunk_ptr, 0x0, buf_len);
		
			memcpy(chunk_ptr, chunk_header_ptr, sizeof(struct chunk_header_t));

			if(chunk_header_ptr)
				delete chunk_header_ptr;
	
			while (1) {
				recv_byte = recv(sock, (char *)chunk_ptr + offset, expect_len, 0);
				if (recv_byte < 0) {
		#ifdef _WIN32 
					if (WSAGetLastError() == WSAEWOULDBLOCK) {
		#else
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
		#endif
						continue;
					} else {
						cout << "error in peer_communication::handle_pkt_in (recv -1 payload) error number : "<<WSAGetLastError()<< endl;
						PAUSE
						exit(1);
						cout << "haha5" << endl;
						//PAUSE
						return RET_SOCK_ERROR;
						//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
					}
				}
				else if(recv_byte == 0){
					printf("sock closed\n");
					cout << "error in peer_communication::handle_pkt_in (recv 0 payload) error number : "<<WSAGetLastError()<< endl;
					fprintf(peer_com_log,"error in peer_communication::handle_pkt_in (recv 0) error number : %d\n",WSAGetLastError());
					fflush(peer_com_log);
					fd_close(sock);
					//PAUSE
					//exit(1);
						//PAUSE
					return RET_SOCK_ERROR;
				}
				expect_len -= recv_byte;
				offset += recv_byte;
				if (expect_len == 0)
					break;
			}

			if (chunk_ptr->header.cmd == CHNK_CMD_PEER_CON) {
				cout << "CHNK_CMD_PEER_CON" << endl;
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_CON ");
				fprintf(peer_com_log,"CHNK_CMD_PEER_CON\n");
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
				cout << "error : unknow or cannot handle cmd : in peer_communication::handle_pkt_in"<<chunk_ptr->header.cmd<< endl;
				PAUSE
				exit(1);
			}

			if(chunk_ptr)
				delete chunk_ptr;
		}
		else{
			printf("error : unknow flag in peer_communication::handle_pkt_in\n");
			PAUSE
			exit(1);
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
	map_fd_info_iter = map_fd_info.find(sock);
	if(map_fd_info_iter == map_fd_info.end()){
		printf("cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
		PAUSE
		exit(1);
	}
	else{
		if(map_fd_info_iter->second->flag == 0){	//this fd is rescue peer
			//send peer con
			int ret,send_flag;
			int i;
			/*map<int, unsigned long>::iterator map_fd_session_id_iter;
			map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
			map<int, unsigned long>::iterator map_fd_manifest_iter;*/

			send_flag=0;
			/*map_fd_session_id_iter = map_fd_session_id.find(sock);
			if(map_fd_session_id_iter == map_fd_session_id.end()){
				printf("cannot find fd map_fd_session_id structure in peer_communication::handle_pkt_out\n");
				exit(1);
			}*/

			session_id_candidates_set_iter = session_id_candidates_set.find(map_fd_info_iter->second->session_id);
			if(session_id_candidates_set_iter == session_id_candidates_set.end()){
				printf("cannot find session_id_candidates_set structure in peer_communication::handle_pkt_out\n");
				PAUSE
				exit(1);
			}

			/*map_peer_com_fd_pid_iter = map_peer_com_fd_pid.find(sock);
			if(map_peer_com_fd_pid_iter == map_peer_com_fd_pid.end()){
				printf("cannot find map_peer_com_fd_pid structure in peer_communication::handle_pkt_out\n");
				exit(1);
			}

			map_fd_manifest_iter = map_fd_manifest.find(sock);
			if(map_fd_manifest_iter == map_fd_manifest.end()){
				printf("cannot find map_fd_manifest structure in peer_communication::handle_pkt_out\n");
				exit(1);
			}*/

			/*
			this part use to check, if the cnnection is already exist.
			*/
			multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
			map<unsigned long, int>::iterator map_pid_fd_iter;
			map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
			int check_flag = 0;

			//之前已經建立過連線的 在map_in_pid_fd裡面 則不再建立(保證對同個parent不再建立第二條線)
			for(map_pid_fd_iter = _peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
				if(map_pid_fd_iter->first == map_fd_info_iter->second->pid ){
					fprintf(peer_com_log,"fd already in map_in_pid_fd in peer_communication::handle_pkt_out (rescue peer)\n");
					check_flag = 1;
				}
			}

			/*
			this may have problem****************************************************************
			*/
			pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(map_fd_info_iter->second->pid);
			if(pid_peer_info_iter !=  _pk_mgr_ptr ->map_pid_peer_info.end() ){
				//兩個以上就沿用第一個的連線
				if(_pk_mgr_ptr ->map_pid_peer_info.count(map_fd_info_iter->second->pid) >= 2 ){
					printf("pid =%d already in connect find in map_pid_peer_info  testing in peer_communication::handle_pkt_out\n",map_fd_info_iter->second->pid);
					_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",map_fd_info_iter->second->pid,"already in connect find in map_pid_peer_info testing in peer_communication::handle_pkt_out");
					fprintf(peer_com_log,"fd already in map_pid_peer_info in peer_communication::handle_pkt_out (rescue peer)\n");	
					check_flag = 1;
				}
			}

			//若在map_pid_peerDown_info 則不再次建立連線
			pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(map_fd_info_iter->second->pid);
			if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
				printf("pid =%d already in connect find in map_pid_peerDown_info in peer_communication::handle_pkt_out\n",map_fd_info_iter->second->pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",map_fd_info_iter->second->pid,"already in connect find in map_pid_peerDown_info in peer_communication::handle_pkt_out");
				fprintf(peer_com_log,"fd already in map_pid_peerdown_info in peer_communication::handle_pkt_out (rescue peer)\n");
				check_flag = 1;
			}

			if(check_flag == 1){
				fprintf(peer_com_log,"we won't send CHNK_CMD_PEER_CON, since the connection is already exist\n");
				_net_ptr->set_nonblocking(sock);
				_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
				return RET_OK;
			}

			_net_ptr->set_blocking(sock);

			for(i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
				if(session_id_candidates_set_iter->second->list_info->level_info[i]->pid == map_fd_info_iter->second->pid){
					ret = _peer_ptr->handle_connect_request(sock, session_id_candidates_set_iter->second->list_info->level_info[i], session_id_candidates_set_iter->second->list_info->pid);
					send_flag =1;
				}
			}

			if(send_flag == 0){
				printf("cannot find level info structure in peer_communication::handle_pkt_out\n");
				PAUSE
				exit(1);
			}

			fprintf(peer_com_log,"send CHNK_CMD_PEER_CON\n");
			if(ret < 0) {
				cout << "handle_connect_request error!!!" << endl;
				return 0;
			} else {
				cout << "sock = " << sock << endl;

				_net_ptr->set_nonblocking(sock);
				_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
				return RET_OK;
			}
		}
		else if(map_fd_info_iter->second->flag == 1){	//this fd is candidate
			//do nothing rebind to event in only
			_net_ptr->set_nonblocking(sock);
			fprintf(peer_com_log,"this fd is candidate do nothing rebind to event in only\n");
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
		}
		else{
			printf("error : unknow flag in peer_communication::handle_pkt_out\n");
			PAUSE
			exit(1);
		}
	}

	return RET_OK;
}

void peer_communication::handle_pkt_error(int sock)
{
	cout << "error in peer_communication error number : "<<WSAGetLastError()<< endl;
	PAUSE
	exit(1);
}

void peer_communication::handle_job_realtime()
{

}


void peer_communication::handle_job_timer()
{

}

void peer_communication::handle_sock_error(int sock, basic_class *bcptr){
}