
#include "io_connect.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"

using namespace std;

io_connect::io_connect(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr){
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_communication_ptr = peer_communication_ptr;
}

io_connect::~io_connect(){
}

int io_connect::handle_pkt_in(int sock)
{	
	/*
	we will not inside this part
	*/
	printf("error : place in io_connect::handle_pkt_in\n");
	PAUSE
	exit(1);

	return RET_OK;
}

int io_connect::handle_pkt_out(int sock)
{
	/*
	in this part means the fd is built. it finds its role and sends protocol to another to tell its role.
	bind to peer_com~ for handle_pkt_in/out.
	*/
	int error_return;
	int error_num,error_len;

	error_len = sizeof(error_num);
	error_num = -1;
	error_return = getsockopt(sock,SOL_SOCKET, SO_ERROR, (char*)&error_num, &error_len);
	printf("error_return %d error_num %d\n",error_return,error_num);
	if(error_return<0){
		printf("error : call getsockopt error num : %d\n",WSAGetLastError());
		PAUSE
		exit(1);
	}
	else{
		if(error_num!=0){
			printf("error : call getsockopt get error num : %d\n",WSAGetLastError());
			PAUSE
			exit(1);
		}
		else{
		/*
		the fd is built.
		the peer starts to find the role and tell another. 
		*/
			struct role_struct *role_protocol_ptr = NULL;
			map<int, int>::iterator map_fd_flag_iter;
			map<int, unsigned long>::iterator map_fd_session_id_iter;	//must be store before connect
			map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;	//must be store before connect
			map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
			map<int, struct fd_information *>::iterator map_fd_info_iter;
			int send_byte;

			role_protocol_ptr = new struct role_struct;
			memset(role_protocol_ptr,0x00,sizeof(struct role_struct));

			role_protocol_ptr->header.cmd = CHNK_CMD_ROLE;
			role_protocol_ptr->header.length = sizeof(struct role_struct) - sizeof(struct chunk_header_t);
			role_protocol_ptr->header.rsv_1 = REQUEST;

			map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
			if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){
				printf("error : cannot find map_fd_info in io_connect::handle_pkt_out\n");
				PAUSE
				exit(1);
			}

			if(map_fd_info_iter->second->flag == 0){
				role_protocol_ptr->flag = 1;
			}
			else{
				role_protocol_ptr->flag = 0;
			}

			role_protocol_ptr->manifest = map_fd_info_iter->second->manifest;
			role_protocol_ptr->recv_pid = map_fd_info_iter->second->pid;
			role_protocol_ptr->send_pid = _peer_mgr_ptr->self_pid;
			/*map_fd_flag_iter = _peer_communication_ptr->map_fd_flag.find(sock);
			if(map_fd_flag_iter == _peer_communication_ptr->map_fd_flag.end()){
				printf("cannot find fd in map_fd_flag in io_connect::handle_pkt_out\n");
				exit(1);
			}
			else{
				if(map_fd_flag_iter->second == 0){
					role_protocol_ptr->flag = 1;
				}
				else{
					role_protocol_ptr->flag = 0;
				}
			}*/

			/*map_fd_session_id_iter = _peer_communication_ptr->map_fd_session_id.find(sock);
			if(map_fd_session_id_iter == _peer_communication_ptr->map_fd_session_id.end()){
				printf("cannot find fd in map_fd_session_id in io_connect::handle_pkt_out\n");
				exit(1);
			}
			else{
				session_id_candidates_set_iter = _peer_communication_ptr->session_id_candidates_set.find(map_fd_session_id_iter->second);
				if(session_id_candidates_set_iter == _peer_communication_ptr->session_id_candidates_set.end()){
					printf("error : cannot find session_id_candidates_set structure in io_connect::handle_pkt_out\n");
					exit(1);
				}
				role_protocol_ptr->manifest = session_id_candidates_set_iter->second->manifest;
			}*/

			/*map_peer_com_fd_pid_iter = _peer_communication_ptr->map_peer_com_fd_pid.find(sock);
			if(map_peer_com_fd_pid_iter == _peer_communication_ptr->map_peer_com_fd_pid.end()){
				printf("cannot find fd in map_peer_com_fd_pid in io_connect::handle_pkt_out\n");
				exit(1);
			}
			else{
				role_protocol_ptr->recv_pid = map_peer_com_fd_pid_iter->second;
				role_protocol_ptr->send_pid = _peer_mgr_ptr->self_pid;
			}*/

			//_net_ptr->set_blocking(sock);
			int send_offset,expect_len;
			expect_len = sizeof(struct role_struct);
			send_offset = 0;

			while(1){
				send_byte = _net_ptr->send(sock, (char*)role_protocol_ptr + send_offset, expect_len, 0);
				if(send_byte<0){
					printf("error : send CHNK_CMD_ROLE number :%d\n",WSAGetLastError());
					PAUSE
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
	}


	return RET_OK;
}

void io_connect::handle_pkt_error(int sock)
{
	cout << "error in io_connect handle_pkt_error error number : "<<WSAGetLastError()<< endl;
	PAUSE
	exit(1);
}

void io_connect::handle_job_realtime()
{

}


void io_connect::handle_job_timer()
{

}

void io_connect::handle_sock_error(int sock, basic_class *bcptr){
	cout << "error in io_connect handle_sock_error error number : "<<WSAGetLastError()<< endl;
	PAUSE
	exit(1);
}