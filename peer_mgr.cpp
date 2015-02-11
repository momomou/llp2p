/*

*/

#include "peer_mgr.h"
#include "peer.h"
#include "network.h"
#include "network_udp.h"
#include "logger.h"
#include "pk_mgr.h"
#include "peer_communication.h"
#include "logger_client.h"

using namespace std;


peer_mgr::peer_mgr(list<int> *fd_list)
{
	peer_ptr = NULL;
	fd_list_ptr = fd_list;
	_peer_list_member = 0;
    self_public_ip = 0;
}

peer_mgr::~peer_mgr() 
{
	debug_printf("Have deleted peer_mgr \n");
	if (peer_ptr) {
		delete peer_ptr;
	}
}

void peer_mgr::peer_communication_set(peer_communication *peer_communication_ptr){
	_peer_communication_ptr = peer_communication_ptr;
}

//初始化基本參數
void peer_mgr::peer_mgr_set(network *net_ptr, network_udp *net_udp_ptr, logger *log_ptr , configuration *prep, pk_mgr * pk_mgr_ptr, logger_client * logger_client_ptr)
{
	_net_ptr = net_ptr;
	_net_udp_ptr = net_udp_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;
	_logger_client_ptr = logger_client_ptr;

	peer_ptr = new peer(fd_list_ptr);
	_pk_mgr_ptr ->peer_set(peer_ptr);
	peer_ptr->peer_set(_net_ptr, _net_udp_ptr, _log_ptr, _prep, _pk_mgr_ptr, this,logger_client_ptr);		
}

peer * peer_mgr::get_peer_object(){
	if(peer_ptr == NULL){
		
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","peer object not init in get_peer_object");
		_logger_client_ptr->log_exit();
	}
	else{
		return peer_ptr;
	}
}
//除自己之外和所有在同個lane 下的member做連線要求 呼叫build_connection連線
//只有註冊時收到peer list 才會呼叫
void peer_mgr::connect_peer(struct chunk_level_msg_t *level_msg_ptr, unsigned long pid)
{
	return;
}

//給定一個rescue 的list 然後隨機從list 挑一個peer
//hidden at 2013/01/23
///*
//int peer_mgr::connect_other_lane_peer(struct chunk_rescue_list_reply_t *rescue_list_reply_ptr, unsigned long peer_list_member, unsigned long pid, unsigned long outside_lane_rescue_num)



//利用level_info_ptr 連線到connect的狀態後 呼叫handle_connect_request(傳送request 到sock (其他peer) )
//最後把sock設成 nonblock 然後加入select 的監聽 EPOLLIN | EPOLLOUT ( 由peer 的obj做後續的傳送處理)
int peer_mgr::build_connection(struct level_info_t *level_info_ptr, unsigned long pid)
{
	return 0;
}


//只用來接收 CHNK_CMD_PEER_CON的資訊  並把fd 加入監聽
int peer_mgr::handle_pkt_in(int sock)
{
	debug_printf("-----------------peer_mgr::handle_pkt_in--------------------------");
	
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot inside this scope in peer_mgr::handle_pkt_in\n");
	debug_printf("peer_mgr::pkt_in \n");
	_logger_client_ptr->log_exit();
	return RET_OK;
}


int peer_mgr::handle_pkt_out(int sock)
{
	return RET_OK;
}

void peer_mgr::handle_pkt_error(int sock)
{

}

void peer_mgr::handle_sock_error(int sock, basic_class *bcptr)
{
	_net_ptr->fd_bcptr_map_delete(sock);
	data_close(sock, "peer_mgr handle_sock_error!!");
}

void peer_mgr::handle_job_realtime()
{

}

void peer_mgr::handle_job_timer()
{

}


//2013/03/14　清掉很多沒用的function 
//void peer_mgr::send_cut_peer(unsigned long pid, int sock)

//
//void peer_mgr::rescue_reply(unsigned long pid, unsigned long manifest)

