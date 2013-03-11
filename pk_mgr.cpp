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
	_channel_id = 0;
	lane_member = 0;
	 bit_rate = 0;
	 sub_stream_num = 0;
	 public_ip = 0;
	 inside_lane_rescue_num = 0;
	 outside_lane_rescue_num = 0;
//	 count = 0;
	 _manifest = 0;
//	 _check = 0;
	 current_child_manifest = 0;
	 _least_sequence_number = 0;
	 stream_number=1;
	 _current_send_sequence_number = -1;
	 pkDownInfoPtr =NULL ;
	 childrenSet_ptr = NULL;
	 full_manifest =0 ;

	 
	_prep->read_key("bucket_size", _bucket_size);


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

//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL


void pk_mgr::delay_table_init()
{
	//map<unsigned long, struct source_delay *>::iterator delay_table_iter;
	for(int k=0;k<sub_stream_num;k++){
		struct source_delay *source_delay_temp_ptr = NULL;
		source_delay_temp_ptr = new struct source_delay;
		memset( source_delay_temp_ptr , 0x00 ,sizeof(struct source_delay)); 


		source_delay_temp_ptr->source_delay_time = -1;
		source_delay_temp_ptr->first_pkt_recv = 0;

		source_delay_temp_ptr->end_seq_num = 0;
		source_delay_temp_ptr->end_seq_abs_time = 0;
		source_delay_temp_ptr->rescue_state = 0;
		source_delay_temp_ptr->delay_beyond_count = 0;

		delay_table.insert(pair<unsigned long,struct source_delay *>(k,source_delay_temp_ptr));
		printf("source delay init end\n");
	}
}
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL



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
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL


void pk_mgr::source_delay_detection(int sock,unsigned long sub_id, unsigned int seq_now){
	map<unsigned long,struct source_delay *>::iterator delay_table_iter;

	delay_table_iter = delay_table.find(sub_id);
	if(delay_table_iter == delay_table.end()){
		printf("error : can not find sub stream struct in source_delay_detection\n");
		exit(1);
	}

	if((seq_now > syn_table.start_seq)&&(delay_table_iter->second->rescue_state==0)){
	long long detect_source_delay_time;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	map<int , unsigned long>::iterator detect_map_fd_pid_iter;
	unsigned long detect_peer_id;
	unsigned long testingManifest=0 ;
	unsigned long testingSubStreamID=-1 ;
	unsigned long afterManifest = 0;
	unsigned long tempManifest=0 ;

	detect_map_fd_pid_iter = _peer_ptr->map_fd_pid.find(sock);
	if(detect_map_fd_pid_iter == _peer_ptr->map_fd_pid.end()){
		printf("error : can not find fd pid in source_delay_detection\n");
		exit(1);
	}

	pid_peerDown_info_iter = map_pid_peerDown_info.find(detect_map_fd_pid_iter->second);
	if(pid_peerDown_info_iter == map_pid_peerDown_info.end()){
		printf("error : can not find pid peerinfo in source_delay_detection\n");
		exit(1);
	}

	detect_source_delay_time = (long long)(_log_ptr ->diffTime_us(syn_table.start_clock,delay_table_iter->second->client_end_time));
	if(delay_table_iter->second->end_seq_num == 0){
		printf("start delay end seq error in source_delay_detection\n");
		PAUSE
		exit(1);
	}
	else{
			long long temp;
			long long diff_temp;
			temp = (long long)(delay_table_iter->second->end_seq_abs_time) - (long long)(syn_table.client_abs_start_time/1000);
			diff_temp = (long long)(detect_source_delay_time/1000) - (long long)temp;
			if(diff_temp < 0){
				printf("diff error in source_delay_detection\n");
				printf("differ : %lld ",(long long)diff_temp);
				printf("syn_round : %lld\n",(long long)syn_round_time);
				printf("syn_table.client_abs_start_time : %lld\n",(long long)syn_table.client_abs_start_time);
				syn_table.client_abs_start_time = syn_table.client_abs_start_time + (abs(diff_temp)*1000);
				printf("syn_table.client_abs_start_time : %lld\n",(long long)syn_table.client_abs_start_time);
				//PAUSE
				//exit(1);
			}
			else if(diff_temp > MAX_DELAY){
				delay_table_iter->second->delay_beyond_count = delay_table_iter->second->delay_beyond_count + 1;
				if(delay_table_iter->second->delay_beyond_count > SOURCE_DELAY_CONTINUOUS){
					delay_table_iter->second->delay_beyond_count = 0;

					//找出所有正在測試的substream
					for(int i =0  ; i < sub_stream_num;i++){
						if ((ssDetect_ptr + i) ->isTesting ){
							testingManifest |= SubstreamIDToManifest(i);
						}
					}
					//找出這個peer 所有正在testing 的substream ID
					testingManifest = (testingManifest & pid_peerDown_info_iter->second->peerInfo.manifest);

					if(pid_peerDown_info_iter->second->peerInfo.pid == PK_PID){
						printf("Provider is PK can not rescue\n");
					}
					else if(testingManifest){
						printf("differ : %lld need to rescue source_delay_detection\n",(long long)diff_temp);
						printf("sub stream : %ld pid : %d need to rescue source_delay_detection\n",sub_id,pid_peerDown_info_iter->second->peerInfo.pid);
						printf("The substream is testing!!!!!\n",sub_id,pid_peerDown_info_iter->second->peerInfo.pid);
						//若有多個正在測試中一次只選擇一個substream cut(最右邊的)  並且重設全部記數器的count

						testingSubStreamID = manifestToSubstreamID (testingManifest);

						(ssDetect_ptr + testingSubStreamID) ->isTesting =0 ;  //false  

						//should sent to PK select PK ,再把testing 取消偵測的 pk_manifest 設回來
						pkDownInfoPtr ->peerInfo.manifest |=  SubstreamIDToManifest (testingSubStreamID ) ;
						send_parentToPK ( SubstreamIDToManifest (testingSubStreamID ) ,PK_PID+1 ) ;

						//should sent to peer cut stream
						pid_peerDown_info_iter->second->peerInfo.manifest &= (~ SubstreamIDToManifest (testingSubStreamID )) ;
						_peer_mgr_ptr -> send_manifest_to_parent(pid_peerDown_info_iter->second->peerInfo.manifest ,pid_peerDown_info_iter->second->peerInfo.pid);

						testingManifest &= ( ~SubstreamIDToManifest(testingSubStreamID) );

						while(testingManifest){
							testingSubStreamID = manifestToSubstreamID (testingManifest);
							(ssDetect_ptr + testingSubStreamID) ->testing_count =0;
							testingManifest &= ( ~SubstreamIDToManifest(testingSubStreamID) );
						}

						printf("stream testing fail \n");
					}
					else{
						printf("differ : %lld need to rescue source_delay_detection\n",(long long)diff_temp);
						printf("sub stream : %ld pid : %d need to rescue source_delay_detection\n",sub_id,pid_peerDown_info_iter->second->peerInfo.pid);
						delay_table_iter->second->rescue_state = 1;
						//PAUSE
						afterManifest = manifestFactory (pid_peerDown_info_iter->second->peerInfo.manifest , 1);
						pkDownInfoPtr ->peerInfo.manifest |=(pid_peerDown_info_iter->second->peerInfo.manifest &(~afterManifest) );
						send_rescueManifestToPK(pkDownInfoPtr ->peerInfo.manifest );
						printf("rescue manifest %d after manifest %d  PK=%d \n",pid_peerDown_info_iter->second->peerInfo.manifest,afterManifest,pkDownInfoPtr ->peerInfo.manifest );
						pid_peerDown_info_iter->second->peerInfo.manifest = afterManifest ;
						_peer_mgr_ptr->send_manifest_to_parent(afterManifest ,pid_peerDown_info_iter->second->peerInfo.pid);

						tempManifest =pkDownInfoPtr ->peerInfo.manifest ;

						//把先前的連接的PID存起來
						while(tempManifest){
							testingSubStreamID = manifestToSubstreamID (tempManifest);
							(ssDetect_ptr + testingSubStreamID) ->previousParentPID = pid_peerDown_info_iter->second->peerInfo.pid;
							tempManifest &=  (~SubstreamIDToManifest(testingSubStreamID) );
						}

					}
				}
			}
			else{
				printf("differ : %lld source_delay_detection\n",(long long)diff_temp);
			}
	}
	}
}

