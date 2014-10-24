#include "peer.h"
#include "network.h"
#include "network_udp.h"
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

//清理所有的 map_fd_out_ctrl queue_out_ctrl_ptr 的ptr
void peer::clear_map()
{
	map<int, queue<struct chunk_t *> *>::iterator iter;	
	map<int, Nonblocking_Buff *>::iterator iter2;
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
	
	for (iter2 = map_fd_nonblocking_ctl.begin(); iter2 != map_fd_nonblocking_ctl.end(); iter2++) {
		delete iter2->second;
	}
	map_fd_nonblocking_ctl.clear();
	
	for(substream_first_reply_peer_iter =substream_first_reply_peer.begin() ;substream_first_reply_peer_iter !=substream_first_reply_peer.end() ;substream_first_reply_peer_iter ++){
		delete substream_first_reply_peer_iter->second;
	}
	substream_first_reply_peer.clear();
}

void peer::peer_set(network *net_ptr , network_udp *net_udp_ptr, logger *log_ptr , configuration *prep, pk_mgr *pk_mgr_ptr, peer_mgr *peer_mgr_ptr, logger_client * logger_client_ptr)
{
	_net_ptr = net_ptr;
	_net_udp_ptr = net_udp_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_logger_client_ptr = logger_client_ptr;
	_net_ptr->peer_set(this);
	_net_ptr->log_set(_log_ptr);
	_net_udp_ptr->peer_set(this);
	_net_udp_ptr->log_set(_log_ptr);
}

// Called by parent-peer
//new queue    只有收到CHNK_CMD_PEER_CON才會呼叫
//收到一個建立連線的要求(需down stream)
void peer::handle_connect(int sock, struct chunk_t *chunk_ptr, struct sockaddr_in cin)
{
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	struct peer_info_t *rescue_peer = NULL;
//	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, struct peer_info_t *>::iterator map_pid_child1_iter;
	chunk_request_ptr = (struct chunk_request_msg_t *)chunk_ptr;

	map_pid_child1_iter = _pk_mgr_ptr->map_pid_child.find(chunk_request_ptr->info.pid);
	if (map_pid_child1_iter != _pk_mgr_ptr->map_pid_child.end()) {

		debug_printf("fd error why dup\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] fd error why dup");
		//debug_printf("rescue_peer->pid = %d  socket = %d\n",chunk_request_ptr->info.pid,sock);
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "rescue_peer->pid =", chunk_request_ptr->info.pid, "manifest :", map_pid_child1_iter->second->manifest);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u \n", "rescue_peer->pid =", chunk_request_ptr->info.pid, "manifest :", map_pid_child1_iter->second->manifest, __LINE__);
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "rescue_peer->pid =", map_pid_child1_iter->second->pid, "manifest :", map_pid_child1_iter->second->manifest);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u \n", "rescue_peer->pid =", map_pid_child1_iter->second->pid, "manifest :", map_pid_child1_iter->second->manifest, __LINE__);
		
		//debug_printf("old manifest = %d \n",map_pid_child1_iter ->second->manifest);
		//old socket error and no stream close old
		if (map_pid_child1_iter->second->manifest == 0) {
			//data_close(map_out_pid_fd[map_pid_child1_iter->first], "close_by peer com old socket", CLOSE_CHILD);
			CloseChild(map_pid_child1_iter->first, true, "close_by peer com old socket");
			//close new socket
		}
		else {
			//data_close(sock, "close_by peer com new socket", CLOSE_CHILD);
			CloseChild(map_pid_child1_iter->first, true, "close_by peer com new socket");
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
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::handle_connect  new error", __FUNCTION__, __LINE__);
	}

	memset(Nonblocking_Buff_ptr, 0, sizeof(Nonblocking_Buff));

	map_pid_child1_iter = _pk_mgr_ptr->map_pid_child.find(chunk_request_ptr->info.pid);
	if (map_pid_child1_iter == _pk_mgr_ptr->map_pid_child.end()) {
		// 進到這個情況可能有問題，先暫時放著 20140930
		rescue_peer = new struct peer_info_t;
		if (!rescue_peer) {
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::handle_connect  new error", __FUNCTION__, __LINE__);
		}
		memset(rescue_peer, 0, sizeof(struct peer_info_t));
		
		rescue_peer->pid = chunk_request_ptr->info.pid;
		rescue_peer->public_ip = cin.sin_addr.s_addr;
		rescue_peer->private_ip = chunk_request_ptr->info.private_ip;
		rescue_peer->tcp_port = chunk_request_ptr->info.tcp_port;
		rescue_peer->udp_port = chunk_request_ptr->info.public_udp_port;		// 先暫時使用 external port 
		_pk_mgr_ptr->map_pid_child[rescue_peer->pid] = rescue_peer;
		//debug_printf("rescue_peer->pid = %d  socket=%d\n",rescue_peer->pid,sock);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "rescue_peer->pid =", rescue_peer->pid);
	}
	else {
		debug_printf("rescue_peer->pid = %d  socket = %d\n",chunk_request_ptr->info.pid,sock);
		//data_close(sock,"close_by peer com", CLOSE_CHILD);
		CloseChild(chunk_request_ptr->info.pid, true, "close_by peer com");
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] fd error why dup", __FUNCTION__, __LINE__);
		
		map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
		map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
		for(temp_map_pid_fd_out_iter = map_out_pid_fd.begin();temp_map_pid_fd_out_iter != map_out_pid_fd.end();temp_map_pid_fd_out_iter++){
			debug_printf("map_out_pid_fd  pid : %u fd :%d\n",temp_map_pid_fd_out_iter->first,temp_map_pid_fd_out_iter->second);
			_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ",temp_map_pid_fd_out_iter->first," fd :",temp_map_pid_fd_out_iter->second);

		}
		for(temp_map_pid_fd_in_iter = map_in_pid_fd.begin();temp_map_pid_fd_in_iter != map_in_pid_fd.end();temp_map_pid_fd_in_iter++){
			debug_printf("map_in_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_in_iter->first,temp_map_pid_fd_in_iter->second);
			_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ",temp_map_pid_fd_in_iter->first," fd :",temp_map_pid_fd_in_iter->second);

		}

		//PAUSE
	}
}

// Called by parent-peer. Receive CHNK_CMD_PEER_CON
//new queue    只有收到CHNK_CMD_PEER_CON才會呼叫
//收到一個建立連線的要求(需down stream)
void peer::handle_connect_udp(int sock, struct chunk_t *chunk_ptr, struct sockaddr_in cin)
{
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	struct peer_info_t *rescue_peer = NULL;
	//	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, struct peer_info_t *>::iterator map_pid_child_iter;
	chunk_request_ptr = (struct chunk_request_msg_t *)chunk_ptr;

	/*
	map_pid_child_iter = _pk_mgr_ptr->map_pid_child.find(chunk_request_ptr->info.pid);
	if (map_pid_child_iter == _pk_mgr_ptr->map_pid_child.end()) {
	_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Not found pid", chunk_request_ptr->info.pid, "in map_pid_child");
	_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_pid_child", __FUNCTION__, __LINE__);
	}
	else if (map_pid_child_iter->second->state == PEER_CONNECTED) {
	_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Child", chunk_request_ptr->info.pid, "state shouldn't be PEER_CONNECTED");
	_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Child state shouldn't be PEER_CONNECTED", __FUNCTION__, __LINE__);
	}
	*/

	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;
	Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff;

	if (!Nonblocking_Buff_ptr || !queue_out_ctrl_ptr || !queue_out_data_ptr) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::handle_connect  new error", __FUNCTION__, __LINE__);
	}

	map_udpfd_nonblocking_ctl[sock] = Nonblocking_Buff_ptr;
	map_out_pid_udpfd[chunk_request_ptr->info.pid] = sock;
	map_udpfd_pid[sock] = chunk_request_ptr->info.pid;
	map_udpfd_out_ctrl[sock] = queue_out_ctrl_ptr;
	map_udpfd_out_data[sock] = queue_out_data_ptr;

	memset(Nonblocking_Buff_ptr, 0, sizeof(Nonblocking_Buff));

	if (_pk_mgr_ptr->map_pid_child.find(chunk_request_ptr->info.pid) == _pk_mgr_ptr->map_pid_child.end()) {
		/*
		rescue_peer = new struct peer_info_t;
		if (!rescue_peer) {
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::handle_connect  new error", __FUNCTION__, __LINE__);
		}
		memset(rescue_peer, 0, sizeof(struct peer_info_t));

		rescue_peer->pid = chunk_request_ptr->info.pid;
		rescue_peer->public_ip = cin.sin_addr.s_addr;
		rescue_peer->private_ip = chunk_request_ptr->info.private_ip;
		rescue_peer->tcp_port = chunk_request_ptr->info.tcp_port;
		rescue_peer->udp_port = chunk_request_ptr->info.udp_port;
		_pk_mgr_ptr->map_pid_child[rescue_peer->pid] = rescue_peer;
		//debug_printf("rescue_peer->pid = %d  socket=%d\n",rescue_peer->pid,sock);
		*/
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] Why cannot find this pid in map_pid_child", chunk_request_ptr->info.pid);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u \n", "my pid", _pk_mgr_ptr->my_pid, "[DEBUG] Why cannot find this pid in map_pid_child", chunk_request_ptr->info.pid);
	}
	else {

	}
}