//利用 pid 找到map_out_pid_peer_info  ,map_pid_fd 並且辨別是哪個ss_id
//其實就只是把 chunk_ptr 丟到queue_out_data_ptr 裡面  並把把監聽設為EPOLLOUT , 前提是要 在map_pid_parent_temp ,map_out_pid_fd 留有資訊
//void peer_mgr::add_downstream(unsigned long pid, struct chunk_t *chunk_ptr)

//hidden at 2013/01/27 
//void peer_mgr::cut_rescue_peer(int sock)


//only called by recv pkt from pk ,cmd == CHNK_CMD_PEER_TCN
//只有拓樸改變才會呼叫     
///*hidden at 2013/01/27
//void peer_mgr::del_rescue_downstream()


//hidden at 2013/01/27
///*
//void peer_mgr::cut_rescue_downstream(unsigned long pid)


//把pid output_data queue 全部清空
//pid 可能會對到多個fd 
void peer_mgr::clear_ouput_buffer(unsigned long pid)
{
    map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_data_ptr = NULL;

	pid_fd_iter = peer_ptr->map_out_pid_fd.find(pid);

	if(pid_fd_iter == peer_ptr->map_out_pid_fd.end()) {
		return;
	} else {
		fd_queue_iter = peer_ptr->map_fd_out_data.find(pid_fd_iter->second);
		if(fd_queue_iter == peer_ptr->map_fd_out_data.end()) {
			return;
		} else {
			queue_out_data_ptr = fd_queue_iter->second;		
		}
	}
	
	debug_printf("clear_ouput_buffer size = %d \n",queue_out_data_ptr->size());
	_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"clear_ouput_buffer size =",queue_out_data_ptr->size());


	while(queue_out_data_ptr->size() != 0 ) {
		queue_out_data_ptr->pop();
	} 
}

void peer_mgr::set_up_public_ip(unsigned long public_ip)
{
    if (self_public_ip == 0) {
		self_public_ip = public_ip;
    }
}

//用來測試peer間的delay
void peer_mgr::send_test_delay(int sock,unsigned long manifest, UINT32 session_id)
{
	/* TODO: 如果發生dup pid, 直接取代舊的, 因為udp可能發生child關閉socket, 而parent卻不知道的情況 */
	int send_byte = 0;
//	char html_buf[BIG_CHUNK];
	struct chunk_delay_test_t *chunk_delay_ptr =NULL;
	//struct timeval detail_time;

	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;



	fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(sock);
	if(fd_queue_iter !=  peer_ptr ->map_fd_out_ctrl.end()){
		queue_out_ctrl_ptr =fd_queue_iter ->second;
	}else{
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] fd not here", __FUNCTION__, __LINE__);
		PAUSE
		return;
	}

	//	_net_ptr->set_blocking(sock);	// set to blocking

	chunk_delay_ptr = new struct chunk_delay_test_t;
	if(!(chunk_delay_ptr ) ){
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::chunk_delay_ptr  new error", __FUNCTION__, __LINE__);
	}
	struct chunk_t * html_buf = (struct chunk_t*)new unsigned char [BIG_CHUNK];
	if(!(html_buf ) ){
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::html_buf  new error", __FUNCTION__, __LINE__);
	}
	memset(html_buf, 0x0, sizeof(html_buf));
	memset(chunk_delay_ptr, 0x0, sizeof(struct chunk_delay_test_t));
	
	chunk_delay_ptr->header.cmd = CHNK_CMD_PEER_TEST_DELAY ;
	chunk_delay_ptr->header.length = (BIG_CHUNK -sizeof(chunk_delay_test_t)) ;	//pkt_buf paylod length
	chunk_delay_ptr->header.rsv_1 = REQ;
	//in this test, sequence_number is empty so use sent manifest
	chunk_delay_ptr->header.sequence_number = (unsigned long)manifest;  
//	chunk_delay_ptr->header.pid = _peer_mgr_ptr ->self_pid;
	chunk_delay_ptr->session_id = session_id;
	chunk_delay_ptr->pid = self_pid;  


	memcpy(html_buf, chunk_delay_ptr, sizeof(struct chunk_delay_test_t));
	
