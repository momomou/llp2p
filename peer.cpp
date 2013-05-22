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
	//first_reply_peer =true;
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

	Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff ;
	memset(Nonblocking_Buff_ptr, 0x0 , sizeof(Nonblocking_Buff));
	map_fd_nonblocking_ctl[sock] = Nonblocking_Buff_ptr ;
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
		_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"rescue_peer->pid =",rescue_peer->pid);
	}else{

		printf("fd error why dup");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd error why dup");
		PAUSE
		
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
	
	Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff ;
	memset(Nonblocking_Buff_ptr, 0x0 , sizeof(Nonblocking_Buff));
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

	int offset = 0;
	int recv_byte=0;
	Nonblocking_Ctl * Nonblocking_Recv_ptr =NULL;
	struct chunk_header_t* chunk_header_ptr = NULL;
	struct chunk_t* chunk_ptr = NULL;
	unsigned long buf_len=0;

	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;


	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(sock);
	if(map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) {
		Nonblocking_Recv_ptr = &(map_fd_nonblocking_ctl_iter->second->nonBlockingRecv);
		if(Nonblocking_Recv_ptr ->recv_packet_state == 0){
			Nonblocking_Recv_ptr ->recv_packet_state =READ_HEADER_READY ;
		}

	}else{
		printf("Nonblocking_Buff_ptr NOT FIND \n");
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"Nonblocking_Buff_ptr NOT FIND");
		PAUSE
		return RET_SOCK_ERROR;
	}


	for(int i =0;i<5;i++){


		if(Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_READY){
	
		
			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

			Nonblocking_Recv_ptr ->recv_ctl_info.offset =0 ;
			Nonblocking_Recv_ptr ->recv_ctl_info.total_len = sizeof(chunk_header_t) ;
			Nonblocking_Recv_ptr ->recv_ctl_info.expect_len = sizeof(chunk_header_t) ;
			Nonblocking_Recv_ptr ->recv_ctl_info.buffer = (char *)chunk_header_ptr ;


		}else if(Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_RUNNING){

			//do nothing

		}else if(Nonblocking_Recv_ptr->recv_packet_state == READ_HEADER_OK){

			buf_len = sizeof(chunk_header_t)+ ((chunk_t *)(Nonblocking_Recv_ptr->recv_ctl_info.buffer)) ->header.length ;
			chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

//			printf("buf_len %d \n",buf_len);

			memset(chunk_ptr, 0x0, buf_len);

			memcpy(chunk_ptr,Nonblocking_Recv_ptr->recv_ctl_info.buffer,sizeof(chunk_header_t));

			if (Nonblocking_Recv_ptr->recv_ctl_info.buffer)
			delete [] (unsigned char*)Nonblocking_Recv_ptr->recv_ctl_info.buffer ;

			Nonblocking_Recv_ptr ->recv_ctl_info.offset =sizeof(chunk_header_t) ;
			Nonblocking_Recv_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length ;
			Nonblocking_Recv_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length ;
			Nonblocking_Recv_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;

//			printf("chunk_ptr->header.length = %d  seq = %d\n",chunk_ptr->header.length,chunk_ptr->header.sequence_number);
			Nonblocking_Recv_ptr->recv_packet_state = READ_PAYLOAD_READY ;

		}else if(Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_READY){
		
			//do nothing
//			printf("READ_PAYLOAD_READY not here\n");
//			PAUSE

		}else if(Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_RUNNING){
	
			//do nothing
	
		}else if(Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_OK){
		
//			chunk_ptr =(chunk_t *)Recv_nonblocking_ctl_ptr ->recv_ctl_info.buffer;

//			Recv_nonblocking_ctl_ptr->recv_packet_state = READ_HEADER_READY ;

			break;
		
		}

		recv_byte =_net_ptr->nonblock_recv(sock,Nonblocking_Recv_ptr);


		if(recv_byte < 0) {
			data_close(sock, "error occured in peer recv ",DONT_CARE);
			
			//PAUSE
			return RET_SOCK_ERROR;
		}


	}

	if(Nonblocking_Recv_ptr->recv_packet_state == READ_PAYLOAD_OK){
			
		chunk_ptr =(chunk_t *)Nonblocking_Recv_ptr ->recv_ctl_info.buffer;

		Nonblocking_Recv_ptr->recv_packet_state = READ_HEADER_READY ;
	
	}else{
	
		//other stats
		return RET_OK;

	}



