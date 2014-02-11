/*

*/

#include "peer.h"
#include "network.h"
#include "logger.h"
#include "pk_mgr.h"
#include "peer_mgr.h"
#include "peer_communication.h"
#include "logger_client.h"

using namespace std;

peer::peer(list<int> *fd_list)
{
	queue_out_ctrl_ptr = NULL;
	queue_out_data_ptr = NULL;
	peerInfoPtr = NULL;
	peerDownInfoPtr = NULL;
	_send_byte = 0;
	_expect_len = 0;
	_offset = 0;
	fd_list_ptr = fd_list;
	_chunk_ptr = NULL;
	//first_reply_peer =true;
	firstReplyPid=-1;
	leastSeq_set_childrenPID =0;
}

peer::~peer() 
{
	clear_map();
	debug_printf("==============deldet peer success==========\n");
}

//清理所有的 map_fd_out_ctrl queue_out_data_ptr 的ptr
void peer::clear_map()
{
	map<int, queue<struct chunk_t *> *>::iterator iter;	
	struct chunk_t *chunk_ptr = NULL;
	
	for (iter = map_fd_out_ctrl.begin(); iter != map_fd_out_ctrl.end(); iter++) {
		queue_out_ctrl_ptr = iter->second;
		while(queue_out_ctrl_ptr->size()) {
			chunk_ptr = queue_out_ctrl_ptr->front();
			queue_out_ctrl_ptr->pop();
			if (chunk_ptr) {	
				delete chunk_ptr;
			}
		}

		if (queue_out_ctrl_ptr) {
			delete queue_out_ctrl_ptr;
		}
	}

	for (iter = map_fd_out_data.begin(); iter != map_fd_out_data.end(); iter++) {
		queue_out_data_ptr = iter->second;
		while(queue_out_data_ptr->size()) {
			queue_out_data_ptr->pop();
			//if(chunk_ptr)		delete chunk_ptr;
		}

		if (queue_out_data_ptr) {
			delete queue_out_data_ptr;
		}
	}

	for(map_fd_nonblocking_ctl_iter =map_fd_nonblocking_ctl.begin() ;map_fd_nonblocking_ctl_iter !=map_fd_nonblocking_ctl.end() ;map_fd_nonblocking_ctl_iter ++){
		delete map_fd_nonblocking_ctl_iter->second;
	}
	map_fd_nonblocking_ctl.clear();

	for(substream_first_reply_peer_iter =substream_first_reply_peer.begin() ;substream_first_reply_peer_iter !=substream_first_reply_peer.end() ;substream_first_reply_peer_iter ++){
		delete substream_first_reply_peer_iter->second;
	}
	substream_first_reply_peer.clear();
}

void peer::peer_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr *pk_mgr_ptr, peer_mgr *peer_mgr_ptr, logger_client * logger_client_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_logger_client_ptr = logger_client_ptr;
	_net_ptr->peer_set(this);
}

