
#include "io_nonblocking.h"
#include "peer_communication.h"
#include "network.h"
#include "logger.h"
#include "logger_client.h"
#include "io_accept.h"
#include "io_connect.h"


using namespace std;

io_nonblocking::io_nonblocking(network *net_ptr,logger *log_ptr ,peer_communication* peer_communication_ptr, logger_client * logger_client_ptr){

	_peer_communication_ptr =peer_communication_ptr ;
	_logger_client_ptr = logger_client_ptr ;
	_net_ptr = net_ptr ;
	_log_ptr = log_ptr;

}

io_nonblocking::~io_nonblocking(){
}

int io_nonblocking::handle_pkt_in(int sock)
{	

//	printf("io_nonblocking::handle_pkt_in\n");
	Nonblocking_Ctl * Nonblocking_Recv_Ctl_ptr =NULL;
	int offset = 0;
	int recv_byte=0;

	struct chunk_header_t* chunk_header_ptr = NULL;
	struct chunk_t* chunk_ptr = NULL;
	unsigned long buf_len=0;

	map<int ,  struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
	map_fd_NonBlockIO_iter = _peer_communication_ptr->map_fd_NonBlockIO.find(sock);
	if(map_fd_NonBlockIO_iter == _peer_communication_ptr->map_fd_NonBlockIO.end()){
		printf("(_peer_communication_ptr ->map_fd_NonBlockIO_iter not find ");
		PAUSE
	}

	Nonblocking_Recv_Ctl_ptr = &(map_fd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingRecv);

	//init to READ_HEADER_READY
	if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == 0)
		Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY;


	
	for(int i =0;i<5;i++){


		if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY){


			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =0 ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = sizeof(chunk_header_t) ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = sizeof(chunk_header_t) ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_header_ptr ;
//			printf("io_nonblocking READ_HEADER_READY \n");
//			printf("Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len  =%d",Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len);

		}else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_RUNNING){

			//do nothing

		}else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_OK){

			buf_len = sizeof(chunk_header_t)+ ((chunk_t *)(Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)) ->header.length ;
			chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

//			printf(" READ HEADER OK buf_len %d \n",buf_len);

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

		}else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY){

			//do nothing

		}else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING){

			//do nothing

		}else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){

			//			chunk_ptr =(chunk_t *)Recv_nonblocking_ctl_ptr ->recv_ctl_info.buffer;

			//			Recv_nonblocking_ctl_ptr->recv_packet_state = READ_HEADER_READY ;

			break;

		}


		recv_byte =_net_ptr->nonblock_recv(sock,Nonblocking_Recv_Ctl_ptr);


		if(recv_byte < 0) {
			printf("error occ in nonblocking");
			data_close(sock);

			//PAUSE
			return RET_SOCK_ERROR;
		}


	}

