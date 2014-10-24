#include "io_nonblocking_udp.h"
#include "peer_communication.h"
#include "network_udp.h"
#include "logger.h"
#include "logger_client.h"
#include "io_accept.h"
#include "io_connect.h"

using namespace std;

io_nonblocking_udp::io_nonblocking_udp(network_udp *net_udp_ptr,logger *log_ptr ,peer_communication* peer_communication_ptr, logger_client * logger_client_ptr)
{
	_peer_communication_ptr =peer_communication_ptr ;
	_logger_client_ptr = logger_client_ptr ;
	_net_udp_ptr = net_udp_ptr ;
	_log_ptr = log_ptr;
}

io_nonblocking_udp::~io_nonblocking_udp()
{
	debug_printf("==============deldet io_nonblocking_udp success==========\n");
}

int io_nonblocking_udp::handle_pkt_in(int sock)
{
	return RET_OK;
}

// If the socket is readable, the conection is built.
int io_nonblocking_udp::handle_pkt_in_udp(int sock)
{	
	if (UDT::getsockstate(sock) != UDTSTATUS::CONNECTED) {
		// 曾經發生 state = 6 (broken)
		debug_printf("sock %d  state %d \n", sock, UDT::getsockstate(sock));
		return RET_SOCK_ERROR;
	}

	Nonblocking_Ctl *Nonblocking_Recv_Ctl_ptr = NULL;
	int offset = 0;
	int recv_byte = 0;

	struct chunk_header_t *chunk_header_ptr = NULL;
	struct chunk_t* chunk_ptr = NULL;
	int buf_len=0;

	map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
	map_udpfd_NonBlockIO_iter = _peer_communication_ptr->map_udpfd_NonBlockIO.find(sock);
	if (map_udpfd_NonBlockIO_iter == _peer_communication_ptr->map_udpfd_NonBlockIO.end()) {
		debug_printf("(_peer_communication_ptr ->map_udpfd_NonBlockIO_iter not find \n");
		PAUSE
	}

	Nonblocking_Recv_Ctl_ptr = &(map_udpfd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingRecv);

	//init to READ_HEADER_READY
	if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == 0) {
		Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY;
	}
	
	for (int i = 0; i < 5; i++) {
		if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY) {
			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			if (!chunk_header_ptr) {
				printf("chunk_header_ptr new error \n");
				_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__," chunk_header_ptr new error");
				PAUSE
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
			if(!chunk_ptr){
				printf("chunk_ptr iononblocking new error buf_len=%d\n",buf_len);
				_log_ptr->write_log_format("s =>u s s u \n", __FUNCTION__,__LINE__," chunk_ptr  iononblocking new error","buf_len",buf_len);
				PAUSE
			}

			memset(chunk_ptr, 0x0, buf_len);
			memcpy(chunk_ptr,Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer,sizeof(chunk_header_t));

			if (Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer) {
				delete [] (unsigned char*)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer ;
			}
			
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =sizeof(chunk_header_t) ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;
			Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_PAYLOAD_READY ;
		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY) {
			//do nothing
		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING) {
			//do nothing
		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK) {
			//			chunk_ptr =(chunk_t *)Recv_nonblocking_ctl_ptr ->recv_ctl_info.buffer;
			//			Recv_nonblocking_ctl_ptr->recv_packet_state = READ_HEADER_READY ;
			break;
		}

		recv_byte =_net_udp_ptr->nonblock_recv(sock,Nonblocking_Recv_Ctl_ptr);

		if (recv_byte < 0) {
			printf("error occ in nonblocking");
			data_close(sock);

			//PAUSE
			return RET_SOCK_ERROR;
		}
	}

	if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK) {
		chunk_ptr = (chunk_t *)Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer;
		Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY ;
		buf_len =  sizeof(struct chunk_header_t) +  chunk_ptr->header.length ;
	}
	else {
		//other stats
		return RET_OK;
	}

	if (chunk_ptr->header.cmd == CHNK_CMD_ROLE) {
		cout << "CHNK_CMD_ROLE" << endl;
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_ROLE ");
		_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"CHNK_CMD_ROLE in in io nonblocking : ",sock);
		/*
		this part shows the role of this fd (rescue peer or candidate).
		save it to the table,and bind to peer_com~
		*/


		struct role_struct *role_protocol_ptr = NULL;
		int exist_flag = 0;
		map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
		role_protocol_ptr = (struct role_struct *)chunk_ptr;
		map<int, struct fd_information *>::iterator map_udpfd_info_iter;

		map_udpfd_info_iter = _peer_communication_ptr->map_udpfd_info.find(sock);
		if (map_udpfd_info_iter != _peer_communication_ptr->map_udpfd_info.end()) {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","fd already in map_udpfd_info in in io nonblocking\n");
			_logger_client_ptr->log_exit();
			PAUSE
		}
		else {
			unsigned long his_pid = role_protocol_ptr->send_pid;
			unsigned long his_manifest = role_protocol_ptr->manifest;
			unsigned long his_role = role_protocol_ptr->flag;
			
			_peer_communication_ptr->map_udpfd_info[sock] = new struct fd_information;
			if (!(_peer_communication_ptr->map_udpfd_info[sock])) {
				debug_printf("_peer_communication_ptr->map_udpfd_info[sock] iononblocking new error\n");
				_log_ptr->write_log_format("s(u) s s u \n", __FUNCTION__,__LINE__," _peer_communication_ptr->map_udpfd_info[sock]  iononblocking new error");
				PAUSE
			}
			
			_peer_communication_ptr->map_udpfd_info[sock]->pid = his_pid;
			_peer_communication_ptr->map_udpfd_info[sock]->manifest = his_manifest;
			_peer_communication_ptr->map_udpfd_info[sock]->role = his_role;
		
			_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "pid", his_pid, "manifest", his_manifest, "role", his_role);

			_net_udp_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
			
		}
	} 
	else {
		debug_printf("unknow or cannot handle cmd  =%d\n",chunk_ptr->header.cmd);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : unknow or cannot handle cmd in io nonblocking:",chunk_ptr->header.cmd);
		_logger_client_ptr->log_exit();
		PAUSE
	}



	if(chunk_ptr)
		delete chunk_ptr;


	return RET_OK;

}

int io_nonblocking_udp::handle_pkt_out(int sock)
{
	return RET_OK;
}

int io_nonblocking_udp::handle_pkt_out_udp(int sock)
{
	if (UDT::getsockstate(sock) != UDTSTATUS::CONNECTED) {
		// 曾經發生 state = 6 (broken)
		debug_printf("sock %d  state %d \n", sock, UDT::getsockstate(sock));
		return RET_SOCK_ERROR;
	}
	return RET_OK;
}

void io_nonblocking_udp::handle_pkt_error(int sock)
{

}

void io_nonblocking_udp::handle_pkt_error_udp(int sock)
{

}

void io_nonblocking_udp::handle_job_realtime()
{

}

void io_nonblocking_udp::handle_job_timer()
{

}

void io_nonblocking_udp::handle_sock_error(int sock, basic_class *bcptr)
{
	_peer_communication_ptr->fd_close(sock);
}

void io_nonblocking_udp::data_close(int sock)
{
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