// Called by children-peer. Send 送CHNK_CMD_PEER_CON
int peer::handle_connect_request(int sock, struct level_info_t *level_info_ptr, unsigned long pid,Nonblocking_Ctl *Nonblocking_Send_Ctrl_ptr)
{
	debug_printf("peer::handle_connect_request \n");
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);

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
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::handle_connect  new error", __FUNCTION__, __LINE__);
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
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::chunk_request_ptr  new error", __FUNCTION__, __LINE__);
		}
		memset(chunk_request_ptr, 0, sizeof(struct chunk_request_msg_t));

		chunk_request_ptr->header.cmd = CHNK_CMD_PEER_CON;
		chunk_request_ptr->header.rsv_1 = REQUEST;
		chunk_request_ptr->header.length = sizeof(struct chunk_request_msg_t) - sizeof(struct chunk_header_t);
		chunk_request_ptr->info.pid = pid;
		chunk_request_ptr->info.channel_id = channel_id;
		//chunk_request_ptr->info.private_ip = _net_ptr->getLocalIpv4();
		chunk_request_ptr->info.private_ip = _pk_mgr_ptr->my_private_ip;
		chunk_request_ptr->info.tcp_port = (unsigned short)atoi(svc_tcp_port.c_str());
		chunk_request_ptr->info.public_udp_port = (unsigned short)atoi(svc_udp_port.c_str());		// 先暫時隨便給個值
		chunk_request_ptr->info.private_udp_port = (unsigned short)atoi(svc_udp_port.c_str());		// 先暫時隨便給個值

		_net_ptr->set_nonblocking(sock);


		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = sizeof(struct chunk_request_msg_t);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = sizeof(struct chunk_request_msg_t);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = chunk_request_ptr->header.sequence_number;

		//debug_printf("header.cmd = %d, total_len = %d \n", chunk_request_ptr->header.cmd, chunk_request_ptr->header.length);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "chunk_request_ptr->info.pid =", pid);

		
		_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
		//_net_ptr->set_blocking(sock);
		//_send_byte = send(sock, (char *)chunk_request_ptr, sizeof(struct role_struct), 0);
		//_net_ptr->set_nonblocking(sock);
		
		if (_send_byte < 0) {
			if (chunk_request_ptr) {
				delete chunk_request_ptr;
			}
			//if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
			//	delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			//}
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
			//data_close(sock, "PEER　COM error", CLOSE_PARENT);
			CloseParent(pid, true, "PEER　COM error");
			
			return RET_SOCK_ERROR;

		}
		else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ) {
			debug_printf("111 \n");
			if (chunk_request_ptr) {
				debug_printf("222 \n");
				delete chunk_request_ptr;
			}
			debug_printf("333 \n");
			//if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
			//	debug_printf("444 \n");
			//	delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			//}
			debug_printf("555 \n");
			
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
			//data_close(sock, "PEER　COM error",CLOSE_PARENT);
			CloseParent(pid, true, "PEER　COM error");
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


// Called by children-peer. Send CHNK_CMD_PEER_CON
int peer::handle_connect_request_udp(int sock, struct level_info_t *level_info_ptr, unsigned long my_pid, Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr)
{
	unsigned long parent_pid = level_info_ptr->pid;

	debug_printf("sock = %d  parent pid = %d  my pid = %d \n", sock, parent_pid, my_pid);
	_log_ptr->write_log_format("s(u) d d d \n", __FUNCTION__, __LINE__, sock, parent_pid, my_pid);

	if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY) {

		map<unsigned long, int>::iterator map_pid_udpfd_iter;
		map_pid_udpfd_iter = map_in_pid_udpfd.find(parent_pid);
		if (map_pid_udpfd_iter == map_in_pid_udpfd.end()) {
			map_in_pid_udpfd[parent_pid] = sock;
		}
		
		queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
		queue_out_data_ptr = new std::queue<struct chunk_t *>;
		Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff;
		if (!Nonblocking_Buff_ptr || !queue_out_ctrl_ptr || !queue_out_data_ptr) {
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::handle_connect  new error", __FUNCTION__, __LINE__);
		}
		memset(Nonblocking_Buff_ptr, 0, sizeof(Nonblocking_Buff));
		
		map_udpfd_nonblocking_ctl[sock] = Nonblocking_Buff_ptr ;
		map_udpfd_pid[sock] = parent_pid;
		map_udpfd_out_ctrl[sock] = queue_out_ctrl_ptr;
		map_udpfd_out_data[sock] = queue_out_data_ptr;

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
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::chunk_request_ptr  new error", __FUNCTION__, __LINE__);
		}
		memset(chunk_request_ptr, 0, sizeof(struct chunk_request_msg_t));

		chunk_request_ptr->header.cmd = CHNK_CMD_PEER_CON;
		chunk_request_ptr->header.rsv_1 = REQUEST;
		chunk_request_ptr->header.length = sizeof(struct chunk_request_msg_t) - sizeof(struct chunk_header_t);
		chunk_request_ptr->info.pid = my_pid;
		chunk_request_ptr->info.channel_id = channel_id;
		chunk_request_ptr->info.private_ip = _pk_mgr_ptr->my_private_ip;
		chunk_request_ptr->info.tcp_port = (unsigned short)atoi(svc_tcp_port.c_str());
		chunk_request_ptr->info.public_udp_port = (unsigned short)atoi(svc_udp_port.c_str());		// 先暫時隨便給個值
		chunk_request_ptr->info.private_udp_port = (unsigned short)atoi(svc_udp_port.c_str());		// 先暫時隨便給個值

		_net_udp_ptr->set_nonblocking(sock);


		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = sizeof(struct chunk_request_msg_t);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = sizeof(struct chunk_request_msg_t);
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_request_ptr;
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = chunk_request_ptr->header.sequence_number;

		//debug_printf("header.cmd = %d, total_len = %d \n", chunk_request_ptr->header.cmd, chunk_request_ptr->header.length);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "chunk_request_ptr->info.pid =", my_pid);

		
		_send_byte = _net_udp_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
		
		if (_send_byte < 0) {
			if (chunk_request_ptr) {
				delete chunk_request_ptr;
			}
			//if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
			//	delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			//}
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
			//data_close(sock, "PEER　COM error", CLOSE_PARENT);
			CloseParent(parent_pid, true, "PEER　COM error");
			
			return RET_SOCK_ERROR;
		}
		else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ) {
			debug_printf("111 \n");
			if (chunk_request_ptr) {
				debug_printf("222 \n");
				delete chunk_request_ptr;
			}
			debug_printf("333 \n");
			//if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
			//	debug_printf("444 \n");
			//	delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			//}
			debug_printf("555 \n");
			
			Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
			return 0;
		}
	}
	else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING ) {
		_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));

		if (_send_byte < 0) {
			if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
				delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
			}
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
			//data_close(sock, "PEER　COM error",CLOSE_PARENT);
			CloseParent(parent_pid, true, "PEER　COM error");
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
	unsigned long pid = 0;
	int offset = 0;
	int recv_byte = 0;
	Nonblocking_Ctl * Nonblocking_Recv_ptr = NULL;
	struct chunk_header_t* chunk_header_ptr = NULL;
	struct chunk_t* chunk_ptr = NULL;
	unsigned long buf_len = 0;
	
	if (map_fd_pid.find(sock) != map_fd_pid.end()) {
		pid = map_fd_pid[sock];
	}
	else {
		debug_printf("[ERROR] cannot find this fd in map_fd_pid \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] cannot find this fd in map_fd_pid");
		PAUSE
	}
	
	if (map_fd_nonblocking_ctl.find(sock) != map_fd_nonblocking_ctl.end()) {
		Nonblocking_Recv_ptr = &(map_fd_nonblocking_ctl[sock]->nonBlockingRecv);
		if (Nonblocking_Recv_ptr ->recv_packet_state == 0) {
			Nonblocking_Recv_ptr ->recv_packet_state = READ_HEADER_READY ;
		}
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Nonblocking_Buff_ptr NOT FIND", __FUNCTION__, __LINE__);
		return RET_SOCK_ERROR;
	}
	
	for (int i = 0; i < 5; i++) {
		if (Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_READY) {
			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			if (!chunk_header_ptr) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::chunk_header_ptr  new error", __FUNCTION__, __LINE__);
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
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::chunk_ptr  new error", __FUNCTION__, __LINE__);
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
			//data_close(sock, "[DEBUG] error occured in peer recv ",DONT_CARE);
			CloseParent(pid, false, "[DEBUG] error occured in peer recv ");
			CloseChild(pid, false, "[DEBUG] error occured in peer recv ");
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
		//_pk_mgr_ptr->handle_stream(chunk_ptr, sock);
		_pk_mgr_ptr->HandleStream(chunk_ptr, sock);
		
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
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] fd not here", __FUNCTION__, __LINE__);
				return RET_OK;
			}

			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_TEST_DELAY REQUEST");

			chunk_ptr->header.rsv_1 = REPLY;

			queue_out_ctrl_ptr->push((struct chunk_t *)chunk_ptr);

			if (queue_out_ctrl_ptr->size() != 0 ) {
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
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

			if (substream_first_reply_peer_iter->second->firstReplyFlag == FALSE) {
				map_fd_pid_iter = map_fd_pid.find(sock);
				if (map_fd_pid_iter != map_fd_pid.end()) {
					firstReplyPid = map_fd_pid_iter->second;

					//在這裡面sequence_number 裡面塞的是manifest
					debug_printf("first_reply_peer=%d  manifest =%d \n",firstReplyPid,replyManifest);
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "first_reply_peer =", firstReplyPid, "manifest =", replyManifest);

					pid_peer_info_iter = _pk_mgr_ptr ->map_pid_parent_temp.find(firstReplyPid);
					if (pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end()) {

						for (unsigned long i = 0; i < _pk_mgr_ptr->map_pid_parent_temp.count(firstReplyPid); i++) {
							peerInfoPtr = pid_peer_info_iter->second;
							if (pid_peer_info_iter->second->manifest == replyManifest) {
								_pk_mgr_ptr->map_pid_parent_temp.erase(pid_peer_info_iter);

								pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_parent.find(firstReplyPid);
								if (pid_peerDown_info_iter == _pk_mgr_ptr ->map_pid_parent.end()) {

									peerDownInfoPtr = new struct peer_connect_down_t ;
									if (!peerDownInfoPtr) {
										_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::peerDownInfoPtr  new error", __FUNCTION__, __LINE__);
									}

									memset(peerDownInfoPtr, 0, sizeof(struct peer_connect_down_t));
									memcpy(peerDownInfoPtr, peerInfoPtr, sizeof(struct peer_info_t));
									
									_pk_mgr_ptr->map_pid_parent[firstReplyPid] = peerDownInfoPtr;
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
						if (replyManifest == _pk_mgr_ptr->full_manifest && _pk_mgr_ptr ->map_pid_parent.size() == 2) {
							for (unsigned long substreamID = 0; substreamID < _pk_mgr_ptr->sub_stream_num; substreamID++) {
								// send topology to PK
								//_pk_mgr_ptr->send_parentToPK(_pk_mgr_ptr->SubstreamIDToManifest(substreamID), PK_PID+1);
								
							}
							_pk_mgr_ptr->pkSendCapacity = true;
							//_pk_mgr_ptr->send_capacity_to_pk(_pk_mgr_ptr->_sock);
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
						_log_ptr->write_log_format("s(u) d d \n", __FUNCTION__, __LINE__, replyManifest, _pk_mgr_ptr->sub_stream_num);

						UINT32 temp_manifest = replyManifest;
						for (unsigned long i = 0; i < _pk_mgr_ptr->sub_stream_num; i++) {
							if ((1 << i) & temp_manifest) {
								_pk_mgr_ptr->SetSubstreamState(i, SS_TEST);
								struct timerStruct timer;
								_log_ptr->timerGet(&timer);
								memcpy(&(_pk_mgr_ptr->ss_table[i]->parent_changed_time), &timer, sizeof(timer));
								
							}
						}
						
						
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

						
						for (pid_peer_info_iter = _pk_mgr_ptr ->map_pid_parent_temp.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end(); pid_peer_info_iter++) {
							debug_printf("_pk_mgr_ptr ->map_pid_parent_temp  pid : %d \n", pid_peer_info_iter->first);
							_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"_pk_mgr_ptr ->map_pid_parent_temp  pid : ", pid_peer_info_iter->first);
						}
						debug_printf("_pk_mgr_ptr ->map_pid_parent_temp.size(): %d \n", _pk_mgr_ptr ->map_pid_parent_temp.size());
						
						for (pid_peer_info_iter = _pk_mgr_ptr ->map_pid_parent_temp.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end(); ) {
							debug_printf("_pk_mgr_ptr ->map_pid_parent_temp.size(): %d \n", _pk_mgr_ptr ->map_pid_parent_temp.size());
							debug_printf("_pk_mgr_ptr ->map_pid_parent_temp  pid : %d \n", pid_peer_info_iter->first);
							peerInfoPtr = pid_peer_info_iter->second;
							_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__,
																			"pid =", peerInfoPtr->pid,
																			"manifest =", peerInfoPtr->manifest,
																			"replyManifest =", replyManifest,
																			"count =", _pk_mgr_ptr ->map_pid_parent_temp.count(peerInfoPtr->pid));
							if (peerInfoPtr->manifest == replyManifest) {
								_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
								//若是自己或是先前已經建立過連線的parent 則不close (會關到正常連線)  
								if (pid_peer_info_iter ->first == _peer_mgr_ptr->self_pid) {
									_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
									pid_peer_info_iter++;
									continue;
								} else if (_pk_mgr_ptr->map_pid_parent.find(pid_peer_info_iter ->first) != _pk_mgr_ptr->map_pid_parent.end()) {
									_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
									pid_peer_info_iter++;
									continue;
								}
								_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
								if (map_in_pid_fd.find( peerInfoPtr->pid ) != map_in_pid_fd.end()) {
									_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
									//只剩最後一個才關socket
									if(_pk_mgr_ptr ->map_pid_parent_temp.count(peerInfoPtr->pid) == 1){
										_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
										//data_close(map_in_pid_fd[peerInfoPtr->pid ],"close by firstReplyPid ",CLOSE_PARENT);
										CloseParent(peerInfoPtr->pid, true, "close by firstReplyPid ");
										pid_peer_info_iter = _pk_mgr_ptr ->map_pid_parent_temp.begin();
										continue;	// 為了不要在刪除iter後還跑到650行的iter++ 因此加上continue
									}else{
										_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
									//do nothing 
									}
									_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
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
						for (pid_peer_info_iter = _pk_mgr_ptr ->map_pid_parent_temp.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end(); pid_peer_info_iter++) {
							debug_printf("_pk_mgr_ptr ->map_pid_parent_temp.size(): %d \n", _pk_mgr_ptr ->map_pid_parent_temp.size());
							debug_printf("_pk_mgr_ptr ->map_pid_parent_temp  pid : %d \n", pid_peer_info_iter->first);
							peerInfoPtr = pid_peer_info_iter->second;

							if (peerInfoPtr->manifest == replyManifest) {
								//若是自己或是先前已經建立過連線的parent 則不close (會關到正常連線)  
								if (pid_peer_info_iter ->first == _peer_mgr_ptr->self_pid) {
									continue;
								} else if (_pk_mgr_ptr->map_pid_parent.find(pid_peer_info_iter ->first) != _pk_mgr_ptr->map_pid_parent.end()) {
									continue;
								}

								if (map_in_pid_fd.find( peerInfoPtr->pid ) != map_in_pid_fd.end()) {
									//只剩最後一個才關socket
									if(_pk_mgr_ptr ->map_pid_parent_temp.count(peerInfoPtr->pid) == 1){
										
										data_close(map_in_pid_fd[peerInfoPtr->pid ],"close by firstReplyPid ",CLOSE_PARENT);
										pid_peer_info_iter = _pk_mgr_ptr ->map_pid_parent_temp.begin();
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
						_pk_mgr_ptr ->clear_map_pid_parent_temp(replyManifest);
					}
					else {
						_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] pid_peer_info_iter not found", __FUNCTION__, __LINE__);
					}
				}

				substream_first_reply_peer_iter->second->firstReplyFlag = TRUE;

				delete [] (unsigned char*) substream_first_reply_peer_iter->second;
				substream_first_reply_peer.erase(substream_first_reply_peer_iter);
			}
		}
		
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST) {
		_log_ptr->write_log_format("s(u) s\n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SET_MANIFEST");

		_peer_mgr_ptr ->handle_manifestSet((struct chunk_manifest_set_t *)chunk_ptr); 
		
		if (((struct chunk_manifest_set_t *)chunk_ptr)->manifest == 0) {
			//data_close(sock, "close by Children SET Manifest = 0", CLOSE_PARENT);
			CloseParent(pid, true, "close by Children SET Manifest = 0");
		}
		_pk_mgr_ptr->pkSendCapacity = true;
		//_pk_mgr_ptr->send_capacity_to_pk(_pk_mgr_ptr->_sock);
	} 
	else {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Unknown header.cmd", __FUNCTION__, __LINE__);
	}

	if (chunk_ptr) {
		delete [] (unsigned char*)chunk_ptr;
	}

	return RET_OK;
}


// CHNK_CMD_PEER_DATA
// CHNK_CMD_PARENT_PEER
// CHNK_CMD_PEER_TEST_DELAY
// CHNK_CMD_PEER_SET_MANIFEST
int peer::handle_pkt_in_udp(int sock)
{
	bool isUDP = false;
	unsigned long pid;
	int offset = 0;
	int recv_byte = 0;
	Nonblocking_Ctl * Nonblocking_Recv_ptr = NULL;
	struct chunk_header_t* chunk_header_ptr = NULL;
	struct chunk_t* chunk_ptr = NULL;
	unsigned long buf_len = 0;
	
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);

	if (map_udpfd_pid.find(sock) != map_udpfd_pid.end()) {
		isUDP = true;
		pid = map_udpfd_pid[sock];
	}
	else {
		// 關閉socket後還是可能會收到pkt??
		//debug_printf("[ERROR] cannot find this fd %d in map_fd_pid \n", sock);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] cannot find this fd in map_fd_pid", sock);
		//PAUSE
		return RET_OK;
	}
	
	
	if (map_udpfd_nonblocking_ctl.find(sock) != map_udpfd_nonblocking_ctl.end()) {
		Nonblocking_Recv_ptr = &(map_udpfd_nonblocking_ctl[sock]->nonBlockingRecv);
		if (Nonblocking_Recv_ptr ->recv_packet_state == 0) {
			Nonblocking_Recv_ptr ->recv_packet_state = READ_HEADER_READY ;
		}
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Nonblocking_Buff_ptr NOT FIND", __FUNCTION__, __LINE__);
		return RET_SOCK_ERROR;
	}
	
	for (int i = 0; i < 5; i++) {
		if (Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_READY) {
			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			if (!chunk_header_ptr) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::chunk_header_ptr  new error", __FUNCTION__, __LINE__);
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
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::chunk_ptr  new error", __FUNCTION__, __LINE__);
			}

			memset(chunk_ptr, 0, buf_len);
			memcpy(chunk_ptr, Nonblocking_Recv_ptr->recv_ctl_info.buffer, sizeof(chunk_header_t));

			_log_ptr->write_log_format("s =>u s u s u s u \n", __FUNCTION__, __LINE__,
				"PEER RECV DATA READ_HEADER_OK cmd =", chunk_ptr->header.cmd,
				"total_len =", Nonblocking_Recv_ptr->recv_ctl_info.total_len,
				"expect_len", Nonblocking_Recv_ptr->recv_ctl_info.expect_len);


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
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "break");
			break;
		}

		recv_byte =_net_udp_ptr->nonblock_recv(sock,Nonblocking_Recv_ptr);
		_log_ptr->write_log_format("s(u) s d s d(d)\n", __FUNCTION__, __LINE__, "recv", recv_byte, "bytes from", sock, UDT::getsockstate(sock));
		if (recv_byte < 0) {
			// 如果對方不正常斷線，一樣會在過一段時間後 recv 的時發現錯誤
			CloseParent(pid, false, "[DEBUG] error occured in peer recv ");
			CloseChild(pid, false, "[DEBUG] error occured in peer recv ");
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
		//_pk_mgr_ptr->handle_stream(chunk_ptr, sock);
		_pk_mgr_ptr->HandleStream(chunk_ptr, sock);
		
		return RET_OK;
	}
	//	just send return
	else if (chunk_ptr->header.cmd == CHNK_CMD_PARENT_PEER) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "CHNK_CMD_PARENT_PEER");
		_logger_client_ptr->log_exit();
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_TEST_DELAY ) {
		// CHNK_CMD_PEER_TEST_DELAY: 透過送出一個大封包來找出最適合的 parent。
		// 因為目前是"以最快建好連線的 parent 視為最適合的 parent"，所以 CHNK_CMD_PEER_TEST_DELAY 只會送給一個 parent
		if (chunk_ptr->header.rsv_1 == REQ) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_TEST_DELAY  REQ");
			queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
				
			if (map_udpfd_out_ctrl.find(sock) ==  map_udpfd_out_ctrl.end()) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Not found in map_udpfd_out_ctrl");
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_out_ctrl", __FUNCTION__, __LINE__);
				return RET_OK;
			}
			queue_out_ctrl_ptr = map_udpfd_out_ctrl[sock];
			chunk_ptr->header.rsv_1 = REQ_ACK;
			((struct chunk_delay_test_t *)chunk_ptr)->pid = _peer_mgr_ptr->self_pid;
			queue_out_ctrl_ptr->push((struct chunk_t *)chunk_ptr);

			return RET_OK;
		}
		else if (chunk_ptr->header.rsv_1 == ACK) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_TEST_DELAY  ACK");

			unsigned long pid = ((struct chunk_delay_test_t *)chunk_ptr)->pid;
			unsigned long manifest = chunk_ptr->header.sequence_number;
			map<unsigned long, struct manifest_timmer_flag *>::iterator substream_first_reply_peer_iter;
			
			
			
			
			if (_pk_mgr_ptr->map_pid_child.find(pid) == _pk_mgr_ptr->map_pid_child.end()) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Not found in map_pid_child");
				//_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_pid_child", __FUNCTION__, __LINE__);
				return RET_OK;
			}
			
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "size() =", substream_first_reply_peer.size());

			for (substream_first_reply_peer_iter = substream_first_reply_peer.begin(); substream_first_reply_peer_iter != substream_first_reply_peer.end(); substream_first_reply_peer_iter++) {
				// Parent 的 session 只會有一個 peer，理論上 manifest 跟 pid 有對上的話就可以反推出是哪個 parent 的 session_id
				// p.s 這裡的做法以後要改，child 和 parent 各自的 session_id 要知道
				_log_ptr->write_log_format("s(u) d d  d d \n", __FUNCTION__, __LINE__, substream_first_reply_peer_iter->second->rescue_manifest, manifest, substream_first_reply_peer_iter->second->child_pid, pid);
				if (substream_first_reply_peer_iter->second->rescue_manifest == manifest && substream_first_reply_peer_iter->second->child_pid == pid) {
					substream_first_reply_peer_iter->second->session_state = FIRST_REPLY_OK;
				}
			}

			_pk_mgr_ptr->map_pid_child.find(pid)->second->state = PEER_CONNECTED_CHILD;
			
			return RET_OK;
		}
		else if (chunk_ptr->header.rsv_1 == REQ_ACK) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_TEST_DELAY  REQ_ACK");

			unsigned long replyManifest = chunk_ptr->header.sequence_number;
			INT32 session_id = ((struct chunk_delay_test_t *)chunk_ptr)->session_id;
			UINT32 firstReplyPid = ((struct chunk_delay_test_t *)chunk_ptr)->pid;
			map<unsigned long, struct manifest_timmer_flag *>::iterator substream_first_reply_peer_iter;
			multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
			struct peer_info_t *peerInfoPtr;
			struct peer_connect_down_t *peerDownInfoPtr;
				
			// If cannot find session_id, the first-reply-peer has appeared
			substream_first_reply_peer_iter = substream_first_reply_peer.find(session_id);
			if (substream_first_reply_peer_iter == substream_first_reply_peer.end()) {
				_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
																"session_id", session_id,
																"pid", ((struct chunk_delay_test_t *)chunk_ptr)->pid,
																"manifest", replyManifest);
				//_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found session in substream_first_reply_peer", __FUNCTION__, __LINE__);
				if (chunk_ptr) {
					delete [] (unsigned char*)chunk_ptr;
				}
				return RET_OK;
			}
			
			_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
				"firstReplyPid", firstReplyPid,
				"session_id", session_id,
				"manifest", replyManifest);


			if (substream_first_reply_peer_iter->second->session_state == FIRST_REPLY_OK) {
				// This session has got the first reply peer
				return RET_OK;
			}
			
			
				
			substream_first_reply_peer_iter->second->selected_pid = firstReplyPid;
			if (_pk_mgr_ptr->map_pid_parent.find(firstReplyPid) != _pk_mgr_ptr->map_pid_parent.end()) {
				_pk_mgr_ptr->map_pid_parent[firstReplyPid]->peerInfo.state = PEER_CONNECTED_PARENT;
			}
			else {
				// 曾經發生 20140815
				//_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_pid_parent", __FUNCTION__, __LINE__);
				// 直接忽略它
				debug_printf("[DEBUG] Not found pid %d in map_pid_parent", firstReplyPid);
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found pid", firstReplyPid, "in map_pid_parent");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found pid", firstReplyPid, "in map_pid_parent");
				return RET_OK;
			}
			

			
			// Remove parent_temp map
			//delete pid_peer_info_iter->second;
			//_pk_mgr_ptr->map_pid_parent_temp.erase(pid_peer_info_iter);
				
			
				
			// Set the substream to TEST state, and send the manifest to the peer
			UINT32 temp_manifest = replyManifest;
			for (unsigned long i = 0; i < _pk_mgr_ptr->sub_stream_num; i++) {
				if ((1 << i) & temp_manifest) {
					_pk_mgr_ptr->SetSubstreamState(i, SS_TEST);
					struct timerStruct timer;
					_log_ptr->timerGet(&timer);
					memcpy(&(_pk_mgr_ptr->ss_table[i]->parent_changed_time), &timer, sizeof(timer));
				}
			}

			// Send manifest to the parent
			if (_pk_mgr_ptr->map_pid_parent.find(firstReplyPid) == _pk_mgr_ptr->map_pid_parent.end()) {
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_pid_parent", __FUNCTION__, __LINE__);
			}
			peerDownInfoPtr = _pk_mgr_ptr->map_pid_parent[firstReplyPid];
			_peer_mgr_ptr -> send_manifest_to_parent(peerDownInfoPtr ->peerInfo.manifest ,firstReplyPid);
			//_pk_mgr_ptr->reSet_detectionInfo();
			_pk_mgr_ptr->ResetDetection();

			// Log to server
			if (replyManifest == _pk_mgr_ptr->full_manifest) {
				_logger_client_ptr->log_to_server(LOG_REG_LIST_TESTING, replyManifest, firstReplyPid);
			}
			else {
				_logger_client_ptr->log_to_server(LOG_RESCUE_TESTING, replyManifest, firstReplyPid);
			}

			for (pid_peer_info_iter = _pk_mgr_ptr ->map_pid_parent_temp.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end(); pid_peer_info_iter++) {
				//debug_printf("map_pid_parent_temp size %d pid %d \n", _pk_mgr_ptr->map_pid_parent_temp.size(), pid_peer_info_iter->first);
				_log_ptr->write_log_format("s(u) s d s u \n", __FUNCTION__, __LINE__,
															"map_pid_parent_temp size", _pk_mgr_ptr->map_pid_parent_temp.size(),
															"pid", pid_peer_info_iter->first);
			}
			
			substream_first_reply_peer_iter->second->session_state = FIRST_REPLY_OK;
			

			// Ack to the parent to inform that it is my seleceted parent
			queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
			if (map_udpfd_out_ctrl.find(sock) == map_udpfd_out_ctrl.end()) {
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] fd not here", __FUNCTION__, __LINE__);
				return RET_OK;
			}
			queue_out_ctrl_ptr = map_udpfd_out_ctrl[sock];
			chunk_ptr->header.rsv_1 = ACK;
			((struct chunk_delay_test_t *)chunk_ptr)->pid = _peer_mgr_ptr->self_pid;
			queue_out_ctrl_ptr->push((struct chunk_t *)chunk_ptr);

			return RET_OK;
		}
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST) {
		unsigned long manifest = ((struct chunk_manifest_set_t *)chunk_ptr)->manifest;
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SET_MANIFEST");
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "pid", pid, "manifest", manifest);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u)\ts\tu\ts\tu\tu \n", __FUNCTION__, __LINE__, "[CHNK_CMD_PEER_SET_MANIFEST] RECV mypid", _pk_mgr_ptr->my_pid, "child", pid, manifest);

		_peer_mgr_ptr ->handle_manifestSet((struct chunk_manifest_set_t *)chunk_ptr); 
		
		if (((struct chunk_manifest_set_t *)chunk_ptr)->manifest == 0) {
			//data_close(sock, "close by Children SET Manifest = 0", CLOSE_PARENT);
			CloseChild(pid, true, "close by Children SET Manifest = 0");
		}
		_pk_mgr_ptr->SendCapacityToPK();
	} 
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_BLOCK_RESCUE) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_BLOCK_RESCUE");
		_peer_mgr_ptr->HandleBlockRescue((struct chunk_block_rescue_t *)chunk_ptr);
	}
	else {
		debug_printf("chunk_ptr->header.cmd = 0x%02x  len = %d \n", chunk_ptr->header.cmd, chunk_ptr->header.length);
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Unknown header.cmd", __FUNCTION__, __LINE__);
	}
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
	if (chunk_ptr) {
		delete [] (unsigned char*)chunk_ptr;
	}
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
	return RET_OK;
}


