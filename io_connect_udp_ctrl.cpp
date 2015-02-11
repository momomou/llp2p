#include "io_connect_udp_ctrl.h"
#include "pk_mgr.h"
#include "network_udp.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "io_nonblocking_udp.h"

using namespace std;

io_connect_udp_ctrl::io_connect_udp_ctrl(network_udp *net_udp_ptr, logger *log_ptr, configuration *prep_ptr, peer_mgr * peer_mgr_ptr, peer *peer_ptr, pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr, logger_client * logger_client_ptr)
{
	_net_udp_ptr = net_udp_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_communication_ptr = peer_communication_ptr;
	_logger_client_ptr = logger_client_ptr;
}

io_connect_udp_ctrl::~io_connect_udp_ctrl()
{
	debug_printf("Have deleted io_connect_udp_ctrl \n");
}

int io_connect_udp_ctrl::handle_pkt_in(int sock)
{

	return RET_OK;
}

int io_connect_udp_ctrl::handle_pkt_in_udp(int sock)
{	
	/*
	we will not inside this part
	*/
	
	debug_printf("sock %d  state %d \n", sock, UDT::getsockstate(sock));
	if (UDT::getsockstate(sock) != UDTSTATUS::CONNECTED) {
		debug_printf("sock %d  state %d \n", sock, UDT::getsockstate(sock));
	}
	//_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : place in io_connect_udp::handle_pkt_in\n");
	//_logger_client_ptr->log_exit();

	//PAUSE
	
	return RET_OK;
}

int io_connect_udp_ctrl::handle_pkt_out(int sock)
{
	return RET_OK;
}

// If the socket is writable, the conection is built.
// Then send "CHNK_CMD_ROLE" to determine the role
// Send peercomm_session
int io_connect_udp_ctrl::handle_pkt_out_udp(int sock)
{
	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "state", UDT::getsockstate(sock));

	struct rtt_struct *rtt_protocol_ptr = NULL;
	Nonblocking_Ctl *Nonblocking_Send_Ctrl_ptr = NULL;
	map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
	struct peer_info_t *peer_info_ptr = NULL;
	
	for (map<UINT32, struct peer_info_t>::iterator iter = _peer_communication_ptr->_pk_mgr_ptr->rtt_table.begin(); iter != _peer_communication_ptr->_pk_mgr_ptr->rtt_table.end(); iter++) {
		if (sock == iter->second.sock) {
			peer_info_ptr = &(iter->second);
		}
	}

	if (peer_info_ptr == NULL) {
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Not Found sock", sock, "state", UDT::getsockstate(sock));
		return RET_SOCK_ERROR;		// 關閉這條 socket
	}
	
	//_log_ptr->timerGet(&(peer_info_ptr->time_end));
	
	// Get iter of map_udpfd_NonBlockIO
	map_udpfd_NonBlockIO_iter = _peer_communication_ptr->map_udpfd_NonBlockIO.find(sock);
	if (map_udpfd_NonBlockIO_iter == _peer_communication_ptr->map_udpfd_NonBlockIO.end()) {
		debug_printf("[DEBUG] Not found map_udpfd_NonBlockIO %d \n", sock);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Not found map_udpfd_NonBlockIO", sock);
		return RET_SOCK_ERROR;		// 關閉這條 socket
	}
	Nonblocking_Send_Ctrl_ptr = &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl);

	if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {
		int send_byte;

		rtt_protocol_ptr = new struct rtt_struct;
		if (!rtt_protocol_ptr) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] role_protocol_ptr new error", sock);
			//_peer_communication_ptr->_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] role_protocol_ptr new error", __FUNCTION__, __LINE__);
			return RET_SOCK_ERROR;		// 關閉這條 socket
		}
		memset(rtt_protocol_ptr, 0, sizeof(struct rtt_struct));

		rtt_protocol_ptr->header.cmd = CHNK_CMD_PEER_RTT;
		rtt_protocol_ptr->header.length = sizeof(struct rtt_struct) - sizeof(struct chunk_header_t);
		rtt_protocol_ptr->header.rsv_1 = REQUEST;
		rtt_protocol_ptr->padding = 1;

		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = sizeof(struct rtt_struct);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = sizeof(struct rtt_struct);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)rtt_protocol_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)rtt_protocol_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = rtt_protocol_ptr->header.sequence_number;

		send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Send", send_byte, "bytes to sock", sock);

		if (send_byte < 0) {
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] Send bytes", send_byte, sock);
			return RET_SOCK_ERROR;		// 關閉這條 socket
		}
		else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Send CHNK_CMD_PEER_RTT to sock", sock);
			_net_udp_ptr->epoll_control(sock, EPOLL_CTL_DEL, UDT_EPOLL_OUT);		// 暫時關閉 writable, 等 timeout 再打開
			if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
		}
	}
	else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == RUNNING) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING state send ");
	}

	return RET_OK;
}

void io_connect_udp_ctrl::handle_pkt_error(int sock)
{

}

void io_connect_udp_ctrl::handle_pkt_error_udp(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_connect_udp handle_pkt_error error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void io_connect_udp_ctrl::handle_job_realtime()
{

}

void io_connect_udp_ctrl::handle_job_timer()
{

}

void io_connect_udp_ctrl::handle_sock_error(int sock, basic_class *bcptr)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_peer_communication_ptr->fd_close(sock);
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_connect_udp handle_sock_error error number : ",socketErr);
	_logger_client_ptr->log_exit();
}


void io_connect_udp_ctrl::data_close(int cfd, const char *reason)
{
	return ;
}