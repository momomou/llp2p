/*
對於manifest 的概念 ,他只代表sub-stream ID的表示法
每個 peer 應該顧好自己的 manifest table  確保每個sub -straem 都有來源

*/


#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "librtsp/rtsp_viewer.h"

using namespace std;


pk_mgr::pk_mgr(unsigned long html_size, list<int> *fd_list, network *net_ptr , logger *log_ptr , configuration *prep)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_html_size = html_size;
	fd_list_ptr = fd_list;
	level_msg_ptr = NULL;
//	rescue_list_reply_ptr = NULL;
	_channel_id = 0;
//	_current_pos = 0;
	lane_member = 0;
//	_time_start = 0;
//	_recv_byte_count = 0;
	 bit_rate = 0;
	 sub_stream_num = 0;
	 parallel_rescue_num = 0;
	 inside_lane_rescue_num = 0;
	 outside_lane_rescue_num = 0;
//	 peer_list_member = 0;
//	 count = 0;
//	 avg_bandwidth = 0;
	 _manifest = 0;
//	 _check = 0;
//	 current_child_pid = 0;
	 current_child_manifest = 0;
	 _least_sequence_number = 0;
	 stream_number=1;
	 _current_send_sequence_number = -1;
	 pkDownInfoPtr =NULL ;
	 
	_prep->read_key("bucket_size", _bucket_size);

/*
	for(unsigned long i = 0; i < BANDWIDTH_BUCKET; i++) {
		bandwidth_bucket[i] = 0;
	}
*/

//主要的大buffer
	_chunk_bitstream = (struct chunk_bitstream_t *)malloc(RTP_PKT_BUF_MAX * _bucket_size);
	memset( _chunk_bitstream, 0x0, _bucket_size * RTP_PKT_BUF_MAX );

}

pk_mgr::~pk_mgr() 
{

	if(_chunk_bitstream)
		free(_chunk_bitstream);
	if(ssDetect_ptr)
		free(ssDetect_ptr);
	if(statsArryCount_ptr)
		free(statsArryCount_ptr);

	clear_map_pid_peer_info();
	clear_map_pid_peerDown_info();
	clear_map_pid_rescue_peer_info();
	
}

void pk_mgr::peer_mgr_set(peer_mgr *peer_mgr_ptr)
{
	_peer_mgr_ptr = peer_mgr_ptr;

}

void pk_mgr::peer_set(peer *peer_ptr)
{
	_peer_ptr = peer_ptr;

}




void pk_mgr::rtsp_viewer_set(rtsp_viewer *rtsp_viewer_ptr)
{
	_rtsp_viewer_ptr = rtsp_viewer_ptr;

}

/*
void pk_mgr::rtmp_sock_set(int sock)
{
	_rtmp_sock = sock;

}
*/


void pk_mgr::init()
{



	string pk_ip("");
	string pk_port("");
	string svc_tcp_port("");
	string svc_udp_port("");
	
    _prep->read_key("pk_ip", pk_ip);
    _prep->read_key("pk_port", pk_port);
	_prep->read_key("channel_id", _channel_id);
	_prep->read_key("svc_tcp_port", svc_tcp_port);
	_prep->read_key("svc_udp_port", svc_udp_port);

	cout << "pk_ip=" << pk_ip << endl;
	cout << "pk_port=" << pk_port << endl;
	cout << "channel_id=" << _channel_id << endl;
	cout << "svc_tcp_port=" << svc_tcp_port << endl;
	cout << "svc_udp_port=" << svc_udp_port << endl;
	
    //web_ctrl_sever_ptr = new web_ctrl_sever(_net_ptr, _log_ptr, fd_list_ptr, &map_stream_name_id); 
    //web_ctrl_sever_ptr->init();
	
	if (build_connection(pk_ip, pk_port)) {
		cout << "pk_mgr build_connection() success" << endl;
	} else {
		cout << "pk_mgr build_connection() fail" << endl;
		PAUSE
		exit(0);
	}

	if (handle_register(svc_tcp_port, svc_udp_port)) {
		cout << "pk_mgr handle_ register() success" << endl;
		_net_ptr->set_nonblocking(_sock);	// set to non-blocking
		_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN);
		_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (this));
		fd_list_ptr->push_back(_sock);

	} else {
		cout << "pk_mgr handle_ register() fail" << endl;
		PAUSE
		exit(0);
	}
	
}



// build_connection to (string ip , string port) ,if failure return 0,else return 1
int pk_mgr::build_connection(string ip, string port)
{

	if((_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		cout << "init create socket failure" << endl;
#ifdef _WIN32
		::WSACleanup();
#endif
		return 0;
	}

	struct sockaddr_in pk_saddr;

	memset((struct sockaddr_in*)&pk_saddr, 0x0, sizeof(struct sockaddr_in));

	pk_saddr.sin_addr.s_addr = inet_addr(ip.c_str());
	pk_saddr.sin_port = htons((unsigned short)atoi(port.c_str()));
	pk_saddr.sin_family = AF_INET;

	if(connect(_sock, (struct sockaddr*)&pk_saddr, sizeof(pk_saddr)) < 0) {
		cout << "build_connection failure" << endl;
#ifdef _WIN32
		::closesocket(_sock);
		::WSACleanup();
#else
		::close(_sock);
#endif
		return 0;
	}
	
	return 1;

}

//Follow light protocol spec send register message to pk,  HTTP | light | content(request_info_t)
//This function include send() function ,send  register packet to PK Server
int pk_mgr::handle_register(string svc_tcp_port, string svc_udp_port)
{
	struct chunk_t *chunk_ptr = NULL;
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	int send_byte;
	char *crlf_ptr = NULL;		 // it need to point to   -> \r\n\r\n
	char html_buf[8192];
	unsigned long html_hdr_size; // HTTP protocol  len
	unsigned long buf_len;		 // HTTP protocol  len + HTTP content len
	
	chunk_request_ptr = (struct chunk_request_msg_t *)new unsigned char[sizeof(struct chunk_request_msg_t)];
	
	memset(html_buf, 0x0, _html_size);
	memset(chunk_request_ptr, 0x0, sizeof(struct chunk_request_msg_t));

		
	strcat(html_buf, "GET / HTTP/1.1\r\nAccept: */*\r\n");
	strcat(html_buf, "User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n\r\n");
	
	chunk_request_ptr->header.cmd = CHNK_CMD_PEER_REG;
	chunk_request_ptr->header.rsv_1 = REQUEST;
	chunk_request_ptr->header.length = sizeof(struct request_info_t);
	chunk_request_ptr->info.pid = 0;
	chunk_request_ptr->info.channel_id = _channel_id;
	chunk_request_ptr->info.private_ip = _net_ptr->getLocalIpv4();
	chunk_request_ptr->info.tcp_port = (unsigned short)atoi(svc_tcp_port.c_str());
	chunk_request_ptr->info.udp_port = (unsigned short)atoi(svc_udp_port.c_str());

	if((crlf_ptr = strstr(html_buf, "\r\n\r\n")) != NULL) {
		crlf_ptr += CRLF_LEN;	
		html_hdr_size = crlf_ptr - html_buf;
		cout << "html_hdr_size =" << html_hdr_size << endl;
	} 

	memcpy(html_buf+html_hdr_size, chunk_request_ptr, sizeof(struct chunk_request_msg_t));
	
	buf_len = html_hdr_size + sizeof(struct chunk_request_msg_t);

	send_byte = _net_ptr->send(_sock, html_buf, buf_len, 0);

	if(chunk_request_ptr)
	delete chunk_request_ptr;

	if( send_byte <= 0 ) {
		data_close(_sock, "send html_buf error");
		_log_ptr->exit(0, "send html_buf error");
		return 0;
	} else {		//success
		return 1;
	}
	
}


int pk_mgr::handle_pkt_in(int sock)
{
//	ftime(&interval_time);	//--!! 0215
	unsigned long i;
	unsigned long buf_len;
	unsigned long level_msg_size;
	int recv_byte;
	int expect_len = 0;
	int offset = 0;
	int ret = -1;
//    unsigned long total_map_num;
//	unsigned long msg_size;
	unsigned long total_bit_rate = 0;
	unsigned long ss_id = 0;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, unsigned long>::iterator map_pid_manifest_iter;
	list<int>::iterator outside_rescue_list_iter;
//	struct timeb tmpt;
	
	struct chunk_t *chunk_ptr = NULL;
	struct chunk_header_t *chunk_header_ptr = NULL;
	struct peer_info_t *new_peer = NULL;
	struct peer_info_t *child_peer = NULL;

	chunk_header_ptr = new struct chunk_header_t;
	memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));
	
	expect_len = sizeof(struct chunk_header_t) ;

//expect recv light header
	while (1) {
		recv_byte = recv(sock, (char *)chunk_header_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
			} else {
				data_close(sock, "recv error in pk_mgr::handle_pkt_in");
				printf("recv error in pk_mgr::handle_pkt_in\n");
				PAUSE
				_log_ptr->exit(0, "recv error in pk_mgr::handle_pkt_in");\
			}
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		
		if (!expect_len)
			break;
	}


	expect_len = chunk_header_ptr->length;
	buf_len = sizeof(struct chunk_header_t) + expect_len;

	chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

	if (!chunk_ptr) {
		data_close(sock, "memory not enough");
		printf("memory not enough\n");
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
				data_close(sock, "recv error in pk_mgr::handle_pkt_in");
				printf("recv error in pk_mgr::handle_pkt_in\n");
				DBG_PRINTF("here\n");
				PAUSE
			}
		}

		expect_len -= recv_byte;
		offset += recv_byte;
		if (expect_len == 0)
			break;
	}