//new queue    只有收到CHNK_CMD_PEER_CON才會呼叫
//收到一個建立連線的要求(需down stream)
void peer::handle_connect(int sock, struct chunk_t *chunk_ptr, struct sockaddr_in cin)
{
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	struct peer_info_t *rescue_peer = NULL;
//	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;
	chunk_request_ptr = (struct chunk_request_msg_t *)chunk_ptr;

	map_pid_rescue_peer_info_iter = _pk_mgr_ptr->map_pid_rescue_peer_info.find(chunk_request_ptr->info.pid);
	if (map_pid_rescue_peer_info_iter != _pk_mgr_ptr->map_pid_rescue_peer_info.end()) {

		debug_printf("fd error why dup\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] fd error why dup");
		//debug_printf("rescue_peer->pid = %d  socket = %d\n",chunk_request_ptr->info.pid,sock);
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "rescue_peer->pid =", chunk_request_ptr->info.pid, "manifest :", map_pid_rescue_peer_info_iter->second->manifest);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u \n", "rescue_peer->pid =", chunk_request_ptr->info.pid, "manifest :", map_pid_rescue_peer_info_iter->second->manifest, __LINE__);
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "rescue_peer->pid =", map_pid_rescue_peer_info_iter->second->pid, "manifest :", map_pid_rescue_peer_info_iter->second->manifest);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u \n", "rescue_peer->pid =", map_pid_rescue_peer_info_iter->second->pid, "manifest :", map_pid_rescue_peer_info_iter->second->manifest, __LINE__);
		
		//debug_printf("old manifest = %d \n",map_pid_rescue_peer_info_iter ->second->manifest);
		//old socket error and no stream close old
		if (map_pid_rescue_peer_info_iter->second->manifest == 0) {
			data_close(map_out_pid_fd[map_pid_rescue_peer_info_iter->first], "close_by peer com old socket", CLOSE_CHILD);
		//close new socket
		}
		else {
			data_close(sock, "close_by peer com new socket", CLOSE_CHILD);
			return;
		}
	}
	else {
	
	}

	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	map_out_pid_fd[chunk_request_ptr->info.pid] = sock;
	map_fd_pid[sock] = chunk_request_ptr->info.pid;
	map_fd_out_ctrl[sock] = queue_out_ctrl_ptr;
	map_fd_out_data[sock] = queue_out_data_ptr;

	Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff ;
	map_fd_nonblocking_ctl[sock] = Nonblocking_Buff_ptr ;

	if (!Nonblocking_Buff_ptr || !queue_out_ctrl_ptr || !queue_out_data_ptr) {
		_pk_mgr_ptr->exit_code = MALLOC_ERROR;
		debug_printf("peer::handle_connect  new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "peer::handle_connect   new error");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "peer::handle_connect new error \n");
		_logger_client_ptr->log_exit();
		PAUSE
	}

	memset(Nonblocking_Buff_ptr, 0, sizeof(Nonblocking_Buff));

	map_pid_rescue_peer_info_iter = _pk_mgr_ptr->map_pid_rescue_peer_info.find(chunk_request_ptr->info.pid);
	if (map_pid_rescue_peer_info_iter == _pk_mgr_ptr->map_pid_rescue_peer_info.end()) {

		rescue_peer = new struct peer_info_t;
		if (!rescue_peer) {
			_pk_mgr_ptr->exit_code = MALLOC_ERROR;
			debug_printf("peer::handle_connect  new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__," peer::handle_connect   new error");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "peer::handle_connect new error \n");
			_logger_client_ptr->log_exit();
			PAUSE
		}
		memset(rescue_peer, 0, sizeof(struct peer_info_t));
		
		rescue_peer->pid = chunk_request_ptr->info.pid;
		rescue_peer->public_ip = cin.sin_addr.s_addr;
		rescue_peer->private_ip = chunk_request_ptr->info.private_ip;
		rescue_peer->tcp_port = chunk_request_ptr->info.tcp_port;
		rescue_peer->udp_port = chunk_request_ptr->info.udp_port;
		_pk_mgr_ptr->map_pid_rescue_peer_info[rescue_peer->pid] = rescue_peer;
		//debug_printf("rescue_peer->pid = %d  socket=%d\n",rescue_peer->pid,sock);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "rescue_peer->pid =", rescue_peer->pid);
	}
	else {
		_pk_mgr_ptr->exit_code = UNKNOWN;
		debug_printf("fd error why dup\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] fd error why dup");
		debug_printf("rescue_peer->pid = %d  socket = %d\n",chunk_request_ptr->info.pid,sock);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[ERROR] rescue_peer->pid =", chunk_request_ptr->info.pid);
		data_close(sock,"close_by peer com", CLOSE_CHILD);
		*(_net_ptr->_errorRestartFlag) = RESTART;
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "[ERROR] fd error why dup", __LINE__);
		_logger_client_ptr->log_exit();
		
		map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
		map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
		for(temp_map_pid_fd_out_iter = map_out_pid_fd.begin();temp_map_pid_fd_out_iter != map_out_pid_fd.end();temp_map_pid_fd_out_iter++){
			debug_printf("map_out_pid_fd  pid : %d fd :%d\n",temp_map_pid_fd_out_iter->first,temp_map_pid_fd_out_iter->second);
			_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ",temp_map_pid_fd_out_iter->first," fd :",temp_map_pid_fd_out_iter->second);

		}
		for(temp_map_pid_fd_in_iter = map_in_pid_fd.begin();temp_map_pid_fd_in_iter != map_in_pid_fd.end();temp_map_pid_fd_in_iter++){
			debug_printf("map_in_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_in_iter->first,temp_map_pid_fd_in_iter->second);
			_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ",temp_map_pid_fd_in_iter->first," fd :",temp_map_pid_fd_in_iter->second);

		}

		PAUSE
	}
}

//只被build connect 呼叫
//送CHNK_CMD_PEER_ CON 到其他peer
int peer::handle_connect_request(int sock, struct level_info_t *level_info_ptr, unsigned long pid,Nonblocking_Ctl *Nonblocking_Send_Ctrl_ptr)
{
	debug_printf("peer::handle_connect_request \n");
	debug_printf("Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state: %d \n", Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state);
	
	if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY) {

		map<unsigned long, int>::iterator map_pid_fd_iter;
		map_pid_fd_iter = map_in_pid_fd.find(level_info_ptr->pid);
		if (map_pid_fd_iter == map_in_pid_fd.end()) {
			map_in_pid_fd[level_info_ptr->pid] = sock;
		}
		
		queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
		queue_out_data_ptr = new std::queue<struct chunk_t *>;
		Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff;
		if (!Nonblocking_Buff_ptr || !queue_out_ctrl_ptr || !queue_out_data_ptr) {
			_pk_mgr_ptr->exit_code = MALLOC_ERROR;
			debug_printf("peer::handle_connect  new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] peer::handle_connect   new error");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "peer::handle_connect  new error", __LINE__);
			_logger_client_ptr->log_exit();
			PAUSE
		}
		memset(Nonblocking_Buff_ptr, 0, sizeof(Nonblocking_Buff));
		map_fd_nonblocking_ctl[sock] = Nonblocking_Buff_ptr ;

		map_fd_pid[sock] = level_info_ptr->pid;
		map_fd_out_ctrl[sock] = queue_out_ctrl_ptr;
		map_fd_out_data[sock] = queue_out_data_ptr;

		struct chunk_t *chunk_ptr = NULL;
		struct chunk_request_msg_t *chunk_request_ptr = NULL;
		int send_byte;
		unsigned long channel_id;
		string svc_tcp_port("");
		string svc_udp_port("");

		_prep->read_key("channel_id", channel_id);
		_prep->read_key("svc_tcp_port", svc_tcp_port);
		_prep->read_key("svc_udp_port", svc_udp_port);

		chunk_request_ptr = (struct chunk_request_msg_t *)new unsigned char[sizeof(struct chunk_request_msg_t)];
		if (!chunk_request_ptr) {
			_pk_mgr_ptr->exit_code = MALLOC_ERROR;
			debug_printf("peer::chunk_request_ptr  new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] peer::chunk_request_ptr   new error");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "[ERROR] peer::chunk_request_ptr   new error", __LINE__);
			_logger_client_ptr->log_exit();
			PAUSE
		}
		memset(chunk_request_ptr, 0, sizeof(struct chunk_request_msg_t));

		chunk_request_ptr->header.cmd = CHNK_CMD_PEER_CON;
		chunk_request_ptr->header.rsv_1 = REQUEST;
		chunk_request_ptr->header.length = sizeof(struct request_info_t);
		chunk_request_ptr->info.pid = pid;
		chunk_request_ptr->info.channel_id = channel_id;
		chunk_request_ptr->info.private_ip = _net_ptr->getLocalIpv4();
		chunk_request_ptr->info.tcp_port = (unsigned short)atoi(svc_tcp_port.c_str());
		chunk_request_ptr->info.udp_port = (unsigned short)atoi(svc_udp_port.c_str());

		_net_ptr->set_nonblocking(sock);

		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = sizeof(struct role_struct);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = sizeof(struct role_struct);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = chunk_request_ptr->header.sequence_number;

		debug_printf("header.cmd = %d, total_len = %d \n", chunk_request_ptr->header.cmd, chunk_request_ptr->header.length);

		_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));

		if (_send_byte < 0) {
			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
			data_close(sock, "PEER　COM error", CLOSE_PARENT);
			return RET_SOCK_ERROR;

		}
		else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ) {
			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
			return 0;
		}
	}
	else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING ) {
		_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));

		if (_send_byte < 0) {
			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
			data_close(sock, "PEER　COM error",CLOSE_PARENT);
			return RET_SOCK_ERROR;
		}
		else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY){

			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
			return 0;
		}
	}

	//send_byte = send(sock, (char *)chunk_request_ptr, sizeof(struct chunk_request_msg_t), 0);
	//
	//if( send_byte <= 0 ) {
	//	data_close(sock, "send html_buf error",CLOSE_PARENT);
	//	PAUSE
	//	_log_ptr->exit(0, "send html_buf error");
	//	return -1;
	//} else {
	//	if(chunk_request_ptr)
	//		delete chunk_request_ptr;
	//	return 0;
	//}

}