/*

	unsigned long buf_len;
	int recv_byte;
	int expect_len;
	int offset = 0;


	struct peer_info_t *peerInfoPtr;


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
//				DBG_PRINTF("here\n");
				data_close(sock, "recv error in peer::handle_pkt_in",DONT_CARE);
				//PAUSE
				return RET_SOCK_ERROR;
			}
		}
		else if(recv_byte == 0){
			printf("sock closed\n");
			data_close(sock, "recv error in peer::handle_pkt_in",DONT_CARE);
				//PAUSE
			return RET_SOCK_ERROR;
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
		else if(recv_byte == 0){
			printf("sock closed\n");
			data_close(sock, "recv error in peer::handle_pkt_in",DONT_CARE);
				//PAUSE
			return RET_SOCK_ERROR;
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		if (expect_len == 0)
			break;
	}

	offset = 0;



*/


//recv ok  
	
//CHNK_CMD_PEER_DATA
	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_DATA) {

//the main handle

		_pk_mgr_ptr->handle_stream(chunk_ptr, sock);

		//不刪除 chunk_ptr 全權由handle_stream處理
		return RET_OK;

//	just send return
	}else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_TEST_DELAY ){


		if(chunk_ptr->header.rsv_1 == REQUEST){


			queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
			map<unsigned long, int>::iterator map_pid_fd_iter;
			map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;



			fd_queue_iter = map_fd_out_ctrl.find(sock);
			if(fd_queue_iter !=  map_fd_out_ctrl.end()){
				queue_out_ctrl_ptr =fd_queue_iter ->second;
			}else{
				printf("fd not here");
				PAUSE
					return RET_OK;
			}

			printf("CHNK_CMD_PEER_TEST_DELAY REQUEST\n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_TEST_DELAY REQUEST");


			chunk_ptr->header.rsv_1 =REPLY;


			queue_out_ctrl_ptr->push((struct chunk_t *)chunk_ptr);

			if(queue_out_ctrl_ptr->size() != 0 ) {
				_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
			} 

			return RET_OK;
//			_net_ptr->set_blocking(sock);
//			_net_ptr ->send (sock,(char*)chunk_ptr,sizeof(struct chunk_header_t) + chunk_ptr->header.length,0) ;
//			_net_ptr->set_nonblocking(sock);

		}else if(chunk_ptr->header.rsv_1 ==REPLY){

			printf("CHNK_CMD_PEER_TEST_DELAY  REPLY\n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_TEST_DELAY  REPLY");

			unsigned long replyManifest =chunk_ptr->header.sequence_number;
			printf("REPLY  manifest =%d \n",replyManifest);
			_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"REPLY  manifest =",replyManifest);

			//第一個回覆的peer 放入peer_connect_down_t加入測量 並關閉其他連線和清除所有相關table

			substream_first_reply_peer_iter = substream_first_reply_peer.find(replyManifest);
			if(substream_first_reply_peer_iter == substream_first_reply_peer.end()){
				substream_first_reply_peer[replyManifest] = new manifest_timmer_flag ;
				memset(substream_first_reply_peer[replyManifest] ,sizeof(manifest_timmer_flag),0);
				substream_first_reply_peer[replyManifest] ->firstReplyFlag =true ;
			}

			substream_first_reply_peer_iter = substream_first_reply_peer.find(replyManifest);
			if(substream_first_reply_peer_iter == substream_first_reply_peer.end()){
				printf("error : can not find subid_replyManifest in CHNK_CMD_PEER_TEST_DELAY  REPLY\n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"error : can not find subid_replyManifest in CHNK_CMD_PEER_TEST_DELAY  REPLY");
				exit(1);
			}

			if(substream_first_reply_peer_iter->second->firstReplyFlag){

				map_fd_pid_iter = map_fd_pid.find(sock);
				if(map_fd_pid_iter != map_fd_pid.end()){
					firstReplyPid = map_fd_pid_iter ->second;

					//在這裡面sequence_number 裡面塞的是manifest
					printf("first_reply_peer=%d  manifest =%d \n",firstReplyPid,replyManifest);
					_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"first_reply_peer=",firstReplyPid,"manifest",replyManifest);

					pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(firstReplyPid);

					if(pid_peer_info_iter != _pk_mgr_ptr ->map_pid_peer_info.end()){


						for(unsigned long i=0 ;i< _pk_mgr_ptr ->map_pid_peer_info.count(firstReplyPid);i++){
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
									peerDownInfoPtr->peerInfo.manifest |= replyManifest;
								}else{
									peerDownInfoPtr = pid_peerDown_info_iter->second;
									peerDownInfoPtr->peerInfo.manifest |= replyManifest;
							
								}

								break;

							}

								pid_peer_info_iter++;

						}


						//要的是全部的串流且是第二個加入table的 (join且 自己不是seed) (第一個是PK ) 則送拓墣
						if((replyManifest == _pk_mgr_ptr ->full_manifest) && (_pk_mgr_ptr ->map_pid_peerDown_info.size() == 2) ){

							for(unsigned long substreamID =0 ; substreamID < _pk_mgr_ptr->sub_stream_num ;substreamID++)
							{
								_pk_mgr_ptr ->send_parentToPK ( _pk_mgr_ptr ->SubstreamIDToManifest(substreamID) , PK_PID+1 );
							}

//							_pk_mgr_ptr ->send_rescueManifestToPKUpdate(0);

						}/*else{
							//如果這個 peer 來的chunk 裡的substream 從pk和這個peer都有來  (if rescue testing stream)
							

							
								peerDownInfoPtr->outBuffCount = 0;
								(_pk_mgr_ptr->ssDetect_ptr + _pk_mgr_ptr->manifestToSubstreamID(replyManifest)) ->isTesting =1 ;	//ture
								printf("SSID = %d start testing stream\n",_pk_mgr_ptr->manifestToSubstreamID(replyManifest));
								_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"start testing stream SSID = ",_pk_mgr_ptr->manifestToSubstreamID(replyManifest));

								//這邊只是暫時改變PK的substream 實際上還是有串流下來
								_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"PK old manifest",_pk_mgr_ptr->pkDownInfoPtr->peerInfo.manifest);
								_pk_mgr_ptr->pkDownInfoPtr->peerInfo.manifest &= (~replyManifest);
								_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"for tetsing stream PK manifest temp change to ",_pk_mgr_ptr->pkDownInfoPtr->peerInfo.manifest);

								//開始testing 送topology
								_pk_mgr_ptr->send_parentToPK ( _pk_mgr_ptr->SubstreamIDToManifest (_pk_mgr_ptr->manifestToSubstreamID(replyManifest)) , (_pk_mgr_ptr->ssDetect_ptr + temp_sub_id)->previousParentPID ); 

								//testing function
								_pk_mgr_ptr->reSet_detectionInfo();
							
						
						} */


						_peer_mgr_ptr -> send_manifest_to_parent(peerDownInfoPtr ->peerInfo.manifest ,firstReplyPid);
						_pk_mgr_ptr->reSet_detectionInfo();

						printf("sent to parent manifest = %d\n",peerDownInfoPtr ->peerInfo.manifest);
						_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"first_reply_peer=",firstReplyPid,"manifest",peerDownInfoPtr ->peerInfo.manifest);


//						}


						for(pid_peer_info_iter =_pk_mgr_ptr ->map_pid_peer_info.begin();pid_peer_info_iter!= _pk_mgr_ptr ->map_pid_peer_info.end();pid_peer_info_iter++){
							peerInfoPtr = pid_peer_info_iter->second;

							if (peerInfoPtr->manifest == replyManifest){
							//若是自己或是先前已經建立過連線的parent 則不close (會關到正常連線)  
								if(pid_peer_info_iter ->first == _peer_mgr_ptr ->self_pid){
									continue;
								}else if(_pk_mgr_ptr ->map_pid_peerDown_info.find(pid_peer_info_iter ->first) != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
									continue;
								}

								/*else if (_pk_mgr_ptr ->map_pid_rescue_peer_info.find(pid_peer_info_iter ->first) !=_pk_mgr_ptr ->map_pid_rescue_peer_info.end()){
									continue;
								}*/

								if(map_in_pid_fd.find( peerInfoPtr->pid ) != map_in_pid_fd.end()){
									//只剩最後一個才關socket
									if(map_in_pid_fd.count(peerInfoPtr->pid) == 1)
										data_close(map_in_pid_fd[peerInfoPtr->pid ],"close by firstReplyPid ",CLOSE_PARENT);

//									pid_peer_info_iter  = _pk_mgr_ptr ->map_pid_peer_info.begin() ;
									//刪掉最後一個  離開
//									if(pid_peer_info_iter == _pk_mgr_ptr ->map_pid_peer_info.end())
//										break;
								}else{

//若在傳送期間PARENT socket error 則應該是可忽略
									printf("testdelay 485 \n");
									_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"testdelay 485 ");
//									PAUSE
								}
							}
						}
						_pk_mgr_ptr ->clear_map_pid_peer_info(replyManifest);

					}else{
						printf("what peer.cpp 492!?");
						_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"what peer.cpp 492!?");
						PAUSE
					}
				}

				substream_first_reply_peer_iter->second->firstReplyFlag =false;
			}

		} // END ...  else if(chunk_ptr->header.rsv_1 ==REPLY){

	}else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_SET_MANIFEST){
		printf("CHNK_CMD_PEER_SET_MANIFEST\n");
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_SET_MANIFEST");

		_peer_mgr_ptr ->handle_manifestSet((struct chunk_manifest_set_t *)chunk_ptr); 

		if(((struct chunk_manifest_set_t *)chunk_ptr) ->manifest ==0 ){
			data_close(sock ,"close by Children SET Manifest = 0", CLOSE_PARENT);
		}

		_pk_mgr_ptr->send_capacity_to_pk(_pk_mgr_ptr->_sock);
	


	
	} else {
		printf ("%d   " , chunk_ptr->header.cmd);

		cout << "what's this?" << endl;
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"what's this? from peer");
		PAUSE
	}


	if (chunk_ptr)
		delete [] (unsigned char*)chunk_ptr;


	return RET_OK;
}

 
//送queue_out_ctrl_ptr 和queue_out_data_ptr出去
//0311 這邊改成如果阻塞了  就會blocking 住直到送出去為止
int peer::handle_pkt_out(int sock)
{
	struct chunk_t *chunk_ptr=NULL;
	
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;

	map<int , unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_in_iter;
	map<unsigned long, int>::iterator map_pid_fd_out_iter;
//	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
//	struct peer_connect_down_t* peerDownInfo ;
	struct peer_info_t *peerInfoPtr =NULL;

	Nonblocking_Ctl * Nonblocking_Send_Data_ptr =NULL;
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr =NULL;

	fd_out_ctrl_iter = map_fd_out_ctrl.find(sock);
	if(fd_out_ctrl_iter != map_fd_out_ctrl.end()) {
		queue_out_ctrl_ptr = fd_out_ctrl_iter->second;
	}else{
		printf("queue_out_ctrl_ptr NOT FIND \n");
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"queue_out_ctrl_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}
	
	fd_out_data_iter = map_fd_out_data.find(sock);
	if(fd_out_data_iter != map_fd_out_data.end()) {
		queue_out_data_ptr = fd_out_data_iter->second;
	}else{
		printf("queue_out_data_ptr NOT FIND \n");
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"queue_out_data_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(sock);
	if(map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) {
		Nonblocking_Send_Data_ptr = &map_fd_nonblocking_ctl_iter->second->nonBlockingSendData;
		Nonblocking_Send_Ctrl_ptr = &map_fd_nonblocking_ctl_iter->second->nonBlockingSendCtrl;
	}else{
		printf("Nonblocking_Buff_ptr NOT FIND \n");
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"Nonblocking_Buff_ptr NOT FIND");
		return RET_SOCK_ERROR;
	}



//測試性功能
////////////////////////////////////////////////////////////
	map_fd_pid_iter= map_fd_pid .find(sock) ;
	if(map_fd_pid_iter != map_fd_pid.end()){
		map_pid_rescue_peer_info_iter = _pk_mgr_ptr->map_pid_rescue_peer_info .find(map_fd_pid_iter ->second);
		if(map_pid_rescue_peer_info_iter ==  _pk_mgr_ptr->map_pid_rescue_peer_info.end()){
//			printf("peer::handle_pkt_out where is the peer\n");
			peerInfoPtr =NULL ;
//			PAUSE
//			return RET_SOCK_ERROR;
		}else{
			peerInfoPtr = map_pid_rescue_peer_info_iter ->second ;
		}
	}else{
		printf("map_fd_pid NOT FIND \n");
		PAUSE
		return RET_SOCK_ERROR;
	}
//////////////////////////////////////////////////////////////

//	while(queue_out_ctrl_ptr->size() != 0  ||  queue_out_data_ptr->size() != 0){

		if(queue_out_ctrl_ptr->size() != 0 && chunk_ptr == NULL) {

			printf("queue_out_ctrl_ptr->size() = %d\n",queue_out_ctrl_ptr->size());

			if(Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ){

				chunk_ptr = queue_out_ctrl_ptr->front();

				Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.offset =0 ;
				Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length + sizeof(chunk_header_t) ;
				Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length + sizeof(chunk_header_t) ;
				Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;
				Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_ptr;
				Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.serial_num =  chunk_ptr->header.sequence_number;

				printf("cmd= %d total_len = %d\n",chunk_ptr->header.cmd,chunk_ptr->header.length );

				_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));

//				_send_byte = send(sock, (char *)chunk_ptr+_offset, _expect_len, 0);

				if(_send_byte < 0) {
					data_close(sock, "error occured in send  READY queue_out_ctrl",DONT_CARE);
					return RET_SOCK_ERROR;

				} else if(Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ){
						queue_out_ctrl_ptr->pop();
						if(chunk_ptr)
							delete chunk_ptr;
						chunk_ptr = NULL;
					}

			}else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING ){
				_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
			
				if(_send_byte < 0) {
						data_close(sock, "error occured in send RUNNING queue_out_data",DONT_CARE);
						//PAUSE
						return RET_SOCK_ERROR;
				}else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY){
					queue_out_data_ptr->pop();
					if(chunk_ptr)
						delete chunk_ptr;
					chunk_ptr = NULL;
				}
			}
		
		// DATA
		} else if(queue_out_data_ptr->size() != 0) {

			if(queue_out_data_ptr->size() >100){
				printf("queue_out_data_ptr->size() = %d",queue_out_data_ptr->size());
				_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"queue_out_data_ptr->size() =",queue_out_data_ptr->size());

			}

			if(Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY ){

				chunk_ptr = queue_out_data_ptr->front();

//測試性功能
//////////////////////////////////////////////////////////////////////////////

				if(peerInfoPtr){

					while(1){
						//如果現在manifest 的值已經沒有要送了  則略過這個
						if (!(peerInfoPtr ->manifest & (_pk_mgr_ptr ->SubstreamIDToManifest ( (chunk_ptr->header.sequence_number % _pk_mgr_ptr ->sub_stream_num)) ) )){
							if(queue_out_data_ptr->size() !=0){
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

				Nonblocking_Send_Data_ptr ->recv_ctl_info.offset =0 ;
				Nonblocking_Send_Data_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length + sizeof(chunk_header_t) ;
				Nonblocking_Send_Data_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length + sizeof(chunk_header_t) ;
				Nonblocking_Send_Data_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;
				Nonblocking_Send_Data_ptr ->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_ptr;
				Nonblocking_Send_Data_ptr ->recv_ctl_info.serial_num =  chunk_ptr->header.sequence_number;

				_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));


				if(_send_byte < 0) {
					data_close(sock, "error occured in send queue_out_data",DONT_CARE);
					//PAUSE
					return RET_SOCK_ERROR;
				} else if(Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY ){
					queue_out_data_ptr->pop();
					chunk_ptr = NULL;
				}

			}else if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == RUNNING){
		
				_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Data_ptr->recv_ctl_info ));
			
				if(_send_byte < 0) {
						data_close(sock, "error occured in send queue_out_data",DONT_CARE);
						//PAUSE
						return RET_SOCK_ERROR;
				}else if (Nonblocking_Send_Data_ptr ->recv_ctl_info.ctl_state == READY){
					queue_out_data_ptr->pop();
					chunk_ptr = NULL;
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

//這邊的區域變數都不參考環境變數(因位會開thread 來close socket 會搶 變數 使用)
////暫時不close  pid_peer_info 須自行清理
void peer::data_close(int cfd, const char *reason ,int type) 
{
	unsigned long pid = -1;
	list<int>::iterator fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator map_fd_queue_iter;
	map<int , unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;
//	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

	map<unsigned long, int>::iterator map_pid_fd_in_iter;
	map<unsigned long, int>::iterator map_pid_fd_out_iter;
	struct peer_info_t *peerInfoPtr = NULL;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;


//	unsigned long manifest = 0;	

	unsigned long  peerTestingManifest=0;

	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "pk", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".." << endl;
//	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);

	unsigned long  testingManifest=0;
	for(unsigned long i =0  ; i < _pk_mgr_ptr ->sub_stream_num;i++){
//		if(  ((_pk_mgr_ptr->ssDetect_ptr) + i) ->isTesting ){
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



	map_fd_queue_iter = map_fd_out_ctrl.find(cfd);
	if(map_fd_queue_iter != map_fd_out_ctrl.end()) 
		map_fd_out_ctrl.erase(map_fd_queue_iter);
	

	map_fd_queue_iter = map_fd_out_data.find(cfd);
	if(map_fd_queue_iter != map_fd_out_data.end()) 
		map_fd_out_data.erase(map_fd_queue_iter);

	map_fd_nonblocking_ctl_iter = map_fd_nonblocking_ctl.find(cfd);
	if(map_fd_nonblocking_ctl_iter != map_fd_nonblocking_ctl.end()) 
		map_fd_nonblocking_ctl.erase(map_fd_nonblocking_ctl_iter);



	map_fd_pid_iter= map_fd_pid.find(cfd) ;

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

			printf("CLOSE CHILD\n");

			map_pid_fd_iter = map_out_pid_fd.find(pid);
			if(map_pid_fd_iter != map_out_pid_fd.end()) 
				map_out_pid_fd.erase(map_pid_fd_iter);


			map_pid_rescue_peer_info_iter =_pk_mgr_ptr ->map_pid_rescue_peer_info.find(pid);
			if( map_pid_rescue_peer_info_iter != _pk_mgr_ptr ->map_pid_rescue_peer_info.end() ){
				peerInfoPtr = map_pid_rescue_peer_info_iter ->second ;

				_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE CHILD","PID=",map_pid_rescue_peer_info_iter->first,"manifest",peerInfoPtr->manifest);

				delete peerInfoPtr;
				_pk_mgr_ptr ->map_pid_rescue_peer_info.erase(map_pid_rescue_peer_info_iter);

			}


			map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
			map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
			for(temp_map_pid_fd_out_iter = map_out_pid_fd.begin();temp_map_pid_fd_out_iter != map_out_pid_fd.end();temp_map_pid_fd_out_iter++){
				printf("map_out_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_out_iter->first,temp_map_pid_fd_out_iter->second);
			}
			for(temp_map_pid_fd_in_iter = map_in_pid_fd.begin();temp_map_pid_fd_in_iter != map_in_pid_fd.end();temp_map_pid_fd_in_iter++){
				printf("map_in_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_in_iter->first,temp_map_pid_fd_in_iter->second);
			}



		//CLOSE  PARENT
		}else if (map_pid_fd_in_iter != map_in_pid_fd.end() && map_pid_fd_in_iter->second == cfd ){

			printf("CLOSE PARENT\n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CLOSE PARENT");

			pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(pid) ;
			if (pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end() ){

				peerDownInfoPtr = pid_peerDown_info_iter ->second ;

				//防止peer離開 狀態卡在testing
				if(peerDownInfoPtr->peerInfo.manifest & testingManifest){
					peerTestingManifest = peerDownInfoPtr->peerInfo.manifest & testingManifest;

					for(unsigned long i=0 ; i<  _pk_mgr_ptr->sub_stream_num ; i++){
						if( peerTestingManifest & i<<1){
							 (_pk_mgr_ptr->ssDetect_ptr +i) ->isTesting =false ;
							 (_pk_mgr_ptr->ssDetect_ptr +i) ->previousParentPID = PK_PID +1 ;
							 //set recue stat true
							 if(_pk_mgr_ptr->check_rescue_state(i,1)){
								 _pk_mgr_ptr->set_rescue_state(i,2);
								 _pk_mgr_ptr->set_rescue_state(i,0);
							 }else {
								 _pk_mgr_ptr->set_rescue_state(i,0);
							 }
						}
					}
				}
				_pk_mgr_ptr ->reSet_detectionInfo();

				_log_ptr->write_log_format("s =>u s s u s u\n", __FUNCTION__,__LINE__,"CLOSE PARENT","PID=",pid_peerDown_info_iter->first,"manifest",peerDownInfoPtr->peerInfo.manifest);

				delete peerDownInfoPtr;
				_pk_mgr_ptr ->map_pid_peerDown_info.erase(pid_peerDown_info_iter);

			}

			map_pid_fd_iter = map_in_pid_fd.find(pid);
			if(map_pid_fd_iter != map_in_pid_fd.end()) 
				map_in_pid_fd.erase(map_pid_fd_iter);


			map<unsigned long, int>::iterator temp_map_pid_fd_in_iter;
			map<unsigned long, int>::iterator temp_map_pid_fd_out_iter;
			for(temp_map_pid_fd_out_iter = map_out_pid_fd.begin();temp_map_pid_fd_out_iter != map_out_pid_fd.end();temp_map_pid_fd_out_iter++){
				printf("map_out_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_out_iter->first,temp_map_pid_fd_out_iter->second);
			}
			for(temp_map_pid_fd_in_iter = map_in_pid_fd.begin();temp_map_pid_fd_in_iter != map_in_pid_fd.end();temp_map_pid_fd_in_iter++){
				printf("map_in_pid_fd pid : %d fd : %d\n",temp_map_pid_fd_in_iter->first,temp_map_pid_fd_in_iter->second);
			}



		}else{

			printf("peer:: not parent and not children\n");
			PAUSE

		}
		


		map_fd_pid.erase(map_fd_pid_iter);


	}else{
	printf("peer:: CLOSE map_fd_pid not find why \n");
	PAUSE
	}

}

