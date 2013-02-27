/*

*/

#include "peer.h"
#include "network.h"
#include "logger.h"
#include "pk_mgr.h"
#include "peer_mgr.h"

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
//	_recv_byte_count = 0;
    _recv_parent_byte_count = 0;
//	_time_start = 0;
//	count = 0;
//	avg_bandwidth = 0;
//    parent_bandwidth = 0;
    parent_manifest = 0;
	fd_list_ptr = fd_list;
	_chunk_ptr = NULL;
	first_reply_peer =true;
	firstReplyPid=-1;
	leastSeq_set_childrenPID =0;
/*
	for(unsigned long i = 0; i < BANDWIDTH_BUCKET; i++) {
		bandwidth_bucket[i] = 0;
	}

*/

}

peer::~peer() 
{
	clear_map();

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
			if(chunk_ptr)
				delete chunk_ptr;
		}

		if(queue_out_ctrl_ptr)
			delete queue_out_ctrl_ptr;
	}

	for (iter = map_fd_out_data.begin(); iter != map_fd_out_data.end(); iter++) {
		queue_out_data_ptr = iter->second;
		while(queue_out_data_ptr->size()) {
			queue_out_data_ptr->pop();
			if(chunk_ptr)
				delete chunk_ptr;
		}

		if(queue_out_data_ptr)
			delete queue_out_data_ptr;
	}

}

void peer::peer_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr *pk_mgr_ptr, peer_mgr *peer_mgr_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
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

	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	map_out_pid_fd[chunk_request_ptr->info.pid] = sock;
	map_fd_pid[sock] = chunk_request_ptr->info.pid;
	map_fd_out_ctrl[sock] = queue_out_ctrl_ptr;
	map_fd_out_data[sock] = queue_out_data_ptr;

//	pid_peer_info_iter = _pk_mgr_ptr->map_pid_peer_info.find(chunk_request_ptr->info.pid);	//2013/01/24
	
//	if(pid_peer_info_iter == _pk_mgr_ptr->map_pid_peer_info.end()) {						//2013/01/24

	map_pid_rescue_peer_info_iter = _pk_mgr_ptr->map_pid_rescue_peer_info.find(chunk_request_ptr->info.pid);
	if(map_pid_rescue_peer_info_iter == _pk_mgr_ptr->map_pid_rescue_peer_info.end()){

		rescue_peer = new struct peer_info_t;
		memset(rescue_peer, 0x0 , sizeof(struct peer_info_t));
		rescue_peer->pid = chunk_request_ptr->info.pid;
		rescue_peer->public_ip = cin.sin_addr.s_addr;
		rescue_peer->private_ip = chunk_request_ptr->info.private_ip;
		rescue_peer->tcp_port = chunk_request_ptr->info.tcp_port;
		rescue_peer->udp_port = chunk_request_ptr->info.udp_port;
		_pk_mgr_ptr->map_pid_rescue_peer_info[rescue_peer->pid] = rescue_peer;
		printf("rescue_peer->pid = %d",rescue_peer->pid);
	}
//	}																			//2013/01/24
	
}

//只被build connect 呼叫
//送CHNK_CMD_PEER_ CON 到其他peer
int peer::handle_connect_request(int sock, struct level_info_t *level_info_ptr, unsigned long pid)
{
	map<unsigned long, int>::iterator map_pid_fd_iter;
	
	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	map_pid_fd_iter = map_in_pid_fd.find(level_info_ptr->pid);

	if(map_pid_fd_iter == map_in_pid_fd.end())
		map_in_pid_fd[level_info_ptr->pid] = sock;
	
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
	
	memset(chunk_request_ptr, 0x0, sizeof(struct chunk_request_msg_t));
		
	chunk_request_ptr->header.cmd = CHNK_CMD_PEER_CON;
	chunk_request_ptr->header.rsv_1 = REQUEST;
	chunk_request_ptr->header.length = sizeof(struct request_info_t);
	chunk_request_ptr->info.pid = pid;
	chunk_request_ptr->info.channel_id = channel_id;
	chunk_request_ptr->info.private_ip = _net_ptr->getLocalIpv4();
	chunk_request_ptr->info.tcp_port = (unsigned short)atoi(svc_tcp_port.c_str());
	chunk_request_ptr->info.udp_port = (unsigned short)atoi(svc_udp_port.c_str());

	send_byte = send(sock, (char *)chunk_request_ptr, sizeof(struct chunk_request_msg_t), 0);
	
	if( send_byte <= 0 ) {
		data_close(sock, "send html_buf error",CLOSE_PARENT);		
		_log_ptr->exit(0, "send html_buf error");
		return -1;
	} else {
		if(chunk_request_ptr)
			delete chunk_request_ptr;
		return 0;
	}

}