// CHNK_CMD_PEER_DATA
// CHNK_CMD_PARENT_PEER
// CHNK_CMD_PEER_TEST_DELAY
// CHNK_CMD_PEER_SET_MANIFEST
int peer::handle_pkt_in(int sock)
{
	map<int, unsigned long>::iterator iter = map_fd_pid.find(sock);
	if (iter != map_fd_pid.end()) {
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "find this fd in map_fd_pid, pid =", iter->second);
	}
	else {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] cannot find this fd in map_fd_pid");	
	}
	
	int offset = 0;
	int recv_byte = 0;
	Nonblocking_Ctl * Nonblocking_Recv_ptr = NULL;
	struct chunk_header_t* chunk_header_ptr = NULL;
	struct chunk_t* chunk_ptr = NULL;
	unsigned long buf_len = 0;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(sock);
	if (map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) {
		Nonblocking_Recv_ptr = &(map_fd_nonblocking_ctl_iter->second->nonBlockingRecv);
		if (Nonblocking_Recv_ptr ->recv_packet_state == 0) {
			Nonblocking_Recv_ptr ->recv_packet_state = READ_HEADER_READY ;
		}
	}
	else {
		_pk_mgr_ptr->exit_code = MACCESS_ERROR;
		debug_printf("Nonblocking_Buff_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Nonblocking_Buff_ptr NOT FIND");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "Nonblocking_Buff_ptr NOT FIND", __LINE__);
		_logger_client_ptr->log_exit();
		PAUSE
		return RET_SOCK_ERROR;
	}

	for (int i = 0; i < 5; i++) {
		if (Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_READY) {
			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			if (!chunk_header_ptr) {
				_pk_mgr_ptr->exit_code = MALLOC_ERROR;
				debug_printf("peer::chunk_header_ptr  new error \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, " peer::chunk_header_ptr   new error");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "peer::chunk_header_ptr  new error", __LINE__);
				_logger_client_ptr->log_exit();
				PAUSE
			}
			memset(chunk_header_ptr, 0, sizeof(struct chunk_header_t));

			Nonblocking_Recv_ptr->recv_ctl_info.offset = 0;
			Nonblocking_Recv_ptr->recv_ctl_info.total_len = sizeof(chunk_header_t);
			Nonblocking_Recv_ptr->recv_ctl_info.expect_len = sizeof(chunk_header_t);
			Nonblocking_Recv_ptr->recv_ctl_info.buffer = (char *)chunk_header_ptr;
			/*Nonblocking_Recv_ptr->recv_packet_state become READ_HEADER_OK*/
		}
		else if (Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_RUNNING) {
			//do nothing
		}
		else if (Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_OK) {
			buf_len = sizeof(chunk_header_t) + ((chunk_t *)(Nonblocking_Recv_ptr->recv_ctl_info.buffer))->header.length;
			
			chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];
			if (!chunk_ptr) {
				_pk_mgr_ptr->exit_code = MALLOC_ERROR;
				debug_printf("peer::chunk_ptr  new error \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__," peer::chunk_ptr   new error");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "peer::chunk_ptr  new error", __LINE__);
				_logger_client_ptr->log_exit();
				PAUSE
			}

			memset(chunk_ptr, 0, buf_len);
			memcpy(chunk_ptr, Nonblocking_Recv_ptr->recv_ctl_info.buffer, sizeof(chunk_header_t));

			if (Nonblocking_Recv_ptr->recv_ctl_info.buffer) {
				delete [] (unsigned char*)Nonblocking_Recv_ptr->recv_ctl_info.buffer ;
			}
			
			Nonblocking_Recv_ptr ->recv_ctl_info.offset = sizeof(chunk_header_t) ;
			Nonblocking_Recv_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length ;
			Nonblocking_Recv_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length ;
			Nonblocking_Recv_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;

			_log_ptr->write_log_format("s =>u s u s u s u \n", __FUNCTION__, __LINE__,
															   "PEER RECV DATA READ_HEADER_OK cmd =", chunk_ptr->header.cmd,
															   "total_len =", Nonblocking_Recv_ptr->recv_ctl_info.total_len,
															   "expect_len", Nonblocking_Recv_ptr->recv_ctl_info.expect_len);

			Nonblocking_Recv_ptr->recv_packet_state = READ_PAYLOAD_READY ;
		}
		else if(Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_READY) {
			//do nothing
		}
		else if(Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_RUNNING) {
			//do nothing
			//_log_ptr->write_log_format("s =>u s u s u s u\n", __FUNCTION__,__LINE__,"PEER RECV DATA READ_PAYLOAD_RUNNING cmd= ",((chunk_t *)Nonblocking_Recv_ptr ->recv_ctl_info.buffer)->header.cmd,"total_len =",((chunk_t *)Nonblocking_Recv_ptr ->recv_ctl_info.buffer)->header.length,"expect_len",Nonblocking_Recv_ptr ->recv_ctl_info.expect_len);
		}
		else if (Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_OK) {
			break;
		}

		recv_byte =_net_ptr->nonblock_recv(sock,Nonblocking_Recv_ptr);

		if (recv_byte < 0) {
			data_close(sock, "[DEBUG] error occured in peer recv ",DONT_CARE);
			//PAUSE
			return RET_SOCK_ERROR;
		}
	}

	if (Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_OK) {
		chunk_ptr = (chunk_t *)Nonblocking_Recv_ptr->recv_ctl_info.buffer;
		Nonblocking_Recv_ptr->recv_packet_state = READ_HEADER_READY ;
	}
	else {
		//other stats
		return RET_OK;
	}
	/*Recieve done*/  
	
	_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "PEER RECV OK. cmd =", chunk_ptr->header.cmd, "total_len =", chunk_ptr->header.length);

	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_DATA) {
		//the main handle

		//不刪除 chunk_ptr 全權由handle_stream處理
		_pk_mgr_ptr->handle_stream(chunk_ptr, sock);
		
		return RET_OK;
	}
	//	just send return
	else if (chunk_ptr->header.cmd == CHNK_CMD_PARENT_PEER) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "CHNK_CMD_PARENT_PEER");
		_logger_client_ptr->log_exit();
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_TEST_DELAY ) {
	
		if (chunk_ptr->header.rsv_1 == REQUEST) {
			queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
			map<unsigned long, int>::iterator map_pid_fd_iter;
			map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;

			fd_queue_iter = map_fd_out_ctrl.find(sock);
			if (fd_queue_iter !=  map_fd_out_ctrl.end()) {
				queue_out_ctrl_ptr = fd_queue_iter ->second;
			}
			else {
				_pk_mgr_ptr->exit_code = MACCESS_ERROR;
				debug_printf("fd not here\n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "fd not here");
				*(_net_ptr->_errorRestartFlag) = RESTART;
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "fd not here", __LINE__);
				_logger_client_ptr->log_exit();
				PAUSE
				return RET_OK;
			}

			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_TEST_DELAY REQUEST");

			chunk_ptr->header.rsv_1 = REPLY;

			queue_out_ctrl_ptr->push((struct chunk_t *)chunk_ptr);

			if (queue_out_ctrl_ptr->size() != 0 ) {
				_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
			} 
			return RET_OK;
		}
		else if (chunk_ptr->header.rsv_1 == REPLY) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_TEST_DELAY  REPLY");

			unsigned long replyManifest = chunk_ptr->header.sequence_number;
			//debug_printf("REPLY  manifest =%d \n", replyManifest);
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "REPLY  manifest =", replyManifest);

			//第一個回覆的peer 放入peer_connect_down_t加入測量 並關閉其他連線和清除所有相關table
			for (substream_first_reply_peer_iter = substream_first_reply_peer.begin(); substream_first_reply_peer_iter != substream_first_reply_peer.end(); substream_first_reply_peer_iter++) {
				if (substream_first_reply_peer_iter->second->rescue_manifest == replyManifest && substream_first_reply_peer_iter->second->peer_role == 0) {
					break;
				}
			}
			
			if (substream_first_reply_peer_iter == substream_first_reply_peer.end()) {
				debug_printf("[ERROR] can not find subid_replyManifest in CHNK_CMD_PEER_TEST_DELAY  REPLY\n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"[ERROR] can not find subid_replyManifest in CHNK_CMD_PEER_TEST_DELAY  REPLY");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] not found subid_replyManifest in CHNK_CMD_PEER_TEST_DELAY  REPLY \n");
				if (chunk_ptr) {
					delete [] (unsigned char*)chunk_ptr;
				}
				return RET_OK;
			}

			if (substream_first_reply_peer_iter->second->firstReplyFlag) {
				map_fd_pid_iter = map_fd_pid.find(sock);
				if (map_fd_pid_iter != map_fd_pid.end()) {
					firstReplyPid = map_fd_pid_iter->second;

					//在這裡面sequence_number 裡面塞的是manifest
					debug_printf("first_reply_peer=%d  manifest =%d \n",firstReplyPid,replyManifest);
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "first_reply_peer =", firstReplyPid, "manifest =", replyManifest);

					pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(firstReplyPid);
					if (pid_peer_info_iter != _pk_mgr_ptr->map_pid_peer_info.end()) {

						for (unsigned long i = 0; i < _pk_mgr_ptr->map_pid_peer_info.count(firstReplyPid); i++) {
							peerInfoPtr = pid_peer_info_iter->second;
							if (pid_peer_info_iter->second->manifest == replyManifest) {
								_pk_mgr_ptr->map_pid_peer_info.erase(pid_peer_info_iter);

								pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(firstReplyPid);
								if (pid_peerDown_info_iter == _pk_mgr_ptr ->map_pid_peerDown_info.end()) {

									peerDownInfoPtr = new struct peer_connect_down_t ;
									if (!peerDownInfoPtr) {
										_pk_mgr_ptr->exit_code = MALLOC_ERROR;
										debug_printf("peer::peerDownInfoPtr  new error \n");
										_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__," peer::peerDownInfoPtr new error");
										_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] peer::peerDownInfoPtr new error");
										_logger_client_ptr->log_exit();
										PAUSE
									}

									memset(peerDownInfoPtr, 0, sizeof(struct peer_connect_down_t));
									memcpy(peerDownInfoPtr, peerInfoPtr, sizeof(struct peer_info_t));
									
									_pk_mgr_ptr->map_pid_peerDown_info[firstReplyPid] = peerDownInfoPtr;
									_pk_mgr_ptr->set_parent_manifest(peerDownInfoPtr, peerDownInfoPtr->peerInfo.manifest | replyManifest);
									
									delete peerInfoPtr;
								}
								else {
									peerDownInfoPtr = pid_peerDown_info_iter->second;
									_pk_mgr_ptr->set_parent_manifest(peerDownInfoPtr, peerDownInfoPtr->peerInfo.manifest | replyManifest);
							
								}
								break;
							}
							pid_peer_info_iter++;
						}

						//要的是全部的串流且是第二個加入table的 (join且 自己不是seed) (第一個是PK ) 則送拓墣
						if (replyManifest == _pk_mgr_ptr->full_manifest && _pk_mgr_ptr ->map_pid_peerDown_info.size() == 2) {
							for (unsigned long substreamID = 0; substreamID < _pk_mgr_ptr->sub_stream_num; substreamID++) {
								// send topology to PK
								_pk_mgr_ptr->send_parentToPK(_pk_mgr_ptr->SubstreamIDToManifest(substreamID), PK_PID+1);
								
							}
							_pk_mgr_ptr->send_capacity_to_pk(_pk_mgr_ptr->_sock);
							//_pk_mgr_ptr ->send_rescueManifestToPKUpdate(0);
						}
						/*
						else{
							//如果這個 peer 來的chunk 裡的substream 從pk和這個peer都有來  (if rescue testing stream)
							
							peerDownInfoPtr->outBuffCount = 0;
							(_pk_mgr_ptr->ssDetect_ptr + _pk_mgr_ptr->manifestToSubstreamID(replyManifest)) ->isTesting =1 ;	//ture
							debug_printf("SSID = %d start testing stream\n",_pk_mgr_ptr->manifestToSubstreamID(replyManifest));
							_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"start testing stream SSID = ",_pk_mgr_ptr->manifestToSubstreamID(replyManifest));

							//這邊只是暫時改變PK的substream 實際上還是有串流下來
							_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"PK old manifest",_pk_mgr_ptr->pkDownInfoPtr->peerInfo.manifest);
							_pk_mgr_ptr->pkDownInfoPtr->peerInfo.manifest &= (~replyManifest);
							_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"for tetsing stream PK manifest temp change to ",_pk_mgr_ptr->pkDownInfoPtr->peerInfo.manifest);

							//開始testing 送topology
							_pk_mgr_ptr->send_parentToPK ( _pk_mgr_ptr->SubstreamIDToManifest (_pk_mgr_ptr->manifestToSubstreamID(replyManifest)) , (_pk_mgr_ptr->ssDetect_ptr + temp_sub_id)->previousParentPID ); 

							//testing function
							_pk_mgr_ptr->reSet_detectionInfo();
						} 
						*/

						_peer_mgr_ptr -> send_manifest_to_parent(peerDownInfoPtr ->peerInfo.manifest ,firstReplyPid);
						_pk_mgr_ptr->reSet_detectionInfo();

						if (replyManifest == _pk_mgr_ptr ->full_manifest) {
							_logger_client_ptr->log_to_server(LOG_REG_LIST_TESTING,replyManifest,firstReplyPid);
						}
						else {
							_logger_client_ptr->log_to_server(LOG_RESCUE_TESTING,replyManifest,firstReplyPid);
						}

						debug_printf("sent to parent manifest = %d\n",peerDownInfoPtr ->peerInfo.manifest);
						_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"first_reply_peer=",firstReplyPid,"manifest",peerDownInfoPtr ->peerInfo.manifest);

						
						for (pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_peer_info.end(); pid_peer_info_iter++) {
							debug_printf("_pk_mgr_ptr ->map_pid_peer_info  pid : %d \n", pid_peer_info_iter->first);
							_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"_pk_mgr_ptr ->map_pid_peer_info  pid : ", pid_peer_info_iter->first);
						}
						debug_printf("_pk_mgr_ptr ->map_pid_peer_info.size(): %d \n", _pk_mgr_ptr ->map_pid_peer_info.size());
						
						for (pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_peer_info.end(); ) {
							debug_printf("_pk_mgr_ptr ->map_pid_peer_info.size(): %d \n", _pk_mgr_ptr ->map_pid_peer_info.size());
							debug_printf("_pk_mgr_ptr ->map_pid_peer_info  pid : %d \n", pid_peer_info_iter->first);
							peerInfoPtr = pid_peer_info_iter->second;

							if (peerInfoPtr->manifest == replyManifest) {
								//若是自己或是先前已經建立過連線的parent 則不close (會關到正常連線)  
								if (pid_peer_info_iter ->first == _peer_mgr_ptr->self_pid) {
									pid_peer_info_iter++;
									continue;
								} else if (_pk_mgr_ptr->map_pid_peerDown_info.find(pid_peer_info_iter ->first) != _pk_mgr_ptr->map_pid_peerDown_info.end()) {
									pid_peer_info_iter++;
									continue;
								}

								if (map_in_pid_fd.find( peerInfoPtr->pid ) != map_in_pid_fd.end()) {
									//只剩最後一個才關socket
									if(_pk_mgr_ptr ->map_pid_peer_info.count(peerInfoPtr->pid) == 1){
										
										data_close(map_in_pid_fd[peerInfoPtr->pid ],"close by firstReplyPid ",CLOSE_PARENT);
										pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.begin();
										continue;	// 為了不要在刪除iter後還跑到650行的iter++ 因此加上continue
									}else{
									//do nothing 
									}
								}
								else{
								
									//若在傳送期間PARENT socket error  已經被關閉則應該是可忽略
									debug_printf("testdelay 485 \n");
									_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"testdelay 485 ");
									//PAUSE
								}
							}
							pid_peer_info_iter++;
						}
						
						
						////	Commented on 20131222
						/*
						for (pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_peer_info.end(); pid_peer_info_iter++) {
							debug_printf("_pk_mgr_ptr ->map_pid_peer_info.size(): %d \n", _pk_mgr_ptr ->map_pid_peer_info.size());
							debug_printf("_pk_mgr_ptr ->map_pid_peer_info  pid : %d \n", pid_peer_info_iter->first);
							peerInfoPtr = pid_peer_info_iter->second;

							if (peerInfoPtr->manifest == replyManifest) {
								//若是自己或是先前已經建立過連線的parent 則不close (會關到正常連線)  
								if (pid_peer_info_iter ->first == _peer_mgr_ptr->self_pid) {
									continue;
								} else if (_pk_mgr_ptr->map_pid_peerDown_info.find(pid_peer_info_iter ->first) != _pk_mgr_ptr->map_pid_peerDown_info.end()) {
									continue;
								}

								if (map_in_pid_fd.find( peerInfoPtr->pid ) != map_in_pid_fd.end()) {
									//只剩最後一個才關socket
									if(_pk_mgr_ptr ->map_pid_peer_info.count(peerInfoPtr->pid) == 1){
										
										data_close(map_in_pid_fd[peerInfoPtr->pid ],"close by firstReplyPid ",CLOSE_PARENT);
										pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.begin();
									}else{
									//do nothing 
									}
								}
								else{
									//若在傳送期間PARENT socket error  已經被關閉則應該是可忽略
									debug_printf("testdelay 485 \n");
									_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"testdelay 485 ");
									//PAUSE
								}
							}
						}*/
						_pk_mgr_ptr ->clear_map_pid_peer_info(replyManifest);
					}
					else {
						_pk_mgr_ptr->exit_code = MACCESS_ERROR;
						debug_printf("pid_peer_info_iter not found \n");
						_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "pid_peer_info_iter not found");
						_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "pid_peer_info_iter not found", __LINE__);
						_logger_client_ptr->log_exit();
						PAUSE
					}
				}

				substream_first_reply_peer_iter->second->firstReplyFlag = false;

				delete [] (unsigned char*) substream_first_reply_peer_iter->second;
				substream_first_reply_peer.erase(substream_first_reply_peer_iter);
			}
		} // END ...  else if(chunk_ptr->header.rsv_1 ==REPLY){
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST) {
		_log_ptr->write_log_format("s(u) s\n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SET_MANIFEST");

		_peer_mgr_ptr ->handle_manifestSet((struct chunk_manifest_set_t *)chunk_ptr); 
		
		if (((struct chunk_manifest_set_t *)chunk_ptr)->manifest == 0) {
			data_close(sock, "close by Children SET Manifest = 0", CLOSE_PARENT);
		}
		_pk_mgr_ptr->send_capacity_to_pk(_pk_mgr_ptr->_sock);
	} 
	else {
		_pk_mgr_ptr->exit_code = UNKNOWN;
		debug_printf("Unknown header.cmd %d \n", chunk_ptr->header.cmd);
		_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__, __LINE__, "Unknown header.cmd %d", chunk_ptr->header.cmd);
		*(_net_ptr->_errorRestartFlag) =RESTART;
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d u \n", "Unknown header.cmd", chunk_ptr->header.cmd, __LINE__);
		_logger_client_ptr->log_exit();
		PAUSE
	}

	if (chunk_ptr) {
		delete [] (unsigned char*)chunk_ptr;
	}

	return RET_OK;
}

