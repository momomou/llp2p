
#include "io_accept_udp.h"
#include "pk_mgr.h"
#include "network_udp.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "io_nonblocking_udp.h"
#include "udt_lib/udt.h"

using namespace UDT;
using namespace std;

io_accept_udp::io_accept_udp(network_udp *net_udp_ptr,logger *log_ptr,peer_mgr * peer_mgr_ptr, peer_communication *peer_communication_ptr, logger_client * logger_client_ptr)
{
	_net_udp_ptr = net_udp_ptr;
	_log_ptr = log_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_communication_ptr = peer_communication_ptr;
	_logger_client_ptr = logger_client_ptr;
}

io_accept_udp::~io_accept_udp(){
	printf("==============deldet io_accept success==========\n");
}

int io_accept_udp::handle_pkt_in(int sock)
{
	return RET_OK;
}

// Accept and throw to non-blocking-udp
int io_accept_udp::handle_pkt_in_udp(int sock)
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
	
	int state;
	int size = sizeof(state);
	UDT::getsockopt(sock, 0, UDT_STATE, &state, &size);
	

	offset = 0;
	int new_fd = UDT::accept(sock, (struct sockaddr *)&_cin, &sin_len);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Accept fd", new_fd);
	
	if (new_fd < 0) {
		return RET_SOCK_ERROR;
	}
	else {
		
		// Check wheather new_fd is already in the map_udpfd_NonBlockIO or not
		if (_peer_communication_ptr->map_udpfd_NonBlockIO.find(new_fd) ==_peer_communication_ptr->map_udpfd_NonBlockIO.end()) {
			struct ioNonBlocking* ioNonBlocking_ptr = new struct ioNonBlocking;
			if (!ioNonBlocking_ptr) {
				_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] ioNonBlocking_ptr new error", sock, new_fd);
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "[ERROR] ioNonBlocking_ptr new error", sock, new_fd);
				debug_printf("[ERROR] ioNonBlocking_ptr new error %d %d \n", sock, new_fd);
				return RET_OK;
			}
			
			_log_ptr->write_log_format("s(u) s d s \n", __FUNCTION__, __LINE__, "Add fd", new_fd, "into map_udpfd_NonBlockIO");
			
			memset(ioNonBlocking_ptr, 0, sizeof(struct ioNonBlocking));
			ioNonBlocking_ptr->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
			_peer_communication_ptr->map_udpfd_NonBlockIO[new_fd] = ioNonBlocking_ptr;
		}
		else {
			debug_printf("[ERROR] new sock %d duplicate \n", new_fd);
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] new sock %d duplicate", new_fd);
			PAUSE
			return RET_OK;
		}

		//_net_ptr->set_blocking(new_fd);
		
		_net_udp_ptr->set_nonblocking(new_fd);
		_net_udp_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
		_net_udp_ptr->set_fd_bcptr_map(new_fd, dynamic_cast<basic_class *> (_peer_communication_ptr->_io_nonblocking_udp_ptr));
		//_peer_mgr_ptr->udp_fd_list_ptr->push_back(new_fd);

	}

	return RET_OK;
}

int io_accept_udp::handle_pkt_out(int sock)
{
	return RET_OK;
}

int io_accept_udp::handle_pkt_out_udp(int sock)
{
	debug_printf("%s \n", __FUNCTION__);
	/*
	we will not inside this part
	*/
	//_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : place in io_accept::handle_pkt_out\n");
	//_logger_client_ptr->log_exit();
	return RET_OK;
}

void io_accept_udp::handle_pkt_error(int sock)
{

}

void io_accept_udp::handle_pkt_error_udp(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept handle_pkt_error error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void io_accept_udp::handle_job_realtime()
{

}


void io_accept_udp::handle_job_timer()
{

}

void io_accept_udp::handle_sock_error(int sock, basic_class *bcptr){
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_peer_communication_ptr->fd_close(sock);
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_accept handle_sock_error error number : ",socketErr);
	_logger_client_ptr->log_exit();
}