//送queue_out_ctrl_ptr 和queue_out_data_ptr出去
//0311 這邊改成如果阻塞了  就會blocking 住直到送出去為止
int peer::handle_pkt_out(int sock)
{
	bool isUDP = false;		// The socket is TCP or UDP
	unsigned long pid;
	struct chunk_manifest_set_t chunk_manifestSet;
	struct chunk_t *chunk_ptr=NULL;
	
	map<int, unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_in_iter;
	map<unsigned long, int>::iterator map_pid_fd_out_iter;
//	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr;
//	struct peer_connect_down_t* peerDownInfo ;
	struct peer_info_t *peerInfoPtr = NULL;

	Nonblocking_Ctl * Nonblocking_Send_Data_ptr = NULL;
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr = NULL;

	if (map_fd_out_ctrl.find(sock) != map_fd_out_ctrl.end()) {
		isUDP = false;
		queue_out_ctrl_ptr = map_fd_out_ctrl.find(sock)->second;
	}
	else if (map_udpfd_out_ctrl.find(sock) != map_udpfd_out_ctrl.end()) {
		isUDP = true;
		queue_out_ctrl_ptr = map_udpfd_out_ctrl.find(sock)->second;
	}
	else{
		debug_printf("queue_out_ctrl_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG](this condition shouldn't happen) queue_out_ctrl_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}
	
	if (map_fd_out_data.find(sock) != map_fd_out_data.end()) {
		if (isUDP)	PAUSE
		queue_out_data_ptr = map_fd_out_data.find(sock)->second;
	}
	else if (map_udpfd_out_data.find(sock) != map_udpfd_out_data.end()) {
		queue_out_data_ptr = map_udpfd_out_data.find(sock)->second;
	}
	else {
		debug_printf("queue_out_data_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"[DEBUG](this condition shouldn't happen) queue_out_data_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(sock);
	if (map_fd_nonblocking_ctl.find(sock) != map_fd_nonblocking_ctl.end()) {
		if (isUDP)	PAUSE
		Nonblocking_Send_Data_ptr = &map_fd_nonblocking_ctl.find(sock)->second->nonBlockingSendData;
		Nonblocking_Send_Ctrl_ptr = &map_fd_nonblocking_ctl.find(sock)->second->nonBlockingSendCtrl;
	}
	else if (map_udpfd_nonblocking_ctl.find(sock) != map_udpfd_nonblocking_ctl.end()) {
		Nonblocking_Send_Data_ptr = &map_udpfd_nonblocking_ctl.find(sock)->second->nonBlockingSendData;
		Nonblocking_Send_Ctrl_ptr = &map_udpfd_nonblocking_ctl.find(sock)->second->nonBlockingSendCtrl;
	}
	else{
		debug_printf("Nonblocking_Buff_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s\n", __FUNCTION__,__LINE__,"[DEBUG](this condition shouldn't happen) Nonblocking_Buff_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}


	//測試性功能
	////////////////////////////////////////////////////////////
	if (map_fd_pid.find(sock) != map_fd_pid.end()) {
		if (isUDP)	PAUSE
		pid = map_fd_pid.find(sock)->second;
		if (_pk_mgr_ptr->map_pid_child.find(pid) != _pk_mgr_ptr->map_pid_child.end()) {
			peerInfoPtr = _pk_mgr_ptr->map_pid_child.find(pid)->second ;
		}
	}
	else if (map_udpfd_pid.find(sock) != map_udpfd_pid.end()) {
		pid = map_udpfd_pid.find(sock)->second;
		if (_pk_mgr_ptr->map_pid_child.find(pid) != _pk_mgr_ptr->map_pid_child.end()) {
			peerInfoPtr = _pk_mgr_ptr->map_pid_child.find(pid)->second ;
		}
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] map_fd_pid NOT FIND", __FUNCTION__, __LINE__);
		return RET_SOCK_ERROR;
	}
	//////////////////////////////////////////////////////////////

//	while(queue_out_ctrl_ptr->size() != 0  ||  queue_out_data_ptr->size() != 0){

		

		if (queue_out_ctrl_ptr->size() != 0 && chunk_ptr == NULL) {
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size());
			debug_printf("queue_out_ctrl_ptr->size() = %d \n", queue_out_ctrl_ptr->size());
			
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

				if (isUDP == false) {
					_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
				}
				else {
					_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
				}
				_log_ptr->write_log_format("s(u) s u s s d s d \n", __FUNCTION__, __LINE__, 
																	"sent ctrl", _send_byte, "bytes", 
																	"queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size(),
																	"header.cmd =", chunk_ptr->header.cmd);
				
				memcpy(&chunk_manifestSet, chunk_ptr, sizeof(chunk_manifestSet));
				if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST) {
					_log_ptr->write_log_format("s =>u s d s u s u s u \n", __FUNCTION__, __LINE__,
																			   "header.cmd =", chunk_manifestSet.header.cmd,
																			   "manifest =", chunk_manifestSet.manifest,
																			   "my pid =", chunk_manifestSet.pid,
																			   "peer pid =", pid);
				}

				if(_send_byte < 0) {
					//data_close(sock, "error occured in send  READY queue_out_ctrl",DONT_CARE);
					CloseParent(pid, false, "error occured in send  READY queue_out_ctrl");
					CloseChild(pid, false, "error occured in send  READY queue_out_ctrl");
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
							if (isUDP == false) {
								map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
								map<int , unsigned long>::iterator detect_map_fd_pid_iter;
								detect_map_fd_pid_iter = map_fd_pid.find(sock);
								if(detect_map_fd_pid_iter == map_fd_pid.end()){
									_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find fd pid in source_delay_detection", __FUNCTION__, __LINE__);
								}
								pid_peerDown_info_iter = _pk_mgr_ptr->map_pid_parent.find(detect_map_fd_pid_iter->second);
								if(pid_peerDown_info_iter == _pk_mgr_ptr->map_pid_parent.end()){
									_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find pid peerinfo in source_delay_detection", __FUNCTION__, __LINE__);
								}
								if (pid_peerDown_info_iter->second->peerInfo.manifest == 0) {
									map<unsigned long, int>::iterator iter = map_in_pid_fd.find(pid_peerDown_info_iter ->first) ;
									if (iter != map_in_pid_fd.end()) {
										//data_close(map_in_pid_fd[pid_peerDown_info_iter ->first], "manifest=0", CLOSE_PARENT) ;
										CloseParent(pid_peerDown_info_iter ->first, true, "manifest=0") ;
									}
									else{
										_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Set parent manifest=0 but cannot find this parent's pid in map_in_pid_fd", __FUNCTION__, __LINE__);
									}
								}
							}
							else {
								unsigned long parent_pid;
								if (map_udpfd_pid.find(sock) == map_udpfd_pid.end()) {
									_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find fd pid in source_delay_detection", __FUNCTION__, __LINE__);
								}
								parent_pid = map_udpfd_pid.find(sock)->second;
								if (_pk_mgr_ptr->map_pid_parent.find(parent_pid) == _pk_mgr_ptr->map_pid_parent.end()) {
									_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find pid peerinfo in source_delay_detection", __FUNCTION__, __LINE__);
								}
								if (_pk_mgr_ptr->map_pid_parent.find(parent_pid)->second->peerInfo.manifest == 0) {
									if (map_in_pid_udpfd.find(parent_pid) != map_in_pid_udpfd.end()) {
										//data_close(map_in_pid_fd[parent_pid], "manifest=0", CLOSE_PARENT) ;
										CloseParent(parent_pid, true, "manifest=0") ;
									}
									else{
										_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Set parent manifest=0 but cannot find this parent's pid in map_in_pid_fd", __FUNCTION__, __LINE__);
									}
								}
							}
						}
					}
					
				}
			}
			else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING ) {
				if (isUDP == false) {
					_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
				}
				else {
					_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
				}
				
				if (_send_byte < 0) {
						//data_close(sock, "error occured in send RUNNING queue_out_data", DONT_CARE);
						CloseParent(pid, false, "error occured in send RUNNING queue_out_data");
						CloseChild(pid, false, "error occured in send RUNNING queue_out_data");
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
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_data_ptr->size() =", queue_out_data_ptr->size());
			debug_printf("queue_out_data_ptr->size() = %d \n", queue_out_data_ptr->size());
		
			if (queue_out_data_ptr->size() > 100) {
				//debug_printf("queue_out_data_ptr->size() = %d \n", queue_out_data_ptr->size());
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_data_ptr->size() =", queue_out_data_ptr->size());
			}
			
			if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY ) {

				chunk_ptr = queue_out_data_ptr->front();

				//測試性功能
				//////////////////////////////////////////////////////////////////////////////
				if (peerInfoPtr) {
					while (1) {
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
				
				if (isUDP == false) {
					_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
				}
				else {
					_send_byte = _net_udp_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
				}
				_log_ptr->write_log_format("s(u) s u s s d s d \n", __FUNCTION__, __LINE__, 
																	"sent data", _send_byte, "bytes", 
																	"queue_out_data_ptr->size() =", queue_out_data_ptr->size(),
																	"header.cmd =", chunk_ptr->header.cmd);
				
				if (_send_byte < 0) {
					//data_close(sock, "error occured in send queue_out_data", DONT_CARE);
					CloseParent(pid, false, "error occured in send queue_out_data");
					CloseChild(pid, false, "error occured in send queue_out_data");
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
				if (isUDP == false) {
					_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
				}
				else {
					_send_byte = _net_udp_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
				}
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
					//data_close(sock, "error occured in send queue_out_data", DONT_CARE);
					CloseParent(pid, false, "error occured in send queue_out_data");
					CloseChild(pid, false, "error occured in send queue_out_data");
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
			if (isUDP == false) {
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
			}
			else {
				//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN);
				//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN);
			}
		}
	//}	
	//debug_printf("peer::handle_pkt_out end \n");
	return RET_OK;



	/*
	struct sockaddr_in addr;
	int addrLen = sizeof(struct sockaddr_in);
	int n = getpeername(sock, (struct sockaddr *)&addr, (socklen_t *)&addrLen);
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
		map_pid_child1_iter = _pk_mgr_ptr->map_pid_child1 .find(map_fd_pid_iter ->second);
		if(map_pid_child1_iter ==  _pk_mgr_ptr->map_pid_child1.end()){
			//printf("peer::handle_pkt_out where is the peer\n");
			peerInfoPtr =NULL ;
			//PAUSE
			//return RET_SOCK_ERROR;
		}else{
			peerInfoPtr = map_pid_child1_iter ->second ;
		}
	}else{
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] map_fd_pid NOT FIND", __FUNCTION__, __LINE__);
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
								_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find fd pid in source_delay_detection", __FUNCTION__, __LINE__);
							}
							pid_peerDown_info_iter = _pk_mgr_ptr->map_pid_parent.find(detect_map_fd_pid_iter->second);
							if(pid_peerDown_info_iter == _pk_mgr_ptr->map_pid_parent.end()){
								_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find pid peerinfo in source_delay_detection", __FUNCTION__, __LINE__);
							}
							if (pid_peerDown_info_iter->second->peerInfo.manifest == 0) {
								map<unsigned long, int>::iterator iter = map_in_pid_fd.find(pid_peerDown_info_iter ->first) ;
								if (iter != map_in_pid_fd.end()) {
									data_close(map_in_pid_fd[pid_peerDown_info_iter ->first], "manifest=0", CLOSE_PARENT) ;
								}
								else{
									_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Set parent manifest=0 but cannot find this parent's pid in map_in_pid_fd", __FUNCTION__, __LINE__);
								}
							}
						}
					}
					
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
	*/
}


//送queue_out_ctrl_ptr 和queue_out_data_ptr出去
//0311 這邊改成如果阻塞了  就會blocking 住直到送出去為止
int peer::handle_pkt_out_udp(int sock)
{
	bool isUDP = false;		// The socket is TCP or UDP
	unsigned long pid;
	struct chunk_manifest_set_t chunk_manifestSet;
	struct chunk_t *chunk_ptr=NULL;
	
	map<int, unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_in_iter;
	map<unsigned long, int>::iterator map_pid_fd_out_iter;
//	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr;
//	struct peer_connect_down_t* peerDownInfo ;
	struct peer_info_t *peerInfoPtr = NULL;

	Nonblocking_Ctl * Nonblocking_Send_Data_ptr = NULL;
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr = NULL;

	
	if (map_udpfd_out_ctrl.find(sock) != map_udpfd_out_ctrl.end()) {
		isUDP = true;
		queue_out_ctrl_ptr = map_udpfd_out_ctrl.find(sock)->second;
	}
	else{
		//debug_printf("queue_out_ctrl_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "queue_out_ctrl_ptr NOT FIND", sock);
		CloseSocketUDP(sock, false, "Not found in queue_out_ctrl_ptr");
		return RET_SOCK_ERROR;
	}
	
	if (map_udpfd_out_data.find(sock) != map_udpfd_out_data.end()) {
		queue_out_data_ptr = map_udpfd_out_data.find(sock)->second;
	}
	else {
		debug_printf("queue_out_data_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"queue_out_data_ptr NOT FIND");
		CloseSocketUDP(sock, false, "Not found in queue_out_ctrl_ptr");
		return RET_SOCK_ERROR;
	}

	if (map_udpfd_nonblocking_ctl.find(sock) != map_udpfd_nonblocking_ctl.end()) {
		Nonblocking_Send_Data_ptr = &map_udpfd_nonblocking_ctl.find(sock)->second->nonBlockingSendData;
		Nonblocking_Send_Ctrl_ptr = &map_udpfd_nonblocking_ctl.find(sock)->second->nonBlockingSendCtrl;
	}
	else{
		debug_printf("Nonblocking_Buff_ptr NOT FIND \n");
		_log_ptr->write_log_format("s(u) s\n", __FUNCTION__,__LINE__,"Nonblocking_Buff_ptr NOT FIND");
		CloseSocketUDP(sock, false, "Not found in queue_out_ctrl_ptr");
		return RET_SOCK_ERROR;
	}


	//測試性功能
	////////////////////////////////////////////////////////////
	if (map_udpfd_pid.find(sock) != map_udpfd_pid.end()) {
		pid = map_udpfd_pid.find(sock)->second;
		if (_pk_mgr_ptr->map_pid_child.find(pid) != _pk_mgr_ptr->map_pid_child.end()) {
			peerInfoPtr = _pk_mgr_ptr->map_pid_child.find(pid)->second ;
		}
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] map_fd_pid NOT FIND", __FUNCTION__, __LINE__);
		return RET_SOCK_ERROR;
	}
	//////////////////////////////////////////////////////////////
	int32_t pendingpkt_size = 0;
	int len = sizeof(pendingpkt_size);
	UDT::getsockopt(sock, 0, UDT_SNDDATA, &pendingpkt_size, &len);
	if (pendingpkt_size > 0 || queue_out_ctrl_ptr->size() != 0 || queue_out_data_ptr->size() != 0) {
		_log_ptr->write_log_format("s(u) s u s d\td\td\n", __FUNCTION__, __LINE__, "sock", sock, "pending", pendingpkt_size, queue_out_ctrl_ptr->size(), queue_out_data_ptr->size());
	}
	if (queue_out_data_ptr->size() > _pk_mgr_ptr->pkt_rate*10) {
		debug_printf("child %d data size %d \n", pid, queue_out_data_ptr->size());
		CloseSocketUDP(sock, false, "data size too much");
		return RET_OK;
	}


//	while(queue_out_ctrl_ptr->size() != 0  ||  queue_out_data_ptr->size() != 0){

		if (queue_out_ctrl_ptr->size() != 0 && chunk_ptr == NULL) {
			// 優先將 Ctrl message 送出去
			while (queue_out_ctrl_ptr->size() != 0) {
				//_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size());
				//debug_printf("queue_out_ctrl_ptr->size() = %d \n", queue_out_ctrl_ptr->size());

				if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {

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

					_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));

					_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
						"sent ctrl", _send_byte, "bytes to", sock,
						"queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size(),
						"header.cmd =", chunk_ptr->header.cmd);

					memcpy(&chunk_manifestSet, chunk_ptr, sizeof(chunk_manifestSet));
					if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST) {
						_log_ptr->write_log_format("s =>u s d s u s u s u \n", __FUNCTION__, __LINE__,
							"header.cmd =", chunk_manifestSet.header.cmd,
							"manifest =", chunk_manifestSet.manifest,
							"my pid =", chunk_manifestSet.pid,
							"peer pid =", pid);
					}

					if (_send_byte < 0) {
						//data_close(sock, "error occured in send  READY queue_out_ctrl",DONT_CARE);
						CloseSocketUDP(sock, false, "error occured in send  READY queue_out_ctrl");
						return RET_SOCK_ERROR;
					}
					else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {
						queue_out_ctrl_ptr->pop();
						if (chunk_ptr) {
							delete chunk_ptr;
						}
						chunk_ptr = NULL;
						_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "chunk_manifestSet.manifest =", chunk_manifestSet.manifest);
						//// 0903新增，確定送給parent manifest=0這個訊息後，刪除整個相關的table
						/*
						if (chunk_manifestSet.header.cmd != CHNK_CMD_PEER_TEST_DELAY) {
						if (chunk_manifestSet.manifest == 0) {

						unsigned long parent_pid;
						if (map_udpfd_pid.find(sock) == map_udpfd_pid.end()) {
						_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find fd pid in source_delay_detection", __FUNCTION__, __LINE__);
						}
						parent_pid = map_udpfd_pid.find(sock)->second;
						if (_pk_mgr_ptr->map_pid_parent.find(parent_pid) == _pk_mgr_ptr->map_pid_parent.end()) {
						_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Cannot find pid peerinfo in source_delay_detection", __FUNCTION__, __LINE__);
						}
						if (_pk_mgr_ptr->map_pid_parent.find(parent_pid)->second->peerInfo.manifest == 0) {
						if (map_in_pid_udpfd.find(parent_pid) != map_in_pid_udpfd.end()) {
						//data_close(map_in_pid_fd[parent_pid], "manifest=0", CLOSE_PARENT) ;
						CloseParent(parent_pid, true, "manifest=0") ;
						}
						else{
						_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Set parent manifest=0 but cannot find this parent's pid in map_in_pid_fd", __FUNCTION__, __LINE__);
						}
						}

						}
						}
						*/
					}
				}
				else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == RUNNING) {

					_send_byte = _net_udp_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));


					if (_send_byte < 0) {
						//data_close(sock, "error occured in send RUNNING queue_out_data", DONT_CARE);
						CloseParent(pid, false, "error occured in send RUNNING queue_out_data");
						CloseChild(pid, false, "error occured in send RUNNING queue_out_data");
						//PAUSE
						return RET_SOCK_ERROR;
					}
					else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {
						queue_out_data_ptr->pop();
						if (chunk_ptr) {
							delete chunk_ptr;
						}
						chunk_ptr = NULL;
					}
				}
			}
		} 
		// DATA
		else if (queue_out_data_ptr->size() != 0) {


			//_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_data_ptr->size() =", queue_out_data_ptr->size());
			//debug_printf("queue_out_data_ptr->size() = %d \n", queue_out_data_ptr->size());
		
			///////////// TEST for Scheduling//////////
			bool can_send_data = true;
			int32_t pendingpkt_size = 0;
			int len = sizeof(pendingpkt_size);
			UDT::getsockopt(sock, 0, UDT_SNDDATA, &pendingpkt_size, &len);
			if (pendingpkt_size > 10) {
				return RET_OK;
			}
			
			for (list<unsigned long>::iterator iter = priority_children.begin(); iter != priority_children.end(); iter++) {
				// iterator 只向這次的 sock，可能是因為自己的最高 priority 或是 前面的 child 的 queue 都已經空了
				if (*iter == pid) {
					break;
				}

				// 檢查那些更高 priority 的 child 的 queue 是否為空
				int other_child_sock = -1;
				queue<struct chunk_t *> *other_child_queue_out_ctrl_ptr;
				queue<struct chunk_t *> *other_child_queue_out_data_ptr;

				if (_pk_mgr_ptr->map_pid_child.find(*iter) != _pk_mgr_ptr->map_pid_child.end()) {
					if (_pk_mgr_ptr->map_pid_child.find(*iter)->second->state == PEER_CONNECTED_CHILD) {
						if (map_out_pid_udpfd.find(*iter) != map_out_pid_udpfd.end()) {
							other_child_sock = map_out_pid_udpfd.find(*iter)->second;
							if (map_udpfd_out_data.find(other_child_sock) != map_udpfd_out_data.end()) {
								other_child_queue_out_data_ptr = map_udpfd_out_data[other_child_sock];	// Get queue_out_data_ptr
							}
							else {
								_pk_mgr_ptr->handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
							}

							if (map_udpfd_out_ctrl.find(other_child_sock) != map_udpfd_out_ctrl.end()) {
								other_child_queue_out_ctrl_ptr = map_udpfd_out_ctrl[other_child_sock];	// Get queue_out_data_ptr
							}
							else {
								_pk_mgr_ptr->handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
							}

							// 找到更高 priority 的 child，檢查它 ctrl queue 和 data queue 是否都為空
							if (other_child_queue_out_ctrl_ptr->size() != 0 || other_child_queue_out_data_ptr->size() != 0) {
								can_send_data = false;
							}
						}
					}
				}
			}
			if (can_send_data == false) {
				return RET_OK;
			}
			///////////////////////////

			if (queue_out_data_ptr->size() > 100) {
				//debug_printf("queue_out_data_ptr->size() = %d \n", queue_out_data_ptr->size());
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "queue_out_data_ptr->size() =", queue_out_data_ptr->size());
			}
			
			if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY ) {

				chunk_ptr = queue_out_data_ptr->front();

				/*
				//測試性功能
				//////////////////////////////////////////////////////////////////////////////
				if (peerInfoPtr) {
					while (1) {
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
				*/

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
				//debug_printf("[DEBUG] \n");

				_send_byte = _net_udp_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
				//debug_printf("[DEBUG] \n");
				_log_ptr->write_log_format("s(u) s d s d s d s d s u \n", __FUNCTION__, __LINE__, 
																	"sent data", _send_byte, "bytes to", sock,
																	"queue_out_data_ptr->size() =", queue_out_data_ptr->size(),
																	"header.cmd =", chunk_ptr->header.cmd,
																	"pid", pid);

				if (_send_byte < 0) {
					//data_close(sock, "error occured in send queue_out_data", DONT_CARE);
					CloseParent(pid, false, "error occured in send queue_out_data");
					CloseChild(pid, false, "error occured in send queue_out_data");
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
				
				_send_byte = _net_udp_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
				
				_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
																"PEER SEND DATA READY cmd =", Nonblocking_Send_Data_ptr->recv_ctl_info.chunk_ptr->header.cmd,
																"total_len =", Nonblocking_Send_Data_ptr->recv_ctl_info.total_len,
																"expect_len =", Nonblocking_Send_Data_ptr->recv_ctl_info.expect_len);


				//if(!(_logger_client_ptr->log_bw_out_init_flag)){
				//	_logger_client_ptr->log_bw_out_init_flag = 1;
				//	_logger_client_ptr->bw_out_struct_init(_send_byte);
				//}
				//else{
				//	_logger_client_ptr->set_out_bw(_send_byte);
				//}

				if (_send_byte < 0) {
					//data_close(sock, "error occured in send queue_out_data", DONT_CARE);
					CloseParent(pid, false, "error occured in send queue_out_data");
					CloseChild(pid, false, "error occured in send queue_out_data");
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
		
			//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN);
			//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN);
			
		}
		
	//debug_printf("peer::handle_pkt_out end \n");
	return RET_OK;
}


