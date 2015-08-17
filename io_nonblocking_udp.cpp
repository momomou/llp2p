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
	debug_printf("Have deleted io_nonblocking_udp \n");
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
	
	// Get iter of map_udpfd_NonBlockIO
	map_udpfd_NonBlockIO_iter = _peer_communication_ptr->map_udpfd_NonBlockIO.find(sock);
	if (map_udpfd_NonBlockIO_iter == _peer_communication_ptr->map_udpfd_NonBlockIO.end()) {
		debug_printf("[DEBUG] Not found map_udpfd_NonBlockIO %d \n", sock);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Not found map_udpfd_NonBlockIO", sock);
		return RET_OK;
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
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_header_ptr new error", sock);
				_peer_communication_ptr->_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] chunk_header_ptr new error", __FUNCTION__, __LINE__);
				return RET_SOCK_ERROR;		// 關閉這條 socket
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
			if (!chunk_ptr) {
				_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] chunk_ptr new error", sock, buf_len);
				_peer_communication_ptr->_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] chunk_ptr new error", __FUNCTION__, __LINE__);
				return RET_SOCK_ERROR;		// 關閉這條 socket
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
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] Recv bytes", recv_byte, sock);
			return RET_SOCK_ERROR;		// 關閉這條 socket
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

		struct role_struct *role_protocol_ptr = (struct role_struct *)chunk_ptr;
		map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter = _peer_communication_ptr->map_mysession_candidates.end();
		struct peer_info_t *peer_info_ptr = NULL;

		// Get iter of map_mysession_candidates
		for (map<unsigned long, struct mysession_candidates *>::iterator iter = _peer_communication_ptr->map_mysession_candidates.begin(); iter != _peer_communication_ptr->map_mysession_candidates.end(); iter++) {
			for (int i = 0; i != iter->second->candidates_num; i++) {
				if (iter->second->n_candidates_info[i].pid == role_protocol_ptr->send_pid && iter->second->n_candidates_info[i].peercomm_session == role_protocol_ptr->peercomm_session) {
					map_mysession_candidates_iter = iter;
					peer_info_ptr = &(iter->second->n_candidates_info[i]);
					peer_info_ptr->estimated_delay = role_protocol_ptr->parent_src_delay + role_protocol_ptr->queueing_time + role_protocol_ptr->transmission_time;
					peer_info_ptr->PS_class = role_protocol_ptr->PS_class;
					_log_ptr->write_log_format("s(u) s d(d) u u u \n", __FUNCTION__, __LINE__, "peer", peer_info_ptr->pid, sock, role_protocol_ptr->parent_src_delay, role_protocol_ptr->queueing_time, role_protocol_ptr->transmission_time);
					
					UDT::TRACEINFO trace;
					memset(&trace, 0, sizeof(UDT::TRACEINFO));
					int nnn = UDT::perfmon(sock, &trace);
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u s d d d d d \n", "my_pid", _peer_communication_ptr->_pk_mgr_ptr->my_pid, "peer", peer_info_ptr->pid, "estimated_delay", peer_info_ptr->estimated_delay, role_protocol_ptr->parent_src_delay, role_protocol_ptr->queueing_time, role_protocol_ptr->transmission_time, (UINT32)(trace.msRTT));
				}
			}
		}
		if (map_mysession_candidates_iter == _peer_communication_ptr->map_mysession_candidates.end()) {
			// 可能是 peer-list 還沒收到，對方就先連進來
			return RET_OK;
		}

		// Add socket to map_mysession_candidates table
		peer_info_ptr->sock = sock;

		// Update connection_state and fd in map_pid_parent/map_pid_child
		peer_info_ptr->connection_state = PEER_CONNECTED;
		if (map_mysession_candidates_iter->second->myrole == CHILD_PEER) {
			if (_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid) != _peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.end()) {
				if (_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid)->second->peerInfo.connection_state == PEER_CONNECTING) {
					//_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid)->second->peerInfo.sock = sock;
					_peer_communication_ptr->_pk_mgr_ptr->map_pid_parent.find(peer_info_ptr->pid)->second->peerInfo.connection_state = PEER_CONNECTED;
				}
				else {
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[DEBUG] Duplicate connection to parent? ", peer_info_ptr->pid);
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
				if (iter->second->n_candidates_info[i].pid == peer_info_ptr->pid && iter->second->n_candidates_info[i].connection_state == PEER_CONNECTING) {
					iter->second->n_candidates_info[i].connection_state = PEER_CONNECTED;
					iter->second->n_candidates_info[i].sock = sock;
					_log_ptr->write_log_format("s(u) s u s u s d \n", __FUNCTION__, __LINE__, "Set peer_pid", iter->second->n_candidates_info[i].pid, "PEER_CONNECTED in my_session", iter->first, "sock", sock);
				}
			}
		}

		_log_ptr->write_log_format("s(u) s u s u s d s u s u s u s u s u \n", __FUNCTION__, __LINE__, "Recv CHNK_CMD_ROLE from sock", sock, 
																						"pid", role_protocol_ptr->send_pid, 
																						"role", role_protocol_ptr->flag, 
																						"manifest", role_protocol_ptr->manifest,
																						"class", role_protocol_ptr->PS_class,
																						"parent_src_delay", role_protocol_ptr->parent_src_delay,
																						"queueint_time", role_protocol_ptr->queueing_time,
																						"transmission_time", role_protocol_ptr->transmission_time);
		if (map_mysession_candidates_iter->second->myrole == PARENT_PEER) {
			HandleCMDRole(sock, role_protocol_ptr);
		}

		_net_udp_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
		_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
	} 
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_RTT) {
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Recv CHNK_CMD_PEER_RTT from sock", sock);
	}
	else {
		// 儘管延後開始建立連線的時間，還是可能發生尚未收到 peer-list，對方就先連過來的情況，此 case 就放棄
		debug_printf("[ERROR] Recv error CMD %d from sock %d \n", chunk_ptr->header.cmd, sock);
		_log_ptr->write_log_format("s(u) s u u \n", __FUNCTION__, __LINE__, "[ERROR] Recv error CMD from sock", sock, chunk_ptr->header.cmd);
	}

	if (chunk_ptr) {
		delete chunk_ptr;
	}
		
	return RET_OK;
}