//recv whole packet to chunk_ptr done (size is buf_len),packet include (light header + content ) 

	offset = 0;

//handle CHNK_CMD_PEER_ REG, expect recv  chunk_register_reply_t    from  PK
//ligh |  pid |  level |   bit_rate|   sub_stream_num |  parallel_rescue_num |  inside_lane_rescue_num | n*struct level_info_t
//這邊應該包含整條lane 的peer_info 包含自己

	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_REG  ) {
		lane_member = (buf_len - sizeof(struct chunk_header_t) - 6 * sizeof(unsigned long)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + lane_member * sizeof(struct level_info_t *);

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		memset(level_msg_ptr, 0x0, level_msg_size);
		memcpy(level_msg_ptr, chunk_ptr, (level_msg_size - lane_member * sizeof(struct level_info_t *)));
		
		offset += (level_msg_size - lane_member * sizeof(struct level_info_t *));
		
		memcpy(&bit_rate, (char *)chunk_ptr + offset, sizeof(unsigned long));
		memcpy(&sub_stream_num, ((char *)chunk_ptr + offset + sizeof(unsigned long)), sizeof(unsigned long));
		memcpy(&parallel_rescue_num, ((char *)chunk_ptr + offset + 2 * sizeof(unsigned long)), sizeof(unsigned long));
		memcpy(&inside_lane_rescue_num, ((char *)chunk_ptr + offset + 3 * sizeof(unsigned long)), sizeof(unsigned long));
		

		cout<< "bit_rate = " <<  bit_rate << endl;
		cout<< "sub_stream_num = " <<  sub_stream_num << endl;
		cout<< "parallel_rescue_num = " <<  parallel_rescue_num << endl;
		cout<< "inside_lane_rescue_num = " <<  inside_lane_rescue_num << endl;

		offset += sizeof(unsigned long) * 4;

	//將收到的封包放進  去除掉bit_rate .sub_stream_num .parallel_rescue_num . inside_lane_rescue_num  ,後放進  chunk_level_msg_t
		for (i = 0; i < lane_member; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			new_peer = new struct peer_info_t;
			
			memset(level_msg_ptr->level_info[i], 0x0 , sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			
			memset(new_peer, 0x0 , sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));

			offset += sizeof(struct level_info_t);


	//add lane peer_info to map table  NOused
			map_pid_peer_info[new_peer->pid] = new_peer;

		}

		pkDownInfoPtr = new struct peer_connect_down_t ;
		memset(pkDownInfoPtr , 0x0,sizeof( struct peer_connect_down_t));
		pkDownInfoPtr ->peerInfo.pid =PK_PID;
		map_pid_peerDown_info[PK_PID] =pkDownInfoPtr;

		_peer_mgr_ptr -> self_pid = level_msg_ptr ->pid ;

		//收到sub_stream_num後對rescue 偵測結構做初始化
		init_rescue_detection();


	//和lane 每個peer 先建立好連線 
		if(lane_member >= 1)
			_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);

		_peer_mgr_ptr->handle_test_delay();

		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->level_info[i])
				delete level_msg_ptr->level_info[i];
		}
		
		if(level_msg_ptr)
			delete level_msg_ptr;



// light | pid | level   | n*struct level_info_t
	} else if (chunk_ptr->header.cmd == CHNK_CMD_RESCUE_LIST) {
		printf("CHNK_CMD_RESCUE_LIST\n");

//hidden at 2012/01/23


////////////////////////////////////////////////
		clear_map_pid_peer_info();

		lane_member = (buf_len - sizeof(struct chunk_header_t) - sizeof(unsigned long) - sizeof(unsigned long)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + lane_member * sizeof(struct level_info_t *);

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		memset(level_msg_ptr, 0x0, level_msg_size);
		memcpy(level_msg_ptr, chunk_ptr, (level_msg_size - lane_member * sizeof(struct level_info_t *)));
		
		offset += (level_msg_size - lane_member * sizeof(struct level_info_t *));


		for (i = 0; i < lane_member; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			new_peer = new struct peer_info_t;
			memset(level_msg_ptr->level_info[i], 0x0 , sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			memset(new_peer, 0x0 , sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));
///////////////
			offset += sizeof(struct level_info_t);

/*
			pid_peerDown_info_iter = map_pid_peerDown_info.find(new_peer ->pid);
			if(pid_peerDown_info_iter != map_pid_peerDown_info.end()){
				delete new_peer;
				continue;
			}
*/


			map_pid_peer_info[new_peer->pid] = new_peer;


		}


//////////////////////////////////

		_peer_ptr->first_reply_peer =true;
	

//和lane 每個peer 先建立好連線	
		if(lane_member >= 1)
			_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);

		_peer_mgr_ptr->handle_test_delay();

		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->level_info[i])
				delete level_msg_ptr->level_info[i];
			}
		
		if(level_msg_ptr)
			delete level_msg_ptr;

		