void peer::handle_pkt_error(int sock)
{

}

void peer::handle_pkt_error_udp(int sock)
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

// Remove certain iterator in "substream_first_reply_peer", "map_(udp)fd_info", "map_(udp)fd_NonBlockIO", and "(udp)fd_list"
// When connect time triggered or one of connections has built, this function will be called
// Close other peers which is not be selected as parent in that session
void peer::StopSession(unsigned long session_id)
{
	map<unsigned long, struct manifest_timmer_flag *>::iterator substream_first_reply_peer_iter = substream_first_reply_peer.find(session_id);
	
	if (substream_first_reply_peer_iter == substream_first_reply_peer.end()) {
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "session id", session_id);
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found session in substream_first_reply_peer", __FUNCTION__, __LINE__);
	}
	else {
		if (substream_first_reply_peer_iter->second->peer_role == CHILD_PEER) {

		}
		else if (substream_first_reply_peer_iter->second->peer_role == CHILD_PEER) {
			for (multimap <unsigned long, struct peer_info_t *>::iterator iter = _pk_mgr_ptr->map_pid_parent_temp.begin(); iter != _pk_mgr_ptr->map_pid_parent_temp.end(); iter++) {
				if (iter->second->session_id == session_id && _pk_mgr_ptr->parents_table[iter->second->pid] != PEER_CONNECTED) {
					// Clear other peers in this session, except the selected peer
					CloseParent(peerInfoPtr->pid, false, "Close by session stop");
				}
			}
		}

		delete[](unsigned char*) substream_first_reply_peer_iter->second;
		substream_first_reply_peer.erase(substream_first_reply_peer_iter);

		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "Delete session", session_id, "size", substream_first_reply_peer.size());
	}
	
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
		//if(!(_pk_mgr_ptr ->check_rescue_state(i,0))){
		if (_pk_mgr_ptr->ss_table[i]->state.state == SS_TEST) {
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
		/*
		//清除所有跟這個peer相關的 previousParentPID
		for(unsigned long i=0 ; i<  _pk_mgr_ptr->sub_stream_num ; i++){
			if ((_pk_mgr_ptr->ssDetect_ptr +i) -> previousParentPID == pid) {
				(_pk_mgr_ptr->ssDetect_ptr +i) -> previousParentPID = PK_PID +1 ;
			}

		}
		*/
		//CLOSE CHILD
		if (map_pid_fd_out_iter != map_out_pid_fd.end()  && (map_pid_fd_out_iter ->second == cfd)){

			debug_printf("CLOSE CHILD \n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CLOSE CHILD");
			
			map_pid_fd_iter = map_out_pid_fd.find(pid);
			if(map_pid_fd_iter != map_out_pid_fd.end()) 
				map_out_pid_fd.erase(map_pid_fd_iter);
			
			// Temp child-peer
			for(pid_child_peer_info_iter =_pk_mgr_ptr-> map_pid_child_temp.begin();pid_child_peer_info_iter!= _pk_mgr_ptr-> map_pid_child_temp.end();pid_child_peer_info_iter++){
				if(pid_child_peer_info_iter->first ==pid){
					delete pid_child_peer_info_iter->second;
					_pk_mgr_ptr-> map_pid_child_temp.erase(pid_child_peer_info_iter);
					pid_child_peer_info_iter = _pk_mgr_ptr-> map_pid_child_temp.begin();
					if(pid_child_peer_info_iter == _pk_mgr_ptr-> map_pid_child_temp.end())
						break;
				}	
			}

			// Real child-peer
			map_pid_child1_iter =_pk_mgr_ptr ->map_pid_child.find(pid);
			if( map_pid_child1_iter != _pk_mgr_ptr ->map_pid_child.end() ){
				peerInfoPtr = map_pid_child1_iter ->second ;

				_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE CHILD","PID=",map_pid_child1_iter->first,"manifest",peerInfoPtr->manifest);

				for(substream_first_reply_peer_iter =substream_first_reply_peer.begin();substream_first_reply_peer_iter !=substream_first_reply_peer.end();substream_first_reply_peer_iter++){

					_log_ptr->write_log_format("s(u) s u s u s u s u s u\n", __FUNCTION__,__LINE__,
																					"CLOSE CHILD ALL session. Session_id =", substream_first_reply_peer_iter->first,
																					"session's pid =", substream_first_reply_peer_iter->second->child_pid,
																					"manifest", peerInfoPtr->manifest ,
																					"session rescue manifest", substream_first_reply_peer_iter ->second->rescue_manifest,
																					//"substream_first_reply_peer_iter ->second ->connectTimeOutFlag", substream_first_reply_peer_iter ->second ->connectTimeOutFlag,
																					"ROLE =",substream_first_reply_peer_iter ->second ->peer_role);
					if(peerInfoPtr ->manifest == 0  && substream_first_reply_peer_iter ->second->peer_role ==1  && peerInfoPtr->pid ==substream_first_reply_peer_iter->second->child_pid ){
						
						_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE CHILD","session_id=",substream_first_reply_peer_iter ->first,"manifest",peerInfoPtr->manifest);
						_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__," peerInfoPtr->pid=", peerInfoPtr->pid,"substream_first_reply_peer_iter->second->pid",substream_first_reply_peer_iter->second->child_pid);

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
				_pk_mgr_ptr ->map_pid_child.erase(map_pid_child1_iter);
				priority_children.remove(pid);
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
			for(pid_peer_info_iter = _pk_mgr_ptr->map_pid_parent_temp.begin();pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end() ;pid_peer_info_iter++){
				//if(pid_peer_info_iter ->first ==cfd ){	// Commented on 20131222
				if(pid_peer_info_iter ->first ==pid ){
					delete pid_peer_info_iter->second;
					_pk_mgr_ptr->map_pid_parent_temp.erase(pid_peer_info_iter);
					pid_peer_info_iter =_pk_mgr_ptr->map_pid_parent_temp.begin();
					if(pid_peer_info_iter == _pk_mgr_ptr->map_pid_parent_temp.end())
						break;
				}
			}

			// Real parent-peer
			pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_parent.find(pid) ;
			if (pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_parent.end() ){

				peerDownInfoPtr = pid_peerDown_info_iter ->second ;

				//防止peer離開 狀態卡在testing
				if(peerDownInfoPtr->peerInfo.manifest & testingManifest){
					peerTestingManifest = peerDownInfoPtr->peerInfo.manifest & testingManifest;
					_pk_mgr_ptr->set_parent_manifest(peerDownInfoPtr, 0);
					for(unsigned long i=0 ; i<  _pk_mgr_ptr->sub_stream_num ; i++){
						if( peerTestingManifest & _pk_mgr_ptr->SubstreamIDToManifest(i)){
							 //(_pk_mgr_ptr->ssDetect_ptr +i) ->isTesting =false ;
							 //(_pk_mgr_ptr->ssDetect_ptr +i) ->previousParentPID = PK_PID +1 ;
							 //set recue stat 0
							 /*
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
							 */
							 //_pk_mgr_ptr->ss_table[i]->state.state = SS_STABLE;
							 //_pk_mgr_ptr->ss_table[i]->state.is_testing = false;
							 //_pk_mgr_ptr->send_parentToPK(_pk_mgr_ptr->SubstreamIDToManifest(i),PK_PID +1);

						}
					}
				}
				_pk_mgr_ptr ->reSet_detectionInfo();

				_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE PARENT","PID=",pid_peerDown_info_iter->first,"manifest",peerDownInfoPtr->peerInfo.manifest);

				delete peerDownInfoPtr;
				_pk_mgr_ptr ->map_pid_parent.erase(pid_peerDown_info_iter);
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
			_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] peer:: not parent and not children", __FUNCTION__, __LINE__);
		}
		debug_printf("test2 \n");
		map_fd_pid.erase(map_fd_pid_iter);		// 這行(這附近)曾發生segmentation fault
		debug_printf("test3 \n");
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] peer:: CLOSE map_fd_pid not find why", __FUNCTION__, __LINE__);
	}
}