void io_nonblocking_udp::HandleCMDRole(int sock, struct role_struct *role_protocol_ptr)
{
	UINT32 transmisstion_time = 0;
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
		return ;
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
			if (iter->second->n_candidates_info[i].sock == sock) {
				map_mysession_candidates_iter = iter;
				peer_info_ptr = &iter->second->p_candidates_info[i];
				_log_ptr->timerGet(&(iter->second->p_candidates_info[i].time_end));
				transmisstion_time = _log_ptr->diff_TimerGet_ms(&(iter->second->p_candidates_info[i].time_start), &(iter->second->p_candidates_info[i].time_end)) / 2;
			}
		}
	}
	if (map_mysession_candidates_iter == _peer_communication_ptr->map_mysession_candidates.end()) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Not found map_udpfd_NonBlockIO", sock);
		return ;		// 關閉這條 socket
	}

	if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {

		map<int, int>::iterator map_fd_flag_iter;
		map<int, unsigned long>::iterator map_fd_session_id_iter;		//must be store before connect
		map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;		//must be store before connect
		map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;

		int send_byte;

		role_protocol_ptr->header.rsv_1 = REPLY;
		if (role_protocol_ptr->flag == PARENT_PEER) {
			role_protocol_ptr->PS_class = _peer_communication_ptr->_pk_mgr_ptr->GetPSClass();
			role_protocol_ptr->parent_src_delay = _peer_communication_ptr->_pk_mgr_ptr->ss_table[_peer_communication_ptr->_pk_mgr_ptr->manifestToSubstreamID(map_mysession_candidates_iter->second->manifest)]->data.avg_src_delay;
			role_protocol_ptr->queueing_time = _peer_communication_ptr->_pk_mgr_ptr->GetQueueTime();
			//role_protocol_ptr->transmission_time = transmisstion_time + 100;
			UDT::TRACEINFO trace;
			memset(&trace, 0, sizeof(UDT::TRACEINFO));
			int nnn = UDT::perfmon(sock, &trace);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u s u s u u \n", "my_pid", _peer_communication_ptr->_pk_mgr_ptr->my_pid, "Reply", role_protocol_ptr->send_pid, "->", role_protocol_ptr->recv_pid, "[PS] RTT", transmisstion_time, (UINT32)(trace.msRTT));
		}
		role_protocol_ptr->flag = map_mysession_candidates_iter->second->myrole == CHILD_PEER ? PARENT_PEER : CHILD_PEER;

		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = sizeof(struct role_struct);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = sizeof(struct role_struct);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)role_protocol_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)role_protocol_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = role_protocol_ptr->header.sequence_number;

		_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Send", _send_byte, "bytes to sock", sock);

		if (_send_byte < 0) {
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[DEBUG] Send bytes", _send_byte, sock);
		}
		return;
	}
}

int io_nonblocking_udp::handle_pkt_out(int sock)
{
	return RET_OK;
}

int io_nonblocking_udp::handle_pkt_out_udp(int sock)
{
	// 處理【可能是 peer-list 還沒收到，對方就先連進來】的情況
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
	return ;
}

