/*

*/

#include "peer_mgr.h"
#include "peer.h"
#include "network.h"
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
	debug_printf("==============deldet peer_mgr success==========\n");

	if(peer_ptr)
		delete peer_ptr;

}

void peer_mgr::peer_communication_set(peer_communication *peer_communication_ptr){
	_peer_communication_ptr = peer_communication_ptr;
}

//初始化基本參數
void peer_mgr::peer_mgr_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr * pk_mgr_ptr, logger_client * logger_client_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;
	_logger_client_ptr = logger_client_ptr;

	peer_ptr = new peer(fd_list_ptr);
	_pk_mgr_ptr ->peer_set(peer_ptr);
	peer_ptr->peer_set(_net_ptr, _log_ptr, _prep, _pk_mgr_ptr, this,logger_client_ptr);		
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
//其實就只是把 chunk_ptr 丟到queue_out_data_ptr 裡面  並把把監聽設為EPOLLOUT , 前提是要 在map_pid_peer_info ,map_out_pid_fd 留有資訊
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
void peer_mgr::send_test_delay(int sock,unsigned long manifest)
{
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
		debug_printf("fd not here\n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd not here");
		*(_net_ptr->_errorRestartFlag) =RESTART;
		PAUSE
		return;
	}

	//	_net_ptr->set_blocking(sock);	// set to blocking

	chunk_delay_ptr = new struct chunk_delay_test_t;
	if(!(chunk_delay_ptr ) ){
		debug_printf("peer_mgr::chunk_delay_ptr  new error \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"chunk_delay_ptr new error");
		PAUSE
	}
	struct chunk_t * html_buf = (struct chunk_t*)new unsigned char [BIG_CHUNK];
	if(!(html_buf ) ){
		debug_printf("peer_mgr::html_buf  new error \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"html_buf new error");
		PAUSE
	}
	memset(html_buf, 0x0, sizeof(html_buf));
	memset(chunk_delay_ptr, 0x0, sizeof(struct chunk_delay_test_t));
	
	chunk_delay_ptr->header.cmd = CHNK_CMD_PEER_TEST_DELAY ;
	chunk_delay_ptr->header.length = (BIG_CHUNK -sizeof(chunk_delay_test_t)) ;	//pkt_buf paylod length
	chunk_delay_ptr->header.rsv_1 = REQUEST ;
	//in this test, sequence_number is empty so use sent manifest
	chunk_delay_ptr->header.sequence_number = (unsigned long)manifest;  
//	chunk_delay_ptr->header.pid = _peer_mgr_ptr ->self_pid;
	


	memcpy(html_buf, chunk_delay_ptr, sizeof(struct chunk_delay_test_t));
	
//	send_byte = _net_ptr->send(sock, html_buf, sizeof(html_buf), 0);
	queue_out_ctrl_ptr->push((struct chunk_t *)html_buf);

	if(queue_out_ctrl_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
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




//select_peer test delay
int peer_mgr::handle_test_delay(unsigned long manifest)
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
	int offset = 0;
	list<unsigned long> list_member;
	list<unsigned long> connect_member;
	list<unsigned long>::iterator list_member_iter;
	list<unsigned long>::iterator connect_member_iter;
	unsigned char log_protocol = LOG_RESCUE_LIST;

	for(pid_peer_info_iter = (_pk_mgr_ptr ->map_pid_peer_info).begin(); pid_peer_info_iter != (_pk_mgr_ptr ->map_pid_peer_info).end(); pid_peer_info_iter++) {

		if(pid_peer_info_iter ->second->manifest == manifest){
			pid = (pid_peer_info_iter ->first) ;

			list_num++;
			list_member.push_back(pid);

			//testing in PC room avoid connect to self
			if(pid_peer_info_iter->second->public_ip == self_public_ip && pid_peer_info_iter->second->private_ip == _net_ptr->getLocalIpv4()){
				continue;
			}

			map_pid_fd_iter = peer_ptr ->map_in_pid_fd.find(pid);
			if(map_pid_fd_iter != peer_ptr ->map_in_pid_fd.end() ){
				sock =peer_ptr ->map_in_pid_fd [pid] ;
				debug_printf("pid : %d sock : %d iter : %d\n",pid,sock,map_pid_fd_iter->second);
//				_log_ptr->write_log_format("s =>u s u s d s u \n", __FUNCTION__,__LINE__,"pid",pid,"sock",sock,map_pid_fd_iter->second,"manifest",manifest);

				connect_num++;
				connect_member.push_back(pid);

				send_test_delay (sock,manifest);
				sockOKcount++;

			}

			if(manifest == _pk_mgr_ptr ->full_manifest){
				log_protocol = LOG_REG_LIST;
			}
		}
	
	}

	if(list_num != 0){
		list_array = (unsigned long *)new unsigned char[(sizeof(unsigned long) * list_num)];
		offset = 0;
		if(!(list_array ) ){
			debug_printf("peer_mgr::list_array  new error \n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"list_array new error");
			PAUSE
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
			debug_printf("peer_mgr::connect_array  new error \n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"connect_array new error");
			PAUSE
		}
		for(connect_member_iter = connect_member.begin();connect_member_iter != connect_member.end();connect_member_iter++){
			unsigned long temp = *connect_member_iter;
			memcpy(connect_array,&temp,sizeof(unsigned long));
			offset += sizeof(unsigned long);
		}
	}

	_logger_client_ptr->log_to_server(log_protocol,manifest,list_num,connect_num,list_array,connect_array);

	//all fail send topology
	unsigned long tempManifest =manifest ;
	unsigned long sendSubStreamID;
	if(sockOKcount == 0){
		_logger_client_ptr->log_to_server(LOG_TEST_DELAY_FAIL,manifest);
		_logger_client_ptr->log_to_server(LOG_DATA_COME_PK,manifest);

		while(tempManifest){
				sendSubStreamID = _pk_mgr_ptr->manifestToSubstreamID (tempManifest);
				_pk_mgr_ptr ->set_rescue_state(sendSubStreamID,0);
				_pk_mgr_ptr->send_parentToPK( _pk_mgr_ptr->SubstreamIDToManifest(sendSubStreamID) ,PK_PID +1);
				tempManifest &=  (~ _pk_mgr_ptr->SubstreamIDToManifest(sendSubStreamID)) ;
		}
	}



	return sockOKcount;


}

void peer_mgr::send_manifest_to_parent(unsigned long manifestValue,unsigned long parentPid )
{

	int parentSock;
	struct chunk_manifest_set_t *chunk_manifestSetPtr =NULL;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;


	map_pid_fd_iter = peer_ptr ->map_in_pid_fd.find(parentPid);
	if(map_pid_fd_iter !=  peer_ptr ->map_in_pid_fd.end()){
		parentSock =map_pid_fd_iter ->second;
	}else{
		debug_printf("pid not here");
		PAUSE
		return;
	}

	fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(parentSock);
	if(fd_queue_iter !=  peer_ptr ->map_fd_out_ctrl.end()){
	queue_out_ctrl_ptr =fd_queue_iter ->second;
	}else{
		debug_printf("fd not here\n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd not here");
		*(_net_ptr->_errorRestartFlag) =RESTART;
		PAUSE
		return;
	}


	chunk_manifestSetPtr = new struct chunk_manifest_set_t;
	if(!(chunk_manifestSetPtr ) ){
		debug_printf("peer_mgr::chunk_manifestSetPtr  new error \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"chunk_manifestSetPtr new error");
		PAUSE
	}
	memset(chunk_manifestSetPtr, 0x0, sizeof(struct chunk_manifest_set_t));
	
	chunk_manifestSetPtr->header.cmd = CHNK_CMD_PEER_SET_MANIFEST ;
	chunk_manifestSetPtr->header.length = (sizeof(struct chunk_manifest_set_t)-sizeof(struct chunk_header_t)) ;	//pkt_buf paylod length
	chunk_manifestSetPtr->header.rsv_1 = REQUEST ;
	chunk_manifestSetPtr->pid = self_pid ;
	chunk_manifestSetPtr->manifest = manifestValue ;

//	_net_ptr ->set_blocking(parentSock);
//	_net_ptr->send(parentSock,(char*)chunk_manifestSetPtr,sizeof(struct chunk_manifest_set_t),0);
//	_net_ptr ->set_nonblocking(parentSock);
//	if(chunk_manifestSetPtr){
//		delete chunk_manifestSetPtr;
//		chunk_manifestSetPtr =NULL;
//	}

	queue_out_ctrl_ptr->push((struct chunk_t *)chunk_manifestSetPtr);
	_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"push into queue_out_ctrl, queue_out_ctrl_ptr->size() =", queue_out_ctrl_ptr->size());
	
	if(queue_out_ctrl_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 



	_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"sent to parentPid=",parentPid,"manifestValue",manifestValue);
	 
}




void peer_mgr::handle_manifestSet(struct chunk_manifest_set_t *chunk_ptr)
{
	map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;
	struct peer_info_t *rescuePeerInfoPtr ;


	map_pid_rescue_peer_info_iter = _pk_mgr_ptr->map_pid_rescue_peer_info.find( chunk_ptr ->pid);
	
	if(map_pid_rescue_peer_info_iter !=  _pk_mgr_ptr->map_pid_rescue_peer_info.end()){
	rescuePeerInfoPtr = map_pid_rescue_peer_info_iter ->second;

		//如果Substream 的數量是變少的話 ,只有在給的串流變少的時候才Clean
/*
		if(_pk_mgr_ptr->manifestToSubstreamNum (chunk_ptr ->manifest) < _pk_mgr_ptr->manifestToSubstreamNum(rescuePeerInfoPtr ->manifest)){
		//clear_ouput_buffer( chunk_ptr ->pid);

		_pk_mgr_ptr->stopsleep++ ;
		}
*/

	rescuePeerInfoPtr ->manifest = chunk_ptr ->manifest ;

	debug_printf("children pid= %u set manifest=%d\n",rescuePeerInfoPtr ->pid,rescuePeerInfoPtr ->manifest);
	_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"children pid=",rescuePeerInfoPtr ->pid,"set manifest=",rescuePeerInfoPtr ->manifest);


	}else{
		debug_printf("handle_manifestSet what happen\n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"handle_manifestSet what happen");
		*(_net_ptr->_errorRestartFlag) =RESTART;
		PAUSE
	}
	
	//如果Substream 的數量是變少的話 ,只有在給的串流變少的時候才Clean


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