//cmd == CHNK_CMD_PEER_DATA			
	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_DATA) {
		//printf("%s, CHNK_CMD_PEER_DATA\n", __FUNCTION__);
		handle_stream(chunk_ptr, sock);	

//cmd == CHNK_CMD_PEER_RSC_LIST	
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_RSC_LIST) {
	printf("cmd =recv CHNK_CMD_PEER_RSC_LIST\n");
//hidden at 2013/01/16
/*
		//printf("%s, CHNK_CMD_PEER_RSC_LIST\n", __FUNCTION__);
		if(chunk_ptr->header.length == (2 * sizeof(unsigned long))) {
			cout << "no other peer can rescue" << endl;
		} else {
			//cout << "CHNK_CMD_PEER_RSC_LIST" << endl;

			for (i = 0; i < peer_list_member; i++) {
				if(rescue_list_reply_ptr->level_info[i])
					delete rescue_list_reply_ptr->level_info[i];
			}
		
			if(rescue_list_reply_ptr)
				delete rescue_list_reply_ptr;

			peer_list_member = (buf_len - sizeof(struct chunk_header_t) - 2 * sizeof(unsigned long)) / sizeof(struct level_info_t);
			msg_size = sizeof(struct chunk_header_t) + 2 * sizeof(unsigned long) + peer_list_member * sizeof(struct level_info_t *);
			
			rescue_list_reply_ptr = (struct chunk_rescue_list_reply_t *) new unsigned char[msg_size];
			
			memset(rescue_list_reply_ptr, 0x0, msg_size);
			memcpy(rescue_list_reply_ptr, chunk_ptr, (msg_size - peer_list_member * sizeof(struct level_info_t *)));
		
			offset += (msg_size - peer_list_member * sizeof(struct level_info_t *));
		
			for (i = 0; i < peer_list_member; i++) {
				rescue_list_reply_ptr->level_info[i] = new struct level_info_t;
				memset(rescue_list_reply_ptr->level_info[i], 0x0 , sizeof(struct level_info_t));
				memcpy(rescue_list_reply_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
				offset += sizeof(struct level_info_t);
				cout << "rescue_list_pid = " << rescue_list_reply_ptr->level_info[i]->pid << endl;
			}

			if(outside_lane_rescue_num > peer_list_member) 
				outside_lane_rescue_num = peer_list_member;
			
			while(outside_lane_rescue_num > 0) {

				if(peer_list_member > 0) {
					ret = _peer_mgr_ptr->connect_other_lane_peer(rescue_list_reply_ptr, peer_list_member, rescue_list_reply_ptr->pid, outside_lane_rescue_num);
				}

				for(outside_rescue_list_iter = outside_rescue_list.begin(); outside_rescue_list_iter != outside_rescue_list.end(); outside_rescue_list_iter++) {
					if(*outside_rescue_list_iter == ret) {
						ret = -1;
						break;
					}
				}
				
				if(ret >= 0) {
//					_peer_mgr_ptr->send_rescue(rescue_list_reply_ptr->level_info[ret]->pid, level_msg_ptr->pid, _manifest);
					outside_lane_rescue_num --;
					outside_rescue_list.push_back(ret);
				}
			}
		
		}
*/
	

	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_NOTIFY) {
		printf("cmd =recv CHNK_CMD_PEER_NOTIFY\n");

//rtmp_chunk_size change
//  hidden at 2013/01/16
/*
        unsigned char stream_id, control_type;
        int rtmp_chunk_size;

        printf("recieve CHNK_CMD_PEER_NOTIFY\n");
        memcpy(&total_map_num, (char *)chunk_ptr + sizeof(struct chunk_t), sizeof(unsigned long));
        for(int i = 0;i < total_map_num;i ++){
            memcpy(&stream_id, (char *)chunk_ptr + sizeof(struct chunk_t) + sizeof(unsigned long) + i * sizeof(struct channel_chunk_size_info_t), sizeof(unsigned char));
            //memcpy(&control_type, (char *)chunk_ptr + sizeof(struct chunk_t) + sizeof(unsigned long) + i * sizeof(struct channel_stream_map_info_t) + sizeof(unsigned char), sizeof(unsigned char));
            memcpy(&rtmp_chunk_size, (char *)chunk_ptr + sizeof(struct chunk_t) + sizeof(unsigned long) + i * sizeof(struct channel_chunk_size_info_t) + 2 * sizeof(unsigned char), 4);

            printf("stream_id = %d, chunk_size = %d \n", stream_id, rtmp_chunk_size);
            map_rtmp_chunk_size[stream_id] = rtmp_chunk_size;

        }
*/


//header.cmd == CHNK_CMD_PEER_LATENCY
    } else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_LATENCY){
//	printf("cmd =recv CHNK_CMD_PEER_LATENCY\n");
//hidden at 2013/01/16
/*
        unsigned long sec, usec, peer_id;
        unsigned long peer_num;

        memcpy(&sec, (char *)chunk_ptr + sizeof(struct chunk_t), sizeof(unsigned long));
        memcpy(&usec, (char *)chunk_ptr + sizeof(struct chunk_t) + sizeof(unsigned long), sizeof(unsigned long));
        memcpy(&peer_num, (char *)chunk_ptr + sizeof(struct chunk_t) + 2 * sizeof(unsigned long), sizeof(unsigned long));
        //printf("server time: %ld.%06ld\n", sec, usec);
        for(int i = 0;i < peer_num;i ++){
            memcpy(&peer_id, (char *)chunk_ptr + sizeof(struct chunk_t) + 3 * sizeof(unsigned long) + i * sizeof(struct peer_timestamp_info_t), sizeof(unsigned long));
            memcpy(&sec, (char *)chunk_ptr + sizeof(struct chunk_t) + 4 * sizeof(unsigned long) + i * sizeof(struct peer_timestamp_info_t), sizeof(unsigned long));
            memcpy(&usec, (char *)chunk_ptr + sizeof(struct chunk_t) + 4 * sizeof(unsigned long) + i * sizeof(struct peer_timestamp_info_t), sizeof(unsigned long));
            //printf("peer %d time: %ld.%06ld\n", peer_id, sec, usec);
        }

        handle_latency(chunk_ptr, sock);
*/


		
//cmd == CHNK_CMD_RT_NLM   //network latency measurement
    } else if(chunk_ptr->header.cmd == CHNK_CMD_RT_NLM) {	//--!! 0128 rcv from lightning
	printf("cmd =recv CHNK_CMD_RT_NLM\n");
// hidden at 2013/01/13
/*
		map<unsigned long, int>::iterator pid_fd_iter;
		map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
		queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
        if(!level_msg_ptr) {
//            if (chunk_ptr)
//				delete chunk_ptr;
            return RET_OK;	
        }

		if(chunk_ptr->header.rsv_1 ==  REQUEST) {
			if(current_child_pid == 0) {	//reply
				struct chunk_t *reply_msg_ptr = chunk_ptr;
				reply_msg_ptr->header.rsv_1 = REPLY;
				//send $(*reply_msg_ptr) to parent
				if (level_msg_ptr->level == 0) {
					//to lightning
					ftime(&tmpt);
					cout << "before send : " << tmpt.time * 1000ull + tmpt.millitm << endl;
					_net_ptr->send(_sock, (char *)reply_msg_ptr, sizeof(struct chunk_header_t) + chunk_ptr->header.length, 0);
					ftime(&tmpt);
					cout << "after send : " << tmpt.time * 1000ull + tmpt.millitm << endl;
					if (chunk_ptr)
						delete chunk_ptr;					
				} else {
					pid_fd_iter = _peer_mgr_ptr->peer_ptr->map_pid_fd.find(level_msg_ptr->level_info[level_msg_ptr->level-1]->pid);
					fd_queue_iter = _peer_mgr_ptr->peer_ptr->map_fd_out_ctrl.find(pid_fd_iter->second);
					if (pid_fd_iter == _peer_mgr_ptr->peer_ptr->map_pid_fd.end() || fd_queue_iter == _peer_mgr_ptr->peer_ptr->map_fd_out_ctrl.end()) {
						if (chunk_ptr)
							delete chunk_ptr;
						return RET_OK;
					}
					queue_out_ctrl_ptr = fd_queue_iter->second;
					queue_out_ctrl_ptr->push((struct chunk_t *)reply_msg_ptr);
					_net_ptr->epoll_control(pid_fd_iter->second, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
				}
			} else {	//--!! 0218
				struct timeb tmpt;
				ftime(&tmpt);
				//check if sending 
				pid_fd_iter = _peer_mgr_ptr->peer_ptr->map_pid_fd.find(current_child_pid);
				if (pid_fd_iter != _peer_mgr_ptr->peer_ptr->map_pid_fd.end()) {
					fd_queue_iter = _peer_mgr_ptr->peer_ptr->map_fd_out_ctrl.find(pid_fd_iter->second);
					if (fd_queue_iter != _peer_mgr_ptr->peer_ptr->map_fd_out_ctrl.end()) {
						//send $(*req_msg_ptr) to his child !!--!!0128
						int chunk_size = sizeof(struct chunk_header_t) + chunk_ptr->header.length;
						struct chunk_rt_latency_t *req_msg_ptr = (struct chunk_rt_latency_t *) new unsigned char[(chunk_size + sizeof(struct ts_block_t))];
						struct ts_block_t *ts_ptr =(struct ts_block_t*)( (char*)req_msg_ptr + chunk_size);
				
						memset(ts_ptr, 0x0, sizeof(struct ts_block_t));
						memcpy(req_msg_ptr, chunk_ptr, chunk_size);
						//cout << "req_msg_ptr->dts_length = " << req_msg_ptr->dts_length << endl;
						//cout << "req_msg_ptr->dts_offset = " << req_msg_ptr->dts_offset << endl;
						req_msg_ptr->dts_offset += sizeof(struct ts_block_t);
						ts_ptr->pid = level_msg_ptr->pid;
						ts_ptr->time_stamp = tmpt.time * 1000ull + tmpt.millitm;
						req_msg_ptr->header.length += sizeof(struct ts_block_t);
						
						queue_out_ctrl_ptr = fd_queue_iter->second;
						queue_out_ctrl_ptr->push((struct chunk_t *)req_msg_ptr);
						_net_ptr->epoll_control(pid_fd_iter->second, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
					}
			}

///				if(chunk_ptr)	//--!! 0218 
///					delete chunk_ptr;
			}
		} else {
				struct chunk_rt_latency_t *reply_msg_ptr = (struct chunk_rt_latency_t*) chunk_ptr;
				struct ts_block_t *ts_ptr = (struct ts_block_t*)( (char*)reply_msg_ptr->buf + reply_msg_ptr->dts_offset - sizeof(struct ts_block_t));
				struct timeb tmpt;
				//if() compare if pid equals
				if (ts_ptr->pid == level_msg_ptr->pid) {
					ftime(&tmpt);
					ts_ptr->time_stamp = tmpt.time * 1000ull + tmpt.millitm - ts_ptr->time_stamp;
					ts_ptr->isDST = REPLY;
					
					reply_msg_ptr->dts_offset = reply_msg_ptr->dts_offset - sizeof(struct ts_block_t);
					reply_msg_ptr->dts_length = reply_msg_ptr->dts_length + sizeof(struct ts_block_t);
					//transmit to parent
					if (level_msg_ptr->level == 0) {
						//to lightning
						_net_ptr->send(_sock, (char *)reply_msg_ptr, sizeof(struct chunk_header_t) + chunk_ptr->header.length, 0);
						if (chunk_ptr)
							delete chunk_ptr;
						//no check 4 success
						
					} else {
						pid_fd_iter = _peer_mgr_ptr->peer_ptr->map_pid_fd.find(level_msg_ptr->level_info[level_msg_ptr->level-1]->pid);
						fd_queue_iter = _peer_mgr_ptr->peer_ptr->map_fd_out_ctrl.find(pid_fd_iter->second);
						if (pid_fd_iter == _peer_mgr_ptr->peer_ptr->map_pid_fd.end() || fd_queue_iter == _peer_mgr_ptr->peer_ptr->map_fd_out_ctrl.end()) {
							if (chunk_ptr)
								delete chunk_ptr;
							return RET_OK;
						}
						queue_out_ctrl_ptr = fd_queue_iter->second;
						queue_out_ctrl_ptr->push((struct chunk_t *)reply_msg_ptr);
						_net_ptr->epoll_control(pid_fd_iter->second, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
					}
				} else {
					cout << "should be the same, what's going on?" << endl;
				}
		}
	    unsigned long tmp =  interval_time.time * 1000ull + interval_time.millitm;
	    ftime(&interval_time);
	    tmp = interval_time.time * 1000ull + interval_time.millitm - tmp;
	    cout<<"interval delay = "<<tmp<<endl;
*/

//   light | int streanID |int streanID | .....
//  CHNK_CMD_CHN_UPDATA_DATA store new streamID in streamID_list
	}else if(chunk_ptr->header.cmd == CHNK_CMD_CHN_UPDATA_DATA){
		printf("cmd =recv CHNK_CMD_CHN_UPDATA_DATA\n");

		stream_number=(chunk_ptr->header.length)/sizeof(int);
		int *intptr=(int *)((char*)chunk_ptr +sizeof(chunk_header_t));
		streamID_list.clear();
		for(int i=0 ; i< stream_number ;i++){
		printf("streamID = %d\n",*(intptr+i));
		streamID_list.push_back(*(intptr+i));
	}

//other
	}else{
		printf("cmd =%d else\n", chunk_ptr->header.cmd);
	}

	if (chunk_ptr)
		delete [] (unsigned char*)chunk_ptr;


	return RET_OK;		
	}


int pk_mgr::handle_pkt_out(int sock)
{
	return RET_OK;
}

void pk_mgr::handle_pkt_error(int sock)
{

}

void pk_mgr::handle_sock_error(int sock, basic_class *bcptr)
{

}

void pk_mgr::handle_job_realtime()
{

}

void pk_mgr::handle_job_timer()
{

}