// care: take care if the pid doesn't exist in the table
void peer::CloseParent(unsigned long pid, bool care, const char *reason) 
{
	//CloseParentTCP(pid, reason);
	CloseParentUDP(pid, reason);
	/*
	if (map_in_pid_fd.find(pid) != map_in_pid_fd.end()) {
		CloseParentTCP(pid, reason);
	}
	else if (map_in_pid_udpfd.find(pid) != map_in_pid_udpfd.end()) {
		CloseParentUDP(pid, reason);
	}
	else {
		for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
			debug_printf("parent-pid %d  fd %d \n", iter->first, iter->second);
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "parent-pid", iter->first, "fd", iter->second);
		}
		for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
			debug_printf("parent-pid %d  fd %d \n", iter->first, iter->second);
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "parent-pid", iter->first, "fd", iter->second);
		}
		if (care == true) {
			_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] not found in map_in_pid_fd", __FUNCTION__, __LINE__);
		}
		return ;
	}
	*/
}

void peer::CloseParentTCP(unsigned long pid, const char *reason) 
{
	int fd = -1;
	list<int>::iterator fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter; 
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;
	
	unsigned long  peerTestingManifest=0;

	fd = map_in_pid_fd.find(pid)->second;
	
	_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, reason);
	debug_printf("PEER DATA CLOSE  colse fd = %d  %s \n", fd, reason);
	
	debug_printf("Before close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close parent", pid);
	map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_out_iter;
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	
	//_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	
	// Close socket in _map_fd_bc_tbl
	_net_ptr->close(fd);

	unsigned long  testingManifest=0;
	for(unsigned long i =0  ; i < _pk_mgr_ptr ->sub_stream_num;i++){
		if (_pk_mgr_ptr->ss_table[i]->state.state == SS_TEST) {
			testingManifest |= _pk_mgr_ptr ->SubstreamIDToManifest(i);
		}
	}

	for (fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if (*fd_iter == fd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
	
	//clean all fd table in _peer_communication_ptr OBJ
	_peer_mgr_ptr->_peer_communication_ptr->clear_fd_in_peer_com(fd);

	fd_out_ctrl_iter = map_fd_out_ctrl.find(fd);
	if (fd_out_ctrl_iter != map_fd_out_ctrl.end()) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete it, fd_out_ctrl_iter->size() =", fd_out_ctrl_iter->second->size());
		delete fd_out_ctrl_iter->second;
		map_fd_out_ctrl.erase(fd_out_ctrl_iter);
	}
	
	fd_out_data_iter = map_fd_out_data.find(fd);
	if (fd_out_data_iter != map_fd_out_data.end()) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete it, fd_out_data_iter->size() =", fd_out_data_iter->second->size());
		delete fd_out_data_iter->second;
		map_fd_out_data.erase(fd_out_data_iter);
	}

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(fd);
	if (map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) {
		//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete it, map_fd_nonblocking_ctl_iter->size() =", map_fd_nonblocking_ctl_iter->second->size());
		delete map_fd_nonblocking_ctl_iter ->second;
		map_fd_nonblocking_ctl.erase(map_fd_nonblocking_ctl_iter);
	}
	
	if (map_in_pid_fd.find(pid) != map_in_pid_fd.end())  {
		map_in_pid_fd.erase(map_in_pid_fd.find(pid));
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_in_pid_fd", __FUNCTION__, __LINE__);
	}
	
	// Temp parent-peer
	for (pid_peer_info_iter = _pk_mgr_ptr->map_pid_parent_temp.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end(); pid_peer_info_iter++) {
		if (pid_peer_info_iter->first == pid) {
			delete pid_peer_info_iter->second;
			_pk_mgr_ptr->map_pid_parent_temp.erase(pid_peer_info_iter);
			pid_peer_info_iter =_pk_mgr_ptr->map_pid_parent_temp.begin();
			if (pid_peer_info_iter == _pk_mgr_ptr->map_pid_parent_temp.end()) {
				break;
			}
		}
	}
	
	// Real parent-peer
	pid_peerDown_info_iter = _pk_mgr_ptr->map_pid_parent.find(pid);
	if (pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_parent.end()) {
	
		peerDownInfoPtr = pid_peerDown_info_iter->second ;

		//防止peer離開 狀態卡在testing
		if(peerDownInfoPtr->peerInfo.manifest & testingManifest){
			peerTestingManifest = peerDownInfoPtr->peerInfo.manifest & testingManifest;
			_pk_mgr_ptr->set_parent_manifest(peerDownInfoPtr, 0);
		}
		//_pk_mgr_ptr ->reSet_detectionInfo();
		_pk_mgr_ptr->ResetDetection();

		delete peerDownInfoPtr;
		_pk_mgr_ptr ->map_pid_parent.erase(pid_peerDown_info_iter);
	}
	
	// parents table
	if (_pk_mgr_ptr->parents_table.find(pid) != _pk_mgr_ptr->parents_table.end()) {
		_pk_mgr_ptr->parents_table.erase(_pk_mgr_ptr->parents_table.find(pid));
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in parents_table", __FUNCTION__, __LINE__);
	}
	
	if (map_fd_pid.find(fd) != map_fd_pid.end()) {
		map_fd_pid.erase(map_fd_pid.find(fd));
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_fd_pid", __FUNCTION__, __LINE__);
	}
	
	debug_printf("After close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close parent", pid);
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
}

