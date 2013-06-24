
#include "io_nat_punch.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "nat_interface.h"

using namespace std;

nat_interface::nat_interface(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr){
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_communication_ptr = peer_communication_ptr;

	_io_nat_punch_ptr = new io_nat_punch(_net_ptr,_log_ptr,_prep,_peer_mgr_ptr,_peer_ptr,_pk_mgr_ptr,_peer_communication_ptr,this);
}

nat_interface::~nat_interface(){
}

void nat_interface::nat_register(unsigned long self_pid){
	_io_nat_punch_ptr->register_to_nat_server(self_pid);
}

void nat_interface::nat_stop(unsigned long stop_session_id){
}

void nat_interface::nat_start(int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id){
	_io_nat_punch_ptr->start_nat_punch(fd_role,manifest,fd_pid,session_id);
}

int nat_interface::handle_pkt_in(int sock)
{	
	/*
	this part means the fd recv the ROLE protocol
	if the fd is built by connect it won't recv any protocol. we will rebind it to handle pkt out.
	finnally rebind this fd to peer_com~
	*/
	map<unsigned long,struct nat_con_thread_struct *>::iterator map_counter_thread_iter;
	int find_flag = 0;
	for(map_counter_thread_iter = _io_nat_punch_ptr->map_counter_thread.begin();map_counter_thread_iter != _io_nat_punch_ptr->map_counter_thread.end();map_counter_thread_iter++){
		if(map_counter_thread_iter->second->Xconn.sPeer == sock){
			find_flag = 1;
			break;
		}
	}

	if(find_flag == 0){
		printf("cannot find fd in nat_interface::handle_pkt_out\n");
		exit(1);
	}

	if(map_counter_thread_iter->second->role == -1){	//this fd is xlisten
		socklen_t sin_len = sizeof(struct sockaddr_in);
		struct chunk_header_t *chunk_header_ptr = NULL;
		int expect_len;
		int recv_byte,offset,buf_len;
		struct chunk_t *chunk_ptr = NULL;
		map<int, int>::iterator map_fd_flag_iter;

		offset = 0;
		
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
					cout << "error in nat_interface::handle_pkt_in (recv -1) error number : "<<WSAGetLastError()<< endl;
					exit(1);
					return RET_SOCK_ERROR;
					//PAUSE
					//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
				}
			
			}
			else if(recv_byte == 0){
				printf("sock closed\n");
				cout << "error in nat_interface::handle_pkt_in (recv 0) error number : "<<WSAGetLastError()<< endl;
				exit(1);
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
			cout << "error in nat_interface::handle_pkt_in (new chunk failed) error number : "<<WSAGetLastError()<< endl;
			exit(1);
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
					
					cout << "haha5" << endl;
					cout << "error in nat_interface::handle_pkt_in (recv -1 payload) error number : "<<WSAGetLastError()<< endl;
					exit(1);
					//PAUSE
					return RET_SOCK_ERROR;
					//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
				}
			}
			else if(recv_byte == 0){
				printf("sock closed\n");
				cout << "error in nat_interface::handle_pkt_in (recv 0 payload) error number : "<<WSAGetLastError()<< endl;
				exit(1);
					//PAUSE
				return RET_SOCK_ERROR;
			}
			expect_len -= recv_byte;
			offset += recv_byte;
			if (expect_len == 0)
				break;
		}

		if (chunk_ptr->header.cmd == CHNK_CMD_ROLE) {
			cout << "CHNK_CMD_ROLE" << endl;
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_ROLE ");
			
			/*
			this part shows the role of this fd (rescue peer or candidate).
			save it to the table,and bind to peer_com~
			*/
			struct role_struct *role_protocol_ptr = NULL;
			int exist_flag = 0;
			map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
			role_protocol_ptr = (struct role_struct *)chunk_ptr;
			map<int, struct fd_information *>::iterator map_fd_info_iter;

			map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
			if(map_fd_info_iter != _peer_communication_ptr->map_fd_info.end()){
				printf("fd already in map_fd_info in nat_interface::handle_pkt_in\n");
				exit(1);
			}
			else{

				for(session_id_candidates_set_iter = _peer_communication_ptr->session_id_candidates_set.begin();session_id_candidates_set_iter != _peer_communication_ptr->session_id_candidates_set.end();session_id_candidates_set_iter++){
					if((session_id_candidates_set_iter->second->manifest == role_protocol_ptr->manifest)&&(session_id_candidates_set_iter->second->role == role_protocol_ptr->flag)){
						for(int i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
							if(session_id_candidates_set_iter->second->list_info->level_info[i]->pid == role_protocol_ptr->send_pid){

								/*_peer_communication_ptr->map_fd_session_id[sock] = session_id_candidates_set_iter->first;
								_peer_communication_ptr->map_peer_com_fd_pid[sock] = role_protocol_ptr->send_pid;
								_peer_communication_ptr->map_fd_manifest[sock] = role_protocol_ptr->manifest;*/
								_peer_communication_ptr->map_fd_info[sock] = new struct fd_information;

								map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
								if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){
									printf("error : cannot new nat_interface::handle_pkt_in\n");
									exit(1);
								}

								memset(map_fd_info_iter->second,0x00,sizeof(struct fd_information));

								map_fd_info_iter->second->manifest = role_protocol_ptr->manifest;
								map_fd_info_iter->second->pid = role_protocol_ptr->send_pid;
								map_fd_info_iter->second->session_id = session_id_candidates_set_iter->first;

								map_counter_thread_iter->second->manifest = role_protocol_ptr->manifest;
								map_counter_thread_iter->second->pid = role_protocol_ptr->send_pid;

								if(role_protocol_ptr->flag == 0){	//rescue peer
									//_peer_communication_ptr->map_fd_flag[sock] = 0;
									map_counter_thread_iter->second->role = 0;
									map_fd_info_iter->second->flag = 0;
								}
								else{	//candidate
									//_peer_communication_ptr->map_fd_flag[sock] = 1;
									map_counter_thread_iter->second->role = 1;
									map_fd_info_iter->second->flag = 1;
								}

								_io_nat_punch_ptr->map_counter_session_id_iter = _io_nat_punch_ptr->map_counter_session_id.find(map_counter_thread_iter->first);
								if(_io_nat_punch_ptr->map_counter_session_id_iter != _io_nat_punch_ptr->map_counter_session_id.end()){
									printf("error : already in map_counter_session_id in nat_interface::handle_pkt_in\n");
									exit(1);
								}
								_io_nat_punch_ptr->map_counter_session_id[map_counter_thread_iter->first] = session_id_candidates_set_iter->first;

								exist_flag = 1;
							}
						}
					}
				}
				
				if(exist_flag == 0){
					printf("fd : %d cannot find list information in nat_interface::handle_pkt_in\n",sock);
//					fprintf(_peer_communication_ptr->peer_com_log,"fd : %d cannot find list information in nat_interface::handle_pkt_in\n",sock);

					/*_peer_communication_ptr->map_peer_com_fd_pid[sock] = role_protocol_ptr->send_pid;
					_peer_communication_ptr->map_fd_manifest[sock] = role_protocol_ptr->manifest;*/
					_peer_communication_ptr->map_fd_info[sock] = new struct fd_information;

					map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
					if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){
						printf("error : cannot new nat_interface::handle_pkt_in\n");
						exit(1);
					}

					memset(map_fd_info_iter->second,0x00,sizeof(struct fd_information));

					map_fd_info_iter->second->manifest = role_protocol_ptr->manifest;
					map_fd_info_iter->second->pid = role_protocol_ptr->send_pid;

					map_counter_thread_iter->second->manifest = role_protocol_ptr->manifest;
					map_counter_thread_iter->second->pid = role_protocol_ptr->send_pid;

					if(role_protocol_ptr->flag == 0){	//rescue peer
						//_peer_communication_ptr->map_fd_flag[sock] = 0;
						map_fd_info_iter->second->flag = 0;
						map_counter_thread_iter->second->role = 0;
					}
					else{	//candidate
						//_peer_communication_ptr->map_fd_flag[sock] = 1;
						map_fd_info_iter->second->flag = 1;
						map_counter_thread_iter->second->role = 1;
					}

					map_nat_fd_unknown_iter = map_nat_fd_unknown.find(sock);
					if(map_nat_fd_unknown_iter != map_nat_fd_unknown.end()){
						printf("this fd : %d already in map_nat_fd_unknown in nat_interface::handle_pkt_in\n",sock);
						exit(1);
					}
					else{
						map_nat_fd_unknown[sock] = 1;
					}
				}
				else{
					/*
					bind to peer_com~ object
					*/
//					fprintf(_peer_communication_ptr->peer_com_log,"fd : %d find list information in nat_interface::handle_pkt_in\n",sock);

					_net_ptr->set_nonblocking(sock);

					_net_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
					_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
					_peer_mgr_ptr->fd_list_ptr->push_back(sock);
				}