//	printf("stats = %d \n",Nonblocking_Recv_Ctl_ptr->recv_packet_state);


	if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){

		chunk_ptr =(chunk_t *)Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer;

		Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY ;

		buf_len =  sizeof(struct chunk_header_t) +  chunk_ptr->header.length ;

	}else{

		//other stats
		return RET_OK;

	}





	if (chunk_ptr->header.cmd == CHNK_CMD_ROLE) {
		cout << "CHNK_CMD_ROLE" << endl;
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_ROLE ");
		_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"CHNK_CMD_ROLE in in io nonblocking : ",sock);
		/*
		this part shows the roleof this fd (rescue peer or candidate).
		save it to the table,and bind to peer_com~
		*/


		struct role_struct *role_protocol_ptr = NULL;
		int exist_flag = 0;
		map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
		role_protocol_ptr = (struct role_struct *)chunk_ptr;
		map<int, struct fd_information *>::iterator map_fd_info_iter;

		map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
		if(map_fd_info_iter != _peer_communication_ptr->map_fd_info.end()){
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","fd already in map_fd_info in in io nonblocking\n");
			_logger_client_ptr->log_exit();
		}
		else{

			for(session_id_candidates_set_iter = _peer_communication_ptr->session_id_candidates_set.begin();session_id_candidates_set_iter != _peer_communication_ptr->session_id_candidates_set.end();session_id_candidates_set_iter++){
				if((session_id_candidates_set_iter->second->manifest == role_protocol_ptr->manifest)&&(session_id_candidates_set_iter->second->role == role_protocol_ptr->flag)){
					for(int i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
						if(session_id_candidates_set_iter->second->list_info->level_info[i]->pid == role_protocol_ptr->send_pid){

							//_peer_communication_ptr->map_fd_session_id[sock] = session_id_candidates_set_iter->first;
							//_peer_communication_ptr->map_peer_com_fd_pid[sock] = role_protocol_ptr->send_pid;
							//_peer_communication_ptr->map_fd_manifest[sock] = role_protocol_ptr->manifest;

							//if(role_protocol_ptr->flag == 0){	//rescue peer
							//_peer_communication_ptr->map_fd_flag[sock] = 0;
							//}
							//else{	//candidate
							//_peer_communication_ptr->map_fd_flag[sock] = 1;
							//}

							_peer_communication_ptr->map_fd_info[sock] = new struct fd_information;

							map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
							if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){

								_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : cannot new in io nonblocking\n");
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
				printf("fd : %d cannot find list information in io nonblocking\n",sock);
				_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"cannot find list information in in io nonblocking : ",sock);

				_peer_communication_ptr->map_fd_info[sock] = new struct fd_information;

				map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
				if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){

					_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : cannot new in io nonblocking\n");
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

				_peer_communication_ptr->_io_accept_ptr->map_fd_unknown.push_back(sock);
				

				//testing bug
				_net_ptr->set_nonblocking(sock);
				_net_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
			}
			else{
				
//				bind to peer_com~ object
				
				_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"find list information in in io nonblocking : ",sock);

				_net_ptr->set_nonblocking(sock);

				_net_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
				//					_peer_mgr_ptr->fd_list_ptr->push_back(sock);
			}

		}

		//


	} else{

		if(chunk_ptr->header.cmd == CHNK_CMD_PEER_CON){
		
			struct chunk_request_msg_t *chunk_request_ptr = (struct chunk_request_msg_t *)chunk_ptr ;
			printf( "chunk_request_ptr ->info.pid  =%d  \n",chunk_request_ptr ->info.pid   );
		}



		printf("unknow or cannot handle cmd  =%d\n",chunk_ptr->header.cmd);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : unknow or cannot handle cmd in io nonblocking:",chunk_ptr->header.cmd);
		_logger_client_ptr->log_exit();
		PAUSE
	}



	if(chunk_ptr)
		delete chunk_ptr;


	return RET_OK;


}

int io_nonblocking::handle_pkt_out(int sock)
{
	


	return RET_OK;
}

void io_nonblocking::handle_pkt_error(int sock)
{

}

void io_nonblocking::handle_job_realtime()
{

}


void io_nonblocking::handle_job_timer()
{

}

void io_nonblocking::handle_sock_error(int sock, basic_class *bcptr){
	

}

void io_nonblocking::data_close(int sock){
	

	//map<int ,  struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
	//map_fd_NonBlockIO_iter =_peer_communication_ptr->map_fd_NonBlockIO.find(sock);
	//if(map_fd_NonBlockIO_iter == _peer_communication_ptr->map_fd_NonBlockIO.end()){
	//	printf("(_peer_communication_ptr ->map_fd_NonBlockIO_iter not find ");
	//	PAUSE
	//}
	//delete map_fd_NonBlockIO_iter->second;
	//_peer_communication_ptr->map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);

	//_net_ptr->close(sock);

	//list<int>::iterator fd_iter;
	//for(fd_iter = _peer_communication_ptr->fd_list_ptr->begin(); fd_iter != _peer_communication_ptr ->fd_list_ptr->end(); fd_iter++) {
	//	if(*fd_iter == sock) {
	//		_peer_communication_ptr ->fd_list_ptr->erase(fd_iter);
	//		break;
	//	}
	//}

	_peer_communication_ptr->fd_close(sock);

}