void pk_mgr::send_capacity_to_pk(int sock){
	/*map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;*/
	map<int , unsigned long>::iterator temp_map_fd_pid_iter;
	struct rescue_peer_capacity_measurement *chunk_capacity_ptr = NULL;
	//struct chunk_t * chunk_ptr = NULL;
	int msg_size,send_size;
	map<unsigned long, struct source_delay *>::iterator delay_table_iter;

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
	chunk_capacity_ptr->rescue_num = rescueNumAccumulate();
	for(int i=0;i<sub_stream_num;i++){
		if(syn_table.init_flag != 2){
			printf("send capacity error not syn\n");
			PAUSE
			exit(1);
		}
		else{
			delay_table_iter = delay_table.find(i);
			if(delay_table_iter == delay_table.end()){
				printf("error : can not find source struct in table in send_capacity_to_pk\n");
				exit(1);
			}
			delay_table_iter->second->source_delay_time = (long long)(_log_ptr ->diffTime_us(syn_table.start_clock,delay_table_iter->second->client_end_time));
			if(delay_table_iter->second->end_seq_num == 0){
				printf("start delay end seq error in send_capacity_to_pk\n");
				PAUSE
				exit(1);
			}
			/*else if((delay_table_iter->second->end_seq_num)< (delay_table_iter->second->start_seq_num)){
				printf("start delay end seq < start seq error in send_capacity_to_pk\n");
				PAUSE
				exit(1);
			}
			else if((delay_table_iter->second->end_seq_num) == (delay_table_iter->second->start_seq_num)){
				printf("start delay end seq == start seq warning in send_capacity_to_pk\n");
				PAUSE
				exit(1);
			}*/
			else{
				long long temp;
				long long diff_temp;
				temp = (long long)(delay_table_iter->second->end_seq_abs_time) - (long long)(syn_table.client_abs_start_time/1000);
				diff_temp = (long long)(delay_table_iter->second->source_delay_time/1000) - (long long)temp;
				//printf("source_delay_time : %ld",(delay_table+i)->source_delay_time);
				//printf(" time : %d\n",temp);
				printf("abs differ : %lld\n",(long long)temp);
				printf("abs start : %lld\n",(long long)(syn_table.client_abs_start_time/1000));
				printf("abs end : %lld\n",(long long)(delay_table_iter->second->end_seq_abs_time));
				printf("relate differ : %lld\n",(long long)(delay_table_iter->second->source_delay_time/1000));
				printf("differ : %lld\n",(long long)diff_temp);
				if(diff_temp < 0){
					printf("diff error in send_capacity_to_pk\n");
					printf("differ : %lld\n",(long long)diff_temp);
					delay_table_iter->second->source_delay_time = 0;
					printf("syn_table.client_abs_start_time : %lld\n",(long long)syn_table.client_abs_start_time);
					syn_table.client_abs_start_time = syn_table.client_abs_start_time + (abs(diff_temp)*1000);
					printf("syn_table.client_abs_start_time : %lld\n",(long long)syn_table.client_abs_start_time);
					//PAUSE
					//exit(1);
				}
				else{
					delay_table_iter->second->source_delay_time = diff_temp;
				}
			}
			//(delay_table+i)->source_delay_time = (delay_table+i)->source_delay_time + (delay_table+i)->start_delay_struct.start_delay;
			chunk_capacity_ptr->source_delay_measur[i] = new (long long);
			memset(chunk_capacity_ptr->source_delay_measur[i], 0x0 , sizeof(long long));
			memcpy(chunk_capacity_ptr->source_delay_measur[i],&(delay_table_iter->second->source_delay_time),sizeof(long long));
			printf(" source delay : %lld\n",(long long)(*(chunk_capacity_ptr->source_delay_measur[i])));
		}
	}

	//memcpy(chunk_ptr,chunk_capacity_ptr,send_size);

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
	
	//printf("%ld\n",*((long long *)(send_buf+sizeof(chunk_header_t)+sizeof(unsigned int)+sizeof(int)+sizeof(char)+sizeof(char))));
	//printf("%d %d\n",capacity_chunk_offset,expect_len);

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
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL



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

	unsigned long i;
	unsigned long buf_len;
	unsigned long level_msg_size;
	int recv_byte;
	int expect_len = 0;
	int offset = 0;
	int ret = -1;
	unsigned long total_bit_rate = 0;
	unsigned long ss_id = 0;
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, unsigned long>::iterator map_pid_manifest_iter;
	list<int>::iterator outside_rescue_list_iter;

	
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
		else if(recv_byte == 0){
			printf("sock closed\n");
			_peer_ptr->data_close(sock, "recv error in peer::handle_pkt_in",DONT_CARE);
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
		else if(recv_byte == 0){
			printf("sock closed\n");
			_peer_ptr->data_close(sock, "recv error in peer::handle_pkt_in",DONT_CARE);
				//PAUSE
			return RET_SOCK_ERROR;
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
		//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
		syn_table_init(_sock);
		//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
		//////////////////////////////////////////////////////////////////////////////////
	//和lane 每個peer 先建立好連線 
		if(lane_member >= 1){
			_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);

			_peer_mgr_ptr->handle_test_delay(tempManifes);
		}


		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->level_info[i])
				delete level_msg_ptr->level_info[i];
		}
		
		if(level_msg_ptr)
			delete level_msg_ptr;


	//////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SYN) {
	//////////////////////////////////////////////////////////////////////////////////measure start delay

		printf("CHNK_CMD_PEER_SYN pk mgr\n");
		
		if(chunk_ptr->header.rsv_1 == REQUEST){

			printf(" not go here!!!!!!!!!!!!!!!!!!!CHNK_CMD_PEER_START_DELAY pk mgr request\n");
			PAUSE
		}
		else{
			printf("CHNK_CMD_PEER_START_DELAY pk mgr reply\n");
			struct syn_token_receive* syn_token_receive_ptr;
			syn_token_receive_ptr = (struct syn_token_receive*)chunk_ptr;
			syn_recv_handler(syn_token_receive_ptr);

		}

	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

