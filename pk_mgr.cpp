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
	 public_ip = 0;
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
	 childrenSet_ptr = NULL;
	 full_manifest =0 ;

	 
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

//////////////////////////////////////////////////////////////////////////////////measure start delay
void pk_mgr::source_delay_init(unsigned long init_ssid){
	_log_ptr -> getTickTime(&((delay_table+init_ssid)->client_start_time));
	(delay_table+init_ssid)->source_delay_init = 1;
}

void pk_mgr::delay_table_init()
{
	delay_table = (struct source_delay *)malloc(sizeof(struct source_delay) * sub_stream_num );
	memset( delay_table , 0x00 ,sizeof(struct source_delay) * sub_stream_num ); 

	for(int i=0;i<sub_stream_num;i++){
		(delay_table+i)->start_delay_struct.init_flag = 0;
		(delay_table+i)->start_delay_struct.sub_stream_id = i;
		(delay_table+i)->start_delay_struct.start_delay = 0;
		(delay_table+i)->start_delay_struct.end_clock;
		(delay_table+i)->start_delay_struct.start_clock;
		(delay_table+i)->client_start_time;
		(delay_table+i)->source_delay_time = -1;
		(delay_table+i)->start_seq_num = 0;
		(delay_table+i)->source_delay_init = 0;
		(delay_table+i)->start_seq_abs_time = 0;
		(delay_table+i)->end_seq_num = 0;
		(delay_table+i)->end_seq_abs_time = 0;
	}
	printf("%d %d %d\n",(delay_table+2)->start_seq_num,sizeof(struct source_delay),sub_stream_num);
	printf("====================================================================\n");
}
/*
void pk_mgr::send_start_delay_measure_token(int sock,unsigned long sub_id){
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;

	struct start_delay_measure_send *chunk_delay_ptr = NULL;
	map<int , unsigned long>::iterator temp_map_fd_pid_iter;
	
	
	chunk_delay_ptr = (struct start_delay_measure_send *)new unsigned char[sizeof(struct start_delay_measure_send)];
	
	memset(chunk_delay_ptr, 0x0, sizeof(struct start_delay_measure_send));
	
	chunk_delay_ptr->header.cmd = CHNK_CMD_PEER_START_DELAY;
	chunk_delay_ptr->header.rsv_1 = REQUEST;
	chunk_delay_ptr->header.length = sizeof(struct start_delay_measure_send) - sizeof(struct chunk_header_t);
	chunk_delay_ptr->ssid = sub_id;

	temp_map_fd_pid_iter = _peer_ptr->map_fd_pid.find(sock);
	if(temp_map_fd_pid_iter == _peer_ptr->map_fd_pid.end()){
		printf("can not find fd error send_start_delay_measure_token\n");
		//exit(1);
		chunk_delay_ptr->pid = -5;

		printf("starting send start delay token to pk ...\n");
		int send_byte = 0;
		int expect_len = chunk_delay_ptr->header.length + sizeof(struct chunk_header_t);
		char send_buf[sizeof(struct start_delay_measure_send)];
		_net_ptr->set_blocking(sock);	// set to blocking

		memset(send_buf, 0x0, sizeof(struct start_delay_measure_send));
		memcpy(send_buf, chunk_delay_ptr, expect_len);
	
		send_byte = _net_ptr->send(sock, send_buf, expect_len, 0);
		if( send_byte <= 0 ) {
			data_close(sock, "send pkt error");
			_log_ptr->exit(0, "send pkt error");
		} else {
			if(chunk_delay_ptr)
				delete chunk_delay_ptr;
			_net_ptr->set_nonblocking(sock);	// set to non-blocking
		}
		_log_ptr -> getTickTime(&((delay_table+sub_id)->start_delay_struct.start_clock));
	}
	else{
		fd_queue_iter = _peer_ptr->map_fd_out_ctrl.find(sock);
		if(fd_queue_iter == _peer_ptr->map_fd_out_ctrl.end()){
			printf("can not find out ctrl queue error send_start_delay_measure_token\n");
			PAUSE
			exit(1);
		}

		chunk_delay_ptr->pid = temp_map_fd_pid_iter->second;

		queue_out_ctrl_ptr = fd_queue_iter->second;
		queue_out_ctrl_ptr->push((struct chunk_t *)chunk_delay_ptr);

		_log_ptr -> getTickTime(&((delay_table+sub_id)->start_delay_struct.start_clock));

		if(queue_out_ctrl_ptr->size() != 0 ) {
			_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		} 
	}
	printf("send_start_delay_measure_token end\n");
}


void pk_mgr::send_back_start_delay_measure_token(int sock,long long peer_start_delay,unsigned long sub_id){
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;

	struct start_delay_measure_receive *chunk_delay_ptr = NULL;
	map<int , unsigned long>::iterator temp_map_fd_pid_iter;
	
	chunk_delay_ptr = (struct start_delay_measure_receive *)new unsigned char[sizeof(struct start_delay_measure_receive)];
	
	memset(chunk_delay_ptr, 0x0, sizeof(struct start_delay_measure_receive));
	
	chunk_delay_ptr->header.cmd = CHNK_CMD_PEER_START_DELAY;
	chunk_delay_ptr->header.rsv_1 = REPLY;
	chunk_delay_ptr->header.length = sizeof(struct start_delay_measure_receive) - sizeof(struct chunk_header_t);
	chunk_delay_ptr->ssid = sub_id;
	chunk_delay_ptr->source_delay_recev = peer_start_delay;

	temp_map_fd_pid_iter = _peer_ptr->map_fd_pid.find(sock);
	if(temp_map_fd_pid_iter == _peer_ptr->map_fd_pid.end()){
		printf("can not find fd error send_back_start_delay_measure_token\n");
		PAUSE
		exit(1);
	}
	else{
		fd_queue_iter = _peer_ptr->map_fd_out_ctrl.find(sock);
		if(fd_queue_iter == _peer_ptr->map_fd_out_ctrl.end()){
			printf("can not find out ctrl queue error send_back_start_delay_measure_token\n");
			PAUSE
			exit(1);
		}


		chunk_delay_ptr->pid = temp_map_fd_pid_iter->second;
		queue_out_ctrl_ptr = fd_queue_iter->second;
		queue_out_ctrl_ptr->push((struct chunk_t *)chunk_delay_ptr);


		if(queue_out_ctrl_ptr->size() != 0 ) {
			_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		} 
	}
}*/
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////send capacity
	/*
	unsigned int rescue_num;
	int rescue_condition;
	unsigned int source_delay;
	char NAT_status;
	char content_integrity;
	*/