//送queue_out_ctrl_ptr 和queue_out_data_ptr出去
//0311 這邊改成如果阻塞了  就會blocking 住直到送出去為止
int peer::handle_pkt_out(int sock)
{
	struct sockaddr_in addr;
	int addrLen = sizeof(struct sockaddr_in);
	int n = getpeername(sock, (struct sockaddr *)&addr, &addrLen);
	//debug_printf("%s  n:%2d  sock: %2d , DstAddr: %s:%d \n", __FUNCTION__, n, sock, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	map<int, unsigned long>::iterator iter = map_fd_pid.find(sock);
	if (iter != map_fd_pid.end()) {
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "find this fd in map_fd_pid, pid =", iter->second);
	}
	else {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "cannot find this fd in map_fd_pid");	
	}
	
	struct chunk_manifest_set_t chunk_manifestSet;
	struct chunk_t *chunk_ptr=NULL;
	
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;
	map<int, Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;

	map<int, unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_in_iter;
	map<unsigned long, int>::iterator map_pid_fd_out_iter;
//	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
//	struct peer_connect_down_t* peerDownInfo ;
	struct peer_info_t *peerInfoPtr = NULL;

	Nonblocking_Ctl * Nonblocking_Send_Data_ptr = NULL;
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr = NULL;

	fd_out_ctrl_iter = map_fd_out_ctrl.find(sock);
	if (fd_out_ctrl_iter != map_fd_out_ctrl.end()) {
		queue_out_ctrl_ptr = fd_out_ctrl_iter->second;
	}
	else{
		debug_printf("queue_out_ctrl_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG](this condition shouldn't happen) queue_out_ctrl_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}
	
	fd_out_data_iter = map_fd_out_data.find(sock);
	if (fd_out_data_iter != map_fd_out_data.end()) {
		queue_out_data_ptr = fd_out_data_iter->second;
	}
	else{
		debug_printf("queue_out_data_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"[DEBUG](this condition shouldn't happen) queue_out_data_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(sock);
	if (map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) {
		Nonblocking_Send_Data_ptr = &map_fd_nonblocking_ctl_iter->second->nonBlockingSendData;
		Nonblocking_Send_Ctrl_ptr = &map_fd_nonblocking_ctl_iter->second->nonBlockingSendCtrl;
	}
	else{
		debug_printf("Nonblocking_Buff_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s\n", __FUNCTION__,__LINE__,"[DEBUG](this condition shouldn't happen) Nonblocking_Buff_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}


	//測試性功能
	////////////////////////////////////////////////////////////
	map_fd_pid_iter= map_fd_pid.find(sock);
	if(map_fd_pid_iter != map_fd_pid.end()){
		map_pid_rescue_peer_info_iter = _pk_mgr_ptr->map_pid_rescue_peer_info .find(map_fd_pid_iter ->second);
		if(map_pid_rescue_peer_info_iter ==  _pk_mgr_ptr->map_pid_rescue_peer_info.end()){
			//printf("peer::handle_pkt_out where is the peer\n");
			peerInfoPtr =NULL ;
			//PAUSE
			//return RET_SOCK_ERROR;
		}else{
			peerInfoPtr = map_pid_rescue_peer_info_iter ->second ;
		}
	}else{
		_pk_mgr_ptr->exit_code = MACCESS_ERROR;
		debug_printf("map_fd_pid NOT FIND \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"map_fd_pid NOT FIND");
		*(_net_ptr->_errorRestartFlag) =RESTART;
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "map_fd_pid NOT FIND", __LINE__);
		_logger_client_ptr->log_exit();
		PAUSE
		return RET_SOCK_ERROR;
	}
	//////////////////////////////////////////////////////////////

//	while(queue_out_ctrl_ptr->size() != 0  ||  queue_out_data_ptr->size() != 0){

		debug_printf2("queue_out_ctrl_ptr->size() = %d \n", queue_out_ctrl_ptr->size());
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size());

		if (queue_out_ctrl_ptr->size() != 0 && chunk_ptr == NULL) {

			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ) {

				chunk_ptr = queue_out_ctrl_ptr->front();

				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = chunk_ptr->header.length + sizeof(chunk_header_t);
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = chunk_ptr->header.length + sizeof(chunk_header_t);
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)chunk_ptr;
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_ptr;
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = chunk_ptr->header.sequence_number;

				//_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
				//												"PEER SEND CTL READY. header.cmd =", Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr->header.cmd,
				//												"total_len =",Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len,
				//												"expect_len =",Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len);

				_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
				
				_log_ptr->write_log_format("s(u) s u s s d s d \n", __FUNCTION__, __LINE__, 
																	"sent ctrl", _send_byte, "bytes", 
																	"queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size(),
																	"header.cmd =", chunk_ptr->header.cmd);
				
				memcpy(&chunk_manifestSet, chunk_ptr, sizeof(chunk_manifestSet));
				if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST) {
					if (map_fd_pid_iter != map_fd_pid.end()) {
						_log_ptr->write_log_format("s =>u s d s u s u s u \n", __FUNCTION__, __LINE__,
																			   "header.cmd =", chunk_manifestSet.header.cmd,
																			   "manifest =", chunk_manifestSet.manifest,
																			   "my pid =", chunk_manifestSet.pid,
																			   "parent pid =", map_fd_pid_iter->second);
					}
					else {
						_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "map_fd_pid NOT FIND");
					}
				}

				if(_send_byte < 0) {
					data_close(sock, "error occured in send  READY queue_out_ctrl",DONT_CARE);

					return RET_SOCK_ERROR;

				} 
				else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ) {
					queue_out_ctrl_ptr->pop();
					if (chunk_ptr) {
						delete chunk_ptr;
					}
					chunk_ptr = NULL;
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "chunk_manifestSet.manifest =", chunk_manifestSet.manifest);
					//// 0903新增，確定送給parent manifest=0這個訊息後，刪除整個相關的table
					if (chunk_manifestSet.header.cmd != CHNK_CMD_PEER_TEST_DELAY) {
						if (chunk_manifestSet.manifest == 0) {
							map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
							map<int , unsigned long>::iterator detect_map_fd_pid_iter;
							detect_map_fd_pid_iter = map_fd_pid.find(sock);
							if(detect_map_fd_pid_iter == map_fd_pid.end()){
								_pk_mgr_ptr->exit_code = MACCESS_ERROR;
								debug_printf("[ERROR] can not find fd pid in source_delay_detection\n");
								_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "[ERROR] can not find fd pid in source_delay_detection");
								_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "[ERROR] can not find fd pid in source_delay_detection", __LINE__);
								_logger_client_ptr->log_exit();
								PAUSE
							}
							pid_peerDown_info_iter = _pk_mgr_ptr->map_pid_peerDown_info.find(detect_map_fd_pid_iter->second);
							if(pid_peerDown_info_iter == _pk_mgr_ptr->map_pid_peerDown_info.end()){
								_pk_mgr_ptr->exit_code = MACCESS_ERROR;
								debug_printf("[ERROR] can not find pid peerinfo in source_delay_detection\n");
								_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "[ERROR] can not find pid peerinfo in source_delay_detection");
								_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "[ERROR] can not find pid peerinfo in source_delay_detection", __LINE__);
								_logger_client_ptr->log_exit();
								PAUSE
							}
							if (pid_peerDown_info_iter->second->peerInfo.manifest == 0) {
								map<unsigned long, int>::iterator iter = map_in_pid_fd.find(pid_peerDown_info_iter ->first) ;
								if (iter != map_in_pid_fd.end()) {
									data_close(map_in_pid_fd[pid_peerDown_info_iter ->first], "manifest=0", CLOSE_PARENT) ;
								}
								else{
									_pk_mgr_ptr->exit_code = MACCESS_ERROR;
									_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "[DEBUG] set parent manifest=0 but cannot find this parent's pid in map_in_pid_fd");
									_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "[DEBUG] set parent manifest=0 but cannot find this parent's pid in map_in_pid_fd", __LINE__);
									_logger_client_ptr->log_exit();
									PAUSE
								}
							}
						}
					}
					////
					/*
					////0902
					// This happens when data_close() called but found that queue_out_ctrl_ptr.size()>0, so cannot
					// delete it in data_close(). Once send this ctrl packet out and its size=0, it can be deleted
					if (chunk_manifestSet.manifest == 0) {
						_log_ptr->write_log_format("s =>u s s d s d \n", __FUNCTION__, __LINE__,
																  "[DEBUG] before delete fd_out_ctrl_iter and fd_out_data_iter",
																  "queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size(),
																  "queue_out_data_ptr->size() =", queue_out_data_ptr->size());
		
						delete fd_out_ctrl_iter->second;
						map_fd_out_ctrl.erase(fd_out_ctrl_iter);
						
						// Close socket in _map_fd_bc_tbl
						_net_ptr->close(sock);
						for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
							if(*fd_iter == sock) {
								fd_list_ptr->erase(fd_iter);
								break;
							}
						}
					}
					////
					*/
				}
			}
			else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING ) {
				_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));

				if (_send_byte < 0) {
						data_close(sock, "error occured in send RUNNING queue_out_data", DONT_CARE);
						//PAUSE
						return RET_SOCK_ERROR;
				}
				else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY) {
					queue_out_data_ptr->pop();
					if (chunk_ptr) {
						delete chunk_ptr;
					}
					chunk_ptr = NULL;
				}
			}
		} 
		// DATA
		else if (queue_out_data_ptr->size() != 0) {

			if (queue_out_data_ptr->size() > 100) {
				//debug_printf("queue_out_data_ptr->size() = %d \n", queue_out_data_ptr->size());
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_data_ptr->size() =", queue_out_data_ptr->size());
			}
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_data_ptr->size() =", queue_out_data_ptr->size());

			if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY ) {

				chunk_ptr = queue_out_data_ptr->front();

				//測試性功能
				//////////////////////////////////////////////////////////////////////////////
				if(peerInfoPtr){
					while(1){
						//如果現在manifest 的值已經沒有要送了  則略過這個
						if (!(peerInfoPtr ->manifest & (_pk_mgr_ptr ->SubstreamIDToManifest ( (chunk_ptr->header.sequence_number % _pk_mgr_ptr ->sub_stream_num)) ) )){
							if(queue_out_data_ptr->size() >=2){
								queue_out_data_ptr ->pop() ;
								chunk_ptr = queue_out_data_ptr->front();
								continue ;
							}else{
								return RET_OK;
							}
							//有要送
						}else{
							break;
						}
					}
				}
				//////////////////////////////////////////////////////////////////////////////

				Nonblocking_Send_Data_ptr->recv_ctl_info.offset = 0;
				Nonblocking_Send_Data_ptr->recv_ctl_info.total_len = chunk_ptr->header.length + sizeof(chunk_header_t);
				Nonblocking_Send_Data_ptr->recv_ctl_info.expect_len = chunk_ptr->header.length + sizeof(chunk_header_t);
				Nonblocking_Send_Data_ptr->recv_ctl_info.buffer = (char *)chunk_ptr;
				Nonblocking_Send_Data_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_ptr;
				Nonblocking_Send_Data_ptr->recv_ctl_info.serial_num = chunk_ptr->header.sequence_number;

				//_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
				//												"PEER SEND DATA READY. cmd =", Nonblocking_Send_Data_ptr->recv_ctl_info.chunk_ptr->header.cmd,
				//												"total_len =", Nonblocking_Send_Data_ptr->recv_ctl_info.total_len,
				//												"expect_len =", Nonblocking_Send_Data_ptr->recv_ctl_info.expect_len);

				_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
				
				_log_ptr->write_log_format("s(u) s u s s d s d \n", __FUNCTION__, __LINE__, 
																	"sent data", _send_byte, "bytes", 
																	"queue_out_data_ptr->size() =", queue_out_data_ptr->size(),
																	"header.cmd =", chunk_ptr->header.cmd);
				
				if (_send_byte < 0) {
					data_close(sock, "error occured in send queue_out_data", DONT_CARE);
					//PAUSE
					return RET_SOCK_ERROR;
				}
				else if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY ) {
					if (!_logger_client_ptr->log_bw_out_init_flag) {
						_logger_client_ptr->log_bw_out_init_flag = 1;
						_logger_client_ptr->bw_out_struct_init(_send_byte);
					}
					else {
						_logger_client_ptr->set_out_bw(_send_byte);
					}
					queue_out_data_ptr->pop();
					chunk_ptr = NULL;
				}
			}
			else if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == RUNNING){
		
				_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
			
				//_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
				//												"PEER SEND DATA READY cmd =", Nonblocking_Send_Data_ptr->recv_ctl_info.chunk_ptr->header.cmd,
				//												"total_len =", Nonblocking_Send_Data_ptr->recv_ctl_info.total_len,
				//												"expect_len =", Nonblocking_Send_Data_ptr->recv_ctl_info.expect_len);


				//if(!(_logger_client_ptr->log_bw_out_init_flag)){
				//	_logger_client_ptr->log_bw_out_init_flag = 1;
				//	_logger_client_ptr->bw_out_struct_init(_send_byte);
				//}
				//else{
				//	_logger_client_ptr->set_out_bw(_send_byte);
				//}

				if (_send_byte < 0) {
					data_close(sock, "error occured in send queue_out_data", DONT_CARE);
					//PAUSE
					return RET_SOCK_ERROR;
				}
				else if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY) {
					if (!_logger_client_ptr->log_bw_out_init_flag) {
						_logger_client_ptr->log_bw_out_init_flag = 1;
						_logger_client_ptr->bw_out_struct_init(_send_byte);
					}
					else {
						_logger_client_ptr->set_out_bw(_send_byte);
					}
					queue_out_data_ptr->pop();
					chunk_ptr = NULL;
				}
			}
		} 
		else {
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
		}
		
	//debug_printf("peer::handle_pkt_out end \n");
	return RET_OK;
}