int peer::handle_pkt_in(int sock)
{
//	ftime(&interval_time);	//--!!0215
	unsigned long buf_len;
//	unsigned long i;
	int recv_byte;
	int expect_len;
	int offset = 0;
//	unsigned long total_bit_rate = 0;
//	unsigned long bandwidth = 0;
//	int different = 0;

	struct peer_info_t *peerInfoPtr;
//	unsigned long manifest;
//	unsigned long n_alpha;
//	unsigned char reply;

/*
	if(!_time_start) {
		_log_ptr->time_init();
		_time_start = 1;
	}
*/
	struct chunk_t *chunk_ptr = NULL;
	struct chunk_header_t *chunk_header_ptr = NULL;
	
	chunk_header_ptr = new struct chunk_header_t;
	memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));
	
	expect_len = sizeof(struct chunk_header_t) ;
	
	while (1) {
		//cout << "in sock = " << sock << endl;
		recv_byte = recv(sock, (char *)chunk_header_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
				
			} else {
				DBG_PRINTF("here\n");
				data_close(sock, "recv error in peer::handle_pkt_in",DONT_CARE);
				//PAUSE
				return RET_SOCK_ERROR;
			}
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		
		if (!expect_len)
			break;
	}
	
	expect_len = chunk_header_ptr->length;
	//cout << "sequence_number = " << chunk_header_ptr->sequence_number << endl;
	buf_len = sizeof(struct chunk_header_t) + expect_len;
	//cout << "buf_len = " << buf_len << endl;
	chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

	if (!chunk_ptr) {
		data_close(sock, "memory not enough",DONT_CARE);
		_log_ptr->exit(0, "memory not enough");
		return RET_SOCK_ERROR;
	}

	memset(chunk_ptr, 0x0, buf_len);
		
	memcpy(chunk_ptr, chunk_header_ptr, sizeof(struct chunk_header_t));

	if(chunk_header_ptr)
		delete chunk_header_ptr;
	
	while (1) {
		recv_byte = recv(sock, (char *)chunk_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
			} else {

				data_close(sock, "recv error in peer::handle_pkt_in",DONT_CARE);

				return RET_SOCK_ERROR;
			}
		}

		expect_len -= recv_byte;
		offset += recv_byte;
		if (expect_len == 0)
			break;
	}

	offset = 0;