void pk_mgr::send_capacity_init(){
	peer_start_delay_count = 0;
	//peer_join_send = 0;
}

void pk_mgr::send_capacity_to_pk(int sock){
	/*map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;*/
	map<int , unsigned long>::iterator temp_map_fd_pid_iter;
	struct rescue_peer_capacity_measurement *chunk_capacity_ptr = NULL;
	//struct chunk_t * chunk_ptr = NULL;
	int msg_size,send_size;
	
	msg_size = sizeof(struct rescue_peer_capacity_measurement) + sizeof(long long *)*sub_stream_num;
	send_size = msg_size - sizeof(long long *)*sub_stream_num + sizeof(long long)*sub_stream_num; 

	chunk_capacity_ptr = (struct rescue_peer_capacity_measurement *)new unsigned char[msg_size];
	//chunk_ptr = (struct chunk_t *) new unsigned char[send_size];

	memset(chunk_capacity_ptr, 0x0, msg_size);
	//memset(chunk_ptr, 0x0, send_size);
	
	chunk_capacity_ptr->header.cmd = CHNK_CMD_PEER_RESCUE_CAPACITY;
	chunk_capacity_ptr->header.rsv_1 = REPLY;
	chunk_capacity_ptr->header.length = send_size - sizeof(struct chunk_header_t);
	chunk_capacity_ptr->content_integrity = 1;
	chunk_capacity_ptr->NAT_status = 1;
	//chunk_capacity_ptr->rescue_condition = 0;
	chunk_capacity_ptr->rescue_num = rescueNumAccumulate();
	for(int i=0;i<sub_stream_num;i++){
		if((delay_table+i)->start_delay_struct.init_flag == 0){
			printf("send capacity error start delay have to init\n");
			PAUSE
			exit(1);
		}
		else{
			LARGE_INTEGER client_end_clock;
			_log_ptr -> getTickTime(&client_end_clock);
			(delay_table+i)->source_delay_time = _log_ptr ->diffTime_ms((delay_table+i)->client_start_time,client_end_clock);
			if((delay_table+i)->end_seq_num == 0){
				printf("start delay end seq error in send_capacity_to_pk\n");
				PAUSE
				exit(1);
			}
			else if(((delay_table+i)->end_seq_num)< ((delay_table+i)->start_seq_num)){
				printf("start delay end seq < start seq error in send_capacity_to_pk\n");
				PAUSE
				exit(1);
			}
			else if(((delay_table+i)->end_seq_num) == ((delay_table+i)->start_seq_num)){
				printf("start delay end seq == start seq warning in send_capacity_to_pk\n");
				printf("abs start seq : %d\n",(delay_table+i)->start_seq_num);
			}
			else{
				unsigned int temp;
				long long diff_temp;
				temp = (delay_table+i)->end_seq_abs_time - (delay_table+i)->start_seq_abs_time;
				diff_temp = (delay_table+i)->source_delay_time - (long long)temp;
				//printf("source_delay_time : %ld",(delay_table+i)->source_delay_time);
				//printf(" time : %d\n",temp);
				printf("abs start seq : %d time : %d\n",(delay_table+i)->start_seq_num,(delay_table+i)->start_seq_abs_time);
				printf("abs end seq : %d time : %d\n",(delay_table+i)->end_seq_num,(delay_table+i)->end_seq_abs_time);
				printf("differ : %d\n",diff_temp);
				if(diff_temp < 0){
					printf("diff error in send_capacity_to_pk\n");
					printf("differ : %d\n",diff_temp);
					(delay_table+i)->source_delay_time = 0;
					//exit(1);
				}
				else{
					(delay_table+i)->source_delay_time = diff_temp;
				}
			}
			(delay_table+i)->source_delay_time = (delay_table+i)->source_delay_time + (delay_table+i)->start_delay_struct.start_delay;
			chunk_capacity_ptr->source_delay_measur[i] = new (long long);
			memset(chunk_capacity_ptr->source_delay_measur[i], 0x0 , sizeof(long long));
			memcpy(chunk_capacity_ptr->source_delay_measur[i],&((delay_table+i)->source_delay_time),sizeof(long long));
			printf("start delay : %ld ",(delay_table+i)->start_delay_struct.start_delay);
			printf(" source delay : %ld\n",*(chunk_capacity_ptr->source_delay_measur[i]));
		}
	}

	//memcpy(chunk_ptr,chunk_capacity_ptr,send_size);

	/*if(chunk_capacity_ptr)
		delete chunk_capacity_ptr;*/

	int send_byte = 0;
	int expect_len = chunk_capacity_ptr->header.length + sizeof(struct chunk_header_t);
	char *send_buf;
	int capacity_chunk_offset = expect_len - sizeof(long long)*sub_stream_num;
	_net_ptr->set_blocking(sock);	// set to blocking

	send_buf = (char *)new char[send_size];
	memset(send_buf, 0x0, send_size);
	memcpy(send_buf, chunk_capacity_ptr, capacity_chunk_offset);
	for(int i=0;i<sub_stream_num;i++){
		memcpy((send_buf+capacity_chunk_offset),chunk_capacity_ptr->source_delay_measur[i],sizeof(long long));
		capacity_chunk_offset+=sizeof(long long);
	}
	
	printf("%ld\n",*((long long *)(send_buf+sizeof(chunk_header_t)+sizeof(unsigned int)+sizeof(int)+sizeof(char)+sizeof(char))));
	printf("%d %d\n",capacity_chunk_offset,expect_len);

	send_byte = _net_ptr->send(sock, send_buf, expect_len, 0);
	if( send_byte <= 0 ) {
		data_close(sock, "send pkt error");
		_log_ptr->exit(0, "send pkt error");
	} else {
		if(send_buf)
			delete send_buf;
		if(chunk_capacity_ptr)
			delete chunk_capacity_ptr;
		_net_ptr->set_nonblocking(sock);	// set to non-blocking
	}
	printf("send_capacity_to_pk end\n");
}
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
/*void pk_mgr::send_start_delay_update(int sock, unsigned long start_delay_manifest, int start_delay_differ){
	
	struct update_start_delay *update_start_delay_ptr = NULL;
	struct chunk_t * chunk_ptr = NULL;
	unsigned long msg_size;
	unsigned long reply_size;
	unsigned long offset = 0;
	
	msg_size = sizeof(struct chunk_header_t) + sub_stream_num * sizeof(struct start_delay_update_info *);
	reply_size = msg_size - sub_stream_num * sizeof(struct start_delay_update_info *) + sub_stream_num * sizeof(struct start_delay_update_info);

	update_start_delay_ptr = (struct update_start_delay *) new unsigned char[msg_size];
	chunk_ptr = (struct chunk_t *) new unsigned char[reply_size];
		
	memset(update_start_delay_ptr, 0x0, msg_size );
	memset(chunk_ptr, 0x0, reply_size );

	offset = offset + sizeof(struct chunk_header_t);
	
	for(int i=0;i<sub_stream_num;i++){
		if(start_delay_manifest & (1<<i)){
			update_start_delay_ptr->update_info[i] = new struct start_delay_update_info;
			memset(update_start_delay_ptr->update_info[i], 0x0 , sizeof(struct start_delay_update_info));

			update_start_delay_ptr->update_info[i]->substream_id = i;
			update_start_delay_ptr->update_info[i]->start_delay_update = start_delay_differ;

			memcpy((char *)chunk_ptr + offset, update_start_delay_ptr->update_info[i], sizeof(struct start_delay_update_info));
			if(update_start_delay_ptr->update_info[i])
					delete update_start_delay_ptr->update_info[i];
			offset += sizeof(struct start_delay_update_info);
		}
		else{
			update_start_delay_ptr->update_info[i] = new struct start_delay_update_info;
			memset(update_start_delay_ptr->update_info[i], 0x0 , sizeof(struct start_delay_update_info));

			update_start_delay_ptr->update_info[i]->substream_id = i;
			update_start_delay_ptr->update_info[i]->start_delay_update = 0;

			memcpy((char *)chunk_ptr + offset, update_start_delay_ptr->update_info[i], sizeof(struct start_delay_update_info));
			if(update_start_delay_ptr->update_info[i])
					delete update_start_delay_ptr->update_info[i];
			offset += sizeof(struct start_delay_update_info);
		}
	}

	offset = 0;
	
	update_start_delay_ptr->header.cmd = CHNK_CMD_PEER_START_DELAY_UPDATE;
	update_start_delay_ptr->header.length = reply_size - sizeof(struct chunk_header_t);
	update_start_delay_ptr->header.rsv_1 = REPLY;
	
	
	memcpy((char *)chunk_ptr + offset, update_start_delay_ptr, (sizeof(struct chunk_header_t)));
	
	if(update_start_delay_ptr)
		delete update_start_delay_ptr;
	
	map<unsigned long, struct peer_info_t *>::iterator temp_map_pid_rescue_peer_info;	
	map<unsigned long, int>::iterator temp_map_pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr;
	
	for(temp_map_pid_rescue_peer_info = map_pid_rescue_peer_info.begin(); temp_map_pid_rescue_peer_info != map_pid_rescue_peer_info.end();temp_map_pid_rescue_peer_info++){
		temp_map_pid_fd_iter = _peer_ptr->map_out_pid_fd.find(temp_map_pid_rescue_peer_info->first);
		if(temp_map_pid_fd_iter == _peer_ptr->map_out_pid_fd.end()){
			printf("can not find output pid in map_out_pid_fd (send_start_delay_update)\n");
			PAUSE
			exit(1);
		}
		else{
			fd_out_ctrl_iter = _peer_ptr->map_fd_out_ctrl.find(temp_map_pid_fd_iter->second);
			if(fd_out_ctrl_iter == _peer_ptr->map_fd_out_ctrl.end()){
				printf("can not find output ctrl queue in map_fd_out_ctrl (send_start_delay_update)\n");
				PAUSE
				exit(1);
			}
			queue_out_ctrl_ptr = fd_out_ctrl_iter->second;
			queue_out_ctrl_ptr->push(chunk_ptr);
			_net_ptr->epoll_control(temp_map_pid_fd_iter->second, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		}
	}
}*/
//////////////////////////////////////////////////////////////////////////////////

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

	pkDownInfoPtr = new struct peer_connect_down_t ;
	memset(pkDownInfoPtr , 0x0,sizeof( struct peer_connect_down_t));
	pkDownInfoPtr ->peerInfo.pid =PK_PID;
	map_pid_peerDown_info[PK_PID] =pkDownInfoPtr;
	
    //web_ctrl_sever_ptr = new web_ctrl_sever(_net_ptr, _log_ptr, fd_list_ptr, &map_stream_name_id); 
    //web_ctrl_sever_ptr->init();
	
	if (build_connection(pk_ip, pk_port)) {
		cout << "pk_mgr build_connection() success" << endl;
	} else {
		cout << "pk_mgr build_connection() fail" << endl;
		PAUSE
		exit(0);
	}

	queue<struct chunk_t *> *queue_out_ctrl_ptr;
	queue<struct chunk_t *> *queue_out_data_ptr;

	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	_peer_ptr ->map_out_pid_fd[PK_PID] = _sock;
	_peer_ptr ->map_fd_pid[_sock] = PK_PID;
	_peer_ptr ->map_fd_out_ctrl[_sock] = queue_out_ctrl_ptr;
	_peer_ptr ->map_fd_out_data[_sock] = queue_out_data_ptr;


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
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
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
//ligh |  pid |  level |   bit_rate|   sub_stream_num |  public_ip |  inside_lane_rescue_num | n*struct level_info_t
//這邊應該包含整條lane 的peer_info 包含自己

	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_REG  ) {
		lane_member = (buf_len - sizeof(struct chunk_header_t) - 6 * sizeof(unsigned long)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + lane_member * sizeof(struct level_info_t *);

		printf("lane_member = %d ",lane_member);

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		memset(level_msg_ptr, 0x0, level_msg_size);
		memcpy(level_msg_ptr, chunk_ptr, (level_msg_size - lane_member * sizeof(struct level_info_t *)));
		
		offset += (level_msg_size - lane_member * sizeof(struct level_info_t *));
		
		memcpy(&bit_rate, (char *)chunk_ptr + offset, sizeof(unsigned long));
		memcpy(&sub_stream_num, ((char *)chunk_ptr + offset + sizeof(unsigned long)), sizeof(unsigned long));
		memcpy(&public_ip, ((char *)chunk_ptr + offset + 2 * sizeof(unsigned long)), sizeof(unsigned long));
		memcpy(&inside_lane_rescue_num, ((char *)chunk_ptr + offset + 3 * sizeof(unsigned long)), sizeof(unsigned long));
		

		_peer_mgr_ptr ->set_up_public_ip(public_ip);


		cout<< "bit_rate = " <<  bit_rate << endl;
		cout<< "sub_stream_num = " <<  sub_stream_num << endl;
		cout<< "public_ip = " <<  public_ip << endl;
		cout<< "inside_lane_rescue_num = " <<  inside_lane_rescue_num << endl;

		offset += sizeof(unsigned long) * 4;

	//將收到的封包放進  去除掉bit_rate .sub_stream_num .public_ip . inside_lane_rescue_num  ,後放進  chunk_level_msg_t

		//註冊時要的manifest是要全部的substream
		unsigned long tempManifes=0;
		for(unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
							tempManifes |= (1 << ss_id);
							}


		for (i = 0; i < lane_member; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			new_peer = new struct peer_info_t;
			
			memset(level_msg_ptr->level_info[i], 0x0 , sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			
			memset(new_peer, 0x0 , sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));

			offset += sizeof(struct level_info_t);

	//add lane peer_info to map table
//			map_pid_peer_info[new_peer->pid] = new_peer;
			map_pid_peer_info.insert(pair<unsigned long ,peer_info_t *>(new_peer->pid,new_peer));  

			new_peer ->manifest = tempManifes;
		}

		_peer_mgr_ptr -> self_pid = level_msg_ptr ->pid ;

		//收到sub_stream_num後對rescue 偵測結構做初始化
		init_rescue_detection();

		//////////////////////////////////////////////////////////////////////////////////measure start delay
		delay_table_init();
		send_capacity_init();
		//////////////////////////////////////////////////////////////////////////////////
	//和lane 每個peer 先建立好連線 
		if(lane_member >= 1)
			_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);

		_peer_mgr_ptr->handle_test_delay(tempManifes);

		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->level_info[i])
				delete level_msg_ptr->level_info[i];
		}
		
		if(level_msg_ptr)
			delete level_msg_ptr;



	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_START_DELAY_UPDATE) {
		printf("CHNK_CMD_PEER_START_DELAY_UPDATE pk mgr\n");
		exit(1);
	//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
		/*struct update_start_delay *update_start_delay_ptr = NULL;
		update_start_delay_ptr = (update_start_delay *)chunk_ptr;
		for(int k=0;k<sub_stream_num;k++){
			(delay_table+k)->start_delay_struct.start_delay = (delay_table+k)->start_delay_struct.start_delay + update_start_delay_ptr->update_info[k]->start_delay_update;

			unsigned long temp_manifest = 0;
			temp_manifest = temp_manifest | (1<<k);

			send_start_delay_update(sock, temp_manifest, update_start_delay_ptr->update_info[k]->start_delay_update);
		}*/
	//////////////////////////////////////////////////////////////////////////////////
	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_START_DELAY) {
	//////////////////////////////////////////////////////////////////////////////////measure start delay

		printf("CHNK_CMD_PEER_START_DELAY pk mgr\n");
		exit(1);
		/*
		if(chunk_ptr->header.rsv_1 == REQUEST){

			printf(" not go here!!!!!!!!!!!!!!!!!!!CHNK_CMD_PEER_START_DELAY pk mgr request\n");
			PAUSE
		}
		else{
			printf("CHNK_CMD_PEER_START_DELAY pk mgr reply\n");
			unsigned long request_sub_id;
			long long parent_start_delay;

			memcpy(&request_sub_id, (char *)chunk_ptr + sizeof(struct chunk_header_t) + sizeof(unsigned long), sizeof(unsigned long));
			memcpy(&parent_start_delay, (char *)chunk_ptr + sizeof(struct chunk_header_t) + (2*sizeof(unsigned long)), sizeof(long long));

			if((delay_table+request_sub_id)->start_delay_struct.init_flag == 1){
				_log_ptr -> getTickTime(&((delay_table+request_sub_id)->start_delay_struct.end_clock));
				//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
				int renew_start_delay_flag=0;
				long long old_start_delay = (delay_table+request_sub_id)->start_delay_struct.start_delay;
				if((delay_table+request_sub_id)->start_delay_struct.start_delay != -1){
					renew_start_delay_flag = 1;
				}
				//////////////////////////////////////////////////////////////////////////////////
				(delay_table+request_sub_id)->start_delay_struct.start_delay = _log_ptr ->diffTime_ms((delay_table+request_sub_id)->start_delay_struct.start_clock,(delay_table+request_sub_id)->start_delay_struct.end_clock);
				(delay_table+request_sub_id)->start_delay_struct.start_delay = parent_start_delay + ((delay_table+request_sub_id)->start_delay_struct.start_delay/2);
				//(delay_table+request_sub_id)->start_delay_struct.init_flag = 1;
				printf("start delay : %ld parent start delay : %ld\n",(delay_table+request_sub_id)->start_delay_struct.start_delay,parent_start_delay);
				//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
				if(renew_start_delay_flag == 1){
					printf("start delay renew \n");
					unsigned long temp_manifest = 0;
					
					int delay_differ = (delay_table+request_sub_id)->start_delay_struct.start_delay - old_start_delay;
					temp_manifest = temp_manifest | (1<<request_sub_id);
					send_start_delay_update(sock, temp_manifest, delay_differ);
				}
				//////////////////////////////////////////////////////////////////////////////////

				//////////////////////////////////////////////////////////////////////////////////send capacity
				peer_start_delay_count++;
				if((peer_start_delay_count == sub_stream_num)&&(!peer_join_send)&&((delay_table+request_sub_id)->start_seq_num != 0)&&((delay_table+request_sub_id)->end_seq_num != 0)){
					send_capacity_to_pk(_sock);
					peer_join_send = 1;
				}
				//////////////////////////////////////////////////////////////////////////////////
			}
			else{
				printf("start_delay update error\n");
			}
		}*/
	//////////////////////////////////////////////////////////////////////////////////


// light | pid | level   | n*struct rescue_peer_info
	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_RESCUE_LIST) {
		printf("CHNK_CMD_PEER_RESCUE_LIST\n");
		//system("pause");
////////////////////////////////////////////////
//		clear_map_pid_peer_info();

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


//			map_pid_peer_info[new_peer->pid] = new_peer;
			map_pid_peer_info.insert(pair<unsigned long ,peer_info_t *>(new_peer->pid,new_peer));  

			new_peer ->manifest = ((struct chunk_rescue_list*)chunk_ptr) ->manifest;

		}