//	send_byte = _net_ptr->send(sock, html_buf, sizeof(html_buf), 0);
	queue_out_ctrl_ptr->push((struct chunk_t *)html_buf);

	if(queue_out_ctrl_ptr->size() != 0 ) {
		//_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 

	if (chunk_delay_ptr)
		delete chunk_delay_ptr;

	debug_printf("sent test delay OK !!  len = %d \n", html_buf->header.length);
	_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__, __LINE__, "sent test delay OK !!", html_buf->header.length);

/*
	if( send_byte <= 0 ) {
		data_close(sock, "send send_test_ delay cmd error");
//		_log_ptr->exit(0, "send send_test_ delay cmd error");
	} else {
		if(chunk_delay_ptr)
			delete chunk_delay_ptr;
		_net_ptr->set_nonblocking(sock);	// set to non-blocking
	}
*/
}

//用來測試peer間的delay
void peer_mgr::send_test_delay_udp(int sock, unsigned long manifest, UINT32 session_id)
{
	int send_byte = 0;
	struct chunk_delay_test_t *chunk_delay_ptr =NULL;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	map<unsigned long, int>::iterator map_pid_udpfd_iter;
	map<int, queue<struct chunk_t *> *>::iterator udpfd_queue_iter;

	udpfd_queue_iter = peer_ptr->map_udpfd_out_ctrl.find(sock);
	if (udpfd_queue_iter !=  peer_ptr ->map_udpfd_out_ctrl.end()) {
		queue_out_ctrl_ptr =udpfd_queue_iter ->second;
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] udpfd not here", __FUNCTION__, __LINE__);
		return;
	}

	chunk_delay_ptr = new struct chunk_delay_test_t;
	if (!chunk_delay_ptr) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::chunk_delay_ptr  new error", __FUNCTION__, __LINE__);
	}
	struct chunk_t * html_buf = (struct chunk_t*)new unsigned char [BIG_CHUNK];
	if (!html_buf) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::html_buf  new error", __FUNCTION__, __LINE__);
	}
	memset(html_buf, 0x0, sizeof(html_buf));
	memset(chunk_delay_ptr, 0x0, sizeof(struct chunk_delay_test_t));
	
	chunk_delay_ptr->header.cmd = CHNK_CMD_PEER_TEST_DELAY ;
	chunk_delay_ptr->header.length = (BIG_CHUNK -sizeof(chunk_delay_test_t)) ;	//pkt_buf paylod length 8192-24
	chunk_delay_ptr->header.rsv_1 = REQ;
	//in this test, sequence_number is empty so use sent manifest
	chunk_delay_ptr->header.sequence_number = (unsigned long)manifest;  
//	chunk_delay_ptr->header.pid = _peer_mgr_ptr ->self_pid;
	chunk_delay_ptr->session_id = session_id;  
	chunk_delay_ptr->pid = self_pid;  
	
	memcpy(html_buf, chunk_delay_ptr, sizeof(struct chunk_delay_test_t));
	
	queue_out_ctrl_ptr->push((struct chunk_t *)html_buf);

	if (chunk_delay_ptr) {
		delete chunk_delay_ptr;
	}

	_log_ptr->write_log_format("s(u) s d s u s d \n", __FUNCTION__, __LINE__, "sent CHNK_CMD_PEER_TEST_DELAY to", sock, "manifest", manifest, "session", session_id);
}



