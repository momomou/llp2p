
#include "io_accept.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "io_nonblocking.h"

using namespace std;

io_accept::io_accept(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr, logger_client * logger_client_ptr){
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_communication_ptr = peer_communication_ptr;
	_logger_client_ptr = logger_client_ptr;
}

io_accept::~io_accept(){
	printf("==============deldet io_accept success==========\n");

}


//should change to non-blocking
int io_accept::handle_pkt_in(int sock)
{	
	/*
	accept new peer fd, recv protocol to identify candidate or not.
	save its role(candidate or rescue peer) and bind to peer_com~ for handle_pkt_in/out.
	*/

	socklen_t sin_len = sizeof(struct sockaddr_in);
	struct chunk_header_t *chunk_header_ptr = NULL;
	int expect_len;
	int recv_byte,offset,buf_len;
	struct chunk_t *chunk_ptr = NULL;

	offset = 0;
	int new_fd = _net_ptr->accept(sock, (struct sockaddr *)&_cin, &sin_len);

	if(new_fd < 0) {
		return RET_SOCK_ERROR;
	}else {

		_peer_communication_ptr->map_fd_NonBlockIO_iter =_peer_communication_ptr->map_fd_NonBlockIO.find(new_fd);
		if(_peer_communication_ptr->map_fd_NonBlockIO_iter ==_peer_communication_ptr->map_fd_NonBlockIO.end() ){
		struct ioNonBlocking* ioNonBlocking_ptr =new struct ioNonBlocking;
		memset(ioNonBlocking_ptr,0x00,sizeof(struct ioNonBlocking));
		ioNonBlocking_ptr->io_nonblockBuff.nonBlockingRecv.recv_packet_state =READ_HEADER_READY;
//		printf("ioNonBlocking_ptr->io_nonblockBuff.nonBlockingRecv.recv_packet_state = %d\n ",ioNonBlocking_ptr->io_nonblockBuff.nonBlockingRecv.recv_packet_state);
		_peer_communication_ptr->map_fd_NonBlockIO[new_fd] =ioNonBlocking_ptr;

		}else{
			printf("fd=%d dup in _peer_communication_ptr->map_fd_NonBlockIO_iter  error\n",new_fd);

		}


		//_net_ptr->set_blocking(new_fd);
		cout << "new_fd = " << new_fd << endl;   
		//PAUSE

/*
		chunk_header_ptr = new struct chunk_header_t;
		memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));
		expect_len = sizeof(struct chunk_header_t) ;
	







		while (1) {
			recv_byte = recv(new_fd, (char *)chunk_header_ptr + offset, expect_len, 0);
			//printf("hello :%d\n",recv_byte);
			if (recv_byte < 0) {
	#ifdef _WIN32 
				if (WSAGetLastError() == WSAEWOULDBLOCK) {
	#else
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
	#endif
//					printf("hello :%d\n",recv_byte);
					continue;
				} else {
					DBG_PRINTF("here\n");
					//continue;
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept::handle_pkt_in (recv -1) error number : ",WSAGetLastError());
					_logger_client_ptr->log_exit();
					return RET_SOCK_ERROR;
					//PAUSE
					//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
				}
			
			}
			else if(recv_byte == 0){
				printf("sock closed\n");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept::handle_pkt_in (recv 0) error number : ",WSAGetLastError());
				_logger_client_ptr->log_exit();
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
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept::handle_pkt_in (new chunk failed) error number : ",WSAGetLastError());
			_logger_client_ptr->log_exit();
			return RET_SOCK_ERROR;
		}

		memset(chunk_ptr, 0x0, buf_len);
		memcpy(chunk_ptr, chunk_header_ptr, sizeof(struct chunk_header_t));

		if(chunk_header_ptr)
			delete chunk_header_ptr;
	
		while (1) {
			recv_byte = recv(new_fd, (char *)chunk_ptr + offset, expect_len, 0);
			if (recv_byte < 0) {
	#ifdef _WIN32 
				if (WSAGetLastError() == WSAEWOULDBLOCK) {
	#else
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
	#endif
					continue;
				} else {
					
					cout << "haha5" << endl;
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept::handle_pkt_in (recv -1 payload) error number : ",WSAGetLastError());
					_logger_client_ptr->log_exit();
					//PAUSE
					return RET_SOCK_ERROR;
					//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
				}
			}
			else if(recv_byte == 0){
				printf("sock closed\n");
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept::handle_pkt_in (recv 0 payload) error number : ",WSAGetLastError());
				_logger_client_ptr->log_exit();
				return RET_SOCK_ERROR;
			}
			expect_len -= recv_byte;
			offset += recv_byte;
			if (expect_len == 0)
				break;
		}

*/

		_net_ptr->set_nonblocking(new_fd);

		_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN );
		_net_ptr->set_fd_bcptr_map(new_fd, dynamic_cast<basic_class *> (_peer_communication_ptr->_io_nonblocking_ptr));
		_peer_mgr_ptr->fd_list_ptr->push_back(new_fd);


/*
		if (chunk_ptr->header.cmd == CHNK_CMD_ROLE) {
			cout << "CHNK_CMD_ROLE" << endl;
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_ROLE ");
			_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"CHNK_CMD_ROLE in io_accept::handle_pkt_in fd : ",new_fd);
			
//			this part shows the roleof this fd (rescue peer or candidate).
//			save it to the table,and bind to peer_com~
			


			struct role_struct *role_protocol_ptr = NULL;
			int exist_flag = 0;
			map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
			role_protocol_ptr = (struct role_struct *)chunk_ptr;
			map<int, struct fd_information *>::iterator map_fd_info_iter;

			map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(new_fd);
			if(map_fd_info_iter != _peer_communication_ptr->map_fd_info.end()){
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","fd already in map_fd_info in io_accept::handle_pkt_in\n");
				_logger_client_ptr->log_exit();
			}
			else{

				for(session_id_candidates_set_iter = _peer_communication_ptr->session_id_candidates_set.begin();session_id_candidates_set_iter != _peer_communication_ptr->session_id_candidates_set.end();session_id_candidates_set_iter++){
					if((session_id_candidates_set_iter->second->manifest == role_protocol_ptr->manifest)&&(session_id_candidates_set_iter->second->role == role_protocol_ptr->flag)){
						for(int i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
							if(session_id_candidates_set_iter->second->list_info->level_info[i]->pid == role_protocol_ptr->send_pid){

								//_peer_communication_ptr->map_fd_session_id[new_fd] = session_id_candidates_set_iter->first;
								//_peer_communication_ptr->map_peer_com_fd_pid[new_fd] = role_protocol_ptr->send_pid;
								//_peer_communication_ptr->map_fd_manifest[new_fd] = role_protocol_ptr->manifest;

								//if(role_protocol_ptr->flag == 0){	//rescue peer
								//	_peer_communication_ptr->map_fd_flag[new_fd] = 0;
								//}
								//else{	//candidate
								//	_peer_communication_ptr->map_fd_flag[new_fd] = 1;
								//}

								_peer_communication_ptr->map_fd_info[new_fd] = new struct fd_information;

								map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(new_fd);
								if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){

									_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : cannot new io_accept::handle_pkt_in\n");
									_logger_client_ptr->log_exit();
								}

								memset(map_fd_info_iter->second,0x00,sizeof(struct fd_information));
								
								if(role_protocol_ptr->flag == 0){	//rescue peer
									map_fd_info_iter->second->flag = 0;
								}
								else{	//candidate
									map_fd_info_iter->second->flag = 1;
								}

								map_fd_info_iter->second->manifest = role_protocol_ptr->manifest;
								map_fd_info_iter->second->pid = role_protocol_ptr->send_pid;
								map_fd_info_iter->second->session_id = session_id_candidates_set_iter->first;

								exist_flag = 1;
							}
						}
					}
				}
				
				if(exist_flag == 0){
					printf("fd : %d cannot find list information in io_accept::handle_pkt_in\n",new_fd);
					_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"cannot find list information in io_accept::handle_pkt_in fd : ",new_fd);

					_peer_communication_ptr->map_fd_info[new_fd] = new struct fd_information;

					map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(new_fd);
					if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){
						
						_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : cannot new io_accept::handle_pkt_in\n");
						_logger_client_ptr->log_exit();
					}

					memset(map_fd_info_iter->second,0x00,sizeof(struct fd_information));

					map_fd_info_iter->second->pid = role_protocol_ptr->send_pid;
					map_fd_info_iter->second->manifest = role_protocol_ptr->manifest;

					if(role_protocol_ptr->flag == 0){	//rescue peer
						map_fd_info_iter->second->flag = 0;
					}
					else{	//candidate
						map_fd_info_iter->second->flag = 1;
					}

					map_fd_unknown.push_back(new_fd);
				}
				else{

//					bind to peer_com~ object

					_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"find list information in io_accept::handle_pkt_in fd : ",new_fd);

					_net_ptr->set_nonblocking(new_fd);

					_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
//					_net_ptr->set_fd_bcptr_map(new_fd, dynamic_cast<basic_class *> (_peer_communication_ptr));
//					_peer_mgr_ptr->fd_list_ptr->push_back(new_fd);
				}

			}
		} else{

			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : unknow or cannot handle cmd :",chunk_ptr->header.cmd);
			_logger_client_ptr->log_exit();
		}
*/

	}

	return RET_OK;
}

int io_accept::handle_pkt_out(int sock)
{
	/*
	we will not inside this part
	*/
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : place in io_accept::handle_pkt_out\n");
	_logger_client_ptr->log_exit();
	return RET_OK;
}

void io_accept::handle_pkt_error(int sock)
{
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept handle_pkt_error error number : ",WSAGetLastError());
	_logger_client_ptr->log_exit();
}

void io_accept::handle_job_realtime()
{

}


void io_accept::handle_job_timer()
{

}

void io_accept::handle_sock_error(int sock, basic_class *bcptr){
	
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept handle_sock_error error number : ",WSAGetLastError());
	_logger_client_ptr->log_exit();
}