//will delete
// hidden at 2013/01/15
/* 
void pk_mgr::handle_bandwidth(unsigned long avg_bit_rate)
{
	unsigned long i;
	
	if(lane_member > 1) {
		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->pid == level_msg_ptr->level_info[i]->pid) {
				i++;
					if( i != lane_member) {
//						_peer_mgr_ptr->send_bandwidth(level_msg_ptr->level_info[i]->pid, avg_bit_rate);
						break;
					}
			} 
		}
	}

}
*/

//will delete , send_rescue info to pk??
void pk_mgr::send_rescue(unsigned long manifest)
{
	unsigned long i;
	unsigned long inside_lane_rescue = 0;
	
	if(lane_member > 1) {
		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->pid == level_msg_ptr->level_info[i]->pid) {
				if(i == 1) {
					inside_lane_rescue = 0;
					outside_lane_rescue_num = parallel_rescue_num;
					_manifest = manifest;
					outside_rescue_list.clear();
					send_rescue_to_pk();
				} else {
					i --;
					if(level_msg_ptr->level <= inside_lane_rescue_num) {
						inside_lane_rescue = level_msg_ptr->level - 1;
					} else {
						inside_lane_rescue = inside_lane_rescue_num;
					}
					
					outside_lane_rescue_num = parallel_rescue_num - inside_lane_rescue;

					if(outside_lane_rescue_num > 0) {
						_manifest = manifest;
						outside_rescue_list.clear();
						send_rescue_to_pk();
					}

					while(inside_lane_rescue > 0) {
						i --;
						cout << "rescue_pid = " << level_msg_ptr->level_info[i]->pid << endl;
//						_peer_mgr_ptr->send_rescue(level_msg_ptr->level_info[i]->pid, level_msg_ptr->pid, manifest);
						inside_lane_rescue --;
						_peer_mgr_ptr->add_rescue_fd(level_msg_ptr->level_info[i]->pid);
					}
 
					break;
				}
			} 
		}
	}

}


//will be modify
void pk_mgr::send_rescue_to_pk()
{
	int send_byte = 0;
	char html_buf[8192];
	struct chunk_rescue_list_t *rescue_list_ptr = NULL;
	
	_net_ptr->set_blocking(_sock);	// set to blocking
	
	rescue_list_ptr = new struct chunk_rescue_list_t;

	memset(html_buf, 0x0, _html_size);
	memset(rescue_list_ptr, 0x0, sizeof(struct chunk_rescue_list_t));
	
	rescue_list_ptr->header.cmd = CHNK_CMD_PEER_RSC_LIST;
	rescue_list_ptr->header.length = sizeof(unsigned long) ;	//pkt_buf paylod length
	rescue_list_ptr->header.rsv_1 = REQUEST;
	rescue_list_ptr->pid = level_msg_ptr->pid;


	memcpy(html_buf, rescue_list_ptr, sizeof(struct chunk_rescue_list_t));
	
	send_byte = _net_ptr->send(_sock, html_buf, sizeof(struct chunk_rescue_list_t), 0);

	if( send_byte <= 0 ) {
		data_close(_sock, "send rescue_ list cmd error");
		_log_ptr->exit(0, "send rescue_ list cmd error");
	} else {
		if(rescue_list_ptr)
			delete rescue_list_ptr;
		_net_ptr->set_nonblocking(_sock);	// set to non-blocking
	}

}

//hiddle at 2013/01/16
/*
void pk_mgr::send_rescue_to_upstream(unsigned long manifest)
{
	unsigned long i;
	
	if(lane_member > 1) {
		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->pid == level_msg_ptr->level_info[i]->pid) {
				//DBG_PRINTF("here\n");
				i --;
//				_peer_mgr_ptr->send_rescue(level_msg_ptr->level_info[i]->pid, level_msg_ptr->pid, manifest);
				break;	
			} 
		}
	}

}

*/

//when a peer recv a rescue  packet from other peer how to handle ( only called by peer recv  cmd == CHNK_CMD_PEER_RSC)

void pk_mgr::handle_rescue(unsigned long pid, unsigned long manifest)
{
	unsigned long i;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	struct peer_info_t *rescue_peer = NULL;

	pid_peer_info_iter = map_pid_peer_info.find(pid);
	
	if(pid_peer_info_iter != map_pid_peer_info.end()) {
		for (i = 0; i < lane_member; i++) {
			if(pid == level_msg_ptr->level_info[i]->pid) {
				i --;
				rescue_peer = pid_peer_info_iter->second;
				if(level_msg_ptr->pid == level_msg_ptr->level_info[i]->pid) {
					rescue_peer->manifest = (~manifest);
					current_child_manifest = rescue_peer->manifest;
//hidden at 2013/01/27	
//                    _peer_mgr_ptr->clear_ouput_buffer(pid);
                    cout << "down stream " << pid << " manifest=" << current_child_manifest << endl;
					break;
				} else {
					rescue_peer->manifest = manifest;
					_peer_mgr_ptr->rescue_reply(pid, manifest);
//hidden at 2013/01/27 					
//					_peer_mgr_ptr->add_rescue_downstream(pid);
                    cout << "pid " << pid << " manifest=" << current_child_manifest << endl;
					break;
				} 
			} 
		}
	} else {  //not find in map_pid_peer_info table
		pid_peer_info_iter = map_pid_rescue_peer_info.find(pid);
		if(pid_peer_info_iter != map_pid_rescue_peer_info.end()) {
			rescue_peer = pid_peer_info_iter->second;
			rescue_peer->manifest = manifest;
			_peer_mgr_ptr->rescue_reply(pid, manifest);
//hidden at 2013/01/27	
//			_peer_mgr_ptr->add_rescue_downstream(pid);
		}else{		//not find in map_pid_rescue_ peer_info


		}
		
	}
	
}

//send_request_sequence_number_to_pk   ,req_from   to   req_to
void pk_mgr::send_request_sequence_number_to_pk(unsigned int req_from, unsigned int req_to)
{
	int send_byte = 0;
	char html_buf[8192];
	struct chunk_request_pkt_t *request_pkt_ptr = NULL;
	
	_net_ptr->set_blocking(_sock);	// set to blocking
	
	request_pkt_ptr = new struct chunk_request_pkt_t;

	memset(html_buf, 0x0, _html_size);
	memset(request_pkt_ptr, 0x0, sizeof(struct chunk_request_pkt_t));
	
	request_pkt_ptr->header.cmd = CHNK_CMD_PEER_REQ_FROM;
	request_pkt_ptr->header.length = sizeof(unsigned long) + sizeof(unsigned int) + sizeof(unsigned int);	//pkt_buf paylod length
	request_pkt_ptr->header.rsv_1 = REQUEST;
	request_pkt_ptr->pid = level_msg_ptr->pid;
	request_pkt_ptr->request_from_sequence_number = req_from;
	request_pkt_ptr->request_to_sequence_number = req_to;

	//printf("request seq %d to %d\n",request_pkt_ptr->request_from_sequence_number,request_pkt_ptr->request_to_sequence_number);

	memcpy(html_buf, request_pkt_ptr, sizeof(struct chunk_request_pkt_t));
	
	send_byte = _net_ptr->send(_sock, html_buf, sizeof(struct chunk_request_pkt_t), 0);

	if( send_byte <= 0 ) {
		data_close(_sock, "send request_pkt cmd error");
		_log_ptr->exit(0, "send request_pkt cmd error");
	} else {
		if(request_pkt_ptr)
			delete request_pkt_ptr;
		_net_ptr->set_nonblocking(_sock);	// set to non-blocking
	}

}

//using blocking sent pkt to pk now ( only called by  handle_latency)
void pk_mgr::send_pkt_to_pk(struct chunk_t *chunk_ptr)
{
	int send_byte = 0;
    int expect_len = chunk_ptr->header.length + sizeof(struct chunk_header_t);
	char html_buf[8192];
	
	_net_ptr->set_blocking(_sock);	// set to blocking

	memset(html_buf, 0x0, _html_size);
	memcpy(html_buf, chunk_ptr, expect_len);
	
	send_byte = _net_ptr->send(_sock, html_buf, expect_len, 0);

	if( send_byte <= 0 ) {
		data_close(_sock, "send pkt error");
		_log_ptr->exit(0, "send pkt error");
	} else {
		if(chunk_ptr)
			delete chunk_ptr;
		_net_ptr->set_nonblocking(_sock);	// set to non-blocking
	}

}