// Called by children-peer
//select_peer test delay
int peer_mgr::handle_test_delay(int session_id)
{
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, int> ::iterator map_pid_fd_iter;
	int sock;
	int pid;
	int sockOKcount=0;

	unsigned long list_num = 0;
	unsigned long connect_num = 0;
	unsigned long *list_array = NULL;
	unsigned long *connect_array = NULL;
	unsigned long manifest = 0;				// manifest of this session
	int offset = 0;
	list<unsigned long> list_member;
	list<unsigned long> connect_member;
	list<unsigned long>::iterator list_member_iter;
	list<unsigned long>::iterator connect_member_iter;
	unsigned char log_protocol = LOG_RESCUE_LIST;
	
	_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_pid_parent_temp.size() =", _pk_mgr_ptr->map_pid_parent_temp.size());

	for (map<unsigned long, int>::iterator iter = peer_ptr->map_in_pid_udpfd.begin(); iter != peer_ptr->map_in_pid_udpfd.end(); iter++) {
		_log_ptr->write_log_format("s(u) s u u d \n", __FUNCTION__, __LINE__, "map_in_pid_udpfd.size() =", peer_ptr->map_in_pid_udpfd.size(), iter->first, iter->second);
	}

	// 送 CHNK_CMD_PEER_TEST_DELAY 給符合此 session_id 的所有 parent
	for (pid_peer_info_iter = _pk_mgr_ptr->map_pid_parent_temp.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_parent_temp.end(); pid_peer_info_iter++) {
		
		//debug_printf("map_pid_parent_temp.size() = %d,  pid = %d \n", _pk_mgr_ptr->map_pid_parent_temp.size(), pid_peer_info_iter->first);
		_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
													"map_pid_parent_temp.size() =", _pk_mgr_ptr->map_pid_parent_temp.size(),
													"pid", pid_peer_info_iter->first,
													"manifest", pid_peer_info_iter ->second->manifest );
		if (pid_peer_info_iter->second->priority == session_id) {
			pid = pid_peer_info_iter->second->pid;
			manifest = pid_peer_info_iter->second->manifest;
			list_num++;
			list_member.push_back(pid);

			if (peer_ptr->map_in_pid_fd.find(pid) != peer_ptr->map_in_pid_fd.end()) {
				sock = peer_ptr->map_in_pid_fd[pid];
				_log_ptr->write_log_format("s(u) s d s u s u \n", __FUNCTION__, __LINE__, "sock", sock, "pid", pid, "manifest", manifest);

				connect_num++;
				connect_member.push_back(pid);

				send_test_delay(sock,manifest, session_id);
				sockOKcount++;
			}
			else if (peer_ptr->map_in_pid_udpfd.find(pid) != peer_ptr ->map_in_pid_udpfd.end()) {
				sock = peer_ptr->map_in_pid_udpfd[pid];
				_log_ptr->write_log_format("s(u) s d s u s u s d \n", __FUNCTION__, __LINE__, "sock", sock, "pid", pid, "manifest", manifest, "sock state", UDT::getsockstate(sock));

				connect_num++;
				connect_member.push_back(pid);

				send_test_delay_udp(sock,manifest, session_id);
				sockOKcount++;
			}
			else {
				// Not yet send PEER_CON, so can't find this peer in map_in_pid_fd
				// Close socket when "stop session" is called
				_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Not found pid", pid, "in map_in_pid_fd in session", session_id);
			}
			
			if(manifest == _pk_mgr_ptr ->full_manifest){
				log_protocol = LOG_REG_LIST;
			}
		}
		
	}

	// Log to server
	if(list_num != 0){
		list_array = (unsigned long *)new unsigned char[(sizeof(unsigned long) * list_num)];
		offset = 0;
		if(!(list_array ) ){
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::list_array  new error", __FUNCTION__, __LINE__);
		}

		for(list_member_iter = list_member.begin();list_member_iter != list_member.end();list_member_iter++){
			unsigned long temp = *list_member_iter;
			memcpy(list_array,&temp,sizeof(unsigned long));
			offset += sizeof(unsigned long);
		}
	}
	if(connect_num != 0){
		connect_array = (unsigned long *)new unsigned char[(sizeof(unsigned long) * connect_num)];
		offset = 0;
		if(!(connect_array ) ){
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::connect_array  new error", __FUNCTION__, __LINE__);
		}
		for(connect_member_iter = connect_member.begin();connect_member_iter != connect_member.end();connect_member_iter++){
			unsigned long temp = *connect_member_iter;
			memcpy(connect_array,&temp,sizeof(unsigned long));
			offset += sizeof(unsigned long);
		}
	}

	// 
	//_logger_client_ptr->log_to_server(log_protocol,manifest,list_num,connect_num,list_array,connect_array);

	return sockOKcount;
}