//recv ok  

	
//CHNK_CMD_PEER_DATA
	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_DATA) {

//the main handle
		_pk_mgr_ptr->handle_stream(chunk_ptr, sock);

	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_START_DELAY_UPDATE) {
	//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
		struct update_start_delay *update_start_delay_ptr = NULL;
		update_start_delay_ptr = (update_start_delay *)chunk_ptr;
		for(int k=0;k<_pk_mgr_ptr->sub_stream_num;k++){
			(_pk_mgr_ptr->delay_table+k)->start_delay_struct.start_delay = (_pk_mgr_ptr->delay_table+k)->start_delay_struct.start_delay + update_start_delay_ptr->update_info[k]->start_delay_update;

			unsigned long temp_manifest = 0;
			temp_manifest = temp_manifest | (1<<k);

			_pk_mgr_ptr->send_start_delay_update(sock, temp_manifest, update_start_delay_ptr->update_info[k]->start_delay_update);
		}
	//////////////////////////////////////////////////////////////////////////////////
	}  else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_START_DELAY) {
	//////////////////////////////////////////////////////////////////////////////////measure start delay
		printf("CHNK_CMD_PEER_START_DELAY peer\n");
		if(chunk_ptr->header.rsv_1 == REQUEST){
			printf("CHNK_CMD_PEER_START_DELAY peer request\n");
			unsigned long temp_start_delay;
			unsigned long request_sub_id;

			memcpy(&request_sub_id, (char *)chunk_ptr + sizeof(struct chunk_header_t) + sizeof(unsigned long), sizeof(unsigned long));

			temp_start_delay = (_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay;
			_pk_mgr_ptr->send_back_start_delay_measure_token(sock, temp_start_delay, request_sub_id);
		}
		else{
			printf("CHNK_CMD_PEER_START_DELAY peer reply\n");
			unsigned long request_sub_id;
			long long parent_start_delay;

			memcpy(&request_sub_id, (char *)chunk_ptr + sizeof(struct chunk_header_t) + sizeof(unsigned long), sizeof(unsigned long));
			memcpy(&parent_start_delay, (char *)chunk_ptr + sizeof(struct chunk_header_t) + (2*sizeof(unsigned long)), sizeof(long long));

			if((_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.init_flag == 1){
				_log_ptr -> getTickTime(&((_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.end_clock));
				//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
				int renew_start_delay_flag=0;
				long long old_start_delay = (_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay;
				if((_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay != -1){
					renew_start_delay_flag = 1;
				}
				//////////////////////////////////////////////////////////////////////////////////
				(_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay = _log_ptr ->diffTime_ms((_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_clock,(_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.end_clock);
				(_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay = parent_start_delay + ((_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay/2);
				//(_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.init_flag = 1;
				printf("start delay : %ld\n",(_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay);
				//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
				if(renew_start_delay_flag == 1){
					printf("start delay renew \n");
					unsigned long temp_manifest = 0;

					int delay_differ = (_pk_mgr_ptr->delay_table+request_sub_id)->start_delay_struct.start_delay - old_start_delay;
					temp_manifest = temp_manifest | (1<<request_sub_id);
					_pk_mgr_ptr->send_start_delay_update(sock, temp_manifest, delay_differ);
				}
				//////////////////////////////////////////////////////////////////////////////////

				//////////////////////////////////////////////////////////////////////////////////send capacity
				_pk_mgr_ptr->peer_start_delay_count++;
				if((_pk_mgr_ptr->peer_start_delay_count == _pk_mgr_ptr->sub_stream_num)&&(!(_pk_mgr_ptr->peer_join_send))&&((_pk_mgr_ptr->delay_table+request_sub_id)->start_seq_num != 0)&&((_pk_mgr_ptr->delay_table+request_sub_id)->end_seq_num != 0)){
					_pk_mgr_ptr->send_capacity_to_pk(_pk_mgr_ptr->_sock);
					_pk_mgr_ptr->peer_join_send = 1;
				}
				//////////////////////////////////////////////////////////////////////////////////
			}
			else{
				printf("start_delay error\n");
			}
		}
	//////////////////////////////////////////////////////////////////////////////////

//cmd =CHNK_CMD_PEER_BWN
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_BWN) {




//cmd = CHNK_CMD_ PEER_RSC
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_RSC) {
//hidden at 2013/01/13




//cmd == CHNK_CMD_PEER_CUT
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_CUT) {
		//cout << "CHNK_CMD_PEER_CUT" << endl;

//hidden at 2013/01/27



//cmd == CHNK_CMD_PEER_LATENCY
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_LATENCY){
//hidden at 2013/01/16



//cmd == CHNK_CMD_RT_NLM
    } else if(chunk_ptr->header.cmd == CHNK_CMD_RT_NLM) {	//--!! 0128

//hidden at 2013/01/16


//	just send return
	}else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_TEST_DELAY ){

printf("CHNK_CMD_PEER_TEST_DELAY\n");

		if(chunk_ptr->header.rsv_1 == REQUEST){

			chunk_ptr->header.rsv_1 =REPLY;
			_net_ptr->set_blocking(sock);
			_net_ptr ->send (sock,(char*)chunk_ptr,sizeof(struct chunk_header_t) + chunk_ptr->header.length,0) ;
			_net_ptr->set_nonblocking(sock);

		}else if(chunk_ptr->header.rsv_1 ==REPLY){

			printf("CHNK_CMD_PEER_TEST_DELAY  REPLY\n");

			unsigned long replyManifest =chunk_ptr->header.sequence_number;
			//第一個回覆的peer 放入peer_connect_down_t加入測量 並關閉其他連線和清除所有相關table
			if(first_reply_peer){

				map_fd_pid_iter = map_fd_pid.find(sock);
				if(map_fd_pid_iter != map_fd_pid.end()){
					firstReplyPid = map_fd_pid_iter ->second;

					//在這裡面sequence_number 裡面塞的是manifest
					printf("first_reply_peer=%d  manifest =%d \n",firstReplyPid,replyManifest);

					pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(firstReplyPid);

					if(pid_peer_info_iter != _pk_mgr_ptr ->map_pid_peer_info.end()){


						for(int i=0 ;i< _pk_mgr_ptr ->map_pid_peer_info.count(firstReplyPid);i++){
							peerInfoPtr = pid_peer_info_iter->second;
							if (pid_peer_info_iter->second->manifest == replyManifest){
								_pk_mgr_ptr ->map_pid_peer_info.erase(pid_peer_info_iter);

								pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(firstReplyPid);
								if(pid_peerDown_info_iter == _pk_mgr_ptr ->map_pid_peerDown_info.end()){

									peerDownInfoPtr = new struct peer_connect_down_t ;
									memset(peerDownInfoPtr , 0x0,sizeof( struct peer_connect_down_t));
									memcpy(peerDownInfoPtr ,peerInfoPtr,sizeof(struct peer_info_t));
									delete peerInfoPtr;
									_pk_mgr_ptr ->map_pid_peerDown_info[firstReplyPid] =peerDownInfoPtr ;
								}else{
									pid_peerDown_info_iter ->second->peerInfo.manifest |= replyManifest;
							
								}
								break;

							}

								pid_peer_info_iter++;
							printf("test");
						}



						_peer_mgr_ptr -> send_manifest_to_parent(peerDownInfoPtr ->peerInfo.manifest ,firstReplyPid);

						//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
							for(int k=0;k<_pk_mgr_ptr->sub_stream_num;k++){
								if(peerDownInfoPtr->peerInfo.manifest & (1<<k)){
									_pk_mgr_ptr->send_start_delay_measure_token(sock, k);
								}
							}
						//////////////////////////////////////////////////////////////////////////////////

						for(pid_peer_info_iter =_pk_mgr_ptr ->map_pid_peer_info.begin();pid_peer_info_iter!= _pk_mgr_ptr ->map_pid_peer_info.end();pid_peer_info_iter++){
							peerInfoPtr = pid_peer_info_iter->second;

							if (peerInfoPtr->manifest == replyManifest){
							//若是自己或是先前已經建立過連線的parent 則不close (會關到正常連線)
							if(pid_peer_info_iter ->first == _peer_mgr_ptr ->self_pid){
								continue;
							}else if(_pk_mgr_ptr ->map_pid_peerDown_info.find(pid_peer_info_iter ->first) != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
								continue;
							}

							data_close(map_in_pid_fd[peerInfoPtr->pid ],"close by firstReplyPid",CLOSE_PARENT);
							}
							_pk_mgr_ptr ->clear_map_pid_peer_info(replyManifest);
						}
					}
				}

				first_reply_peer =false;
			}



		}

	}else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST){
		printf("CHNK_CMD_PEER_SET_MANIFEST\n");
		_peer_mgr_ptr ->handle_manifestSet((struct chunk_manifest_set_t *)chunk_ptr); 

		_pk_mgr_ptr->send_capacity_to_pk(_pk_mgr_ptr->_sock);
		
		map_fd_pid_iter= map_fd_pid.find(sock);
		if(map_fd_pid_iter !=map_fd_pid.end())
			_peer_mgr_ptr ->clear_ouput_buffer( map_fd_pid_iter->second);


	}else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_PARENT_CHILDREN){

		if(chunk_ptr->header.rsv_1 == REQUEST){
		
			_pk_mgr_ptr->handleAppenSelfdPid(chunk_ptr);
		
		}else if(chunk_ptr->header.rsv_1 == REPLY && ( leastSeq_set_childrenPID == chunk_ptr ->header.sequence_number )){
		
			_pk_mgr_ptr->storeChildrenToSet(chunk_ptr);
		
		}

	
	} else {
		printf ("%d   " , chunk_ptr->header.cmd);

		cout << "what's this?" << endl;
	}


	if (chunk_ptr)
		delete [] (unsigned char*)chunk_ptr;



	return RET_OK;
}

 
//送queue_out_ctrl_ptr 和queue_out_data_ptr出去
int peer::handle_pkt_out(int sock)
{
	struct chunk_t *chunk_ptr;
	
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;

	fd_out_ctrl_iter = map_fd_out_ctrl.find(sock);
	
	if(fd_out_ctrl_iter != map_fd_out_ctrl.end()) {
		queue_out_ctrl_ptr = fd_out_ctrl_iter->second;
	}
	
	fd_out_data_iter = map_fd_out_data.find(sock);

	if(fd_out_data_iter != map_fd_out_data.end()) {
		queue_out_data_ptr = fd_out_data_iter->second;
	}
	
	if(queue_out_ctrl_ptr->size() != 0 && _chunk_ptr == NULL) {
		chunk_ptr = queue_out_ctrl_ptr->front();
		_expect_len = chunk_ptr->header.length + sizeof(struct chunk_header_t);
		_expect_len = _expect_len - _offset;
		_send_byte = send(sock, (char *)chunk_ptr+_offset, _expect_len, 0);
		if(_send_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				return RET_SOCK_ERROR;
			} else {
				data_close(sock, "error occured in send queue_out_ctrl",DONT_CARE);
				return RET_SOCK_ERROR;
			}
		} else {
			_offset += _send_byte;
			_expect_len = _expect_len - _send_byte;
			if(_expect_len == 0) {
				queue_out_ctrl_ptr->pop();
				_offset = 0;
				if(chunk_ptr)
					delete chunk_ptr;
			}
		}
		
	} else if(queue_out_data_ptr->size() != 0) {
        //cout << "data buffer size =" << queue_out_data_ptr->size() <<endl;
		chunk_ptr = queue_out_data_ptr->front();
		_chunk_ptr = chunk_ptr;
		_expect_len = chunk_ptr->header.length + sizeof(struct chunk_header_t);
		_expect_len = _expect_len - _offset;
		_send_byte = send(sock, (char *)chunk_ptr+_offset, _expect_len, 0);
		if(_send_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				return RET_SOCK_ERROR;
			} else {
				data_close(sock, "error occured in send queue_out_data",DONT_CARE);
				//PAUSE
				return RET_SOCK_ERROR;
			}
		} else {
			_offset += _send_byte;
			_expect_len = _expect_len - _send_byte;
			if(_expect_len == 0) {
				queue_out_data_ptr->pop();
				_offset = 0;
				_chunk_ptr = NULL;
			}
		}
	
	} else {
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
	}

	return RET_OK;
}

void peer::handle_pkt_error(int sock)
{

}

void peer::handle_sock_error(int sock, basic_class *bcptr)
{

}

void peer::handle_job_realtime()
{

}

void peer::handle_job_timer()
{

}



//全部裡面最完整的close
////注意!!!!!!!!map_in_pid_fd 必須由關閉者自行判斷清除(pid -> fd 是一對多 所以可能會刪到其他的table)
//這邊的iter 都不參考class 的iter(因位會開thread 來close socket 會搶iter 使用)
////暫時不close  pid_peer_info 須自行清理
void peer::data_close(int cfd, const char *reason ,int type) 
{
	unsigned long pid = -1;
	list<int>::iterator fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator map_fd_queue_iter;
	map<int , unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
//	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

//	struct peer_info_t *peerInfoPtr = NULL;
//	struct peer_connect_down_t *peerDownInfoPtr = NULL;

//	map<int, int>::iterator map_rescue_fd_count_iter;
//	map<int, int>::iterator map_rescue_fd_count_iter2;
//	int sockfd;
//	list<unsigned long>::iterator rescue_pid_iter;
//	list<unsigned long>::iterator rescue_pid_list_iter;
//	unsigned long manifest = 0;	


	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "pk", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".." << endl;
//	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);
	
	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}

	map_fd_pid_iter = map_fd_pid.find(cfd);
	if(map_fd_pid_iter != map_fd_pid.end()) {
		pid = map_fd_pid_iter->second;
		map_fd_pid.erase(map_fd_pid_iter);
	}


	map_fd_queue_iter = map_fd_out_ctrl.find(cfd);
	if(map_fd_queue_iter != map_fd_out_ctrl.end()) 
		map_fd_out_ctrl.erase(map_fd_queue_iter);
	

	map_fd_queue_iter = map_fd_out_data.find(cfd);
	if(map_fd_queue_iter != map_fd_out_data.end()) 
		map_fd_out_data.erase(map_fd_queue_iter);


	if(type == CLOSE_PARENT){

		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(pid) ;
		if (pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end() ){
			peerDownInfoPtr = pid_peerDown_info_iter ->second ;
			delete peerDownInfoPtr;
			_pk_mgr_ptr ->map_pid_peerDown_info.erase(pid_peerDown_info_iter);

		}


		map_pid_fd_iter = map_in_pid_fd.find(pid);
		if(map_pid_fd_iter != map_in_pid_fd.end()) 
			map_in_pid_fd.erase(map_pid_fd_iter);


	}else if( type ==CLOSE_CHILD){
		

		map_pid_fd_iter = map_out_pid_fd.find(pid);
		if(map_pid_fd_iter != map_out_pid_fd.end()) 
			map_out_pid_fd.erase(map_pid_fd_iter);


		map_pid_rescue_peer_info_iter =_pk_mgr_ptr ->map_pid_rescue_peer_info.find(pid);
		if( map_pid_rescue_peer_info_iter != _pk_mgr_ptr ->map_pid_rescue_peer_info.end() ){
			peerInfoPtr = map_pid_rescue_peer_info_iter ->second ;
			delete peerInfoPtr;
			_pk_mgr_ptr ->map_pid_rescue_peer_info.erase(map_pid_rescue_peer_info_iter);

		}
	
	
	}else if( type ==DONT_CARE){
	
		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(pid) ;
		if (pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end() ){
			peerDownInfoPtr = pid_peerDown_info_iter ->second ;
			delete peerDownInfoPtr;
			_pk_mgr_ptr ->map_pid_peerDown_info.erase(pid_peerDown_info_iter);

		}

		map_pid_rescue_peer_info_iter =_pk_mgr_ptr ->map_pid_rescue_peer_info.find(pid);
		if( map_pid_rescue_peer_info_iter != _pk_mgr_ptr ->map_pid_rescue_peer_info.end() ){
			peerInfoPtr = map_pid_rescue_peer_info_iter ->second ;
			delete peerInfoPtr;
			_pk_mgr_ptr ->map_pid_rescue_peer_info.erase(map_pid_rescue_peer_info_iter);

		}

		map_pid_fd_iter = map_in_pid_fd.find(pid);
		if(map_pid_fd_iter != map_in_pid_fd.end()) 
			map_in_pid_fd.erase(map_pid_fd_iter);

		map_pid_fd_iter = map_out_pid_fd.find(pid);
		if(map_pid_fd_iter != map_out_pid_fd.end()) 
			map_out_pid_fd.erase(map_pid_fd_iter);

	}

//暫時不close  pid_peer_info_iter 
/*
	pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(pid);
	if(pid_peer_info_iter != _pk_mgr_ptr ->map_pid_peer_info.end()) {
		peerInfoPtr = pid_peer_info_iter ->second ;
		delete peerInfoPtr;
		_pk_mgr_ptr ->map_pid_peer_info.erase(pid_peer_info_iter);

	}
*/

}