//				fflush(_peer_communication_ptr->peer_com_log);
			}
		} else{
			cout << "error : unknow or cannot handle cmd :"<<chunk_ptr->header.cmd<< endl;
			exit(1);
		}

		if(chunk_ptr)
			delete chunk_ptr;
	
	}
	else{	//this fd is xconnect
		//do nothing rebind to event out only
		_net_ptr->set_nonblocking(sock);
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLOUT);
		//fprintf(peer_com_log,"this fd is rescue peer do nothing rebind to event out only\n");
	}

	return RET_OK;
}

int nat_interface::handle_pkt_out(int sock)
{
	/*
	if the fd built by xconnect then it will send ROLE protocol in this function.
	else it will rebind to handle pkt in
	finnally rebind this fd to peer_com~
	*/
	map<unsigned long,struct nat_con_thread_struct *>::iterator map_counter_thread_iter;
	int find_flag = 0;
	for(map_counter_thread_iter = _io_nat_punch_ptr->map_counter_thread.begin();map_counter_thread_iter != _io_nat_punch_ptr->map_counter_thread.end();map_counter_thread_iter++){
		if(map_counter_thread_iter->second->Xconn.sPeer == sock){
			find_flag =1;
			break;
		}
	}

	if(find_flag == 0){
		printf("cannot find fd in nat_interface::handle_pkt_out\n");
		exit(1);
	}

	if(map_counter_thread_iter->second->role == -1){	//this fd is xlisten
		//do nothing rebind to event in only
		_net_ptr->set_nonblocking(sock);
		//fprintf(peer_com_log,"this fd is candidate do nothing rebind to event in only\n");
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
	}
	else{	//this fd is xconnect
		struct role_struct *role_protocol_ptr = NULL;
		int send_byte;

		role_protocol_ptr = new struct role_struct;
		memset(role_protocol_ptr,0x00,sizeof(struct role_struct));

		role_protocol_ptr->header.cmd = CHNK_CMD_ROLE;
		role_protocol_ptr->header.length = sizeof(struct role_struct) - sizeof(struct chunk_header_t);
		role_protocol_ptr->header.rsv_1 = REQUEST;

		if(map_counter_thread_iter->second->role == 0){
			role_protocol_ptr->flag = 1;
		}
		else{
			role_protocol_ptr->flag = 0;
		}

		role_protocol_ptr->manifest = map_counter_thread_iter->second->manifest;
		role_protocol_ptr->recv_pid = map_counter_thread_iter->second->pid;
		role_protocol_ptr->send_pid = _io_nat_punch_ptr->self_id_integer;
		
		/*
		this part stores the info in each table.
		*/
		/*map<int, unsigned long>::iterator map_fd_session_id_iter;	//must be store before connect, and delete in stop
		map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;	//must be store before connect, and delete in stop
		map<int, unsigned long>::iterator map_fd_manifest_iter;	//must be store before connect, and delete in stop

		_peer_communication_ptr->map_fd_flag_iter = _peer_communication_ptr->map_fd_flag.find(sock);
		if(_peer_communication_ptr->map_fd_flag_iter != _peer_communication_ptr->map_fd_flag.end()){
			printf("error : fd already in map_fd_flag in nat_interface::handle_pkt_out\n");
			exit(1);
		}
		_peer_communication_ptr->map_fd_flag[sock] = map_counter_thread_iter->second->role;

		_io_nat_punch_ptr->map_counter_session_id_iter = _io_nat_punch_ptr->map_counter_session_id.find(map_counter_thread_iter->first);
		if(_io_nat_punch_ptr->map_counter_session_id_iter == _io_nat_punch_ptr->map_counter_session_id.end()){
			printf("error : cannot find map_counter_session_id in nat_interface::handle_pkt_out\n");
			exit(1);
		}

		map_fd_session_id_iter = _peer_communication_ptr->map_fd_session_id.find(sock);
		if(map_fd_session_id_iter != _peer_communication_ptr->map_fd_session_id.end()){
			printf("error : fd already in map_fd_session_id in in nat_interface::handle_pkt_out\n");
			exit(1);
		}
		_peer_communication_ptr->map_fd_session_id[sock] = _io_nat_punch_ptr->map_counter_session_id_iter->second;

		map_fd_manifest_iter = _peer_communication_ptr->map_fd_manifest.find(sock);
		if(map_fd_manifest_iter != _peer_communication_ptr->map_fd_manifest.end()){
			printf("error : fd already in map_fd_manifest in nat_interface::handle_pkt_out\n");
			exit(1);
		}
		_peer_communication_ptr->map_fd_manifest[sock] = map_counter_thread_iter->second->manifest;
	
		map_peer_com_fd_pid_iter = _peer_communication_ptr->map_peer_com_fd_pid.find(sock);
		if(map_peer_com_fd_pid_iter != _peer_communication_ptr->map_peer_com_fd_pid.end()){
			printf("error : fd already in map_peer_com_fd_pid in nat_interface::handle_pkt_out\n");
			exit(1);
		}
		_peer_communication_ptr->map_peer_com_fd_pid[sock] = map_counter_thread_iter->second->pid;*/
		map<int, struct fd_information *>::iterator map_fd_info_iter;

		_io_nat_punch_ptr->map_counter_session_id_iter = _io_nat_punch_ptr->map_counter_session_id.find(map_counter_thread_iter->first);
		if(_io_nat_punch_ptr->map_counter_session_id_iter == _io_nat_punch_ptr->map_counter_session_id.end()){
			printf("error : cannot find map_counter_session_id in nat_interface::handle_pkt_out\n");
			exit(1);
		}

		map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
		if(map_fd_info_iter != _peer_communication_ptr->map_fd_info.end()){
			printf("error : fd already in map_fd_info in nat_interface::handle_pkt_out\n");
			exit(1);
		}
		_peer_communication_ptr->map_fd_info[sock] = new struct fd_information;

		map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
		if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){
			printf("error : cannot new nat_interface::handle_pkt_out\n");
			exit(1);
		}

		memset(map_fd_info_iter->second,0x00,sizeof(struct fd_information));
		map_fd_info_iter->second->flag = map_counter_thread_iter->second->role;
		map_fd_info_iter->second->manifest = map_counter_thread_iter->second->manifest;
		map_fd_info_iter->second->pid = map_counter_thread_iter->second->pid;
		map_fd_info_iter->second->session_id = _io_nat_punch_ptr->map_counter_session_id_iter->second;


		int send_offset,expect_len;
		expect_len = sizeof(struct role_struct);
		send_offset = 0;

		while(1){
			send_byte = _net_ptr->send(sock, (char*)role_protocol_ptr + send_offset, expect_len, 0);
			if(send_byte<0){
				printf("error : send CHNK_CMD_ROLE number :%d\n",WSAGetLastError());
				exit(1);
			}
			else{
				expect_len = expect_len - send_byte;
				send_offset = send_offset + send_byte;
				if(expect_len == 0){
					send_offset = 0;
					delete role_protocol_ptr;
					break;
				}
			}
		}
		/*
		bind to peer_com~ object
		*/
		_net_ptr->set_nonblocking(sock);
		_net_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
		_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
		//_peer_mgr_ptr->fd_list_ptr->push_back(sock);	//may called in non-blocking_conect

	}

	return RET_OK;
}

void nat_interface::handle_pkt_error(int sock)
{
	cout << "error in nat_interface handle_pkt_error error number : "<<WSAGetLastError()<< endl;
	exit(1);
}

void nat_interface::handle_job_realtime()
{

}


void nat_interface::handle_job_timer()
{

}

void nat_interface::handle_sock_error(int sock, basic_class *bcptr){
	cout << "error in nat_interface handle_sock_error error number : "<<WSAGetLastError()<< endl;
	exit(1);
}