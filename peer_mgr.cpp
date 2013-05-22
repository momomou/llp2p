/*

*/

#include "peer_mgr.h"
#include "peer.h"
#include "network.h"
#include "logger.h"
#include "pk_mgr.h"

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

	if(peer_ptr)
		delete peer_ptr;

}

//初始化基本參數
void peer_mgr::peer_mgr_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr * pk_mgr_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;

	peer_ptr = new peer(fd_list_ptr);
	_pk_mgr_ptr ->peer_set(peer_ptr);
	peer_ptr->peer_set(_net_ptr, _log_ptr, _prep, _pk_mgr_ptr, this);		
}

//除自己之外和所有在同個lane 下的member做連線要求 呼叫build_connection連線
//只有註冊時收到peer list 才會呼叫
void peer_mgr::connect_peer(struct chunk_level_msg_t *level_msg_ptr, unsigned long pid)
{
    

	for(unsigned long i = 0; i < _pk_mgr_ptr->lane_member; i++) {
		if((level_msg_ptr->level_info[i])->pid != level_msg_ptr->pid) {
			if (build_connection(level_msg_ptr->level_info[i], pid)) {
				cout << "peer_mgr build _connection() success" << endl;
			} else {
				cout << "peer_mgr build_ connection() fail" << endl;
//				PAUSE
//				exit(0);
			}

		}
	}

}

//給定一個rescue 的list 然後隨機從list 挑一個peer
//hidden at 2013/01/23
///*
//int peer_mgr::connect_other_lane_peer(struct chunk_rescue_list_reply_t *rescue_list_reply_ptr, unsigned long peer_list_member, unsigned long pid, unsigned long outside_lane_rescue_num)



//利用level_info_ptr 連線到connect的狀態後 呼叫handle_connect_request(傳送request 到sock (其他peer) )
//最後把sock設成 nonblock 然後加入select 的監聽 EPOLLIN | EPOLLOUT ( 由peer 的obj做後續的傳送處理)
int peer_mgr::build_connection(struct level_info_t *level_info_ptr, unsigned long pid)
{
	struct sockaddr_in peer_saddr;
	int ret;
	struct in_addr ip;
	
	struct peer_info_t *peerInfoPtr =NULL;
	multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
//	struct in_addr selfip;
	map<unsigned long, int>::iterator map_pid_fd_iter;

	//之前已經建立過連線的 在map_in_pid_fd裡面 則不再建立(保證對同個parent不再建立第二條線)
	for(map_pid_fd_iter = peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
		if(map_pid_fd_iter->first == level_info_ptr->pid ){
			printf("pid =%d already in connect find in map_in_pid_fd  testing",level_info_ptr ->pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_in_pid_fd testing");
			return 1;
		}
	}


	pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(level_info_ptr ->pid);
	if(pid_peer_info_iter !=  _pk_mgr_ptr ->map_pid_peer_info.end() ){

		//兩個以上就沿用第一個的連線
		if(_pk_mgr_ptr ->map_pid_peer_info.count(level_info_ptr ->pid) >= 2 ){
			printf("pid =%d already in connect find in map_pid_peer_info  testing",level_info_ptr ->pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peer_info testing");
				return 1;
		}
	}

//若在map_pid_peerDown_info 則不再次建立連線
	pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(level_info_ptr ->pid);
	if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
		printf("pid =%d already in connect find in map_pid_peerDown_info",level_info_ptr ->pid);
		_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peerDown_info");
		return 1;
	}


	if((_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		cout << "init create socket failure" << endl;

		_net_ptr ->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
		return 0;
	}

	memset((struct sockaddr_in*)&peer_saddr, 0x0, sizeof(struct sockaddr_in));
	
//在同個NAT 底下
    if(self_public_ip == level_info_ptr->public_ip){
	    peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
		ip.s_addr = level_info_ptr->private_ip;
		printf("connect to private_ip %s port= %d \n" ,inet_ntoa (ip),level_info_ptr->tcp_port );
		_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"private_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );


	}else{
        peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
		ip.s_addr = level_info_ptr->public_ip;
//		selfip.s_addr = self_public_ip ;
		printf("connect to public %s  port= %d \n", inet_ntoa(ip),level_info_ptr->tcp_port);	
		_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"public_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );

	}

	peer_saddr.sin_port = htons(level_info_ptr->tcp_port);
	peer_saddr.sin_family = AF_INET;

	ip.s_addr = level_info_ptr->public_ip;
//	printf("connect to %s  port= %d \n", inet_ntoa(ip),level_info_ptr->tcp_port);	
	
	if(connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr)) < 0) {
		cout << "build_ connection failure" << endl;

//		PAUSE
#ifdef _WIN32
		::closesocket(_sock);
		::WSACleanup();
		data_close(_sock,"peer_mgr::build_connection") ;
#else
		::close(_sock);
#endif
		return 0;
	} else {
#ifdef _WIN32
		u_long iMode = 0;
		ioctlsocket(_sock, FIONBIO, &iMode);
#endif

		ret = peer_ptr->handle_connect_request(_sock, level_info_ptr, pid);

		if(ret < 0) {
			cout << "handle_connect_request error!!!" << endl;
			return 0;
		} else {
			cout << "_sock = " << _sock << endl;
//			_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"new socket =",_sock );

			_net_ptr->set_nonblocking(_sock);
			_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);	
			_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *>(peer_ptr));
			fd_list_ptr->push_back(_sock);
			return 1;
		}
	}
	

}