void peer::CloseParentUDP(unsigned long pid, const char *reason) 
{
	int fd = -1;
	list<int>::iterator udpfd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter; 
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;
	
	unsigned long  peerTestingManifest=0;

	if (map_in_pid_udpfd.find(pid) != map_in_pid_udpfd.end()) {
		fd = map_in_pid_udpfd.find(pid)->second;
	}
	else {
		// 有 BUG
		debug_printf("[DEBUG] Not found parent %d in map_in_pid_udpfd  %s \n", pid, reason);
		_log_ptr->write_log_format("s(u) s u s s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found parent", pid, "in map_in_pid_udpfd", reason);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s u s s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found parent", pid, "in map_in_pid_udpfd", reason);
	}
	
	_log_ptr->write_log_format("s(u) s d s d s \n", __FUNCTION__, __LINE__, "Close parent", pid, "fd", fd, reason);
	debug_printf("PEER DATA CLOSE  colse fd = %d  %s \n", fd, reason);
	
	debug_printf("Before close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close parent", pid);
	map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_out_iter;
	for (map<unsigned long, struct peer_connect_down_t *>::iterator iter = _pk_mgr_ptr->map_pid_parent.begin(); iter != _pk_mgr_ptr->map_pid_parent.end(); iter++) {
		debug_printf("map_pid_parent  pid : %d manifest :%d\n", iter->first, iter->second->peerInfo.manifest);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "map_pid_parent  pid", iter->first, " manifest", iter->second->peerInfo.manifest);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<int, queue<struct chunk_t *> *>::iterator iter = map_udpfd_out_ctrl.begin(); iter != map_udpfd_out_ctrl.end(); iter++) {
		debug_printf("map_udpfd_out_ctrl  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_out_ctrl  fd : ", iter->first);
	}
	for (map<int, queue<struct chunk_t *> *>::iterator iter = map_udpfd_out_data.begin(); iter != map_udpfd_out_data.end(); iter++) {
		debug_printf("map_udpfd_out_data  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_out_data  fd : ", iter->first);
	}
	
	//_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	
	// Close socket in _map_fd_bc_tbl
	_net_udp_ptr->close(fd);

	unsigned long  testingManifest=0;
	for(unsigned long i =0  ; i < _pk_mgr_ptr ->sub_stream_num;i++){
		if (_pk_mgr_ptr->ss_table[i]->state.state == SS_TEST) {
			testingManifest |= _pk_mgr_ptr ->SubstreamIDToManifest(i);
		}
	}
	
	//clean all fd table in _peer_communication_ptr OBJ
	//_peer_mgr_ptr->_peer_communication_ptr->clear_udpfd_in_peer_com(fd);

	fd_out_ctrl_iter = map_udpfd_out_ctrl.find(fd);
	if (fd_out_ctrl_iter != map_udpfd_out_ctrl.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", fd_out_ctrl_iter->first, "in map_udpfd_out_ctrl");
		delete fd_out_ctrl_iter->second;
		map_udpfd_out_ctrl.erase(fd_out_ctrl_iter);
	}
	
	fd_out_data_iter = map_udpfd_out_data.find(fd);
	if (fd_out_data_iter != map_udpfd_out_data.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", fd_out_data_iter->first, "in map_udpfd_out_data");
		delete fd_out_data_iter->second;
		map_udpfd_out_data.erase(fd_out_data_iter);
	}

	map_fd_nonblocking_ctl_iter = map_udpfd_nonblocking_ctl.find(fd);
	if (map_fd_nonblocking_ctl_iter != map_udpfd_nonblocking_ctl.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", map_fd_nonblocking_ctl_iter->first, "in map_udpfd_nonblocking_ctl");
		delete map_fd_nonblocking_ctl_iter ->second;
		map_udpfd_nonblocking_ctl.erase(map_fd_nonblocking_ctl_iter);
	}
	
	if (map_in_pid_udpfd.find(pid) != map_in_pid_udpfd.end())  {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete pid", pid, "in map_in_pid_udpfd");
		map_in_pid_udpfd.erase(map_in_pid_udpfd.find(pid));
	}
	
	
	// Temp parent-peer
	/*
	for (pid_peer_info_iter = _pk_mgr_ptr->map_pid_parent_temp.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end(); pid_peer_info_iter++) {
		if (pid_peer_info_iter->first == pid) {
			delete pid_peer_info_iter->second;
			_pk_mgr_ptr->map_pid_parent_temp.erase(pid_peer_info_iter);
			pid_peer_info_iter =_pk_mgr_ptr->map_pid_parent_temp.begin();
			if (pid_peer_info_iter == _pk_mgr_ptr->map_pid_parent_temp.end()) {
				break;
			}
		}
	}
	*/
	// Real parent-peer
	pid_peerDown_info_iter = _pk_mgr_ptr->map_pid_parent.find(pid);
	if (pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_parent.end()) {
	
		peerDownInfoPtr = pid_peerDown_info_iter->second ;

		//防止peer離開 狀態卡在testing
		if(peerDownInfoPtr->peerInfo.manifest & testingManifest){
			peerTestingManifest = peerDownInfoPtr->peerInfo.manifest & testingManifest;
			_pk_mgr_ptr->set_parent_manifest(peerDownInfoPtr, 0);
		}
		//_pk_mgr_ptr ->reSet_detectionInfo();
		_pk_mgr_ptr->ResetDetection();

		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete pid", pid_peerDown_info_iter->first, "in map_pid_parent");
		delete peerDownInfoPtr;
		_pk_mgr_ptr ->map_pid_parent.erase(pid_peerDown_info_iter);
	}
	/*
	// parents table
	if (_pk_mgr_ptr->parents_table.find(pid) != _pk_mgr_ptr->parents_table.end()) {
		_pk_mgr_ptr->parents_table.erase(_pk_mgr_ptr->parents_table.find(pid));
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in parents_table", __FUNCTION__, __LINE__);
	}
	*/
	if (map_udpfd_pid.find(fd) != map_udpfd_pid.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", fd, "in map_udpfd_pid");
		map_udpfd_pid.erase(map_udpfd_pid.find(fd));
	}
	
	/*
	debug_printf("After close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close parent", pid);
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	*/
}