// Called by parent-peer, send "Block Rescue" to all children relating to this substream
void peer_mgr::SendBlockRescue(int ss_id, int type)
{
	for (map<unsigned long, struct peer_info_t *>::iterator iter = _pk_mgr_ptr->map_pid_child.begin(); iter != _pk_mgr_ptr->map_pid_child.end(); iter++) {
		unsigned long child_pid;
		int child_sock;
		struct peer_info_t *child_peer = NULL;
		queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
		bool can_push_data = true;

		child_pid = iter->first;			// Get child-Pid
		child_peer = iter->second;		// Get child info
		child_sock = iter->second->sock;

		//_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "Child", child_pid, "manifest", child_peer->manifest, "state", pid_peer_info_iter->second->connection_state);

		if (child_peer->connection_state == PEER_SELECTED && (child_peer->manifest & (1 << ss_id))) {

			if (peer_ptr->map_out_pid_fd.find(child_pid) != peer_ptr->map_out_pid_fd.end()) {
				if (peer_ptr->map_fd_out_ctrl.find(child_sock) != peer_ptr->map_fd_out_ctrl.end()) {
					queue_out_ctrl_ptr = peer_ptr->map_fd_out_ctrl[child_sock];	// Get queue_out_data_ptr
				}
			}
			else if (peer_ptr->map_out_pid_udpfd.find(child_pid) != peer_ptr->map_out_pid_udpfd.end()) {
				if (peer_ptr->map_udpfd_out_ctrl.find(child_sock) != peer_ptr->map_udpfd_out_ctrl.end()) {
					queue_out_ctrl_ptr = peer_ptr->map_udpfd_out_ctrl[child_sock];	// Get queue_out_data_ptr
				}
			}
			else {
				//debug_printf("Not found pid %d in map_out_pid_fd \n", child_pid);
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found pid", child_pid, "in map_out_pid_fd");
				//handle_error(UNKNOWN, "[DEBUG] Not found pid in map_out_pid_fd", __FUNCTION__, __LINE__);
				//continue;
			}

			if (queue_out_ctrl_ptr == NULL) {
				// 找不到它的 pointer, 直接跳過
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found pid", child_pid, "in map_out_pid_fd");
				continue;
			}

			struct chunk_block_rescue_t *chunk_block_rescue_ptr = new struct chunk_block_rescue_t;
			if (!chunk_block_rescue_ptr) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::chunk_block_rescue_ptr  new error", __FUNCTION__, __LINE__);
			}
			memset(chunk_block_rescue_ptr, 0, sizeof(struct chunk_block_rescue_t));

			chunk_block_rescue_ptr->header.cmd = CHNK_CMD_PEER_BLOCK_RESCUE;
			chunk_block_rescue_ptr->header.length = (sizeof(struct chunk_block_rescue_t) - sizeof(struct chunk_header_t));
			chunk_block_rescue_ptr->original_pid = self_pid;
			chunk_block_rescue_ptr->ss_id = ss_id;
			chunk_block_rescue_ptr->type = type;
			chunk_block_rescue_ptr->value = 0;

			queue_out_ctrl_ptr->push((struct chunk_t *)chunk_block_rescue_ptr);
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "send type", type, "ss_id", ss_id, "to child", child_pid);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u \n", "CHNK_CMD_PEER_BLOCK_RESCUE", self_pid, "to", child_pid, 0);
		}
	}
}