//handle_latency hidden at 2013/01/16
/*
void pk_mgr::handle_latency(struct chunk_t *chunk_ptr, int sockfd)
{
	int i;
    unsigned long msg_size;
	unsigned long reply_size;
    unsigned long peer_num;
	unsigned long offset = 0;
    int expect_len = chunk_ptr->header.length;
    unsigned long time_diff;

    struct timeval detail_time, start_time;
    struct chunk_t *peer_chunk_ptr = NULL;
    struct peer_timestamp_info_t *peer_timestamp_info_ptr = NULL;
    struct peer_latency_measure *peer_latency_measure_ptr = NULL;

    _log_ptr->gettimeofday(&detail_time,NULL);
    //printf("peer tsp %ld.%06ld\n", detail_time.tv_sec , detail_time.tv_usec);

    if(chunk_ptr->header.rsv_1 == REQUEST) {
        msg_size = sizeof(struct chunk_header_t) + expect_len + sizeof(struct peer_timestamp_info_t *);
	    reply_size = msg_size - sizeof(struct peer_timestamp_info_t *) + sizeof(struct peer_timestamp_info_t);

        peer_timestamp_info_ptr = (struct peer_timestamp_info_t *) new unsigned char[sizeof(struct peer_timestamp_info_t)];
	    peer_chunk_ptr = (struct chunk_t *) new unsigned char[reply_size];

        memset(peer_timestamp_info_ptr, 0x0, sizeof(struct peer_timestamp_info_t));
        memset(peer_chunk_ptr, 0x0, reply_size);
        memcpy((char *)peer_chunk_ptr + offset, chunk_ptr, sizeof(struct chunk_header_t) + expect_len);
	    offset += (sizeof(struct chunk_header_t) + expect_len);
        peer_chunk_ptr->header.length = expect_len + sizeof(struct peer_timestamp_info_t);

        memcpy(&peer_num, (char *)peer_chunk_ptr + sizeof(struct chunk_t) + 2 * sizeof(unsigned long), sizeof(unsigned long));
        peer_num ++;
        memcpy((char *)peer_chunk_ptr + sizeof(struct chunk_t) + 2 * sizeof(unsigned long), &peer_num, sizeof(unsigned long));

        peer_timestamp_info_ptr->pid = level_msg_ptr->pid;
        peer_timestamp_info_ptr->peer_sec = detail_time.tv_sec;
        peer_timestamp_info_ptr->peer_usec = detail_time.tv_usec;

        memcpy((char *)peer_chunk_ptr + offset, peer_timestamp_info_ptr, sizeof(struct peer_timestamp_info_t));
	    if(peer_timestamp_info_ptr)
		    delete peer_timestamp_info_ptr;

        if(lane_member > 1) {
		    for (i = 0; i < lane_member; i++) {
			    if(level_msg_ptr->pid == level_msg_ptr->level_info[i]->pid) {
				    i++;
				    if( i != lane_member) {
                        //printf("send tsp to peer %d\n", level_msg_ptr->level_info[i]->pid);
					    _peer_mgr_ptr->add_downstream(level_msg_ptr->level_info[i]->pid, (struct chunk_t *)peer_chunk_ptr);
					    break;
                    } else {
                        i = i - 2;
                        if(i >= 0){
                            peer_chunk_ptr->header.rsv_1 = REPLY;
                            _peer_mgr_ptr->add_downstream(level_msg_ptr->level_info[i]->pid, (struct chunk_t *)peer_chunk_ptr);
                        }
					    break;
                    }
                } 
            }
        } else{
            peer_chunk_ptr->header.rsv_1 = REPLY;
            send_pkt_to_pk((struct chunk_t *)peer_chunk_ptr);
        }
    } else if(chunk_ptr->header.rsv_1 == REPLY) {
        msg_size = sizeof(struct chunk_header_t) + expect_len;
        peer_latency_measure_ptr = (struct peer_latency_measure *) new unsigned char[msg_size];

        memset(peer_latency_measure_ptr, 0x0, msg_size);
        memcpy((char *)peer_latency_measure_ptr, chunk_ptr, msg_size);

        for (i = 0; i < peer_latency_measure_ptr->total_num; i++) {
            if(level_msg_ptr->pid == peer_latency_measure_ptr->peer_timestamp_info[i].pid) {
                //printf("start %ld.%06ld\n", peer_latency_measure_ptr->peer_timestamp_info[i].peer_sec , peer_latency_measure_ptr->peer_timestamp_info[i].peer_usec); 
	            //printf("finish %ld.%06ld\n", detail_time.tv_sec , detail_time.tv_usec); 

                start_time.tv_sec = peer_latency_measure_ptr->peer_timestamp_info[i].peer_sec;
                start_time.tv_usec = peer_latency_measure_ptr->peer_timestamp_info[i].peer_usec;
                time_diff = _log_ptr->timevaldiff(&start_time, &detail_time);
		        //printf("Elapsed time for lane is: %d milliseconds.\n", time_diff);
                peer_latency_measure_ptr->peer_timestamp_info[i].peer_sec = time_diff;
                peer_latency_measure_ptr->peer_timestamp_info[i].peer_usec = 0;
            } 
        }

        if(lane_member > 1) {
		    for (i = 0; i < lane_member; i++) {
			    if(level_msg_ptr->pid == level_msg_ptr->level_info[i]->pid) {
                    i = i - 1;
                    if(i >= 0){
                        _peer_mgr_ptr->add_downstream(level_msg_ptr->level_info[i]->pid, (struct chunk_t *)peer_latency_measure_ptr);
                    } else {
                        send_pkt_to_pk((struct chunk_t *)peer_latency_measure_ptr);
                    }
                    break;
                } 
            }
        } else{
            send_pkt_to_pk((struct chunk_t *)peer_latency_measure_ptr);
        }
    }
}
*/


