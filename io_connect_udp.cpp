#include "io_connect_udp.h"
#include "pk_mgr.h"
#include "network_udp.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "io_nonblocking_udp.h"

using namespace std;

io_connect_udp::io_connect_udp(network_udp *net_udp_ptr, logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr, logger_client * logger_client_ptr)
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

io_connect_udp::~io_connect_udp()
{
	debug_printf("Have deleted io_connect_udp \n");
}

int io_connect_udp::handle_pkt_in(int sock)
{

	return RET_OK;
}

int io_connect_udp::handle_pkt_in_udp(int sock)
{	
	/*
	we will not inside this part
	*/
	
	debug_printf("sock %d  state %d \n", sock, UDT::getsockstate(sock));
	if (UDT::getsockstate(sock) != UDTSTATUS::CONNECTED) {
		debug_printf("sock %d  state %d \n", sock, UDT::getsockstate(sock));
	}
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : place in io_connect_udp::handle_pkt_in\n");
	//_logger_client_ptr->log_exit();

	//PAUSE
	
	return RET_OK;
}

int io_connect_udp::handle_pkt_out(int sock)
{
	return RET_OK;
}

// If the socket is writable, the conection is built.
// Then send "CHNK_CMD_ROLE" to determine the role
// Send peercomm_session
int io_connect_udp::handle_pkt_out_udp(int sock)
{
	/*
	in this part means the fd is built. it finds its role and sends protocol to another to tell its role.
	bind to peer_com~ for handle_pkt_in/out.
	*/
	UINT32 transmisstion_time = 0;
	struct timerStruct new_timer;
	_log_ptr->timerGet(&new_timer);
#ifdef _WIN32
	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "clocktime", new_timer.clockTime, "ticktime", new_timer.tickTime);