// Call by child-peer when receive the message, and relay to the children
void peer_mgr::HandleBlockRescue(struct chunk_block_rescue_t *chunk_ptr)
{
	unsigned long ss_id = chunk_ptr->ss_id;
	unsigned long type = chunk_ptr->type;
	unsigned long value = chunk_ptr->value;
	_pk_mgr_ptr->SetSubstreamBlockRescue(ss_id, type);

	for (map<unsigned long, struct peer_info_t *>::iterator iter = _pk_mgr_ptr->map_pid_child.begin(); iter != _pk_mgr_ptr->map_pid_child.end(); iter++) {
		unsigned long child_pid;
		int child_sock;
		struct peer_info_t *child_peer = NULL;
		queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
		bool can_push_data = true;

		child_pid = iter->first;			// Get child-Pid
		child_peer = iter->second;		// Get child info
		child_sock = iter->second->sock;

		//_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "Child", child_pid, "manifest", child_peer->manifest, "state", pid_peer_info_iter->second->connection_state);

		if (child_peer->connection_state == PEER_SELECTED && (child_peer->manifest & (1 << ss_id))) {

			if (peer_ptr->map_out_pid_fd.find(child_pid) != peer_ptr->map_out_pid_fd.end()) {
				if (peer_ptr->map_fd_out_ctrl.find(child_sock) != peer_ptr->map_fd_out_ctrl.end()) {
					queue_out_ctrl_ptr = peer_ptr->map_fd_out_ctrl[child_sock];	// Get queue_out_data_ptr
				}
			}
			else if (peer_ptr->map_out_pid_udpfd.find(child_pid) != peer_ptr->map_out_pid_udpfd.end()) {
				if (peer_ptr->map_udpfd_out_ctrl.find(child_sock) != peer_ptr->map_udpfd_out_ctrl.end()) {
					queue_out_ctrl_ptr = peer_ptr->map_udpfd_out_ctrl[child_sock];	// Get queue_out_data_ptr
				}
			}
			else {
				//debug_printf("Not found pid %d in map_out_pid_fd \n", child_pid);
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found pid", child_pid, "in map_out_pid_fd");
				//handle_error(UNKNOWN, "[DEBUG] Not found pid in map_out_pid_fd", __FUNCTION__, __LINE__);
				//continue;
			}

			if (queue_out_ctrl_ptr == NULL) {
				// 找不到它的 pointer, 直接跳過
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] Not found pid", child_pid, "in map_out_pid_fd");
				continue;
			}

			// queue_out_ctrl 的東西送出去後就會 delete，因此必須 malloc 給每一個 child-peer
			struct chunk_block_rescue_t *chunk_block_rescue_ptr = new struct chunk_block_rescue_t;
			if (!chunk_block_rescue_ptr) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_mgr::chunk_block_rescue_ptr  new error", __FUNCTION__, __LINE__);
			}
			memcpy(chunk_block_rescue_ptr, chunk_ptr, sizeof(struct chunk_block_rescue_t));
			chunk_block_rescue_ptr->value = value + 1;

			queue_out_ctrl_ptr->push((struct chunk_t *)chunk_block_rescue_ptr);
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "send type", chunk_block_rescue_ptr->type, "ss_id", chunk_block_rescue_ptr->ss_id, "to child", child_pid);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s (u)u s u u \n", "CHNK_CMD_PEER_BLOCK_RESCUE", chunk_block_rescue_ptr->original_pid, self_pid, "to", child_pid, chunk_block_rescue_ptr->value);
		}
	}
}