//只用來接收 CHNK_CMD_PEER_CON的資訊  並把fd 加入監聽
int peer_mgr::handle_pkt_in(int sock)
{
	int recv_byte;	
	int expect_len;
	int offset = 0;
	unsigned long buf_len;
	struct chunk_t *chunk_ptr = NULL;
	struct chunk_header_t *chunk_header_ptr = NULL;
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	
	socklen_t sin_len = sizeof(struct sockaddr_in);

	int new_fd = _net_ptr->accept(sock, (struct sockaddr *)&_cin, &sin_len);

	if(new_fd < 0) {
		return RET_SOCK_ERROR;
	} else {

		_net_ptr->set_nonblocking(new_fd);
		cout << "new_fd = " << new_fd << endl;   
		//PAUSE
	}
	
	chunk_header_ptr = new struct chunk_header_t;
	
	memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));
	
	expect_len = sizeof(struct chunk_header_t) ;
	
	while (1) {
		recv_byte = recv(new_fd, (char *)chunk_header_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
			} else {
				DBG_PRINTF("here\n");
				//continue;
				data_close(new_fd, "recv error in peer_mgr::handle_pkt_in");
				return RET_SOCK_ERROR;
				//PAUSE
				//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
			}
			
		}
		else if(recv_byte == 0){
			printf("sock closed\n");
			data_close(new_fd, "recv error in peer_mgr::handle_pkt_in");
				//PAUSE
			return RET_SOCK_ERROR;
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		
		if (!expect_len)
			break;
	}

	expect_len = chunk_header_ptr->length;
	
	buf_len = sizeof(struct chunk_header_t) + expect_len;
	cout << "buf_len = " << buf_len << endl;

	chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

	if (!chunk_ptr) {
		data_close(new_fd, "memory not enough");
		_log_ptr->exit(0, "memory not enough");
		return RET_SOCK_ERROR;
	}

	memset(chunk_ptr, 0x0, buf_len);
		
	memcpy(chunk_ptr, chunk_header_ptr, sizeof(struct chunk_header_t));

	if(chunk_header_ptr)
		delete chunk_header_ptr;
	
	while (1) {
		recv_byte = recv(new_fd, (char *)chunk_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
			} else {
				data_close(new_fd, "recv error in peer_mgr::handle_pkt_in");
				cout << "haha5" << endl;
				//PAUSE
				return RET_SOCK_ERROR;
				//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
			}
		}
		else if(recv_byte == 0){
			printf("sock closed\n");
			data_close(sock, "recv error in peer::handle_pkt_in");
				//PAUSE
			return RET_SOCK_ERROR;
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		if (expect_len == 0)
			break;
	}

	_net_ptr->set_nonblocking(new_fd);
	_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);	
	_net_ptr->set_fd_bcptr_map(new_fd, dynamic_cast<basic_class *>(peer_ptr));
	fd_list_ptr->push_back(new_fd);

	
	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_CON) {
		cout << "CHNK_CMD_PEER_CON" << endl;
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_CON ");
		peer_ptr->handle_connect(new_fd, chunk_ptr,_cin);

//	}  else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SYN) {
	//////////////////////////////////////////////////////////////////////////////////measure start delay
//	printf("CHNK_CMD_PEER_SYN not here peer_mgr\n");
//	PAUSE
	//////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
	} else{
	printf("what this\n");
	_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"what this at peer mgr ");
	PAUSE
	}

	if(chunk_ptr)
		delete chunk_ptr;

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
	
	printf("clear_ouput_buffer size = %d \n",queue_out_data_ptr->size());
	_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"clear_ouput_buffer size =",queue_out_data_ptr->size());


	while(queue_out_data_ptr->size() != 0 ) {
		queue_out_data_ptr->pop();
	} 
}