void peer::handle_pkt_error(int sock)
{

}

void peer::handle_sock_error(int sock, basic_class *bcptr)
{
	_net_ptr->fd_bcptr_map_delete(sock);
	data_close(sock, "peer handle_sock_error!!",DONT_CARE);
}

void peer::handle_job_realtime()
{

}

void peer::handle_job_timer()
{

}

//全部裡面最完整的close

//這邊的區域變數都不參考環境變數(因位會開thread 來close socket 會搶 變數 使用)
////暫時不close  pid_peer_info 須自行清理
void peer::data_close(int cfd, const char *reason ,int type) 
{
	unsigned long pid = -1;
	list<int>::iterator fd_iter;
	//map<int, queue<struct chunk_t *> *>::iterator map_fd_queue_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;
	map<int , unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter; 
	multimap <unsigned long, struct peer_info_t *>::iterator pid_child_peer_info_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;


	map<unsigned long, int>::iterator map_pid_fd_in_iter;
	map<unsigned long, int>::iterator map_pid_fd_out_iter;
	struct peer_info_t *peerInfoPtr = NULL;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;
	map<unsigned long, manifest_timmer_flag *>::iterator substream_first_reply_peer_iter;
	map<unsigned long, manifest_timmer_flag *>::iterator temp_substream_first_reply_peer_iter;
	//unsigned long manifest = 0;	

	unsigned long  peerTestingManifest=0;

	_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, reason);
	debug_printf("PEER DATA CLOSE  colse fd = %d  %s \n", cfd, reason);
	
	debug_printf("Before delete this peer. Table information: \n");
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Before delete this peer. Table information:");
	map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
	for(temp_map_pid_fd_out_iter = map_out_pid_fd.begin();temp_map_pid_fd_out_iter != map_out_pid_fd.end();temp_map_pid_fd_out_iter++){
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n",temp_map_pid_fd_out_iter->first,temp_map_pid_fd_out_iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ",temp_map_pid_fd_out_iter->first," fd :",temp_map_pid_fd_out_iter->second);
	}
	for(temp_map_pid_fd_in_iter = map_in_pid_fd.begin();temp_map_pid_fd_in_iter != map_in_pid_fd.end();temp_map_pid_fd_in_iter++){
		debug_printf("map_in_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_in_iter->first,temp_map_pid_fd_in_iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ",temp_map_pid_fd_in_iter->first," fd :",temp_map_pid_fd_in_iter->second);

	}
	
	//_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	
	// Close socket in _map_fd_bc_tbl
	_net_ptr->close(cfd);

	unsigned long  testingManifest=0;
	for(unsigned long i =0  ; i < _pk_mgr_ptr ->sub_stream_num;i++){
		//if(  ((_pk_mgr_ptr->ssDetect_ptr) + i) ->isTesting ){
		if(!(_pk_mgr_ptr ->check_rescue_state(i,0))){
			testingManifest |= _pk_mgr_ptr ->SubstreamIDToManifest(i);
		}
	}

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
	
	//clean all fd table in _peer_communication_ptr OBJ
	_peer_mgr_ptr->_peer_communication_ptr->clear_fd_in_peer_com(cfd);

	fd_out_ctrl_iter = map_fd_out_ctrl.find(cfd);
	if (fd_out_ctrl_iter != map_fd_out_ctrl.end()) {
		_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__, __LINE__, 
													 "before delete it, fd_out_ctrl_iter->second.size() =", fd_out_ctrl_iter->second->size());
		
		debug_printf("11 \n");
		delete fd_out_ctrl_iter->second;
		debug_printf("22 \n");
		map_fd_out_ctrl.erase(fd_out_ctrl_iter);
		//_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "fd_out_ctrl_iter->second.size() = 0, delete it");
	}
	
	fd_out_data_iter = map_fd_out_data.find(cfd);
	if (fd_out_data_iter != map_fd_out_data.end()) {
		_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__, __LINE__, 
													 "before delete it, fd_out_data_iter->second.size() =", fd_out_data_iter->second->size());
		
		delete fd_out_data_iter->second;
		map_fd_out_data.erase(fd_out_data_iter);
		//_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "fd_out_data_iter->second.size() = 0, delete it");
	}

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(cfd);
	if (map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) {
		delete map_fd_nonblocking_ctl_iter ->second;
		map_fd_nonblocking_ctl.erase(map_fd_nonblocking_ctl_iter);
	}

	map_fd_pid_iter= map_fd_pid.find(cfd);
	if(map_fd_pid_iter != map_fd_pid.end()){

		pid = map_fd_pid_iter->second;
		map_pid_fd_out_iter= map_out_pid_fd .find(pid);
		map_pid_fd_in_iter=  map_in_pid_fd.find(pid);

		//清除所有跟這個peer相關的 previousParentPID
		for(unsigned long i=0 ; i<  _pk_mgr_ptr->sub_stream_num ; i++){
			if ((_pk_mgr_ptr->ssDetect_ptr +i) -> previousParentPID == pid) {
				(_pk_mgr_ptr->ssDetect_ptr +i) -> previousParentPID = PK_PID +1 ;
			}

		}
		//CLOSE CHILD
		if (map_pid_fd_out_iter != map_out_pid_fd.end()  && (map_pid_fd_out_iter ->second == cfd)){

			debug_printf("CLOSE CHILD \n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CLOSE CHILD");
			
			map_pid_fd_iter = map_out_pid_fd.find(pid);
			if(map_pid_fd_iter != map_out_pid_fd.end()) 
				map_out_pid_fd.erase(map_pid_fd_iter);
			
			// Temp child-peer
			for(pid_child_peer_info_iter =_pk_mgr_ptr-> map_pid_child_peer_info.begin();pid_child_peer_info_iter!= _pk_mgr_ptr-> map_pid_child_peer_info.end();pid_child_peer_info_iter++){
				if(pid_child_peer_info_iter->first ==pid){
					delete pid_child_peer_info_iter->second;
					_pk_mgr_ptr-> map_pid_child_peer_info.erase(pid_child_peer_info_iter);
					pid_child_peer_info_iter = _pk_mgr_ptr-> map_pid_child_peer_info.begin();
					if(pid_child_peer_info_iter == _pk_mgr_ptr-> map_pid_child_peer_info.end())
						break;
				}	
			}

			// Real child-peer
			map_pid_rescue_peer_info_iter =_pk_mgr_ptr ->map_pid_rescue_peer_info.find(pid);
			if( map_pid_rescue_peer_info_iter != _pk_mgr_ptr ->map_pid_rescue_peer_info.end() ){
				peerInfoPtr = map_pid_rescue_peer_info_iter ->second ;

				_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE CHILD","PID=",map_pid_rescue_peer_info_iter->first,"manifest",peerInfoPtr->manifest);

				for(substream_first_reply_peer_iter =substream_first_reply_peer.begin();substream_first_reply_peer_iter !=substream_first_reply_peer.end();substream_first_reply_peer_iter++){

					_log_ptr->write_log_format("s(u) s u s u s u s u s u s u\n", __FUNCTION__,__LINE__,
																					"CLOSE CHILD ALL session. Session_id =", substream_first_reply_peer_iter->first,
																					"session's pid =", substream_first_reply_peer_iter->second->pid,
																					"manifest", peerInfoPtr->manifest ,
																					"session rescue manifest", substream_first_reply_peer_iter ->second->rescue_manifest,
																					"substream_first_reply_peer_iter ->second ->connectTimeOutFlag", substream_first_reply_peer_iter ->second ->connectTimeOutFlag,
																					"ROLE =",substream_first_reply_peer_iter ->second ->peer_role);
					if(peerInfoPtr ->manifest == 0  && substream_first_reply_peer_iter ->second->peer_role ==1  && peerInfoPtr->pid ==substream_first_reply_peer_iter->second->pid ){
						
						_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE CHILD","session_id=",substream_first_reply_peer_iter ->first,"manifest",peerInfoPtr->manifest);
						_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__," peerInfoPtr->pid=", peerInfoPtr->pid,"substream_first_reply_peer_iter->second->pid",substream_first_reply_peer_iter->second->pid);

						_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"session_id_stop = ",substream_first_reply_peer_iter ->first);

						_peer_mgr_ptr->_peer_communication_ptr->stop_attempt_connect(substream_first_reply_peer_iter ->first);
						temp_substream_first_reply_peer_iter = substream_first_reply_peer_iter ;
						//substream_first_reply_peer_iter++;
						delete [] (unsigned char*)temp_substream_first_reply_peer_iter ->second;
						substream_first_reply_peer.erase(temp_substream_first_reply_peer_iter);
						substream_first_reply_peer_iter =substream_first_reply_peer.begin();
						if(substream_first_reply_peer_iter ==substream_first_reply_peer.end()){
							break;
						}
					}
				}
				delete peerInfoPtr;
				_pk_mgr_ptr ->map_pid_rescue_peer_info.erase(map_pid_rescue_peer_info_iter);
			}

			map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
			map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
			for(temp_map_pid_fd_out_iter = map_out_pid_fd.begin();temp_map_pid_fd_out_iter != map_out_pid_fd.end();temp_map_pid_fd_out_iter++){
				debug_printf("map_out_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_out_iter->first,temp_map_pid_fd_out_iter->second);
			}
			for(temp_map_pid_fd_in_iter = map_in_pid_fd.begin();temp_map_pid_fd_in_iter != map_in_pid_fd.end();temp_map_pid_fd_in_iter++){
				debug_printf("map_in_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_in_iter->first,temp_map_pid_fd_in_iter->second);
			}
		}
		//CLOSE  PARENT
		else if (map_pid_fd_in_iter != map_in_pid_fd.end() && map_pid_fd_in_iter->second == cfd ){

			debug_printf("CLOSE PARENT\n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CLOSE PARENT");

			// Temp parent-peer
			for(pid_peer_info_iter = _pk_mgr_ptr->map_pid_peer_info.begin();pid_peer_info_iter != _pk_mgr_ptr->map_pid_peer_info.end() ;pid_peer_info_iter++){
				//if(pid_peer_info_iter ->first ==cfd ){	// Commented on 20131222
				if(pid_peer_info_iter ->first ==pid ){
					delete pid_peer_info_iter->second;
					_pk_mgr_ptr->map_pid_peer_info.erase(pid_peer_info_iter);
					pid_peer_info_iter =_pk_mgr_ptr->map_pid_peer_info.begin();
					if(pid_peer_info_iter == _pk_mgr_ptr->map_pid_peer_info.end())
						break;
				}
			}

			// Real parent-peer
			pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(pid) ;
			if (pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end() ){

				peerDownInfoPtr = pid_peerDown_info_iter ->second ;

				//防止peer離開 狀態卡在testing
				if(peerDownInfoPtr->peerInfo.manifest & testingManifest){
					peerTestingManifest = peerDownInfoPtr->peerInfo.manifest & testingManifest;
					_pk_mgr_ptr->set_parent_manifest(peerDownInfoPtr, 0);
					for(unsigned long i=0 ; i<  _pk_mgr_ptr->sub_stream_num ; i++){
						if( peerTestingManifest & _pk_mgr_ptr->SubstreamIDToManifest(i)){
							 (_pk_mgr_ptr->ssDetect_ptr +i) ->isTesting =false ;
							 (_pk_mgr_ptr->ssDetect_ptr +i) ->previousParentPID = PK_PID +1 ;
							 //set recue stat 0
							 if(_pk_mgr_ptr->check_rescue_state(i,1)){
								 _pk_mgr_ptr->set_rescue_state(i,2);
								 _pk_mgr_ptr->set_rescue_state(i,0);
							 //TESTING
							 }else {
								 _logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL,_pk_mgr_ptr->SubstreamIDToManifest(i), PK_PID);
								 _logger_client_ptr->log_to_server(LOG_DATA_COME_PK,_pk_mgr_ptr->SubstreamIDToManifest(i));
								 _pk_mgr_ptr->set_parent_manifest(_pk_mgr_ptr->pkDownInfoPtr, _pk_mgr_ptr->pkDownInfoPtr->peerInfo.manifest | _pk_mgr_ptr->SubstreamIDToManifest(i));
								 _pk_mgr_ptr->set_rescue_state(i,0);
							 }
							 _pk_mgr_ptr->send_parentToPK(_pk_mgr_ptr->SubstreamIDToManifest(i),PK_PID +1);

						}
					}
				}
				_pk_mgr_ptr ->reSet_detectionInfo();

				_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE PARENT","PID=",pid_peerDown_info_iter->first,"manifest",peerDownInfoPtr->peerInfo.manifest);

				delete peerDownInfoPtr;
				_pk_mgr_ptr ->map_pid_peerDown_info.erase(pid_peerDown_info_iter);
			}

			if (map_fd_pid_iter != map_fd_pid.end()) {
				debug_printf("map_fd_pid_iter != map_fd_pid.end()\n");
			}
			else {
				debug_printf("map_fd_pid_iter == map_fd_pid.end()\n");
			}
			
			
			map_pid_fd_iter = map_in_pid_fd.find(pid);
			if (map_pid_fd_iter != map_in_pid_fd.end())  {
				map_in_pid_fd.erase(map_pid_fd_iter);
			}
			
			if (map_fd_pid_iter != map_fd_pid.end()) {
				debug_printf("map_fd_pid_iter != map_fd_pid.end()\n");
			}
			else {
				debug_printf("map_fd_pid_iter == map_fd_pid.end()\n");
			}
			
				
			map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
			map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
			for(temp_map_pid_fd_out_iter = map_out_pid_fd.begin();temp_map_pid_fd_out_iter != map_out_pid_fd.end();temp_map_pid_fd_out_iter++){
				debug_printf("map_out_pid_fd  pid : %d fd :%d\n",temp_map_pid_fd_out_iter->first,temp_map_pid_fd_out_iter->second);
				_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ",temp_map_pid_fd_out_iter->first," fd :",temp_map_pid_fd_out_iter->second);

			}
			for(temp_map_pid_fd_in_iter = map_in_pid_fd.begin();temp_map_pid_fd_in_iter != map_in_pid_fd.end();temp_map_pid_fd_in_iter++){
				debug_printf("map_in_pid_fd pid : %d fd : %d\n", temp_map_pid_fd_in_iter->first,temp_map_pid_fd_in_iter->second);
				_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ",temp_map_pid_fd_in_iter->first," fd :",temp_map_pid_fd_in_iter->second);
			}
			debug_printf("test1 \n");
		}
		else {
			_pk_mgr_ptr->exit_code = MACCESS_ERROR;
			debug_printf("peer:: not parent and not children\n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"peer:: not parent and not children");
			*(_net_ptr->_errorRestartFlag) =RESTART;
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "peer:: not parent and not children", __LINE__);
			_logger_client_ptr->log_exit();
			PAUSE
		}
		debug_printf("test2 \n");
		map_fd_pid.erase(map_fd_pid_iter);		// 這行(這附近)曾發生segmentation fault
		debug_printf("test3 \n");
	}
	else {
		_pk_mgr_ptr->exit_code = MACCESS_ERROR;
		debug_printf("peer:: CLOSE map_fd_pid not find why \n");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "peer:: CLOSE map_fd_pid not find why", __LINE__);
		_logger_client_ptr->log_exit();
		PAUSE
	}
}