void peer_mgr::send_manifest_to_parent(UINT32 parent_pid, UINT32 manifest, UINT32 operation)
{
	struct chunk_manifest_set_t *chunk_manifestSetPtr = NULL;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	struct peer_connect_down_t *parent_info = _pk_mgr_ptr->GetParentFromPid(parent_pid);

	if (parent_info == NULL) {
		// 隨時可能因為收到 SEED 造成 table 被清除
		_log_ptr->write_log_format("s(u) s u s u u \n", __FUNCTION__, __LINE__, "parent_pid", parent_pid, "manifest", manifest, operation);
		return;
	}

	if (peer_ptr->map_udpfd_out_ctrl.find(parent_info->peerInfo.sock) != peer_ptr->map_udpfd_out_ctrl.end()) {
		queue_out_ctrl_ptr = peer_ptr->map_udpfd_out_ctrl[parent_info->peerInfo.sock];
	}
	else {
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "[DEBUG] parent_pid", parent_pid, "fd", parent_info->peerInfo.sock);
		//_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] udpfd not here", __FUNCTION__, __LINE__);
		return;
	}

	chunk_manifestSetPtr = new struct chunk_manifest_set_t;
	if (!chunk_manifestSetPtr) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] chunk_manifest_set_t  new error", __FUNCTION__, __LINE__);
		return;
	}
	memset(chunk_manifestSetPtr, 0, sizeof(struct chunk_manifest_set_t));

	chunk_manifestSetPtr->header.cmd = CHNK_CMD_PEER_SET_MANIFEST;
	chunk_manifestSetPtr->header.length = (sizeof(struct chunk_manifest_set_t) - sizeof(struct chunk_header_t));	//pkt_buf paylod length
	chunk_manifestSetPtr->header.rsv_1 = REQUEST;
	chunk_manifestSetPtr->pid = self_pid;
	chunk_manifestSetPtr->manifest = manifest;
	chunk_manifestSetPtr->manifest_op = operation;

	_net_udp_ptr->epoll_control(parent_info->peerInfo.sock, EPOLL_CTL_ADD, UDT_EPOLL_OUT);	// 打開 writable
	queue_out_ctrl_ptr->push((struct chunk_t *)chunk_manifestSetPtr);
	_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "send manifest", manifest, "to parent", parent_pid);
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s(u) s u s u \n", "my_pid", _pk_mgr_ptr->my_pid, "Send CHNK_CMD_PEER_SET_MANIFEST", __LINE__, "parent", parent_pid, "manifest", manifest);
}



void peer_mgr::handle_manifestSet(struct chunk_manifest_set_t *chunk_ptr)
{
	struct peer_info_t *rescuePeerInfoPtr = _pk_mgr_ptr->GetChildFromPid(chunk_ptr->pid);
	_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "child", chunk_ptr->pid, "OP", chunk_ptr->manifest_op);

	if (rescuePeerInfoPtr != NULL) {
		switch (chunk_ptr->manifest_op) {
		case TOTAL_OP:
			_pk_mgr_ptr->SetChildManifest(rescuePeerInfoPtr, chunk_ptr->manifest);
			break;
		case ADD_OP:
			_pk_mgr_ptr->SetChildManifest(rescuePeerInfoPtr, rescuePeerInfoPtr->manifest | chunk_ptr->manifest);
			break;
		case MINUS_OP:
			_pk_mgr_ptr->SetChildManifest(rescuePeerInfoPtr, rescuePeerInfoPtr->manifest & ~(chunk_ptr->manifest));
			break;
		}
	}
	else {
		// 曾經發生
		//_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] handle_manifestSet what happen", __FUNCTION__, __LINE__);
		debug_printf("[DEBUG] Not found child %d manifest %d in map_pid_child \n", chunk_ptr->pid, chunk_ptr->manifest);
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "[DEBUG] Not Found child", chunk_ptr->pid, "manifest", chunk_ptr->manifest);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s u s u s \n", __FUNCTION__, __LINE__, "[DEBUG] Not Found child", chunk_ptr->pid, "manifest", chunk_ptr->manifest);
	}
}

void peer_mgr::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;
	
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "peer_mgr", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".." << endl;
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
}

void peer_mgr::ArrangeResource()
{
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	for (pid_peerDown_info_iter = _pk_mgr_ptr->map_pid_parent.begin(); pid_peerDown_info_iter != _pk_mgr_ptr->map_pid_parent.end(); ) {
		struct peer_connect_down_t *parent_info = pid_peerDown_info_iter->second;
		unsigned long parent_pid = parent_info->peerInfo.pid;
		if (parent_info->peerInfo.manifest == 0) {
			
		}
	}
}