void peer_mgr::set_up_public_ip(unsigned long public_ip)
{
    if(self_public_ip == 0){

			    self_public_ip = public_ip;

    }
}

//用來測試peer間的delay
void peer_mgr::send_test_delay(int sock,unsigned long manifest)
{
	int send_byte = 0;
//	char html_buf[BIG_CHUNK];
	struct chunk_delay_test_t *chunk_delay_ptr =NULL;
	struct timeval detail_time;

	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;



	fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(sock);
	if(fd_queue_iter !=  peer_ptr ->map_fd_out_ctrl.end()){
	queue_out_ctrl_ptr =fd_queue_iter ->second;
	}else{
		printf("fd not here");
		PAUSE
		return;
	}

//	_net_ptr->set_blocking(sock);	// set to blocking
	
	chunk_delay_ptr = new struct chunk_delay_test_t;
	struct chunk_t * html_buf = (struct chunk_t*)new unsigned char [BIG_CHUNK];

	memset(html_buf, 0x0, sizeof(html_buf));
	memset(chunk_delay_ptr, 0x0, sizeof(struct chunk_delay_test_t));
	
	chunk_delay_ptr->header.cmd = CHNK_CMD_PEER_TEST_DELAY ;
	chunk_delay_ptr->header.length = (BIG_CHUNK -sizeof(chunk_delay_test_t)) ;	//pkt_buf paylod length
	chunk_delay_ptr->header.rsv_1 = REQUEST ;
	chunk_delay_ptr->header.timestamp = _log_ptr->gettimeofday_ms(&detail_time);
	//in this test, sequence_number is empty so use sent manifest
	chunk_delay_ptr->header.sequence_number = (unsigned long)manifest;  
//	chunk_delay_ptr->header.pid = _peer_mgr_ptr ->self_pid;
	


	memcpy(html_buf, chunk_delay_ptr, sizeof(struct chunk_delay_test_t));
	
//	send_byte = _net_ptr->send(sock, html_buf, sizeof(html_buf), 0);
	queue_out_ctrl_ptr->push((struct chunk_t *)html_buf);

	if(queue_out_ctrl_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 

	printf("sent test delay OK !!  len = %d\n",html_buf ->header.length);
	_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"sent test delay OK !!");

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
void peer_mgr::handle_test_delay(unsigned long manifest)
{
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, int> ::iterator map_pid_fd_iter;
	int sock;
	int pid;

	for(pid_peer_info_iter = (_pk_mgr_ptr ->map_pid_peer_info).begin(); pid_peer_info_iter != (_pk_mgr_ptr ->map_pid_peer_info).end(); pid_peer_info_iter++) {

		if(pid_peer_info_iter ->second->manifest == manifest){
			pid = (pid_peer_info_iter ->first) ;

			map_pid_fd_iter = peer_ptr ->map_in_pid_fd.find(pid);
			if(map_pid_fd_iter != peer_ptr ->map_in_pid_fd.end() ){
				sock =peer_ptr ->map_in_pid_fd [pid] ;
				printf("pid : %d sock : %d iter : %d\n",pid,sock,map_pid_fd_iter->second);
//				_log_ptr->write_log_format("s =>u s u s d s u \n", __FUNCTION__,__LINE__,"pid",pid,"sock",sock,map_pid_fd_iter->second,"manifest",manifest);

				send_test_delay (sock,manifest);
			}
		}

	}
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
		printf("pid not here");
		PAUSE
		return;
	}

	fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(parentSock);
	if(fd_queue_iter !=  peer_ptr ->map_fd_out_ctrl.end()){
	queue_out_ctrl_ptr =fd_queue_iter ->second;
	}else{
		printf("fd not here");
		PAUSE
		return;
	}

	
	chunk_manifestSetPtr = new struct chunk_manifest_set_t;

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

	printf("children pid= %u set manifest=%d\n",rescuePeerInfoPtr ->pid,rescuePeerInfoPtr ->manifest);
	_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"children pid=",rescuePeerInfoPtr ->pid,"set manifest=",rescuePeerInfoPtr ->manifest);


	}else{
	printf("handle_manifestSet what happen\n");
	PAUSE
	}
	
	//如果Substream 的數量是變少的話 ,只有在給的串流變少的時候才Clean


}




void peer_mgr::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;
	
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "pk", reason);
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