// light | pid | level   | n*struct rescue_peer_info
	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_RESCUE_LIST) {
		printf("CHNK_CMD_PEER_RESCUE_LIST\n");

////////////////////////////////////////////////
//		clear_map_pid_peer_info();

		lane_member = (buf_len - sizeof(struct chunk_header_t) - sizeof(unsigned long) - sizeof(unsigned long)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + lane_member * sizeof(struct level_info_t *);

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		memset(level_msg_ptr, 0x0, level_msg_size);
		memcpy(level_msg_ptr, chunk_ptr, (level_msg_size - lane_member * sizeof(struct level_info_t *)));
		
		offset += (level_msg_size - lane_member * sizeof(struct level_info_t *));

printf("list peer num %d",lane_member);
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
printf( "  pid = %d   ",new_peer->pid);
			map_pid_peer_info.insert(pair<unsigned long ,peer_info_t *>(new_peer->pid,new_peer));  

			new_peer ->manifest = ((struct chunk_rescue_list*)chunk_ptr) ->manifest;

		}
printf("\n");


//////////////////////////////////
		unsigned long subid_replyManifest = 0;
		for(int k=0;k<sub_stream_num;k++){
			if((((struct chunk_rescue_list*)chunk_ptr)->manifest)&(1<<k)){
				subid_replyManifest = k;
			}
		}
		_peer_ptr->substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(subid_replyManifest);
		if(_peer_ptr->substream_first_reply_peer_iter == _peer_ptr->substream_first_reply_peer.end()){
			_peer_ptr->substream_first_reply_peer[subid_replyManifest] = true;
		}
		else{
			_peer_ptr->substream_first_reply_peer_iter->second =true;
		}
	

//和lane 每個peer 先建立好連線	
//
		if(lane_member >= 1){
			_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);
			printf("rescue manifest : %d\n",((struct chunk_rescue_list*)chunk_ptr)->manifest);
			_peer_mgr_ptr->handle_test_delay( ((struct chunk_rescue_list*)chunk_ptr) ->manifest);
		}

		for (i = 0; i < lane_member; i++) {
			if(level_msg_ptr->level_info[i])
				delete level_msg_ptr->level_info[i];
			}
		
		if(level_msg_ptr)
			delete level_msg_ptr;

		