#endif
	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "state", UDT::getsockstate(sock));

	// Check the connection is successful or not
	if (UDT::getsockstate(sock) != UDTSTATUS::CONNECTED) {
		debug_printf("sock %d  state %d \n", sock, UDT::getsockstate(sock));
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "[DEBUG] sock", sock, "state", UDT::getsockstate(sock));
		return RET_SOCK_ERROR;		// 關閉這條 socket
	}
	

	// The connection is built. Next, we should determine the stream direction
	
	struct role_struct *role_protocol_ptr = NULL;
	Nonblocking_Ctl *Nonblocking_Send_Ctrl_ptr = NULL;
	map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
	map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter = _peer_communication_ptr->map_mysession_candidates.end();
	int _send_byte = 0;
	struct peer_info_t *peer_info_ptr = NULL;		// 這個 sock 的 peer

	// Get iter of map_udpfd_NonBlockIO
	map_udpfd_NonBlockIO_iter = _peer_communication_ptr->map_udpfd_NonBlockIO.find(sock);
	if (map_udpfd_NonBlockIO_iter == _peer_communication_ptr->map_udpfd_NonBlockIO.end()) {
		debug_printf("[DEBUG] Not found map_udpfd_NonBlockIO %d \n", sock);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Not found map_udpfd_NonBlockIO", sock);
		return RET_SOCK_ERROR;		// 關閉這條 socket
	}
	Nonblocking_Send_Ctrl_ptr = &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl);
	
	// Get iter of map_mysession_candidates
	for (map<unsigned long, struct mysession_candidates *>::iterator iter = _peer_communication_ptr->map_mysession_candidates.begin(); iter != _peer_communication_ptr->map_mysession_candidates.end(); iter++) {
		for (int i = 0; i != iter->second->candidates_num; i++) {
			_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__, 
				"my_session", iter->first, 
				"peer_pid", iter->second->p_candidates_info[i].pid,
				"fd1", iter->second->p_candidates_info[i].sock, 
				"fd2", iter->second->n_candidates_info[i].sock);
			if (iter->second->p_candidates_info[i].sock == sock) {
				map_mysession_candidates_iter = iter;
				peer_info_ptr = &iter->second->p_candidates_info[i];
				_log_ptr->timerGet(&(iter->second->p_candidates_info[i].time_end));
				transmisstion_time = _log_ptr->diff_TimerGet_ms(&(iter->second->p_candidates_info[i].time_start), &(iter->second->p_candidates_info[i].time_end)) / 2;
				//_log_ptr->write_log_format("s(u) u u \n", __FUNCTION__, __LINE__, _log_ptr->diff_TimerGet_ms(&(iter->second->p_candidates_info[i].time_start), &(iter->second->p_candidates_info[i].time_end)), _log_ptr->diff_TimerGet_ms(&(iter->second->p_candidates_info[i].time_start), &(iter->second->p_candidates_info[i].time_end)) / 2);
			}
		}
	}
	if (map_mysession_candidates_iter == _peer_communication_ptr->map_mysession_candidates.end()) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Not found map_udpfd_NonBlockIO", sock);
		return RET_SOCK_ERROR;		// 關閉這條 socket
	}

	if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {

		map<int, int>::iterator map_fd_flag_iter;
		map<int, unsigned long>::iterator map_fd_session_id_iter;		//must be store before connect
		map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;		//must be store before connect
		map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;

		int send_byte;

		role_protocol_ptr = new struct role_struct;
		if (!role_protocol_ptr) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] role_protocol_ptr new error", sock);
			_peer_communication_ptr->_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] role_protocol_ptr new error", __FUNCTION__, __LINE__);
			return RET_SOCK_ERROR;		// 關閉這條 socket
		}
		memset(role_protocol_ptr, 0, sizeof(struct role_struct));

		role_protocol_ptr->header.cmd = CHNK_CMD_ROLE;
		role_protocol_ptr->header.length = sizeof(struct role_struct) - sizeof(struct chunk_header_t);
		role_protocol_ptr->header.rsv_1 = REQUEST;
		role_protocol_ptr->flag = map_mysession_candidates_iter->second->myrole == CHILD_PEER ? PARENT_PEER : CHILD_PEER;
		role_protocol_ptr->manifest = map_mysession_candidates_iter->second->manifest;
		role_protocol_ptr->send_pid = _peer_mgr_ptr->self_pid;
		role_protocol_ptr->peercomm_session = peer_info_ptr->peercomm_session;
		if (map_mysession_candidates_iter->second->myrole == PARENT_PEER) {
			role_protocol_ptr->PS_class = _pk_mgr_ptr->GetPSClass();
			role_protocol_ptr->parent_src_delay = _pk_mgr_ptr->ss_table[_pk_mgr_ptr->manifestToSubstreamID(map_mysession_candidates_iter->second->manifest)]->data.avg_src_delay;
			role_protocol_ptr->queueing_time = _pk_mgr_ptr->GetQueueTime();
			role_protocol_ptr->transmission_time = transmisstion_time;

			UDT::TRACEINFO trace;
			memset(&trace, 0, sizeof(UDT::TRACEINFO));
			int nnn = UDT::perfmon(sock, &trace);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u s u s u u \n", "my_pid", _pk_mgr_ptr->my_pid, "Request", role_protocol_ptr->send_pid, "->", role_protocol_ptr->recv_pid, "[PS] RTT", transmisstion_time, (UINT32)(trace.msRTT));
		}

		Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.offset = 0;
		Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.total_len = sizeof(struct role_struct);
		Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.expect_len = sizeof(struct role_struct);
		Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.buffer = (char *)role_protocol_ptr;
		Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = (chunk_t *)role_protocol_ptr;
		Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.serial_num =  role_protocol_ptr->header.sequence_number;

		// Update connection_state and fd in map_pid_parent/map_pid_child
		peer_info_ptr->connection_state = PEER_CONNECTED;
		if (map_mysession_candidates_iter->second->myrole == CHILD_PEER) {
			if (_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid) != _peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.end()) {
				if (_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid)->second->peerInfo.connection_state == PEER_CONNECTING) {
					//_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid)->second->peerInfo.sock = sock;
					_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid)->second->peerInfo.connection_state = PEER_CONNECTED;
				}
				else {
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s d \n", "my_pid", _pk_mgr_ptr->my_pid, "[DEBUG] Duplicate connection to parent? ", peer_info_ptr->pid);
				}
			}
			else {
				// 隨時可能因為收到 SEED 造成 table 被清除
				_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "sock", sock, "pid", peer_info_ptr->pid);
			}
		}
		else if (map_mysession_candidates_iter->second->myrole == PARENT_PEER) {
			if (_peer_communication_ptr->_pk_mgr_ptr->map_pid_child.find(peer_info_ptr->pid) != _peer_communication_ptr->_pk_mgr_ptr->map_pid_child.end()) {
				if (_peer_communication_ptr->_pk_mgr_ptr->map_pid_child.find(peer_info_ptr->pid)->second->connection_state == PEER_CONNECTING) {
					//_peer_communication_ptr->_pk_mgr_ptr->map_pid_child.find(peer_info_ptr->pid)->second->sock = sock;
					_peer_communication_ptr->_pk_mgr_ptr->map_pid_child.find(peer_info_ptr->pid)->second->connection_state = PEER_CONNECTED;
				}
				else {
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[DEBUG] Duplicate connection to child? ", peer_info_ptr->pid);
				}
			}
			else {
				// 隨時可能因為收到 SEED 造成 table 被清除
				_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "sock", sock, "pid", peer_info_ptr->pid);
			}
		}

		// 同步其他與此 pid(正在連線中) 有關的 table 
		for (map<unsigned long, struct mysession_candidates *>::iterator iter = _peer_communication_ptr->map_mysession_candidates.begin(); iter != _peer_communication_ptr->map_mysession_candidates.end(); iter++) {
			for (int i = 0; i != iter->second->candidates_num; i++) {
				if (iter->second->p_candidates_info[i].pid == peer_info_ptr->pid && iter->second->p_candidates_info[i].connection_state == PEER_CONNECTING) {
					iter->second->p_candidates_info[i].connection_state = PEER_CONNECTED;
					iter->second->p_candidates_info[i].sock = sock;
				}
			}
		}

		_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Send", _send_byte, "bytes to sock", sock);

		if (_send_byte < 0) {
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] Send bytes", _send_byte, sock);
			return RET_SOCK_ERROR;		// 關閉這條 socket
		} 
		else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Send CHNK_CMD_ROLE to sock", sock);
			_net_udp_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
		}
	}
	else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == RUNNING) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING state send ");

		_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));

		PAUSE
	

		if (_send_byte < 0) {
			data_close(sock, "io connect error\n");
			return RET_SOCK_ERROR;
		}
		else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING  send OK");

			_net_udp_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
			
			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
		}
	}
	
	return RET_OK;
}

void io_connect_udp::handle_pkt_error(int sock)
{

}

void io_connect_udp::handle_pkt_error_udp(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_connect_udp handle_pkt_error error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void io_connect_udp::handle_job_realtime()
{

}

void io_connect_udp::handle_job_timer()
{

}

void io_connect_udp::handle_sock_error(int sock, basic_class *bcptr)
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


void io_connect_udp::data_close(int cfd, const char *reason) 
{
	return ;
}