//////////////////////////////////

		_peer_ptr->first_reply_peer =true;
	

//和lane 每個peer 先建立好連線	
//
		if(lane_member >= 1)
			_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);

		_peer_mgr_ptr->handle_test_delay( ((struct chunk_rescue_list*)chunk_ptr) ->manifest);


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

	

	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_NOTIFY) {
		printf("cmd =recv CHNK_CMD_PEER_NOTIFY\n");

//rtmp_chunk_size change
//  hidden at 2013/01/16


//header.cmd == CHNK_CMD_PEER_LATENCY
    } else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_LATENCY){
//	printf("cmd =recv CHNK_CMD_PEER_LATENCY\n");
//hidden at 2013/01/16

//cmd == CHNK_CMD_RT_NLM   //network latency measurement
    } else if(chunk_ptr->header.cmd == CHNK_CMD_RT_NLM) {	//--!! 0128 rcv from lightning
	printf("cmd =recv CHNK_CMD_RT_NLM\n");
// hidden at 2013/01/13


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
	}else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_SEED){
		printf("cmd =recv CHNK_CMD_PEER_SEED\n");

		pkDownInfoPtr ->peerInfo.manifest = ((struct seed_notify *)chunk_ptr) ->manifest ;

		for(unsigned long substreamID =0 ; substreamID < sub_stream_num ;substreamID++)
		{
			if( pkDownInfoPtr ->peerInfo.manifest &  SubstreamIDToManifest(substreamID))
				send_parentToPK ( SubstreamIDToManifest(substreamID) , PK_PID+1 );
		}

//CHNK_CMD_PEER_PARENT_CHILDREN
	}else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_PARENT_CHILDREN){
		printf("cmd =recv CHNK_CMD_PEER_PARENT_CHILDREN\n");

/*
		if(chunk_ptr->header.rsv_1 == REQUEST){
		
			handleAppenSelfdPid(chunk_ptr);
		
		}else if(chunk_ptr->header.rsv_1 == REPLY && _peer_ptr->leastSeq_set_childrenPID == chunk_ptr ->header.sequence_number){
		
			 storeChildrenToSet(chunk_ptr);
		}
*/

//other 
	}else {
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
	unsigned long temp_sub_id=0;

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

	//還沒註冊拿到substream num
	if(sub_stream_num == 0)
		return;


	temp_sub_id = (chunk_ptr ->header.sequence_number) % sub_stream_num;


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
		


	
		
	} else if((*(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number == chunk_ptr->header.sequence_number){
		chunk_ptr->header.length = 0;
		printf("duplicate sequence number in the buffer  seq=%u\n",chunk_ptr->header.sequence_number);
		return;
	} else {
		chunk_ptr->header.length = 0;
		printf("sequence number smaller than the index in the buffer seq=%u\n",chunk_ptr->header.sequence_number);
		return;
	}
	





/////////////////////////////////////////////測試版新功能///////////////////////////////////////////////////////////////////////
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

	//if rescue testing stream
	if( parentPeerPtr ->peerInfo.manifest & pkDownInfoPtr->peerInfo.manifest  && parentPid != PK_PID){
		(ssDetect_ptr + temp_sub_id) ->isTesting =1 ;	//ture
		//這邊只是暫時改變PK的substream 實際上還是有串流下來
		pkDownInfoPtr->peerInfo.manifest &= ~(pkDownInfoPtr->peerInfo.manifest & parentPeerPtr ->peerInfo.manifest);  
		//開始testing 送topology
		send_parentToPK ( SubstreamIDToManifest (temp_sub_id) , (ssDetect_ptr + temp_sub_id)->previousParentPID ); 
	}

	if((ssDetect_ptr + temp_sub_id) ->isTesting){
		(ssDetect_ptr + temp_sub_id) ->testing_count++ ;
		//測試次數填滿整個狀態  也就是測量了PARAMETER_M 次都沒問題
		if((ssDetect_ptr + temp_sub_id) ->testing_count % (stream_number * PARAMETER_X)  >= PARAMETER_M ){
			(ssDetect_ptr + temp_sub_id) ->isTesting =0 ;  //false
			(ssDetect_ptr + temp_sub_id) ->testing_count =0 ;
			//testing ok should cut this substream from pk
			pkDownInfoPtr->peerInfo.manifest  &=  ~SubstreamIDToManifest(temp_sub_id) ;
			send_rescueManifestToPKUpdate ( pkDownInfoPtr->peerInfo.manifest);

			//選擇selected peer 送topology
			send_parentToPK(SubstreamIDToManifest (temp_sub_id) ,PK_PID +1 );


		}
	}



	//////////////////////////////////////////////////////////////////////////////////measure start delay

	//////////////////////////////////////////////////////////////////////////////////send capacity
	(delay_table+temp_sub_id)->end_seq_num = chunk_ptr ->header.sequence_number;
	(delay_table+temp_sub_id)->end_seq_abs_time = chunk_ptr ->header.timestamp;
	//////////////////////////////////////////////////////////////////////////////////

	//printf("%d %d\n",(delay_table+temp_sub_id)->start_delay_struct.init_flag,temp_sub_id);
	if(!((delay_table+temp_sub_id)->start_delay_struct.init_flag)){
		/*printf("start measure start delay\n");
		send_start_delay_measure_token(sockfd, temp_sub_id);*/
		(delay_table+temp_sub_id)->start_seq_num = chunk_ptr ->header.sequence_number;
		(delay_table+temp_sub_id)->start_seq_abs_time = chunk_ptr->header.timestamp;
		(delay_table+temp_sub_id)->start_delay_struct.init_flag = 1;
		source_delay_init(temp_sub_id);
		peer_start_delay_count++;
		if(peer_start_delay_count == sub_stream_num){
			send_capacity_to_pk(_sock);
		}
	}
	
	//////////////////////////////////////////////////////////////////////////////////
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



				//如果這個sustream 正在測試中則從pk來的不進入測試
				if((ssDetect_ptr + temp_sub_id) ->isTesting && parentPid == PK_PID){
				
				// do nothing
				}else{
					//丟給rescue_detecion一定是有方向性的
					rescue_detecion(chunk_ptr);
				}


//				rescue_detecion(chunk_ptr);

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

	if (strm_type == STRM_TYPE_MEDIA) {
		_map_stream_media[strm_addr] = strm;
	}
}


void pk_mgr::del_stream(int strm_addr, stream *strm, unsigned strm_type)
{

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


// call after register
void pk_mgr::init_rescue_detection()
{
	for(int i =0 ; i<sub_stream_num ; i++ )
		full_manifest |= ( 1<<i );

	//Detect substream 的buff
	ssDetect_ptr = (struct detectionInfo *)malloc(sizeof(struct detectionInfo) * sub_stream_num );
	memset( ssDetect_ptr , 0x00 ,sizeof(struct detectionInfo) * sub_stream_num ); 

	statsArryCount_ptr =(int *)malloc(sizeof(int) * (sub_stream_num+1) );
	memset( statsArryCount_ptr , 0x00 ,sizeof(int) * (sub_stream_num+1)  ); 

//	_beginthread(launchThread, 0,this );

	for(int i =0 ; i<sub_stream_num ;i++){
	childrenSet_ptr  = new std::set<int>;
	map_streamID_childrenSet[i]=childrenSet_ptr;
	}

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

	}else{
		printf("why here old packet here??\n");
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
//	unsigned long sentToPKManifest=0 ;
	int perPeerSS_num = 0;		//針對一個peer sub stream 的個數 
	int peerHighestSSID = -1;	//用來確保是跟這次測量做加總,而不是和上一次
	double totalSourceBitrate =0;
	double totalLocalBitrate =0 ;
	unsigned int count_N=0;
	unsigned int continuous_P=0;
	unsigned int rescueSS=0;
	unsigned int tempMax=0;
	static bool triggerContinue=true;
	static unsigned int lastTrigger=0;
	unsigned long testingSubStreamID=-1 ;
	unsigned long testingManifest=0 ;

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

			//找出所有正在測試的substream
			for(int i =0  ; i < sub_stream_num;i++){
				if ((ssDetect_ptr + i) ->isTesting ){
					testingManifest |= SubstreamIDToManifest(i);
				}
			}
			//找出這個peer 所有正在testing 的substream ID
			testingManifest = (testingManifest & connectPeerInfo ->peerInfo.manifest);


			if(triggerContinue){

				printf("continuous_P =%d\npid=%d need cut %d substream and need rescue\n",continuous_P,connectPeerInfo ->peerInfo.pid,rescueSS);

				//PID是PK的有問題 (代表是這個peer下載能力有問題)
				if(connectPeerInfo ->peerInfo.pid ==PK_PID){
					printf("download have problem , peer need set dead\n");
//should sent capacity
//					PAUSE
//					exit(1);
				
				//PID是正在測試的peer 測試失敗
				}else if(testingManifest){

					//若有多個正在測試中一次只選擇一個substream cut(最右邊的)  並且重設全部記數器的count

					testingSubStreamID = manifestToSubstreamID (testingManifest);

					(ssDetect_ptr + testingSubStreamID) ->isTesting =0 ;  //false  

					//should sent to PK select PK ,再把testing 取消偵測的 pk_manifest 設回來
					pkDownInfoPtr ->peerInfo.manifest |=  SubstreamIDToManifest (testingSubStreamID ) ;
					send_parentToPK ( SubstreamIDToManifest (testingSubStreamID ) ,PK_PID+1 ) ;

					//should sent to peer cut stream
					connectPeerInfo ->peerInfo.manifest &= (~ SubstreamIDToManifest (testingSubStreamID )) ;
					_peer_mgr_ptr -> send_manifest_to_parent(connectPeerInfo ->peerInfo.manifest ,connectPeerInfo ->peerInfo.pid);

					testingManifest &= ( ~SubstreamIDToManifest(testingSubStreamID) );

					while(testingManifest){
					testingSubStreamID = manifestToSubstreamID (testingManifest);
					(ssDetect_ptr + testingSubStreamID) ->testing_count =0;
					testingManifest &= ( ~SubstreamIDToManifest(testingSubStreamID) );
					}

				//PID 是其他peer
				}else{
				afterManifest = manifestFactory (connectPeerInfo ->peerInfo.manifest , rescueSS);
				pkDownInfoPtr ->peerInfo.manifest |=(connectPeerInfo ->peerInfo.manifest &(~afterManifest) );
				send_rescueManifestToPK(pkDownInfoPtr ->peerInfo.manifest );
				printf("rescue manifest %d after manifest %d  PK=%d",connectPeerInfo ->peerInfo.manifest,afterManifest,pkDownInfoPtr ->peerInfo.manifest );
				connectPeerInfo ->peerInfo.manifest = afterManifest ;
				_peer_mgr_ptr->send_manifest_to_parent(afterManifest ,connectPeerInfo ->peerInfo.pid);

				tempManifest =pkDownInfoPtr ->peerInfo.manifest ;

				//把先前的連接的PID存起來
				while(tempManifest){
					testingSubStreamID = manifestToSubstreamID (tempManifest);
					(ssDetect_ptr + testingSubStreamID) ->previousParentPID = connectPeerInfo ->peerInfo.pid;
					tempManifest &=  (~SubstreamIDToManifest(testingSubStreamID) );
				}

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

	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(pid_peer_info_iter =map_pid_peer_info.begin();pid_peer_info_iter!=map_pid_peer_info.end(); pid_peer_info_iter++){
		peerInfoPtr=pid_peer_info_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_peer_info.clear();

}

void pk_mgr::clear_map_pid_peer_info(unsigned long manifest){

	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;


	for(pid_peer_info_iter =map_pid_peer_info.begin();pid_peer_info_iter!=map_pid_peer_info.end(); pid_peer_info_iter++){
		peerInfoPtr=pid_peer_info_iter ->second;

		if(peerInfoPtr ->manifest == manifest){
			delete peerInfoPtr;

			map_pid_peer_info.erase(pid_peer_info_iter);
			pid_peer_info_iter =map_pid_peer_info.begin() ;
			if(pid_peer_info_iter == map_pid_peer_info.end())
				break;

		}
	}


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

	printf("thread start  hello\n");
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


			if(parentPeerPtr->peerInfo.pid!=PK_PID && parentPeerPtr ->timeOutLastSeq == parentPeerPtr->timeOutNewSeq && parentPeerPtr->peerInfo.manifest!=0){
	
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

//include sent functiom
  void pk_mgr::handleAppenSelfdPid(struct chunk_t *chunk_ptr ){
  
	unsigned long pktlen =0;
	unsigned long ploadlen =0;
	struct chunk_t *chunk_delay_ptr=NULL;
	int sock =-1;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	unsigned long tempManifest = 0;
	unsigned long  subStreamID =0;
	set<unsigned long>::iterator set_childrenPID_iter;

	/* (unsigned long) = pid */
	pktlen = sizeof(struct chunk_header_t) + chunk_ptr ->header.length + sizeof(unsigned long);
	ploadlen = chunk_ptr ->header.length + sizeof(unsigned long) ;

	_peer_ptr->leastSeq_set_childrenPID =chunk_ptr ->header.sequence_number;

	memcpy(&tempManifest ,chunk_ptr +sizeof(struct chunk_header_t) ,sizeof(unsigned long));
	for(subStreamID=0 ; subStreamID< sub_stream_num;subStreamID++){
		if((1<<subStreamID) & tempManifest);
		break;
	}

	childrenSet_ptr = map_streamID_childrenSet[subStreamID] ;
	childrenSet_ptr ->clear();


	chunk_delay_ptr = (struct chunk_t *)new unsigned char[pktlen];
	memset(chunk_delay_ptr, 0x0, pktlen);
	memcpy(chunk_delay_ptr ,chunk_ptr ,sizeof(struct chunk_header_t) + chunk_ptr ->header.length);

	chunk_delay_ptr->header.cmd = CHNK_CMD_PEER_PARENT_CHILDREN;
	chunk_delay_ptr->header.rsv_1 = REQUEST;
	chunk_delay_ptr->header.length = ploadlen;
	memcpy(chunk_delay_ptr+ sizeof(struct chunk_header_t) + chunk_ptr ->header.length , &(_peer_mgr_ptr->self_pid) ,sizeof(unsigned long)) ;


	//不是最後一個且要同個subtreamID才送
	if(map_pid_rescue_peer_info.size() != 0 ){

		for(map_pid_rescue_peer_info_iter = map_pid_rescue_peer_info.begin();map_pid_rescue_peer_info_iter!= map_pid_rescue_peer_info.end();map_pid_rescue_peer_info_iter++){
			map_pid_fd_iter = _peer_ptr->map_out_pid_fd.find(map_pid_rescue_peer_info_iter ->first);
			if(map_pid_fd_iter != _peer_ptr->map_out_pid_fd.end() && map_pid_rescue_peer_info_iter->second->manifest | tempManifest){

				_net_ptr->set_nonblocking(sock);
	
				_net_ptr ->send(sock , (char*)chunk_delay_ptr ,pktlen,0) ;
			}
		}

		//為最後一個 須往回傳
		}else if(map_pid_rescue_peer_info.size() == 0 ){
	
			chunk_delay_ptr->header.rsv_1 = REPLY;

			for(pid_peerDown_info_iter = map_pid_peerDown_info.begin();pid_peerDown_info_iter!= map_pid_peerDown_info.end();pid_peerDown_info_iter++){
				map_pid_fd_iter = _peer_ptr->map_in_pid_fd.find(map_pid_rescue_peer_info_iter ->first);
				if(map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end() && pid_peerDown_info_iter->second->peerInfo.manifest | tempManifest){
					_net_ptr->set_nonblocking(sock);
	
					_net_ptr ->send(sock , (char*)chunk_delay_ptr ,pktlen,0) ;
				}
			}


		}


  }


// | header | manifest | pif |pid  | ... 
void pk_mgr::storeChildrenToSet(struct chunk_t *chunk_ptr )
{

	set<unsigned long>::iterator set_childrenPID_iter;
	unsigned long * pidPtr =NULL;
	int childrenNum = 0;
	unsigned long subStreamID  = 0;
	unsigned long tempManifest = 0;


	childrenNum= ( (chunk_ptr->header.length)- sizeof(unsigned long) )/sizeof(unsigned long);
	memcpy(&tempManifest ,chunk_ptr +sizeof(struct chunk_header_t) ,sizeof(unsigned long));

	pidPtr=(unsigned long *)((char*)chunk_ptr +sizeof(chunk_header_t) + sizeof(unsigned long));

	for(subStreamID=0 ; subStreamID< sub_stream_num;subStreamID++){
		if((1<<subStreamID) & tempManifest);
		break;
	}


	childrenSet_ptr = map_streamID_childrenSet[subStreamID] ;
	for(int i =0; i< childrenNum ;i++){
		childrenSet_ptr ->insert( *pidPtr) ;
		pidPtr++;
	}


}


//若對應到多個則只會回傳最小的(最右邊 最低位的)
unsigned long  pk_mgr::manifestToSubstreamID(unsigned long  manifest )

{
	unsigned long  SubstreamID =0;
	for(int i=0 ; i < sub_stream_num;i++){
		if( (1<< i)  & manifest ){
			SubstreamID = i;
			return SubstreamID;
		}
			
	}

}


//會回傳唯一manifest
unsigned long  pk_mgr::SubstreamIDToManifest(unsigned long  SubstreamID )
{
	unsigned long manifest =0;
	manifest |=  (1<<SubstreamID) ;
		return manifest;
}




void pk_mgr::send_rescueManifestToPKUpdate(unsigned long manifestValue)
{

	struct rescue_update_from_server  *chunk_rescueManifestPtr = NULL;
	
	chunk_rescueManifestPtr = new struct rescue_update_from_server;



	memset(chunk_rescueManifestPtr, 0x0, sizeof(struct rescue_update_from_server));
	
	chunk_rescueManifestPtr->header.cmd = CHNK_CMD_PEER_RESCUE_UPDATE ;
	chunk_rescueManifestPtr->header.length = (sizeof(struct rescue_update_from_server)-sizeof(struct chunk_header_t)) ;	//pkt_buf paylod length
	chunk_rescueManifestPtr->header.rsv_1 = REQUEST ;
	chunk_rescueManifestPtr->pid = _peer_mgr_ptr ->self_pid ;
	chunk_rescueManifestPtr->manifest = manifestValue ;


	_net_ptr->set_blocking(_sock);
	
	_net_ptr ->send(_sock , (char*)chunk_rescueManifestPtr ,sizeof(struct rescue_pkt_from_server),0) ;

	_net_ptr->set_nonblocking(_sock);

	delete chunk_rescueManifestPtr;
	
	return ;
	 
}


// 這邊的 manifestValue 只會有一個 sunstream ID
// header | self_pid | manifest | pareentPID  | pareentPID | ... 
// if (oldPID == PK_PID+1 )  沒有舊的parent  for testing stream
void pk_mgr::send_parentToPK(unsigned long manifestValue,unsigned long oldPID){

	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct update_topology_info  *parentListPtr = NULL;
	unsigned long  packetlen =0 ;
	int  i=0 ;

	unsigned long count =0 ;

	if( oldPID != PK_PID+1 ){
		count ++ ;
	} 

	for(pid_peerDown_info_iter =map_pid_peerDown_info.begin() ;pid_peerDown_info_iter != map_pid_peerDown_info.end() ;pid_peerDown_info_iter++ ){
		//這個parent 有傳給自己
		if(pid_peerDown_info_iter ->second ->peerInfo.manifest & manifestValue ){
			count ++;
		}
	}


	packetlen = count * sizeof (unsigned long ) + sizeof(struct update_topology_info) ;
	parentListPtr = (struct update_topology_info *) new char [packetlen];


	memset(parentListPtr, 0x0, sizeof(struct update_topology_info));
	
	parentListPtr->header.cmd = CHNK_CMD_TOPO_INFO ;
	parentListPtr->header.length = ( packetlen-sizeof(struct chunk_header_t)) ;	//pkt_buf = payload length
	parentListPtr->header.rsv_1 = REQUEST ;
	parentListPtr->parent_num = count ; 
	parentListPtr->manifest = manifestValue ;

	if( oldPID != PK_PID+1 ){
		parentListPtr ->parent_pid [i] = oldPID ;
		i ++ ;
	} 


	for(pid_peerDown_info_iter =map_pid_peerDown_info.begin() ;pid_peerDown_info_iter != map_pid_peerDown_info.end() ;pid_peerDown_info_iter++ ){
		//這個parent 有傳給自己
		if(pid_peerDown_info_iter ->second ->peerInfo.manifest & manifestValue ){
			 
			parentListPtr ->parent_pid [i] = pid_peerDown_info_iter ->first ;

			i++ ;

		}
	}

	_net_ptr->set_blocking(_sock);
	
	_net_ptr ->send(_sock , (char*)parentListPtr ,packetlen ,0) ;

	_net_ptr->set_nonblocking(_sock);

	delete parentListPtr;
	
	return ;




}