// care: take care if the pid doesn't exist in the table
void peer::CloseChild(unsigned long pid, bool care, const char *reason) 
{
	//CloseChildTCP(pid, reason);
	CloseChildUDP(pid, reason);
	/*
	if (map_out_pid_fd.find(pid) != map_out_pid_fd.end()) {
		CloseChildTCP(pid, reason);
	}
	else if (map_out_pid_udpfd.find(pid) != map_out_pid_udpfd.end()) {
		CloseChildUDP(pid, reason);
	}
	else {
		for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
			debug_printf("child-pid %d  fd %d \n", iter->first, iter->second);
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "child-pid", iter->first, "fd", iter->second);
		}
		for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
			debug_printf("child-pid %d  fd %d \n", iter->first, iter->second);
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "child-pid", iter->first, "fd", iter->second);
		}
		if (care == true) {
			_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] not found in map_in_pid_fd", __FUNCTION__, __LINE__);
		}
		return ;
	}
	*/
}

void peer::CloseChildTCP(unsigned long pid, const char *reason) 
{
	int fd = -1;
	list<int>::iterator fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter; 
	multimap <unsigned long, struct peer_info_t *>::iterator pid_child_peer_info_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;
	map<unsigned long, manifest_timmer_flag *>::iterator temp_substream_first_reply_peer_iter;
	unsigned long  peerTestingManifest=0;

	fd = map_in_pid_fd.find(pid)->second;
	
	_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, reason);
	debug_printf("PEER DATA CLOSE  colse fd = %d  %s \n", fd, reason);
	
	debug_printf("Before close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close parent", pid);
	map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_out_iter;
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	
	//_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	
	// Close socket in _map_fd_bc_tbl
	_net_ptr->close(fd);

	unsigned long  testingManifest=0;
	for(unsigned long i =0  ; i < _pk_mgr_ptr ->sub_stream_num;i++){
		if (_pk_mgr_ptr->ss_table[i]->state.state == SS_TEST) {
			testingManifest |= _pk_mgr_ptr ->SubstreamIDToManifest(i);
		}
	}

	for (fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if (*fd_iter == fd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
	
	//clean all fd table in _peer_communication_ptr OBJ
	_peer_mgr_ptr->_peer_communication_ptr->clear_fd_in_peer_com(fd);

	fd_out_ctrl_iter = map_fd_out_ctrl.find(fd);
	if (fd_out_ctrl_iter != map_fd_out_ctrl.end()) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete it, fd_out_ctrl_iter->size() =", fd_out_ctrl_iter->second->size());
		delete fd_out_ctrl_iter->second;
		map_fd_out_ctrl.erase(fd_out_ctrl_iter);
	}
	
	fd_out_data_iter = map_fd_out_data.find(fd);
	if (fd_out_data_iter != map_fd_out_data.end()) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete it, fd_out_data_iter->size() =", fd_out_data_iter->second->size());
		delete fd_out_data_iter->second;
		map_fd_out_data.erase(fd_out_data_iter);
	}

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(fd);
	if (map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) {
		//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete it, map_fd_nonblocking_ctl_iter->size() =", map_fd_nonblocking_ctl_iter->second->size());
		delete map_fd_nonblocking_ctl_iter ->second;
		map_fd_nonblocking_ctl.erase(map_fd_nonblocking_ctl_iter);
	}
	
	if (map_out_pid_fd.find(pid) != map_out_pid_fd.end())  {
		map_out_pid_fd.erase(map_out_pid_fd.find(pid));
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_out_pid_fd", __FUNCTION__, __LINE__);
	}
	
	// Temp child-peer
	for (pid_child_peer_info_iter = _pk_mgr_ptr->map_pid_child_temp.begin(); pid_child_peer_info_iter != _pk_mgr_ptr->map_pid_child_temp.end(); pid_child_peer_info_iter++) {
		if (pid_child_peer_info_iter->first == pid) {
			delete pid_child_peer_info_iter->second;
			_pk_mgr_ptr-> map_pid_child_temp.erase(pid_child_peer_info_iter);
			pid_child_peer_info_iter = _pk_mgr_ptr-> map_pid_child_temp.begin();
			if (pid_child_peer_info_iter == _pk_mgr_ptr-> map_pid_child_temp.end()) {
				break;
			}
		}	
	}

	// Real child-peer
	map_pid_child1_iter =_pk_mgr_ptr ->map_pid_child.find(pid);
	if( map_pid_child1_iter != _pk_mgr_ptr ->map_pid_child.end() ){
		peerInfoPtr = map_pid_child1_iter->second;

		_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE CHILD","PID=",map_pid_child1_iter->first,"manifest",peerInfoPtr->manifest);

		for(substream_first_reply_peer_iter =substream_first_reply_peer.begin();substream_first_reply_peer_iter !=substream_first_reply_peer.end();substream_first_reply_peer_iter++){

			_log_ptr->write_log_format("s(u) s u s u s u s u s u\n", __FUNCTION__,__LINE__,
																			"CLOSE CHILD ALL session. Session_id =", substream_first_reply_peer_iter->first,
																			"session's pid =", substream_first_reply_peer_iter->second->child_pid,
																			"manifest", peerInfoPtr->manifest ,
																			"session rescue manifest", substream_first_reply_peer_iter ->second->rescue_manifest,
																			"ROLE =",substream_first_reply_peer_iter ->second ->peer_role);
			if(peerInfoPtr ->manifest == 0  && substream_first_reply_peer_iter ->second->peer_role ==1  && peerInfoPtr->pid ==substream_first_reply_peer_iter->second->child_pid ){
				
				_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE CHILD","session_id=",substream_first_reply_peer_iter ->first,"manifest",peerInfoPtr->manifest);
				_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__," peerInfoPtr->pid=", peerInfoPtr->pid,"substream_first_reply_peer_iter->second->pid",substream_first_reply_peer_iter->second->child_pid);

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
		_pk_mgr_ptr ->map_pid_child.erase(map_pid_child1_iter);
		priority_children.remove(pid);
	}
	
	// parents table
	if (_pk_mgr_ptr->children_table.find(pid) != _pk_mgr_ptr->children_table.end()) {
		_pk_mgr_ptr->children_table.erase(_pk_mgr_ptr->children_table.find(pid));
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in children_table", __FUNCTION__, __LINE__);
	}
	
	if (map_fd_pid.find(fd) != map_fd_pid.end()) {
		map_fd_pid.erase(map_fd_pid.find(fd));
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_fd_pid", __FUNCTION__, __LINE__);
	}
	
	debug_printf("After close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close parent", pid);
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s =>u s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
}