//the main handle steram function 需要處理不同序來源的chunk,		
//送到player 的queue 裡面 必須保證是有方向性的 且最好是依序的
void pk_mgr::handle_stream(struct chunk_t *chunk_ptr, int sockfd)
{
	unsigned long i;
//	unsigned long pid;
	unsigned int seq_ready_to_send=0;
	unsigned long parentPid=-1;
	int downStreamSock=-1;
	int downStreamPid=-1;
	stream *strm_ptr=NULL;
//	struct chunk_rtp_t *temp = NULL;
	struct peer_info_t *peer = NULL;
	struct peer_connect_down_t *parentPeerPtr=NULL;
	map<int, queue<struct chunk_t *> *>::iterator iter;		//fd_downstream
	map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
//	map<int, int>::iterator map_rescue_fd_count_iter;
	int leastCurrDiff=0;
	queue<struct chunk_t *> *queue_out_data_ptr;
//	list<unsigned int>::iterator sequence_number_list_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

//	_log_ptr->write_log_format("s =>s u s u s u s u\n", __FUNCTION__,"stream ID=" ,chunk_ptr->header .stream_id,"recieve pkt seqnum", chunk_ptr->header.sequence_number,"bytes=" ,chunk_ptr ->header.length,"timestamp=",chunk_ptr->header.timestamp);


	//更新最後的seq 用來做time out
	fd_pid_iter = _peer_ptr->map_fd_pid.find(sockfd);
	if(fd_pid_iter !=_peer_ptr->map_fd_pid.end()){
		parentPid = fd_pid_iter->second;						//get parentPid of this chunk

		pid_peerDown_info_iter =map_pid_peerDown_info.find(parentPid);
		if(pid_peerDown_info_iter != map_pid_peerDown_info.end()){
			parentPeerPtr = pid_peerDown_info_iter ->second;	//get parentInfo of this chunk
			parentPeerPtr->timeOutNewSeq =chunk_ptr ->header.sequence_number;


		}

	}

	if(chunk_ptr->header.sequence_number > _least_sequence_number){
		_least_sequence_number = chunk_ptr->header.sequence_number;
	}
	if(_current_send_sequence_number == -1){
		_current_send_sequence_number = chunk_ptr->header.sequence_number;
	}

//to ensure sequence number  higher than previous and overlay _chunk_ptr
	if(chunk_ptr->header.sequence_number > (*(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number) {
		memset((char *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)), 0x0, RTP_PKT_BUF_MAX);
		memcpy((char *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)), chunk_ptr, (sizeof(struct chunk_header_t) + chunk_ptr->header.length));
		

//rescue
//hidden at 2013/01/16

//		map_rescue_fd_count_iter = _peer_mgr_ptr->map_rescue_fd_count.find(sockfd);
/*
		if(map_rescue_fd_count_iter != _peer_mgr_ptr->map_rescue_fd_count.end()) {
			if(_peer_mgr_ptr->map_rescue_fd_count.size() > 1) {
				counter = map_rescue_fd_count_iter->second;
				counter--;
				_peer_mgr_ptr->map_rescue_fd_count[sockfd] = counter;
				//cout << "map_rescue_fd_count _size = " << _peer_mgr_ptr->map_rescue_fd_count.size() << endl;
                cout << "peer sock = " << sockfd << endl;
				cout << "counter = " << counter << endl; 
				if(counter == 0) {
					for(map_rescue_fd_count_iter2 = _peer_mgr_ptr->map_rescue_fd_count.begin(); map_rescue_fd_count_iter2 != _peer_mgr_ptr->map_rescue_fd_count.end(); map_rescue_fd_count_iter2++) {
						if(map_rescue_fd_count_iter2->first != sockfd) {
							_peer_mgr_ptr->handle_cut_peer(level_msg_ptr->pid, map_rescue_fd_count_iter2->first);	
							//_peer_mgr_ptr->cut_rescue_peer(map_rescue_fd_count_iter2->first);
						}
					}
					_peer_mgr_ptr->map_rescue_fd_count.clear();
					_peer_mgr_ptr->map_rescue_fd_count[sockfd] = WIN_COUNTER;
					DBG_PRINTF("here\n");
				}
			}
		}
*/
	
		
	} else if((*(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number == chunk_ptr->header.sequence_number){
		chunk_ptr->header.length = 0;
		printf("duplicate sequence number in the buffer  seq=%u\n",chunk_ptr->header.sequence_number);
		return;
	} else {
		chunk_ptr->header.length = 0;
		printf("sequence number smaller than the index in the buffer seq=%u\n",chunk_ptr->header.sequence_number);
		return;
	}
	



//add_downstream(同個lane)(須修改)
//hidden at 2013/01/28
/*
	if(lane_member > 1) {
		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->pid == level_msg_ptr->level_info[i]->pid) {
				i++;
				if( i != lane_member) {
					_peer_mgr_ptr->add_downstream(level_msg_ptr->level_info[i]->pid, (struct chunk_t *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)));
					break;
				}
			} 
		}
	}
*/

//和上面一樣也是在把chunk丟給和自己連接的下游peer (其他 rescue peer) (須修改)
//hidden at 2013/01/27
/*
	for (iter = _peer_mgr_ptr->_map_fd_downstream.begin(); iter != _peer_mgr_ptr->_map_fd_downstream.end(); iter++) {
		queue_out_data_ptr = iter->second;
		sock = iter->first;
		
		fd_pid_iter = _peer_mgr_ptr->map_fd_pid.find(sock);
		pid = fd_pid_iter->second;
		
		pid_peer_info_iter = map_pid_peer_info.find(pid);
		
		if(pid_peer_info_iter != map_pid_peer_info.end()) {
			peer = pid_peer_info_iter->second;
		} else {
			pid_peer_info_iter = map_pid_rescue_peer_info.find(pid);	
			peer = pid_peer_info_iter->second;
		}

		if((peer->manifest & (1 << (chunk_ptr->header.sequence_number % sub_stream_num))) ) {
			queue_out_data_ptr->push((struct chunk_t *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)));
			cout << "rescue_sequence_number = " << chunk_ptr->header.sequence_number << endl;
			cout << "rescue to pid = " << peer->pid << endl;
			_net_ptr->epoll_control(iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		}
		//_net_ptr->epoll_control(iter->first, EPOLL_CTL_ADD, EPOLLOUT);
		
	}
*/



/////////////////////////////////////////////測試版新功能///////////////////////////////////////////////////////////////////////
///*
	
//send down stream(UPLOAD) to other peer if SSID match and in map_pid_rescue_peer_info
	for(pid_peer_info_iter = map_pid_rescue_peer_info.begin();pid_peer_info_iter !=map_pid_rescue_peer_info.end();pid_peer_info_iter++){
		
		downStreamPid =pid_peer_info_iter ->first;			//get downStreamPid
		peer = pid_peer_info_iter ->second;					//get peer info
		map_pid_fd_iter = _peer_ptr ->map_out_pid_fd.find(downStreamPid) ;
		if(map_pid_fd_iter != _peer_ptr ->map_out_pid_fd.end() ){
			downStreamSock = map_pid_fd_iter ->second;		//get downStreamSock

			iter = _peer_ptr ->map_fd_out_data.find(downStreamSock) ;
			if(iter != _peer_ptr ->map_fd_out_data.end())
				queue_out_data_ptr = iter ->second ;		//get queue_out_data_ptr
		}

		if((peer->manifest & (1 << (chunk_ptr->header.sequence_number % sub_stream_num))) ) {
			queue_out_data_ptr->push((struct chunk_t *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)));
//			printf("chunk_ptr->header.sequence_number =%d \n",chunk_ptr->header.sequence_number);
			_net_ptr->epoll_control(downStreamSock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		}
	}



		//差值在BUFF_SIZE 之外可能某個些seq都不到 ,跳過那些seq直到差值在BUFF_SIZE內
		leastCurrDiff = (int)(_least_sequence_number -_current_send_sequence_number);

		if( BUFF_SIZE  <= leastCurrDiff && leastCurrDiff <_bucket_size){

			for(; leastCurrDiff > BUFF_SIZE || _current_send_sequence_number < _least_sequence_number ;_current_send_sequence_number++ ){
					//代表有封包還沒到.略過
					if((*(_chunk_bitstream + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number){
					printf("here1 leastCurrDiff =%d\n",leastCurrDiff);

//					PAUSE
					continue;
					}else{
					leastCurrDiff =_least_sequence_number -_current_send_sequence_number;

					printf("here2\n");
//					PAUSE
					}
			}
		printf("here3 leastCurrDiff =%d\n",leastCurrDiff);
//		_current_send_sequence_number = seq_ready_to_send;

		//可能某個subtream 追過_bucket_size,直接跳到最後一個 (應該不會發生)
		}else if (leastCurrDiff > _bucket_size) {

//			PAUSE
			printf("leastCurrDiff =%d\n",leastCurrDiff);
			_current_send_sequence_number = _least_sequence_number;  

		//在正常範圍內 正常傳輸丟給player 留給下面處理
		}else{

		}

		
		//正常傳輸丟給player
		for(;_current_send_sequence_number <= _least_sequence_number;){

			//下一個還沒到 ,不做處理等待並return
			if((*(_chunk_bitstream + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number){
				printf("wait packet,seq= %d  SSID =%d\n",_current_send_sequence_number,_current_send_sequence_number%sub_stream_num);
//				_current_send_sequence_number = seq_ready_to_send;
//	_log_ptr->write_log_format("s u s u s u s u\n","seq_ready_to_send",seq_ready_to_send,"_least_sequence_number",_least_sequence_number);
//				PAUSE
				return ;
//				continue;

			//為連續,丟給player
			}else if((*(_chunk_bitstream + (_current_send_sequence_number % _bucket_size))).header.stream == STRM_TYPE_MEDIA) {

				//丟給rescue_detecion一定是有方向性的
				rescue_detecion(chunk_ptr);


				for (_map_stream_iter = _map_stream_media.begin(); _map_stream_iter != _map_stream_media.end(); _map_stream_iter++) {
					//per fd mean a player   
					strm_ptr = _map_stream_iter->second;
					 //stream_id 和request 一樣才add chunk
					if((strm_ptr -> _reqStreamID) == (*(_chunk_bitstream + (_current_send_sequence_number % _bucket_size))).header.stream_id ){ 
						strm_ptr->add_chunk((struct chunk_t *)(_chunk_bitstream + (_current_send_sequence_number % _bucket_size)));
						_net_ptr->epoll_control(_map_stream_iter->first, EPOLL_CTL_MOD, EPOLLOUT);
					}
				}
				_current_send_sequence_number++;
			}

			}

//			_current_send_sequence_number = seq_ready_to_send;

		




//*/



/////////////////////////////////////////////測試版新功能///////////////////////////////////////////////////////////////////////



//hidden at 2012/01/23
/*
	for(seq_ready_to_send = _current_send_sequence_number;seq_ready_to_send <= _least_sequence_number;seq_ready_to_send ++){

		//error handle
		//代表有空隔
		if((*(_chunk_bitstream + (seq_ready_to_send % _bucket_size))).header.sequence_number != seq_ready_to_send){

			//seq_ready_to_send substreamID 和chunk的 substreamID一樣 ,且chunk sequence_number >= seq_ready_to_send
			//lose substream 的packet
            if((seq_ready_to_send % sub_stream_num) == (chunk_ptr->header.sequence_number % sub_stream_num) && chunk_ptr->header.sequence_number >= seq_ready_to_send) {


				//相差在10 * sub_stream_num 之內跟PK Server重新要封包
                if((chunk_ptr->header.sequence_number - seq_ready_to_send) <= (10 * sub_stream_num)) { //sub_stream_num < gap < 2 * sub_stream_num
                
//					send_request_sequence_number_to_pk(seq_ready_to_send,seq_ready_to_send);
			        printf("recieve pkt %d, request seq %d from pk\n", chunk_ptr->header.sequence_number, seq_ready_to_send);
                    break;
				//差距太大直接跳到最後一個
                } else { // 3 * sub_stream_num < gap
                    printf("seq_ready_to_send is %d\n", seq_ready_to_send);
                    seq_ready_to_send = _least_sequence_number;  
                    printf("jump seq pointer to %d!\n", seq_ready_to_send);

                    continue;

                }
            } else {
				//lose substream的封包
                //printf("sub stream not complete!\n");
                break;
            }

*/

//normal handle stream
//hidden at 2012/01/22
/*		}else if((*(_chunk_bitstream + (seq_ready_to_send % _bucket_size))).header.stream == STRM_TYPE_AUDIO) {
			for (_map_stream_iter = _map_stream_audio.begin(); _map_stream_iter != _map_stream_audio.end(); _map_stream_iter++) {
				strm_ptr = _map_stream_iter->second;
				strm_ptr->add_chunk((struct chunk_t *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)));
				_net_ptr->epoll_control(_rtsp_viewer_ptr->_sock_udp_audio, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
			}
			
		}else if((*(_chunk_bitstream + (seq_ready_to_send % _bucket_size))).header.stream == STRM_TYPE_VIDEO) {
			for (_map_stream_iter = _map_stream_video.begin(); _map_stream_iter != _map_stream_video.end(); _map_stream_iter++) {
				strm_ptr = _map_stream_iter->second;
				strm_ptr->add_chunk((struct chunk_t *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)));
				_net_ptr->epoll_control(_rtsp_viewer_ptr->_sock_udp_video, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
			}
*/

//hidden at 2012/01/23
/*
// STRM_TYPE_MEDIA, main type
		}else if((*(_chunk_bitstream + (seq_ready_to_send % _bucket_size))).header.stream == STRM_TYPE_MEDIA){
			for (_map_stream_iter = _map_stream_media.begin(); _map_stream_iter != _map_stream_media.end(); _map_stream_iter++) {
//per fd mean a player   
				strm_ptr = _map_stream_iter->second;
				if((strm_ptr -> _reqStreamID) == (*(_chunk_bitstream + (seq_ready_to_send % _bucket_size))).header.stream_id ){  //stream_id 和request 一樣才add chunk
				    strm_ptr->add_chunk((struct chunk_t *)(_chunk_bitstream + (seq_ready_to_send % _bucket_size)));
					_net_ptr->epoll_control(_map_stream_iter->first, EPOLL_CTL_MOD, EPOLLOUT);
				}
			}
		}
	} //end for(seq_ready_to_send = _current_send_sequence_number; ....
	
	_current_send_sequence_number = seq_ready_to_send;


	//if(sequence_number_list.size() == ((unsigned int)_bucket_size)) {	
		//sequence_number_list.clear();
	//}
*/

}

void pk_mgr::add_stream(int strm_addr, stream *strm, unsigned strm_type)
{
/*
	if (strm_type == STRM_TYPE_AUDIO) {
		_map_stream_audio[strm_addr] = strm;
	} else if (strm_type == STRM_TYPE_VIDEO) {
		_map_stream_video[strm_addr] = strm;
	} else */
	if (strm_type == STRM_TYPE_MEDIA) {
		_map_stream_media[strm_addr] = strm;
	}
}


void pk_mgr::del_stream(int strm_addr, stream *strm, unsigned strm_type)
{
/*
	if (strm_type == STRM_TYPE_AUDIO) {
		_map_stream_audio.erase(strm_addr);
	} else if (strm_type == STRM_TYPE_VIDEO) {
		_map_stream_video.erase(strm_addr);
	} else */
	if (strm_type == STRM_TYPE_MEDIA) {
		_map_stream_media.erase(strm_addr);
	}
}

void pk_mgr::data_close(int cfd, const char *reason) 
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

int pk_mgr::get_sock()
{
	return _sock;
}

/////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////Rescue////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////



void pk_mgr::init_rescue_detection()
{
	//Detect substream 的buff
	ssDetect_ptr = (struct detectionInfo *)malloc(sizeof(struct detectionInfo) * sub_stream_num );
	memset( ssDetect_ptr , 0x00 ,sizeof(struct detectionInfo) * sub_stream_num ); 

	statsArryCount_ptr =(int *)malloc(sizeof(int) * (sub_stream_num+1) );
	memset( statsArryCount_ptr , 0x00 ,sizeof(int) * (sub_stream_num+1)  ); 

//	_beginthread(threadTimeout(), 0,NULL );
	_beginthread(launchThread, 0,this );


}



//必須保證進入這個function 的substream 是依序的
void pk_mgr::rescue_detecion(struct chunk_t *chunk_ptr)
{

	int substreamID;
	LARGE_INTEGER	newAlarm; 
	unsigned long sourceTimeDiffOne;
	unsigned long localTimeDiffOne;
	unsigned long sourceTimeDiffTwo;
	unsigned long localTimeDiffTwo;
	double sourceBitrate;
	double localBitrate;


	substreamID= (chunk_ptr ->header.sequence_number) % sub_stream_num  ;
	int X = stream_number * PARAMETER_X;								//測量參數X 和stream_number成正比

//	static bool first=true;

//
//	if(first){

//		printf("X= %d\n",X);
		
//		first=false;
//	}
//


	if(! ( (ssDetect_ptr + substreamID) -> last_seq ) ){  //第一個封包做初始化
	(ssDetect_ptr + substreamID) ->last_timestamp = chunk_ptr->header.timestamp;
	(ssDetect_ptr + substreamID) ->last_seq = chunk_ptr->header.sequence_number;
	(ssDetect_ptr + substreamID) ->first_timestamp = chunk_ptr->header.timestamp;
	_log_ptr -> getTickTime(&((ssDetect_ptr + substreamID) ->lastAlarm));
	_log_ptr -> getTickTime(&((ssDetect_ptr + substreamID) ->firstAlarm));

	}
	//開始計算偵測
	//只有是 比上次記錄新的sequence_number 才做處理
	else if(  ((ssDetect_ptr + substreamID) ->last_seq ) < (chunk_ptr->header.sequence_number) ){
	_log_ptr -> getTickTime(&newAlarm);

//////////////////////////////////////利用頻寬判斷(測量方法一)////////////////////////////////////////////
	((ssDetect_ptr + substreamID) ->  count_X ) ++;
	((ssDetect_ptr + substreamID) ->total_byte ) += (chunk_ptr->header.length);

//只有第一次計算會跑
	if((ssDetect_ptr + substreamID) ->  count_X   == 1)
		(ssDetect_ptr + substreamID) ->first_timestamp = chunk_ptr->header.timestamp;

	if( (ssDetect_ptr + substreamID) ->  count_X   == (X -1) )
		_log_ptr -> getTickTime(&((ssDetect_ptr + substreamID) ->previousAlarm));

//累積X個封包後做判斷
	if(  (ssDetect_ptr + substreamID) ->  count_X  == X ){

		( (ssDetect_ptr + substreamID) ->measure_N )++;  //從1開始計

		sourceTimeDiffOne =  (chunk_ptr->header.timestamp) - (ssDetect_ptr + substreamID) ->first_timestamp;
		localTimeDiffOne=	_log_ptr ->diffTime_ms((ssDetect_ptr + substreamID) ->firstAlarm ,newAlarm);

		sourceBitrate = ( ( double)((ssDetect_ptr + substreamID) ->total_byte ) /(double)sourceTimeDiffOne )*8*1000 ;
		localBitrate  = ( ( double)((ssDetect_ptr + substreamID) ->total_byte ) /(double)localTimeDiffOne  )*8*1000 ;

		(ssDetect_ptr + substreamID) ->last_sourceBitrate =sourceBitrate ;
		(ssDetect_ptr + substreamID) ->last_localBitrate =localBitrate;

//		printf("source_bitrate=%.5f   local_bitrate=%.5f\n",sourceBitrate,localBitrate);
		
		//做每個peer substream的加總 且判斷需不需要救
		measure();


		((ssDetect_ptr + substreamID) ->total_byte ) =chunk_ptr->header.length;
		(ssDetect_ptr + substreamID) ->  count_X =1;
		(ssDetect_ptr + substreamID) ->firstAlarm = (ssDetect_ptr + substreamID) ->previousAlarm ;
		(ssDetect_ptr + substreamID) ->first_timestamp = chunk_ptr->header.timestamp;
	}
//////////////////////////////////////(測量方法一結束)///////////////////////////////////////////////////////////


////////////////////////////////////單看兩個連續封包的delay取max (測量方法二)///////////////////////////////////
	sourceTimeDiffTwo =  (chunk_ptr->header.timestamp) - (ssDetect_ptr + substreamID) ->last_timestamp;
	localTimeDiffTwo=	_log_ptr ->diffTime_ms((ssDetect_ptr + substreamID) ->lastAlarm ,newAlarm);

	if( localTimeDiffTwo > sourceTimeDiffTwo ){ 
			
	if((ssDetect_ptr + substreamID) ->total_buffer_delay  <  (localTimeDiffTwo - sourceTimeDiffTwo)){
		(ssDetect_ptr + substreamID) ->total_buffer_delay = (localTimeDiffTwo - sourceTimeDiffTwo);
//		printf("SSID=%d Max total_buffer_delay %u\n",substreamID,(ssDetect_ptr + substreamID) ->total_buffer_delay);
		}
	}else{
//	printf("on time \n");
	}		

	(ssDetect_ptr + substreamID) ->lastAlarm = newAlarm ;
	(ssDetect_ptr + substreamID) ->last_timestamp = chunk_ptr->header.timestamp;
//////////////////////////////////////(測量方法二結束)///////////////////////////////////////////////////////////




	(ssDetect_ptr + substreamID) ->last_seq = chunk_ptr->header.sequence_number;

	}

	return ;
}



//做每個peer substream的加總 且判斷需不需要救
void pk_mgr::measure()
{	
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *connectPeerInfo = NULL;
	unsigned long tempManifest=0 ;
	unsigned long afterManifest=0 ;
	unsigned long sentToPKManifest=0 ;
	int perPeerSS_num = 0;		//針對一個peer sub stream 的個數 
	int peerHighestSSID = -1;	//用來確保是跟這次測量做加總,而不是和上一次
	double totalSourceBitrate =0;
	double totalLocalBitrate =0 ;
	unsigned int count_N=0;
	unsigned int continuous_P=0;
	unsigned int rescueSS=0;
	unsigned int tempMax=0;
	static bool triggerContinue=true;
	unsigned int lastTrigger=0;

	memset( statsArryCount_ptr , 0x00 ,sizeof(int) * (sub_stream_num+1)  ); 

	for(pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter != map_pid_peerDown_info.end(); pid_peerDown_info_iter++) {

		tempManifest = (pid_peerDown_info_iter->second)-> peerInfo.manifest;
		if(tempManifest ==0)
			continue;

		connectPeerInfo =pid_peerDown_info_iter->second ;
		
		//先取得perPeerSS_num ,和peerHighestSSID
		for(int i=0 ;i< sub_stream_num ;i++ ){
			//i=substreamID
			if(  ((tempManifest >> i)  & 1)  &&  (i >= peerHighestSSID)  ){
			perPeerSS_num++;
			peerHighestSSID =i;
			}
		}

		//計算出totalSourceBitrate ,totalLocalBitrate
		for(int i=0 ;i< sub_stream_num ;i++ ){
			//i=substreamID 
			if(  ((tempManifest >> i) & 1)  ){

				//等到最後一個substream 到期再做加總
				if((ssDetect_ptr + i) ->measure_N  == (ssDetect_ptr + peerHighestSSID)->measure_N){
				totalSourceBitrate += (ssDetect_ptr + i) ->last_sourceBitrate ;
				totalLocalBitrate  += (ssDetect_ptr + i) ->last_localBitrate ;

				}else{
				//還沒等到最後一個substream 到期 提前結束不做運算
					return ;
				}
			}
		}

		printf("totalSourceBitrate=%.5f   totalLocalBitrate=%.5f\n",totalSourceBitrate,totalLocalBitrate);

		//設定最近偵測的狀態到rescueStatsArry
		for(int i=0 ; i<= perPeerSS_num ;i++){
			//介在需要rescue i 個substream之間
			if ( (totalLocalBitrate < (1 - (double)(2*i-1)/(double)(2*perPeerSS_num) )*totalSourceBitrate ) && ( totalLocalBitrate > (1 - (double)(2*(i+1)-1)/(double)(2*perPeerSS_num) )*totalSourceBitrate)){
			//rescue i substream
			connectPeerInfo ->rescueStatsArry[( (ssDetect_ptr + peerHighestSSID)->measure_N -1)% PARAMETER_M ] =i;
			}
		}
		if( (totalLocalBitrate > (1 - (double)(-1)/(double)(2*perPeerSS_num) )*totalSourceBitrate )){
			connectPeerInfo ->rescueStatsArry[( (ssDetect_ptr + peerHighestSSID)->measure_N -1)% PARAMETER_M ] =0;
		}

		printf("****************   PID=%d   *******************\n",connectPeerInfo ->peerInfo.pid);

		//根據rescueStatsArry 來決定要不要觸發rescue
		for(int i=0 ; i<PARAMETER_M ;i++){

			//做統計
			( *(statsArryCount_ptr + connectPeerInfo ->rescueStatsArry[i]) )++ ;

			if(connectPeerInfo ->rescueStatsArry[i] >0){
			count_N++;
//			totalStats+=( pid_peer_info_iter->second ) ->rescueStatsArry[i] ;
//			rescueSS= (totalStats/count_N) +1 ;
			}


			
			printf("%d  ",connectPeerInfo ->rescueStatsArry[i]);
		}
		printf("\n");

			//近PARAMETER_P次 發生 P次
			for(int j=0 ; j<PARAMETER_P ;j++){
				if ( connectPeerInfo ->rescueStatsArry[ (PARAMETER_M +( (ssDetect_ptr + peerHighestSSID)->measure_N -1)-j )% PARAMETER_M ] >0 ){
					continuous_P++ ;
				}
			}


		for(int k=0 ; k<( (ssDetect_ptr + peerHighestSSID)->measure_N -1)% PARAMETER_M ;k++)
		printf("   ");
		printf("↑\n");

		//找出統計最多的值
		for(int k=0 ;k< (sub_stream_num+1) ;k++){
		printf("substream%d = %d   \n",k,*(statsArryCount_ptr+ k) );

		if( k != 0 && tempMax < (*(statsArryCount_ptr+ k)) ){
				tempMax =(*(statsArryCount_ptr+ k)) ;
				rescueSS = k ;
				}
		}

		//符合條件觸發rescue 需要救rescue_ss 個
		if(count_N >= PARAMETER_N  || continuous_P == PARAMETER_P){

			if(triggerContinue){

				printf("continuous_P =%d\npid=%d need cut %d substream and need rescue\n",continuous_P,connectPeerInfo ->peerInfo.pid,rescueSS);

				//PID是PK的有問題 (代表是這個peer下載能力有問題)
				if(connectPeerInfo ->peerInfo.pid ==PK_PID){
					printf("peer need set dead\n");
					PAUSE
				
				//PID是其他peer
				}else{
				afterManifest = manifestFactory (connectPeerInfo ->peerInfo.manifest , rescueSS);
				pkDownInfoPtr ->peerInfo.manifest |=(connectPeerInfo ->peerInfo.manifest &(~afterManifest) );
				send_rescueManifestToPK(pkDownInfoPtr ->peerInfo.manifest );
				printf("rescue manifest %d after manifest %d  PK=%d",connectPeerInfo ->peerInfo.manifest,afterManifest,sentToPKManifest);
				connectPeerInfo ->peerInfo.manifest = afterManifest ;
				_peer_mgr_ptr->send_manifest_to_parent(afterManifest ,connectPeerInfo ->peerInfo.pid);
				}


				lastTrigger = (ssDetect_ptr + peerHighestSSID)->measure_N ;
				triggerContinue =false ;
			}else{
				
				if(((ssDetect_ptr + peerHighestSSID)->measure_N  -lastTrigger ) >=PARAMETER_N)
				triggerContinue =true ;
			}

		}


	}
	
}



void pk_mgr::send_rescueManifestToPK(unsigned long manifestValue)
{

	struct rescue_pkt_from_server  *chunk_rescueManifestPtr = NULL;
	
	chunk_rescueManifestPtr = new struct rescue_pkt_from_server;



	memset(chunk_rescueManifestPtr, 0x0, sizeof(struct rescue_pkt_from_server));
	
	chunk_rescueManifestPtr->header.cmd = CHNK_CMD_PEER_RESCUE ;
	chunk_rescueManifestPtr->header.length = (sizeof(struct rescue_pkt_from_server)-sizeof(struct chunk_header_t)) ;	//pkt_buf paylod length
	chunk_rescueManifestPtr->header.rsv_1 = REQUEST ;
	chunk_rescueManifestPtr->pid = _peer_mgr_ptr ->self_pid ;
	chunk_rescueManifestPtr->manifest = manifestValue ;
	chunk_rescueManifestPtr->rescue_seq_start =_current_send_sequence_number ;

	_net_ptr->set_blocking(_sock);
	
	_net_ptr ->send(_sock , (char*)chunk_rescueManifestPtr ,sizeof(struct rescue_pkt_from_server),0) ;

	_net_ptr->set_nonblocking(_sock);

	delete chunk_rescueManifestPtr;
	
	return ;
	 
}




void pk_mgr::clear_map_pid_peer_info(){

	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(pid_peer_info_iter =map_pid_peer_info.begin();pid_peer_info_iter!=map_pid_peer_info.end(); pid_peer_info_iter++){
		peerInfoPtr=pid_peer_info_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_peer_info.clear();

}

void pk_mgr::clear_map_pid_peerDown_info(){

	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *peerDownInfoPtr =NULL;

	for(pid_peerDown_info_iter =map_pid_peerDown_info.begin();pid_peerDown_info_iter!= map_pid_peerDown_info.end(); pid_peerDown_info_iter++){
		peerDownInfoPtr=pid_peerDown_info_iter ->second;
		delete peerDownInfoPtr;
	}

	map_pid_peerDown_info.clear();

}

void pk_mgr::clear_map_pid_rescue_peer_info(){

	map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(map_pid_rescue_peer_info_iter =map_pid_rescue_peer_info.begin();map_pid_rescue_peer_info_iter!= map_pid_rescue_peer_info.end(); map_pid_rescue_peer_info_iter++){
		peerInfoPtr=map_pid_rescue_peer_info_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_rescue_peer_info.clear();

}

//回傳cut掉ssNumber 數量的manifestValue
unsigned long pk_mgr::manifestFactory(unsigned long manifestValue,unsigned int ssNumber)
{

	unsigned long afterManifestValue=0;
	unsigned int countss=0;

	for(int i=0 ;  i< sub_stream_num; i++){
	
		if(  (1 << i) & manifestValue){
		
		afterManifestValue |= (1 << i) ;
		countss++;
		if(countss == ssNumber)
			break;
		}
		
	}

	manifestValue &= (~afterManifestValue);
	return manifestValue;
	


}

//close socket and sent rescue to pk

 void pk_mgr::threadTimeout()
{

	printf("thread start \n");
	int sock=-1;
	struct peer_connect_down_t *parentPeerPtr=NULL;
	unsigned long parentPid=0;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;


	while(1){


		for(pid_peerDown_info_iter =map_pid_peerDown_info.begin(); pid_peerDown_info_iter!= map_pid_peerDown_info.end() ;pid_peerDown_info_iter++)	{

			parentPeerPtr = pid_peerDown_info_iter ->second;	//get parent peer info 
			parentPid = parentPeerPtr ->peerInfo.pid;			//get parent pid

			map_pid_fd_iter = _peer_ptr ->map_in_pid_fd.find(parentPid);
			if(map_pid_fd_iter!= _peer_ptr ->map_in_pid_fd.end())
				sock=map_pid_fd_iter ->second;					//get parent sock


			if(parentPeerPtr ->timeOutLastSeq == parentPeerPtr->timeOutNewSeq && parentPeerPtr->peerInfo.manifest!=0){
	
				printf("Pid =%d Time out\n",parentPid);
				send_rescueManifestToPK (parentPeerPtr->peerInfo.manifest);
				_peer_ptr ->data_close(sock ,"time out data_close ",CLOSE_PARENT);
				pid_peerDown_info_iter =map_pid_peerDown_info.begin();

			}else{


			parentPeerPtr ->timeOutLastSeq =parentPeerPtr ->timeOutNewSeq ;

			}

		}

//		printf("hello thread\n");
		Sleep(3000);	

	}



}


  void pk_mgr::launchThread(void * arg){
 
	pk_mgr * pk_mgr_ptr = NULL ;

	pk_mgr_ptr = static_cast<pk_mgr *>(arg);
	pk_mgr_ptr ->threadTimeout();
	
	printf("not go to here\n");
 }


  unsigned int pk_mgr::rescueNumAccumulate(){
  
  	map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;
	unsigned long tempManifest=0;
	unsigned int totalRescueNum =0;

	for(map_pid_rescue_peer_info_iter = map_pid_rescue_peer_info.begin();map_pid_rescue_peer_info_iter!=map_pid_rescue_peer_info.end();map_pid_rescue_peer_info_iter++){
	
		tempManifest = map_pid_rescue_peer_info_iter ->second ->manifest;
		for(int i=0 ; i<sub_stream_num ; i++){
			if( (1 << i) &  tempManifest )
				totalRescueNum++;
		}

	
	}

	printf("totalRescueNum = %d \n",totalRescueNum);
	return totalRescueNum ;
  
  }