//cmd == CHNK_CMD_PEER_DATA			
	} else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_DATA) {
		//printf("%s, CHNK_CMD_PEER_DATA\n", __FUNCTION__);
		//printf("seq : %d , sub id : %d\n",chunk_ptr ->header.sequence_number,(chunk_ptr ->header.sequence_number) % sub_stream_num);
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
			if( pkDownInfoPtr ->peerInfo.manifest &  SubstreamIDToManifest(substreamID)){
				printf("send topology %d\n",pkDownInfoPtr ->peerInfo.manifest);
				send_parentToPK ( SubstreamIDToManifest(substreamID) , PK_PID+1 );
			}
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
	unsigned int seq_ready_to_send=0;
	unsigned long parentPid=-1;
	int downStreamSock=-1;
	int downStreamPid=-1;
	stream *strm_ptr=NULL;
	struct peer_info_t *peer = NULL;
	struct peer_connect_down_t *parentPeerPtr=NULL;
	unsigned long temp_sub_id=0;
	map<int, queue<struct chunk_t *> *>::iterator iter;		//fd_downstream
	map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	int leastCurrDiff=0;
	queue<struct chunk_t *> *queue_out_data_ptr;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

//	_log_ptr->write_log_format("s =>s u s u s u s u\n", __FUNCTION__,"stream ID=" ,chunk_ptr->header .stream_id,"recieve pkt seqnum", chunk_ptr->header.sequence_number,"bytes=" ,chunk_ptr ->header.length,"timestamp=",chunk_ptr->header.timestamp);

	//還沒註冊拿到substream num  不做任何偵測和運算
	if(sub_stream_num == 0)
		return;

//↓↓↓↓↓↓↓↓↓↓↓↓任何chunk 都會run↓↓↓↓↓↓↓↓↓↓↓↓↓



	temp_sub_id = (chunk_ptr ->header.sequence_number) % sub_stream_num;


	if(chunk_ptr->header.sequence_number > _least_sequence_number){
		_least_sequence_number = chunk_ptr->header.sequence_number;
	}
	if(_current_send_sequence_number == -1){
		_current_send_sequence_number = chunk_ptr->header.sequence_number;
	}
	 
	//get parentPid  and parentInfo of this chunk
	fd_pid_iter = _peer_ptr->map_fd_pid.find(sockfd);
	if(fd_pid_iter !=_peer_ptr->map_fd_pid.end()){
		parentPid = fd_pid_iter->second;						//get parentPid of this chunk
		pid_peerDown_info_iter =map_pid_peerDown_info.find(parentPid);
		if(pid_peerDown_info_iter != map_pid_peerDown_info.end()){
			parentPeerPtr = pid_peerDown_info_iter ->second;	//get parentInfo of this chunk
		}
	}

	//更新最後的seq 用來做time out
	parentPeerPtr->timeOutNewSeq =chunk_ptr ->header.sequence_number;


	if(parentPid !=PK_PID){
//	printf("PK rescue %d \n", (chunk_ptr ->header.sequence_number));
	}


	//如果這個 peer 來的chunk 裡的substream 從pk和這個peer都有來  (if rescue testing stream)
	if ( (SubstreamIDToManifest (temp_sub_id) & parentPeerPtr ->peerInfo.manifest)  &&  (SubstreamIDToManifest (temp_sub_id) & pkDownInfoPtr->peerInfo.manifest) && parentPid != PK_PID){

		(ssDetect_ptr + temp_sub_id) ->isTesting =1 ;	//ture
		printf("SSID = %d start testing stream\n",temp_sub_id);
		//這邊只是暫時改變PK的substream 實際上還是有串流下來
		pkDownInfoPtr->peerInfo.manifest &= (~SubstreamIDToManifest (temp_sub_id));  
		//開始testing 送topology
		send_parentToPK ( SubstreamIDToManifest (temp_sub_id) , (ssDetect_ptr + temp_sub_id)->previousParentPID ); 

		//testing function
		reSet_detectionInfo();
	}

	//如果這個substream正在測試中 且不是從PK來 ( 從peer 來)
	if((ssDetect_ptr + temp_sub_id) ->isTesting && parentPid != PK_PID){

		(ssDetect_ptr + temp_sub_id) ->testing_count++ ;

		//下面會濾掉慢到的封包 所以在此進入偵測
		rescue_detecion(chunk_ptr);

		//測試次數填滿兩次整個狀態  也就是測量了PARAMETER_M  次都沒問題 ( 其中有PARAMETER_M 次計算不會連續觸發)
		if(((ssDetect_ptr + temp_sub_id) ->testing_count / (stream_number * PARAMETER_M )  )  >= (PARAMETER_X *2) ){
			(ssDetect_ptr + temp_sub_id) ->isTesting =0 ;  //false
			(ssDetect_ptr + temp_sub_id) ->testing_count =0 ;
			//testing ok should cut this substream from pk
			pkDownInfoPtr->peerInfo.manifest  &=  ~SubstreamIDToManifest(temp_sub_id) ;
			send_rescueManifestToPKUpdate ( pkDownInfoPtr->peerInfo.manifest);
			printf("testing ok  cut pk substream= %d  manifest=%d sent new topology \n",temp_sub_id,pkDownInfoPtr->peerInfo.manifest);
			//選擇selected peer 送topology
			send_parentToPK(SubstreamIDToManifest (temp_sub_id) ,PK_PID +1 );

			//testing function
			reSet_detectionInfo();

		}
	}



//↑↑↑↑↑↑↑↑↑↑↑↑任何chunk 都會run↑↑↑↑↑↑↑↑↑↑↑↑



	//to ensure sequence number  higher than previous and overlay _chunk_ptr (會過濾一些重複和比原本小的封包)
	if(chunk_ptr->header.sequence_number > (*(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number) {
		memset((char *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)), 0x0, RTP_PKT_BUF_MAX);
		memcpy((char *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)), chunk_ptr, (sizeof(struct chunk_header_t) + chunk_ptr->header.length));
	} else if((*(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number == chunk_ptr->header.sequence_number){
		chunk_ptr->header.length = 0;
//printf("duplicate sequence number in the buffer  seq=%u\n",chunk_ptr->header.sequence_number);
		return;
	} else {
		chunk_ptr->header.length = 0;
		printf("sequence number smaller than the index in the buffer seq=%u\n",chunk_ptr->header.sequence_number);
		return;
	}
	


//↓↓↓↓↓↓↓↓↓↓↓↓以下只有先到的chunk才會run(會濾掉重複的和更小的)↓↓↓↓↓↓↓↓↓↓↓↓↓

	//如果這個sustream 正在測試中則從pk來的不進入測試
	if((ssDetect_ptr + temp_sub_id) ->isTesting && parentPid == PK_PID){
		// do nothing
	}else{
	//丟給rescue_detecion一定是有方向性的
		rescue_detecion(chunk_ptr);
	}



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





	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
	map<unsigned long, struct source_delay *>::iterator delay_table_iter;
	delay_table_iter = delay_table.find(temp_sub_id);
	if(delay_table_iter == delay_table.end()){
		printf("error : can not find source struct in table in send_capacity_to_pk\n");
		exit(1);
	}
	delay_table_iter->second->end_seq_num = chunk_ptr ->header.sequence_number;
	delay_table_iter->second->end_seq_abs_time = chunk_ptr ->header.timestamp;
	//printf("%d %d\n",delay_table_iter->second->first_pkt_recv,syn_table.init_flag);
	_log_ptr -> getTickTime(&(delay_table_iter->second->client_end_time));
	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
	//printf("%d %d\n",(delay_table+temp_sub_id)->start_delay_struct.init_flag,temp_sub_id);
	if((!(delay_table_iter->second->first_pkt_recv))&&(syn_table.init_flag == 2)){
		/*printf("start measure start delay\n");
		send_start_delay_measure_token(sockfd, temp_sub_id);*/
		//(delay_table+temp_sub_id)->start_seq_num = chunk_ptr ->header.sequence_number;
		//(delay_table+temp_sub_id)->start_seq_abs_time = chunk_ptr->header.timestamp;
		//(delay_table+temp_sub_id)->start_delay_struct.init_flag = 1;
		//source_delay_init(temp_sub_id);
		if(chunk_ptr ->header.sequence_number > syn_table.start_seq){
			peer_start_delay_count++;
			delay_table_iter->second->first_pkt_recv = 1;
			if(peer_start_delay_count == sub_stream_num){
				send_capacity_to_pk(_sock);
			}
		}
	}

	if(peer_start_delay_count == sub_stream_num){
//		source_delay_detection(sockfd,temp_sub_id,chunk_ptr ->header.sequence_number);
	}
	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL



		//差值在BUFF_SIZE 之外可能某個些seq都不到 ,跳過那些seq直到差值在BUFF_SIZE內
		leastCurrDiff = (int)(_least_sequence_number -_current_send_sequence_number);

		if( BUFF_SIZE  <= leastCurrDiff && leastCurrDiff <_bucket_size){

			for(; ;_current_send_sequence_number++ ){
					//代表有封包還沒到.略過
					if((*(_chunk_bitstream + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number){

printf("here1 leastCurrDiff =%d  _current= %d SSID =%d\n",leastCurrDiff ,_current_send_sequence_number,_current_send_sequence_number%sub_stream_num);

//					PAUSE
					continue;
					}else{
						leastCurrDiff =_least_sequence_number -_current_send_sequence_number;
//						printf("here2\n");
//					PAUSE
						if((leastCurrDiff < BUFF_SIZE) || _current_send_sequence_number < _least_sequence_number)
							break;
					}
			}
		printf("here3 least CurrDiff =%d\n",leastCurrDiff);

		//可能某個subtream 追過_bucket_size,直接跳到最後一個 (應該不會發生)
		}else if (leastCurrDiff > _bucket_size) {

			PAUSE
			printf("i think not go here leastCurrDiff =%d\n",leastCurrDiff);
			_current_send_sequence_number = _least_sequence_number;  

		//在正常範圍內 正常傳輸丟給player 留給下面處理
		}else{

		}

		
		//正常傳輸丟給player 在這個for 底下給的seq下的一定是有方向性的
		for(;_current_send_sequence_number <= _least_sequence_number;){

			//_current_send_sequence_number 指向的地方還沒到 ,不做處理等待並return
			if((*(_chunk_bitstream + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number){
//				printf("wait packet,seq= %d  SSID =%d\n",_current_send_sequence_number,_current_send_sequence_number%sub_stream_num);
//				_current_send_sequence_number = seq_ready_to_send;
//	_log_ptr->write_log_format("s u s u s u s u\n","seq_ready_to_send",seq_ready_to_send,"_least_sequence_number",_least_sequence_number);
				return ;

			//為連續,丟給player
			}else if((*(_chunk_bitstream + (_current_send_sequence_number % _bucket_size))).header.stream == STRM_TYPE_MEDIA) {

				//以下為丟給player
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


}

//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
void pk_mgr::syn_recv_handler(struct syn_token_receive* syn_struct_back_token){
	if(syn_table.init_flag==0){
		printf("error in syn_recv_handler syn not send\n");
		exit(1);
	}
	else if(syn_table.init_flag==1){
		syn_round_time = 0;
		_log_ptr -> getTickTime(&(syn_table.end_clock));
		syn_round_time = _log_ptr ->diffTime_us(syn_table.start_clock,syn_table.end_clock);
		syn_table.client_abs_start_time = (long long)syn_struct_back_token->pk_time - (long long)(syn_round_time/2);
		if(syn_table.client_abs_start_time < 0){
			printf("warning syn error\n");
			printf("client_abs_start_time : %lld\n",(long long)syn_table.client_abs_start_time);
			printf("pk_time : %lld\n",(long long)syn_struct_back_token->pk_time);
			printf("(syn_round_time/2) : %lld\n",(long long)(syn_round_time/2));
			printf("syn_round_time : %lld\n",(long long)syn_round_time);
			exit(1);
		}
		else{
			printf("client_abs_start_time : %lld\n",(long long)syn_table.client_abs_start_time);
			printf("pk_time : %lld\n",(long long)syn_struct_back_token->pk_time);
			printf("(syn_round_time/2) : %lld\n",(long long)(syn_round_time/2));
			printf("syn_round_time : %lld\n",(long long)syn_round_time);
		}
		syn_table.init_flag = 2;
		syn_table.start_seq = syn_struct_back_token->seq_now;
	}
}

void pk_mgr::send_syn_token_to_pk(int pk_sock){

	struct syn_token_send *syn_token_send_ptr = NULL;
	/*map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;

	map<int , unsigned long>::iterator temp_map_fd_pid_iter;*/
	
	syn_token_send_ptr = (struct syn_token_send *)new unsigned char[sizeof(struct syn_token_send)];
	
	memset(syn_token_send_ptr, 0x0, sizeof(struct syn_token_send));
	
	syn_token_send_ptr->header.cmd = CHNK_CMD_PEER_SYN;
	syn_token_send_ptr->header.rsv_1 = REQUEST;
	syn_token_send_ptr->header.length = sizeof(struct syn_token_send) - sizeof(struct chunk_header_t);

	printf("starting send start delay token to pk ...\n");
	int send_byte = 0;
	int expect_len = syn_token_send_ptr->header.length + sizeof(struct chunk_header_t);
	char send_buf[sizeof(struct syn_token_send)];
	_net_ptr->set_blocking(pk_sock);	// set to blocking

	memset(send_buf, 0x0, sizeof(struct syn_token_send));
	memcpy(send_buf, syn_token_send_ptr, expect_len);
	
	_log_ptr -> getTickTime(&(syn_table.start_clock));
	send_byte = _net_ptr->send(pk_sock, send_buf, expect_len, 0);
	if( send_byte <= 0 ) {
		data_close(pk_sock, "send pkt error");
		_log_ptr->exit(0, "send pkt error");
	} else {
		if(syn_token_send_ptr)
			delete syn_token_send_ptr;
		_net_ptr->set_nonblocking(pk_sock);	// set to non-blocking
	}
}

void pk_mgr::syn_table_init(int pk_sock){
	syn_table.client_abs_start_time = -1;
	syn_table.init_flag = 1;
	syn_table.start_seq = 0;
	send_syn_token_to_pk(pk_sock);
}
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

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
//		printf("substreamID=%d  measure_N =%d \n",substreamID,(ssDetect_ptr + substreamID) ->measure_N);
		
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

	}else if (  ((ssDetect_ptr + substreamID) ->last_seq ) == (chunk_ptr->header.sequence_number)){
	//doing nothing
	}else{
			PAUSE
			printf("why here old packet here??\n");
	}

	return ;
}



//做每個peer substream的加總 且判斷需不需要救
void pk_mgr::measure()
{	
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;



	//for each peer
	for(pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter != map_pid_peerDown_info.end(); pid_peerDown_info_iter++) {

//////////////////
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
//	static bool triggerContinue=true;
//	static unsigned int lastTriggerCount=0;

	unsigned long testingSubStreamID=-1 ;
	unsigned long testingManifest=0 ;

	memset( statsArryCount_ptr , 0x00 ,sizeof(int) * (sub_stream_num+1)  ); 

////////////////////
		tempManifest = (pid_peerDown_info_iter->second)-> peerInfo.manifest;
		if(tempManifest ==0){
			continue;
		}
		connectPeerInfo =pid_peerDown_info_iter->second ;
		
		//先取得perPeerSS_num ,和peerHighestSSID
		for(int i=0 ;i< sub_stream_num ;i++ ){
			//i=substreamID
			if(  ((tempManifest)  & 1<< i)  &&  (i >= peerHighestSSID)  ){
			perPeerSS_num++;
			peerHighestSSID =i;
			}
		}

		//計算出totalSourceBitrate ,totalLocalBitrate
		for(int i=0 ;i< sub_stream_num ;i++ ){
			//i=substreamID 
			if(  ((tempManifest) & 1<<i)  ){

//printf("( i=%d) ->measure_N =%d ( peerHighestSSID=%d)->measure_N=%d\n",i,(ssDetect_ptr + i) ->measure_N  ,peerHighestSSID,(ssDetect_ptr + peerHighestSSID)->measure_N);
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

//printf("totalSourceBitrate=%.5f   totalLocalBitrate=%.5f\n",totalSourceBitrate,totalLocalBitrate);

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

//printf("****************   PID=%d   *******************\n",connectPeerInfo ->peerInfo.pid);

		//根據rescueStatsArry 來決定要不要觸發rescue
		for(int i=0 ; i<PARAMETER_M ;i++){

			//做統計
			( *(statsArryCount_ptr + connectPeerInfo ->rescueStatsArry[i]) )++ ;

			if(connectPeerInfo ->rescueStatsArry[i] >0){
			count_N++;
			}


			
//printf("%d  ",connectPeerInfo ->rescueStatsArry[i]);
		}
//printf("\n");

			//近PARAMETER_P次 發生 P次
			for(int j=0 ; j<PARAMETER_P ;j++){
				if ( connectPeerInfo ->rescueStatsArry[ (PARAMETER_M +( (ssDetect_ptr + peerHighestSSID)->measure_N -1)-j )% PARAMETER_M ] >0 ){
					continuous_P++ ;
				}
			}


		for(int k=0 ; k<( (ssDetect_ptr + peerHighestSSID)->measure_N -1)% PARAMETER_M ;k++)
//printf("   ");
//printf("↑\n");

		//找出統計最多的值
		for(int k=0 ;k< (sub_stream_num+1) ;k++){
//		printf("substream%d = %d   \n",k,*(statsArryCount_ptr+ k) );

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


			if(connectPeerInfo ->lastTriggerCount  == 0){

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

					reSet_detectionInfo();

					printf("stream testing fail \n");

				//PID 是其他peer
				}else{
				afterManifest = manifestFactory (connectPeerInfo ->peerInfo.manifest , rescueSS);
				pkDownInfoPtr ->peerInfo.manifest |=(connectPeerInfo ->peerInfo.manifest &(~afterManifest) );
				send_rescueManifestToPK(pkDownInfoPtr ->peerInfo.manifest );
				printf("rescue manifest %d after manifest %d  PK=%d \n",connectPeerInfo ->peerInfo.manifest,afterManifest,pkDownInfoPtr ->peerInfo.manifest );
				connectPeerInfo ->peerInfo.manifest = afterManifest ;
				_peer_mgr_ptr->send_manifest_to_parent(afterManifest ,connectPeerInfo ->peerInfo.pid);

				tempManifest =pkDownInfoPtr ->peerInfo.manifest ;

				//把先前的連接的PID存起來
				while(tempManifest){
					testingSubStreamID = manifestToSubstreamID (tempManifest);
					(ssDetect_ptr + testingSubStreamID) ->previousParentPID = connectPeerInfo ->peerInfo.pid;
					tempManifest &=  (~SubstreamIDToManifest(testingSubStreamID) );
				}


				reSet_detectionInfo();
				}


				connectPeerInfo ->lastTriggerCount = 1 ;

			//PARAMETER_M 觸發區間最少是再經過PARAMETER_M次 測量
			}else{
				connectPeerInfo->lastTriggerCount ++ ;
				if (connectPeerInfo ->lastTriggerCount   >= PARAMETER_M)
					connectPeerInfo ->lastTriggerCount  = 0 ;

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
	chunk_rescueManifestPtr->rescue_seq_start =_current_send_sequence_number -(sub_stream_num*5) ;


	_net_ptr->set_blocking(_sock);
	
	_net_ptr ->send(_sock , (char*)chunk_rescueManifestPtr ,sizeof(struct rescue_pkt_from_server),0) ;

	_net_ptr->set_nonblocking(_sock);

	printf("sent rescue to PK manifest = %d  start from %d\n",manifestValue,_current_send_sequence_number -(sub_stream_num*5) );

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
	
	_net_ptr ->send(_sock , (char*)chunk_rescueManifestPtr ,sizeof(struct rescue_update_from_server),0) ;

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
		printf("SSID =%d my old parent = %d  ",manifestToSubstreamID(manifestValue),oldPID);
		i ++ ;
	} 


	for(pid_peerDown_info_iter =map_pid_peerDown_info.begin() ;pid_peerDown_info_iter != map_pid_peerDown_info.end() ;pid_peerDown_info_iter++ ){
		//這個parent 有傳給自己
		if(pid_peerDown_info_iter ->second ->peerInfo.manifest & manifestValue ){
			 
			parentListPtr ->parent_pid [i] = pid_peerDown_info_iter ->first ;
			printf("SSID =%d  my new parent = %d  \n",manifestToSubstreamID(manifestValue),pid_peerDown_info_iter ->first );
			i++ ;

		}
	}

	_net_ptr->set_blocking(_sock);
	
	_net_ptr ->send(_sock , (char*)parentListPtr ,packetlen ,0) ;

	_net_ptr->set_nonblocking(_sock);

	delete parentListPtr;
	//printf("send_parentToPK end\n");
	return ;




}



void pk_mgr::reSet_detectionInfo()
{

	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
//	memset( ssDetect_ptr , 0x00 ,sizeof(struct detectionInfo) * sub_stream_num ); 

	for(int i =0 ;i < sub_stream_num;i++){
		(ssDetect_ptr+i) ->count_X=0 ;
		(ssDetect_ptr+i) ->first_timestamp =0 ;
		(ssDetect_ptr+i) ->last_localBitrate =0 ;
		(ssDetect_ptr+i) ->last_seq=0 ;
		(ssDetect_ptr+i) ->last_sourceBitrate =0 ;
		(ssDetect_ptr+i) ->last_timestamp =0;
		(ssDetect_ptr+i) ->measure_N =0 ;
		(ssDetect_ptr+i) ->total_buffer_delay =0;
		(ssDetect_ptr+i) ->total_byte = 0 ;
	}


	for(pid_peerDown_info_iter = map_pid_peerDown_info.begin() ; pid_peerDown_info_iter!= map_pid_peerDown_info.end();pid_peerDown_info_iter++){
		
		for(int i = 0 ;i<PARAMETER_M ;i++)
		pid_peerDown_info_iter ->second ->rescueStatsArry [i] = 0 ;
	
	}


}