// Delete:
//   close socket
//   clear in peer_comm
//   map_fd_out_ctrl
//   map_fd_out_data
//   map_fd_nonblocking_ctl
//   map_out_pid_fd
//   pkmgr::map_pid_child
//   substream_first_reply_peer
//   map_fd_pid
// 送 Capacity
void peer::CloseChildUDP(unsigned long pid, const char *reason) 
{
	int fd = -1;
	list<int>::iterator fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;
	map<unsigned long, struct peer_info_t *>::iterator map_pid_child_iter;
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter; 
	multimap <unsigned long, struct peer_info_t *>::iterator pid_child_peer_info_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;
	map<unsigned long, manifest_timmer_flag *>::iterator temp_substream_first_reply_peer_iter;
	unsigned long  peerTestingManifest=0;

	
	
	if (map_out_pid_udpfd.find(pid) != map_out_pid_udpfd.end()) {
		fd = map_out_pid_udpfd.find(pid)->second;
	}
	else {
		// 有 BUG
		debug_printf("[DEBUG] Not found parent %d in map_out_pid_udpfd  %s \n", pid, reason);
		_log_ptr->write_log_format("s(u) s u s s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found child", pid, "in map_out_pid_udpfd", reason);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s u s s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found child", pid, "in map_out_pid_udpfd", reason);
	}

	_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, reason);
	debug_printf("PEER DATA CLOSE  colse fd = %d  %s \n", fd, reason);
	
	debug_printf("Before close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close child", pid);
	map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_in_iter;
	map<unsigned long, int>::iterator temp_map_pid_udpfd_out_iter;
	for (map<unsigned long, struct peer_info_t *>::iterator iter = _pk_mgr_ptr->map_pid_child.begin(); iter != _pk_mgr_ptr->map_pid_child.end(); iter++) {
		debug_printf("map_pid_child  pid : %d manifest :%d\n", iter->first, iter->second->manifest);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "map_pid_child  pid", iter->first, " manifest", iter->second->manifest);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<int, queue<struct chunk_t *> *>::iterator iter = map_udpfd_out_ctrl.begin(); iter != map_udpfd_out_ctrl.end(); iter++) {
		debug_printf("map_udpfd_out_ctrl  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_out_ctrl  fd : ", iter->first);
	}
	for (map<int, queue<struct chunk_t *> *>::iterator iter = map_udpfd_out_data.begin(); iter != map_udpfd_out_data.end(); iter++) {
		debug_printf("map_udpfd_out_data  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_out_data  fd : ", iter->first);
	}
	
	//_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	
	// Close socket in _map_fd_bc_tbl
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
	_net_udp_ptr->close(fd);
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);

	unsigned long  testingManifest=0;
	for(unsigned long i =0  ; i < _pk_mgr_ptr ->sub_stream_num;i++){
		if (_pk_mgr_ptr->ss_table[i]->state.state == SS_TEST) {
			testingManifest |= _pk_mgr_ptr ->SubstreamIDToManifest(i);
		}
	}

	//clean all fd table in _peer_communication_ptr OBJ
	//_peer_mgr_ptr->_peer_communication_ptr->clear_udpfd_in_peer_com(fd);

	fd_out_ctrl_iter = map_udpfd_out_ctrl.find(fd);
	if (fd_out_ctrl_iter != map_udpfd_out_ctrl.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", fd, "in map_udpfd_out_ctrl");
		delete fd_out_ctrl_iter->second;
		map_udpfd_out_ctrl.erase(fd_out_ctrl_iter);
	}
	
	fd_out_data_iter = map_udpfd_out_data.find(fd);
	if (fd_out_data_iter != map_udpfd_out_data.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", fd, "in map_udpfd_out_data");
		delete fd_out_data_iter->second;
		map_udpfd_out_data.erase(fd_out_data_iter);
	}

	map_fd_nonblocking_ctl_iter = map_udpfd_nonblocking_ctl.find(fd);
	if (map_fd_nonblocking_ctl_iter != map_udpfd_nonblocking_ctl.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", fd, "in map_udpfd_nonblocking_ctl");
		delete map_fd_nonblocking_ctl_iter ->second;
		map_udpfd_nonblocking_ctl.erase(map_fd_nonblocking_ctl_iter);
	}
	
	if (map_out_pid_udpfd.find(pid) != map_out_pid_udpfd.end())  {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete pid", pid, "in map_out_pid_udpfd");
		map_out_pid_udpfd.erase(map_out_pid_udpfd.find(pid));
	}
	
	/*  parent_temp 和 child_temp 由 session stop 的時候清除
	// Temp child-peer
	for (pid_child_peer_info_iter = _pk_mgr_ptr->map_pid_child_temp.begin(); pid_child_peer_info_iter != _pk_mgr_ptr->map_pid_child_temp.end(); ) {
		if (pid_child_peer_info_iter->first == pid) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete map_pid_child_temp, size() =", _pk_mgr_ptr->map_pid_child_temp.size());
			delete pid_child_peer_info_iter->second;
			pid_child_peer_info_iter = _pk_mgr_ptr->map_pid_child_temp.erase(pid_child_peer_info_iter);
		}
		else {
			pid_child_peer_info_iter++;
		}
	}
	*/

	// Real child-peer
	map_pid_child_iter =_pk_mgr_ptr ->map_pid_child.find(pid);
	if (map_pid_child_iter != _pk_mgr_ptr ->map_pid_child.end()) {
		peerInfoPtr = map_pid_child_iter->second;
		
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete pid", pid, "in map_pid_child");
		delete peerInfoPtr;
		_pk_mgr_ptr ->map_pid_child.erase(map_pid_child_iter);
		priority_children.remove(pid);
	}
	/*
	// The peer in the session
	for (substream_first_reply_peer_iter = substream_first_reply_peer.begin(); substream_first_reply_peer_iter != substream_first_reply_peer.end();) {
		if (substream_first_reply_peer_iter->second->pid == pid && substream_first_reply_peer_iter->second->peer_role == CHILD_PEER) {
			//_peer_mgr_ptr->_peer_communication_ptr->stop_attempt_connect(substream_first_reply_peer_iter->first);
			temp_substream_first_reply_peer_iter = substream_first_reply_peer_iter;
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "before delete substream_first_reply_peer, size() =", substream_first_reply_peer.size());
			delete[](unsigned char*)substream_first_reply_peer_iter->second;
			substream_first_reply_peer_iter = substream_first_reply_peer.erase(substream_first_reply_peer_iter);
		}
		else {
			substream_first_reply_peer_iter++;
		}
	}
	*/
	
	// children table
	if (_pk_mgr_ptr->children_table.find(pid) != _pk_mgr_ptr->children_table.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete pid", pid, "in children_table");
		_pk_mgr_ptr->children_table.erase(_pk_mgr_ptr->children_table.find(pid));
	}
	
	
	if (map_udpfd_pid.find(fd) != map_udpfd_pid.end()) {
		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Delete fd", fd, "in map_udpfd_pid");
		map_udpfd_pid.erase(map_udpfd_pid.find(fd));
	}
	
	// Send capacity to PK
	_pk_mgr_ptr->SendCapacityToPK();

	/*
	debug_printf("After close parent %d. Table information: \n", pid);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close parent", pid);
	for (map<unsigned long, int>::iterator iter = map_out_pid_fd.begin(); iter != map_out_pid_fd.end(); iter++) {
		debug_printf("map_out_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_fd.begin(); iter != map_in_pid_fd.end(); iter++) {
		debug_printf("map_in_pid_fd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_fd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
		debug_printf("map_out_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_out_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
		debug_printf("map_in_pid_udpfd  pid : %d fd :%d\n", iter->first, iter->second);
		_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__,__LINE__,"map_in_pid_udpfd  pid : ", iter->first," fd :", iter->second);
	}
	*/
}

void peer::CloseSocketTCP(int sock, bool care, const char *reason)
{
	/* TODO */
	PAUSE
}

// care: take care if the pid doesn't exist in the table
void peer::CloseSocketUDP(int sock, bool care, const char *reason)
{
	unsigned long pid;
	if (map_udpfd_pid.find(sock) == map_udpfd_pid.end()) {
		for (map<unsigned long, int>::iterator iter = map_in_pid_udpfd.begin(); iter != map_in_pid_udpfd.end(); iter++) {
			debug_printf("parent-pid %d  fd %d \n", iter->first, iter->second);
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "parent-pid", iter->first, "fd", iter->second);
		}
		for (map<unsigned long, int>::iterator iter = map_out_pid_udpfd.begin(); iter != map_out_pid_udpfd.end(); iter++) {
			debug_printf("child-pid %d  fd %d \n", iter->first, iter->second);
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "child-pid", iter->first, "fd", iter->second);
		}
		if (care == true) {
			debug_printf("[DEBUG] \n");
			_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] not found in map_in_pid_fd", __FUNCTION__, __LINE__);
		}
		return ;
	}

	pid = map_udpfd_pid.find(sock)->second;

	if (map_in_pid_udpfd.find(pid) != map_in_pid_udpfd.end()) {
		CloseParentUDP(pid, reason);
	}
	if (map_out_pid_udpfd.find(pid) != map_out_pid_udpfd.end()) {
		CloseChildUDP(pid, reason);
	}
}
