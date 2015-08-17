#include "logger_client.h"
#include "common.h"
#include "logger.h"

logger_client::logger_client(logger *log_ptr)
{
	self_pid = 0;
	my_public_ip = 0;
	my_private_port = 0;
	previous_time_differ = 0;
	buffer_clear_flag = 0;
	start_delay = 0;
	sub_stream_number = 0;
	accumulated_packet_size_in = 0;
	accumulated_packet_size_out = 0;
	log_bw_in_init_flag = 0;
	log_bw_out_init_flag = 0;
	log_source_delay_init_flag = 0;
	nat_total_times = 0;
	nat_success_times = 0;
	self_channel_id = 0;
	chunk_buffer =NULL;
	max_source_delay =NULL;
	delay_list=NULL;
	_log_ptr =log_ptr;
	exit_code = PEER_ALIVE;
	bitrate = 0;
	
	chunk_buffer = (struct chunk_t *)new unsigned char[(CHUNK_BUFFER_SIZE + sizeof(struct chunk_header_t))];
	if (!chunk_buffer) {
		handle_error(MALLOC_ERROR, "[ERROR] chunk_buffer loggerClien new error", __FUNCTION__, __LINE__);
	}
	quality_struct_ptr =NULL ;
	buffer_size = 0;

	quality_struct_ptr = (struct quality_struct*)(new struct quality_struct);
	if (!quality_struct_ptr) {
		handle_error(MALLOC_ERROR, "[ERROR] chunk_buffer loggerClien new error", __FUNCTION__, __LINE__);
	}
	memset(quality_struct_ptr,0x00,sizeof(struct quality_struct));
	memset(chunk_buffer,0x00,(CHUNK_BUFFER_SIZE + sizeof(struct chunk_header_t)));
	memset(&non_log_recv_struct,0x00,sizeof(Nonblocking_Buff));

	//memset(&start_out_bw_record,0x00,sizeof(start_out_bw_record));
	//memset(&end_out_bw_record,0x00,sizeof(end_out_bw_record));
	//memset(&start_in_bw_record,0x00,sizeof(struct log_in_bw_struct));
	//memset(&end_in_bw_record,0x00,sizeof(struct log_in_bw_struct));
	memset(&log_period_bw_start,0x00,sizeof(log_period_bw_start));
	memset(&log_period_source_delay_start,0x00,sizeof(log_period_source_delay_start));
	memset(&message_bw_analysis, 0, sizeof(message_bw_analysis));
	log_timer_init();
}

logger_client::~logger_client()
{
	if (chunk_buffer) {
		delete chunk_buffer;
	}
	if (max_source_delay) {
		delete max_source_delay;
	}
	if (delay_list) {
		delete delay_list;
	}
	if (quality_struct_ptr) {
		delete quality_struct_ptr;
	}
	while (1) {
		if (log_buffer.size() != 0) {
			delete log_buffer.front();
			log_buffer.pop();
		}
		else {
			break;
		}
	}
	debug_printf("Have deleted logger_client \n");
}

void logger_client::set_self_pid_channel(unsigned long pid,unsigned long channel_id)
{
	self_pid = pid;
	self_channel_id = channel_id;
	log_to_server(LOG_BEGINE, 15);
	log_clear_buffer();
}

void logger_client::set_self_ip_port(unsigned long public_ip, unsigned short public_port, unsigned long private_ip, unsigned short private_port)
{
	my_public_ip = public_ip;
	my_public_port = public_port;
	my_private_ip = private_ip;
	my_private_port = private_port;
}

void logger_client::set_net_obj(network *net_ptr)
{
	_net_ptr = net_ptr;
}

void logger_client::set_pk_mgr_obj(pk_mgr *pk_mgr_ptr)
{
	_pk_mgr_ptr = pk_mgr_ptr;
}

void logger_client::set_prep_obj(configuration *prep)
{
	_prep = prep;
}

void logger_client::add_nat_total_times()
{
	nat_total_times++;
}

void logger_client::add_nat_success_times()
{
	nat_success_times++;
}

void logger_client::log_init()
{
	int retVal;
	string log_ip("");
	string log_port("");
	struct sockaddr_in log_saddr;
	
	_prep->read_key("log_ip", log_ip);
	_prep->read_key("log_port", log_port);
	buffer_size = 0;
	
	if ((log_server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
		debug_printf("[ERROR] Create log socket failed %d %d \n", log_server_sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create log socket failed", log_server_sock, socketErr);
		handle_error(LOG_BUILD_ERROR, "[ERROR] Create log socket failed", __FUNCTION__, __LINE__);
		return ;
#endif
	}

	memset((struct sockaddr_in*)&log_saddr, 0, sizeof(struct sockaddr_in));
	log_saddr.sin_addr.s_addr = inet_addr(log_ip.c_str());
	log_saddr.sin_port = htons((unsigned short)atoi(log_port.c_str()));
	log_saddr.sin_family = AF_INET;

	if ((retVal = connect(log_server_sock, (struct sockaddr*)&log_saddr, sizeof(log_saddr))) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
		if (socketErr == WSAEWOULDBLOCK) {
		
		}
		else {
			exit_code = LOG_BUILD_ERROR;
			debug_printf("[ERROR] Building Log-server connection failed %d %d \n", retVal, socketErr);
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Building Log-server connection failed", retVal, socketErr);
			handle_error(LOG_BUILD_ERROR, "[ERROR] Building Log-server connection failed", __FUNCTION__, __LINE__);
			return ;
		}
#else
#endif
	}
	
	debug_printf("Succeed to build connection with log-server \n");
	
	_net_ptr->set_nonblocking(log_server_sock);		// set to non-blocking
	_net_ptr->epoll_control(log_server_sock, EPOLL_CTL_ADD, EPOLLOUT);
	_net_ptr->set_fd_bcptr_map(log_server_sock, dynamic_cast<basic_class *> (this));
	_pk_mgr_ptr->fd_list_ptr->push_back(log_server_sock);
	_net_ptr->log_fd = log_server_sock;
}

/*
#ifdef WIN32
void logger_client::logger_client_getTickTime(LARGE_INTEGER *tickTime)
{
	QueryPerformanceCounter(tickTime);
}
#endif


#ifdef WIN32
unsigned int logger_client::logger_client_diffTime_ms(LARGE_INTEGER startTime,LARGE_INTEGER endTime)
{
    LARGE_INTEGER CUPfreq;
    LONGLONG llLastTime;
    QueryPerformanceFrequency(&CUPfreq);
    llLastTime = (unsigned int)  (1000 * (endTime.QuadPart - startTime.QuadPart) / CUPfreq.QuadPart);

    return llLastTime;
}
#endif

*/

void logger_client::log_timer_init(){
	_log_ptr->timerGet(&log_start_time);
}

void logger_client::log_time_differ(){
	_log_ptr->timerGet(&log_record_time);
	log_time_dffer = _log_ptr->diff_TimerGet_ms(&log_start_time,&log_record_time);
}

void logger_client::count_start_delay()
{
	_log_ptr->timerGet(&log_record_time);
	start_delay = (double)_log_ptr->diff_TimerGet_ms(&log_start_time, &log_record_time);
	_log_ptr->write_log_format("s(u) s f \n", __FUNCTION__, __LINE__, "start_delay =", start_delay);
	
	log_to_server(LOG_START_DELAY, 0, start_delay);
	log_to_server(LOG_DATA_START_DELAY, 0, start_delay);
}
	
void logger_client::source_delay_struct_init(unsigned long sub_stream_num)
{
	log_source_delay_init_flag = 1;
	sub_stream_number = sub_stream_num;
	max_source_delay = (struct log_source_delay_struct *)new unsigned char[(sub_stream_num*sizeof(struct log_source_delay_struct))]; 
	if (!max_source_delay) {
		handle_error(MALLOC_ERROR, "[ERROR] max_source_delay loggerClien new error", __FUNCTION__, __LINE__);
	}
	memset(max_source_delay, 0, sub_stream_num * sizeof(struct log_source_delay_struct));
	delay_list = (double *)new unsigned char[(sub_stream_num*sizeof(double))];
	if (!delay_list) {
		handle_error(MALLOC_ERROR, "[ERROR] delay_list loggerClien new error ", __FUNCTION__, __LINE__);
	}
	memset(delay_list, 0, sub_stream_num * sizeof(double));

	_log_ptr->timerGet(&log_period_source_delay_start);
}

void logger_client::set_source_delay(unsigned long substream_id, unsigned long source_delay)
{
	max_source_delay[substream_id].count++;
	max_source_delay[substream_id].delay_now = (double)source_delay;
	max_source_delay[substream_id].accumulated_delay += (double)source_delay;
	//if (max_source_delay[substream_id].count % 20 == 0) {
	//	debug_printf("%d  %d \n", static_cast<int>(max_source_delay[substream_id].accumulated_delay), static_cast<int>(max_source_delay[substream_id].delay_now));
	//}
}

// Send source delay to PK
void logger_client::send_max_source_delay()
{
	double max_delay = 0;
	
	for (unsigned long i = 0; i < sub_stream_number; i++) {
		if (max_source_delay[i].count > 0) {
			max_source_delay[i].average_delay = max_source_delay[i].accumulated_delay / (double)(max_source_delay[i].count);
			delay_list[i] = max_source_delay[i].average_delay;
			//debug_printf("substream %d count = %d average_delay = %d accumulated_delay = %d \n", i, 
			//																					static_cast<int>(delay_list[i]), 
			//																					max_source_delay[i].count, 
			//																					max_source_delay[i].accumulated_delay);
		}
		// If not receive any packet for a while, we still need to calculate the source delay
		else {
			delay_list[i] += LOG_DELAY_SEND_PERIOD;
			_log_ptr->write_log_format("s(u) s d s d s \n", __FUNCTION__, __LINE__, "substream id", i, "has received nothing for", LOG_DELAY_SEND_PERIOD, "milliseconds");
		}
		
		max_delay = max_delay < delay_list[i] ? delay_list[i] : max_delay;
	}
	
	//cout << "max_delay = " << max_delay << endl;
	for (unsigned long i = 0; i < sub_stream_number; i++) {
		//debug_printf("substream %d source delay = %d, count %d \n", i, static_cast<int>(delay_list[i]), max_source_delay[i].count);
		_log_ptr->write_log_format("s(u) s d s f s f s d \n", __FUNCTION__, __LINE__,
																"substream", i,
																"source delay =", delay_list[i],
																"average_delay =", max_source_delay[i].average_delay,
																"count =", max_source_delay[i].count);
	}
	//debug_printf("Maximum source delay = %f \n", max_delay);
	
	log_to_server(LOG_PERIOD_SOURCE_DELAY, 0, max_delay, sub_stream_number, delay_list);
	log_to_server(LOG_DATA_SOURCE_DELAY, 0, max_delay, sub_stream_number, delay_list);
	_log_ptr->write_log_format("s(u) s f s \n", __FUNCTION__, __LINE__, "send max source delay =", max_delay, "to PK");
	
	for (unsigned long i = 0; i < sub_stream_number; i++) {
		max_source_delay[i].accumulated_delay = 0;
		max_source_delay[i].count = 0;
		max_source_delay[i].average_delay = 0;
	}
}

void logger_client::bw_in_struct_init(unsigned long timestamp,unsigned long pkt_size){
	accumulated_packet_size_in = 0;
	pre_in_pkt_size = 0;

	//start_in_bw_record.time_stamp = timestamp;
	//_log_ptr->timerGet(&(start_in_bw_record.client_time));
	_log_ptr->timerGet(&log_period_bw_start);
	accumulated_packet_size_in += pkt_size;
	pre_in_pkt_size = pkt_size;
}

void logger_client::bw_out_struct_init(unsigned long pkt_size){
	pre_out_pkt_size = 0;
	accumulated_packet_size_out = 0;

	//_log_ptr->timerGet(&start_out_bw_record);
	accumulated_packet_size_out += pkt_size;
	pre_out_pkt_size = pkt_size;
}

void logger_client::set_out_bw(unsigned long pkt_size)
{
	//_log_ptr->timerGet(&end_out_bw_record);
	accumulated_packet_size_out += pkt_size*8;
	pre_out_pkt_size = pkt_size;
}

void logger_client::set_out_message_bw(UINT32 pkt_header_size, UINT32 pkt_payload_size, INT32 pkt_type)
{
	switch (pkt_type) {
	case PKT_CONTROL:
		message_bw_analysis.out_control_bw.header += pkt_header_size;
		message_bw_analysis.out_control_bw.payload += pkt_payload_size;
		break;
	case PKT_DATA:
		message_bw_analysis.out_data_bw.header += pkt_header_size;
		message_bw_analysis.out_data_bw.payload += pkt_payload_size;
		break;
	case PKT_LOGGER:
		message_bw_analysis.out_logger_bw.header += pkt_header_size;
		message_bw_analysis.out_logger_bw.payload += pkt_payload_size;
		break;
	}
}

void logger_client::set_in_bw(unsigned long timestamp, unsigned long pkt_size)
{
	//end_in_bw_record.time_stamp = timestamp;
	//_log_ptr->timerGet(&(end_in_bw_record.client_time));
	accumulated_packet_size_in += pkt_size*8;
	pre_in_pkt_size = pkt_size;
}

void logger_client::set_in_message_bw(UINT32 pkt_header_size, UINT32 pkt_payload_size, INT32 pkt_type)
{
	switch (pkt_type) {
	case PKT_CONTROL:
		message_bw_analysis.in_control_bw.header += pkt_header_size;
		message_bw_analysis.in_control_bw.payload += pkt_payload_size;
		break;
	case PKT_DATA:
		message_bw_analysis.in_data_bw.header += pkt_header_size;
		message_bw_analysis.in_data_bw.payload += pkt_payload_size;
		break;
	case PKT_RE_DATA:
		message_bw_analysis.in_redundant_data_bw.header += pkt_header_size;
		message_bw_analysis.in_redundant_data_bw.payload += pkt_payload_size;
		break;
	}
}

void logger_client::send_bw()
{
	double should_in_bw = 0;
	double real_in_bw = 0;
	int period_msec = 0;
	double real_out_bw = 0;
	double quality_result = 0.0;
	double nat_success_ratio = 0.0;

	struct timerStruct new_timer;
	_log_ptr->timerGet(&new_timer);

	// Calculate bandwidth-in
	period_msec = _log_ptr->diff_TimerGet_ms(&log_period_bw_start, &new_timer);
	if (period_msec < 1) {
		period_msec = 1;
	}
	real_in_bw = 1000 * (double)accumulated_packet_size_in / (double)period_msec;
	accumulated_packet_size_in = 0;
	bitrate = 0.5*bitrate + 0.5*static_cast<int>(real_in_bw);

	// Calculate bandwidth-out
	if (log_bw_out_init_flag == 1) {
		real_out_bw = 1000 * (double)accumulated_packet_size_out / (double)period_msec;
		accumulated_packet_size_out = 0;
	}
	
	
	// Calculate delay-quality. delay-quality = 0.5*(1-packet_loss_rate) + 0.5*(delay_quality_average)
	if (quality_struct_ptr->total_chunk != 0) {
		quality_struct_ptr->average_quality = quality_struct_ptr->accumulated_quality / (double)quality_struct_ptr ->total_chunk;
		double delay_quality_average = quality_struct_ptr->average_quality;
		double packet_loss_rate =  (double)quality_struct_ptr->lost_pkt / (double)quality_struct_ptr->total_chunk;
		
		quality_result = 0.5*(1-packet_loss_rate) + 0.5*delay_quality_average;
	}
	else {
		quality_result = 0;
	}
	
	// Calculate NAT success ratio
	if (nat_total_times != 0) {
		nat_success_ratio = nat_success_times / nat_total_times;
	}
	else {
		nat_success_ratio = -1;
	}

	//debug_printf("real_in_bw = %.2lf, real_out_bw = %.2lf, quality_result = %.2lf, total_chunk = %u, accumulated_quality = %.2lf, average_quality = %.2lf, lost_pkt = %u \n", real_in_bw,
	//																																		real_out_bw,
	//																																		quality_result,
	//																																		quality_struct_ptr->total_chunk,
	//																																		quality_struct_ptr->accumulated_quality,
	//																																		quality_struct_ptr->average_quality,
	//																																		quality_struct_ptr->lost_pkt);
	log_to_server(LOG_WRITE_STRING, 0, "s u s u d d d d d d \n", "my_pid", _pk_mgr_ptr->my_pid, "[DATA]", 
		quality_struct_ptr->lost_pkt, 
		quality_struct_ptr->redundatnt_pkt, 
		quality_struct_ptr->expired_pkt, 
		quality_struct_ptr->total_chunk, 
		_pk_mgr_ptr->pkt_count, 
		_pk_mgr_ptr->pkt_rate, 
		_pk_mgr_ptr->Xcount);
	log_to_server(LOG_WRITE_STRING, 0, "s u s d d d d d d d d d d d d d \n", "my_pid", _pk_mgr_ptr->my_pid, "[DATA_BW]", 
		message_bw_analysis.in_control_bw.header, 
		message_bw_analysis.in_control_bw.payload, 
		message_bw_analysis.in_data_bw.header, 
		message_bw_analysis.in_data_bw.payload, 
		message_bw_analysis.in_redundant_data_bw.header, 
		message_bw_analysis.in_redundant_data_bw.payload, 
		message_bw_analysis.out_control_bw.header, 
		message_bw_analysis.out_control_bw.payload,
		message_bw_analysis.out_data_bw.header, 
		message_bw_analysis.out_data_bw.payload,
		message_bw_analysis.out_logger_bw.header, 
		message_bw_analysis.out_logger_bw.payload, 
		period_msec);

	quality_struct_ptr->lost_pkt = 0;
	quality_struct_ptr->redundatnt_pkt = 0;
	quality_struct_ptr->expired_pkt = 0;
	quality_struct_ptr->accumulated_quality = 0.0;
	quality_struct_ptr->total_chunk = 0;

	//log_to_server(LOG_CLIENT_BW, 0, should_in_bw, real_in_bw,real_out_bw, quality_result);
	log_to_server(LOG_DATA_BANDWIDTH, 0, should_in_bw, real_in_bw,real_out_bw, quality_result, nat_success_ratio);
	debug_printf("in_bw: %.2lf Mb/s  out_bw: %.2lf Mb/s \n", real_in_bw/1000000, real_out_bw/1000000);
	_log_ptr->write_log_format("s(u) s f s f \n", __FUNCTION__, __LINE__, "in_bw", real_in_bw/1000000, "out_bw", real_out_bw/1000000);

	_log_ptr->write_log_format("s(u) d d d d d d d d d d d d d \n", __FUNCTION__, __LINE__, message_bw_analysis.in_control_bw.header, message_bw_analysis.in_control_bw.payload, message_bw_analysis.in_data_bw.header, message_bw_analysis.in_data_bw.payload, message_bw_analysis.in_redundant_data_bw.header, message_bw_analysis.in_redundant_data_bw.payload, message_bw_analysis.out_control_bw.header, message_bw_analysis.out_control_bw.payload, message_bw_analysis.out_data_bw.header, message_bw_analysis.out_data_bw.payload, message_bw_analysis.out_logger_bw.header, message_bw_analysis.out_logger_bw.payload, period_msec);
	memset(&message_bw_analysis, 0, sizeof(message_bw_analysis));

	if (real_in_bw != 0) {
		debug_printf("Capacity: %.1lf, RescueNum: %d \n", real_out_bw / real_in_bw, _pk_mgr_ptr->rescueNumAccumulate());
	}
	debug_printf("quality: %.4lf \n", quality_result);
	debug_printf("nat_success_ratio: %.3lf \n", nat_success_ratio);

	//start_in_bw_record.client_time = end_in_bw_record.client_time;
	//start_out_bw_record = end_out_bw_record;
	
}

void logger_client::send_topology(unsigned long* parents, int sub_stream_number)
{
	double max_delay = 0;

	for (int i = 0; i < sub_stream_number; i++) {
		parent_list[i] = parents[i];
	}

	//cout << "max_delay = " << max_delay << endl;
	debug_printf("Parent list: ");
	for (int i = 0; i < sub_stream_number; i++) {
		debug_printf("%u  ", static_cast<int>(parent_list[i]));
	}
	debug_printf("\n");

	log_to_server(LOG_DATA_TOPOLOGY, 0, sub_stream_number, parent_list);
}

void logger_client::log_to_server(int log_mode, ...){
	va_list ap;
	int d;
	unsigned int u;
	char *s;
	unsigned long long int llu;

	log_time_differ();
	va_start(ap,log_mode);
	if (log_mode == LOG_DATA_PEER_INFO) {
		unsigned long manifest = va_arg(ap, unsigned long);
		
		struct log_data_peer_info *log_data_peer_info_ptr = new struct log_data_peer_info;
		if (!log_data_peer_info_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_data_peer_info_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_data_peer_info_ptr, 0, sizeof(struct log_data_peer_info));
	
		
		log_data_peer_info_ptr->log_header.cmd = LOG_DATA_PEER_INFO;
		log_data_peer_info_ptr->log_header.log_time = log_time_dffer;
		log_data_peer_info_ptr->log_header.pid = self_pid;
		log_data_peer_info_ptr->log_header.manifest = manifest;
		log_data_peer_info_ptr->log_header.channel_id = self_channel_id;
		log_data_peer_info_ptr->log_header.length = sizeof(struct log_data_peer_info) - sizeof(struct log_header_t);
		log_data_peer_info_ptr->public_ip = my_public_ip;
		log_data_peer_info_ptr->public_port = my_public_port;
		log_data_peer_info_ptr->private_ip = my_private_ip;
		log_data_peer_info_ptr->private_port = my_private_port;
		
		_log_ptr->write_log_format("s(u) s:d  s:d \n", __FUNCTION__, __LINE__, 
														inet_ntoa(*(struct in_addr *)&my_public_ip), my_public_port, 
														inet_ntoa(*(struct in_addr *)&my_private_ip), my_private_port);
		
		log_buffer.push((struct log_pkt_format_struct *)log_data_peer_info_ptr);
		buffer_size += sizeof(struct log_data_peer_info);
	} 
	else if (log_mode == LOG_DATA_START_DELAY) {
		unsigned long manifest = va_arg(ap, unsigned long);
		double	start_delay = va_arg(ap, double);
		struct log_data_start_delay *log_data_start_delay_ptr = new struct log_data_start_delay;
		if (!log_data_start_delay_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_data_start_delay_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_data_start_delay_ptr, 0, sizeof(struct log_data_start_delay));

		log_data_start_delay_ptr->log_header.cmd = LOG_DATA_START_DELAY;
		log_data_start_delay_ptr->log_header.log_time = log_time_dffer;
		log_data_start_delay_ptr->log_header.pid = self_pid;
		log_data_start_delay_ptr->log_header.manifest = manifest;
		log_data_start_delay_ptr->log_header.channel_id = self_channel_id;
		log_data_start_delay_ptr->log_header.length = sizeof(struct log_data_start_delay) - sizeof(struct log_header_t);
		log_data_start_delay_ptr->start_delay = start_delay;

		log_buffer.push((struct log_pkt_format_struct *)log_data_start_delay_ptr);
		buffer_size += sizeof(struct log_data_start_delay);
	}
	else if (log_mode == LOG_DATA_BANDWIDTH) {
		unsigned long manifest = va_arg(ap, unsigned long);
		double temp_should_in_bw = va_arg(ap, double);
		double temp_real_in_bw = va_arg(ap, double);
		double temp_real_out_bw = va_arg(ap, double);
		double temp_quality = va_arg(ap, double);
		double temp_nat_success_ratio = va_arg(ap, double);

		struct log_data_bw *log_data_bw_ptr = new struct log_data_bw;
		if (!log_data_bw_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_data_bw_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_data_bw_ptr, 0, sizeof(struct log_data_bw));

		log_data_bw_ptr->log_header.cmd = LOG_DATA_BANDWIDTH;
		log_data_bw_ptr->log_header.log_time = log_time_dffer;
		log_data_bw_ptr->log_header.pid = self_pid;
		log_data_bw_ptr->log_header.manifest = manifest;
		log_data_bw_ptr->log_header.channel_id = self_channel_id;
		log_data_bw_ptr->log_header.length = sizeof(struct log_data_bw) - sizeof(struct log_header_t);
		log_data_bw_ptr->should_in_bw = temp_should_in_bw;
		log_data_bw_ptr->real_in_bw = temp_real_in_bw;
		log_data_bw_ptr->real_out_bw = temp_real_out_bw;
		log_data_bw_ptr->quality = temp_quality;
		log_data_bw_ptr->nat_success_ratio = temp_nat_success_ratio;

		log_buffer.push((struct log_pkt_format_struct *)log_data_bw_ptr);
		buffer_size += sizeof(struct log_data_bw);
	}
	else if (log_mode == LOG_DATA_SOURCE_DELAY) {
		unsigned long manifest = va_arg(ap, unsigned long);
		double max_delay = va_arg(ap, double);
		unsigned long sub_number = va_arg(ap, unsigned long);
		double *delay_list = va_arg(ap, double*);
		unsigned int pkt_size = sizeof(struct log_header_t) + sizeof(double) + sizeof(UINT32) + (sub_number * sizeof(double));
		unsigned int offset = sizeof(struct log_header_t) + sizeof(double) + sizeof(UINT32);

		struct log_pkt_format_struct *log_pkt_format_struct_ptr = NULL;
		struct log_data_source_delay *log_data_source_delay_ptr = NULL;
		log_pkt_format_struct_ptr = (struct log_pkt_format_struct*)new unsigned char[pkt_size];
		if (!log_pkt_format_struct_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_pkt_format_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_pkt_format_struct_ptr, 0, pkt_size);
		log_data_source_delay_ptr = (struct log_data_source_delay *)log_pkt_format_struct_ptr;

		log_data_source_delay_ptr->log_header.cmd = LOG_DATA_SOURCE_DELAY;
		log_data_source_delay_ptr->log_header.log_time = log_time_dffer;
		log_data_source_delay_ptr->log_header.pid = self_pid;
		log_data_source_delay_ptr->log_header.manifest = manifest;
		log_data_source_delay_ptr->log_header.channel_id = self_channel_id;
		log_data_source_delay_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_data_source_delay_ptr->max_delay = max_delay;
		log_data_source_delay_ptr->sub_num = sub_number;

		memcpy((char *)log_pkt_format_struct_ptr + offset, delay_list, sub_number*sizeof(double));

		//debug_printf("log_data_source_delay_ptr->max_delay  = %lf\n",log_data_source_delay_ptr->max_delay );

		log_buffer.push((struct log_pkt_format_struct *)log_pkt_format_struct_ptr);
		buffer_size += pkt_size;
	}
	else if (log_mode == LOG_DATA_TOPOLOGY) {
		unsigned long manifest = va_arg(ap, unsigned long);
		unsigned long sub_number = va_arg(ap, unsigned long);
		unsigned long *parents_list = va_arg(ap, unsigned long*);
		unsigned int pkt_size = sizeof(struct log_header_t) + sizeof(UINT32) + (sub_number * sizeof(unsigned long));
		unsigned int offset = sizeof(struct log_header_t) + sizeof(UINT32);

		struct log_pkt_format_struct *log_pkt_format_struct_ptr = NULL;
		struct log_data_topology *log_data_topology_ptr = NULL;
		log_pkt_format_struct_ptr = (struct log_pkt_format_struct*)new unsigned char[pkt_size];
		if (!log_pkt_format_struct_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_pkt_format_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_pkt_format_struct_ptr, 0, pkt_size);
		log_data_topology_ptr = (struct log_data_topology *)log_pkt_format_struct_ptr;

		log_data_topology_ptr->log_header.cmd = LOG_DATA_TOPOLOGY;
		log_data_topology_ptr->log_header.log_time = log_time_dffer;
		log_data_topology_ptr->log_header.pid = self_pid;
		log_data_topology_ptr->log_header.manifest = manifest;
		log_data_topology_ptr->log_header.channel_id = self_channel_id;
		log_data_topology_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_data_topology_ptr->sub_num = sub_number;

		memcpy((char *)log_pkt_format_struct_ptr + offset, parents_list, sub_number*sizeof(UINT32));

		//debug_printf("log_data_source_delay_ptr->max_delay  = %lf\n",log_data_source_delay_ptr->max_delay );

		log_buffer.push((struct log_pkt_format_struct *)log_pkt_format_struct_ptr);
		buffer_size += pkt_size;
	}
	else if (log_mode == LOG_TOPO_PEER_JOIN) {
		unsigned long manifest = va_arg(ap, unsigned long);
		unsigned long selected_pid = va_arg(ap, unsigned long);
		
		struct log_topology *log_topology_ptr = new struct log_topology;
		if (!log_topology_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_topology_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_topology_ptr, 0, sizeof(struct log_topology));
	
		log_topology_ptr->log_header.cmd = LOG_TOPO_PEER_JOIN;
		log_topology_ptr->log_header.log_time = log_time_dffer;
		log_topology_ptr->log_header.pid = self_pid;
		log_topology_ptr->log_header.manifest = manifest;
		log_topology_ptr->log_header.channel_id = self_channel_id;
		log_topology_ptr->log_header.length = sizeof(struct log_topology) - sizeof(struct log_header_t);
		log_topology_ptr->my_pid = self_pid;
		log_topology_ptr->selected_pid = selected_pid;
		log_topology_ptr->manifest = manifest;
		
		log_buffer.push((struct log_pkt_format_struct *)log_topology_ptr);
		buffer_size += sizeof(struct log_topology);
	}
	else if (log_mode == LOG_TOPO_TEST_SUCCESS) {
		unsigned long manifest = va_arg(ap, unsigned long);
		unsigned long selected_pid = va_arg(ap, unsigned long);
		
		struct log_topology *log_topology_ptr = new struct log_topology;
		if (!log_topology_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_topology_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_topology_ptr, 0, sizeof(struct log_topology));
	
		log_topology_ptr->log_header.cmd = LOG_TOPO_TEST_SUCCESS;
		log_topology_ptr->log_header.log_time = log_time_dffer;
		log_topology_ptr->log_header.pid = self_pid;
		log_topology_ptr->log_header.manifest = manifest;
		log_topology_ptr->log_header.channel_id = self_channel_id;
		log_topology_ptr->log_header.length = sizeof(struct log_topology) - sizeof(struct log_header_t);
		log_topology_ptr->my_pid = self_pid;
		log_topology_ptr->selected_pid = selected_pid;
		log_topology_ptr->manifest = manifest;
		
		log_buffer.push((struct log_pkt_format_struct *)log_topology_ptr);
		buffer_size += sizeof(struct log_topology);
	}
	else if (log_mode == LOG_TOPO_RESCUE_TRIGGER) {
		unsigned long manifest = va_arg(ap, unsigned long);
		unsigned long selected_pid = va_arg(ap, unsigned long);
		
		struct log_topology *log_topology_ptr = new struct log_topology;
		if (!log_topology_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_topology_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_topology_ptr, 0, sizeof(struct log_topology));
	
		log_topology_ptr->log_header.cmd = LOG_TOPO_RESCUE_TRIGGER;
		log_topology_ptr->log_header.log_time = log_time_dffer;
		log_topology_ptr->log_header.pid = self_pid;
		log_topology_ptr->log_header.manifest = manifest;
		log_topology_ptr->log_header.channel_id = self_channel_id;
		log_topology_ptr->log_header.length = sizeof(struct log_topology) - sizeof(struct log_header_t);
		log_topology_ptr->my_pid = self_pid;
		log_topology_ptr->selected_pid = selected_pid;
		log_topology_ptr->manifest = manifest;
		
		log_buffer.push((struct log_pkt_format_struct *)log_topology_ptr);
		buffer_size += sizeof(struct log_topology);
	}
	else if (log_mode == LOG_TOPO_PEER_LEAVE) {
		unsigned long manifest = va_arg(ap, unsigned long);
		unsigned long selected_pid = va_arg(ap, unsigned long);
		
		struct log_topology *log_topology_ptr = new struct log_topology;
		if (!log_topology_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_topology_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_topology_ptr, 0, sizeof(struct log_topology));
	
		log_topology_ptr->log_header.cmd = LOG_TOPO_PEER_LEAVE;
		log_topology_ptr->log_header.log_time = log_time_dffer;
		log_topology_ptr->log_header.pid = self_pid;
		log_topology_ptr->log_header.manifest = manifest;
		log_topology_ptr->log_header.channel_id = self_channel_id;
		log_topology_ptr->log_header.length = sizeof(struct log_topology) - sizeof(struct log_header_t);
		log_topology_ptr->my_pid = self_pid;
		log_topology_ptr->selected_pid = selected_pid;
		log_topology_ptr->manifest = manifest;
		
		log_buffer.push((struct log_pkt_format_struct *)log_topology_ptr);
		buffer_size += sizeof(struct log_topology);
	}
#ifdef SEND_LOG_DEBUG
	else if(log_mode == LOG_REGISTER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_register_struct *log_register_struct_ptr = NULL;
		log_register_struct_ptr = new struct log_register_struct;
		if (!log_register_struct_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] log_register_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_register_struct_ptr,0x00,sizeof(struct log_register_struct));

		log_register_struct_ptr->log_header.cmd = LOG_REGISTER;
		log_register_struct_ptr->log_header.log_time = log_time_dffer;
		log_register_struct_ptr->log_header.pid = self_pid;
		log_register_struct_ptr->log_header.manifest = manifest;
		log_register_struct_ptr->log_header.channel_id = self_channel_id;
		log_register_struct_ptr->log_header.length = sizeof(struct log_register_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_register_struct_ptr);
		buffer_size += sizeof(struct log_register_struct);
	}
	else if(log_mode == LOG_REG_LIST){
		unsigned long manifest,list_num,connect_num;
		unsigned long *list_ptr, *connect_list_ptr;
		unsigned int pkt_size = 0;
		unsigned int offset = 0, array_size;
		struct log_pkt_format_struct *log_pkt = NULL;
		struct log_list_struct *log_list_struct_ptr = NULL;

		manifest = va_arg(ap, unsigned long);
		list_num = va_arg(ap, unsigned long);
		connect_num = va_arg(ap, unsigned long);
		list_ptr = va_arg(ap, unsigned long *);
		connect_list_ptr = va_arg(ap, unsigned long *);

		pkt_size = sizeof(struct log_header_t) + (2 * sizeof(unsigned long)) + (list_num * sizeof(unsigned long)) + (connect_num * sizeof(unsigned long));
		log_pkt = (struct log_pkt_format_struct *)new unsigned char[pkt_size];
		if (!log_pkt) {
			handle_error(MALLOC_ERROR, "[ERROR] log_pkt loggerClien new error", __FUNCTION__, __LINE__);
		}
		log_list_struct_ptr = (struct log_list_struct*)log_pkt;

		memset(log_pkt,0x00,pkt_size);

		log_list_struct_ptr->log_header.cmd = LOG_REG_LIST;
		log_list_struct_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_list_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_struct_ptr->log_header.pid = self_pid;
		log_list_struct_ptr->log_header.manifest = manifest; 
		log_list_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_struct_ptr->list_num = list_num;
		log_list_struct_ptr->connect_num = connect_num;

		offset += sizeof(struct log_header_t) + (2 * sizeof(unsigned long));

		if(list_num != 0){
			array_size = list_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,list_ptr,array_size);
			offset += array_size;
		}

		if(connect_num != 0){
			array_size = connect_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,connect_list_ptr,array_size);
			offset += array_size;
		}
		log_buffer.push((struct log_pkt_format_struct *)log_pkt);
		buffer_size += pkt_size;

	}
	else if(log_mode == LOG_REG_LIST_TESTING){
		unsigned long manifest,select_pid;
		manifest = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);

		struct log_list_testing_struct *log_list_testing_struct_ptr = NULL;
		log_list_testing_struct_ptr = new struct log_list_testing_struct;
		if(!(log_list_testing_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_list_testing_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_list_testing_struct_ptr,0x00,sizeof(struct log_list_testing_struct));

		log_list_testing_struct_ptr->log_header.cmd = LOG_REG_LIST_TESTING;
		log_list_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_struct_ptr->log_header.pid = self_pid;
		log_list_testing_struct_ptr->log_header.manifest = manifest;
		log_list_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_struct_ptr->log_header.length = sizeof(struct log_list_testing_struct) - sizeof(struct log_header_t);
		log_list_testing_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_struct);
	}
	else if(log_mode == LOG_REG_LIST_DETECTION_TESTING_SUCCESS){
		unsigned long manifest,testing_result;
		unsigned long select_pid;
		manifest = va_arg(ap, unsigned long);
		testing_result = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);
		
		struct log_list_detection_testing_struct *log_list_detection_testing_struct_ptr = NULL;
		log_list_detection_testing_struct_ptr = new struct log_list_detection_testing_struct;
		if(!(log_list_detection_testing_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_list_detection_testing_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_list_detection_testing_struct_ptr,0x00,sizeof(struct log_list_detection_testing_struct));

		log_list_detection_testing_struct_ptr->log_header.cmd = LOG_REG_LIST_DETECTION_TESTING_SUCCESS;
		log_list_detection_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_detection_testing_struct_ptr->log_header.pid = self_pid;
		log_list_detection_testing_struct_ptr->log_header.manifest = manifest;
		log_list_detection_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_detection_testing_struct_ptr->log_header.length = sizeof(struct log_list_detection_testing_struct) - sizeof(struct log_header_t);
		log_list_detection_testing_struct_ptr->testing_result = testing_result;
		log_list_detection_testing_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_list_detection_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_detection_testing_struct);
	}
	else if(log_mode == LOG_REG_LIST_TESTING_FAIL){
		unsigned long manifest;
		unsigned long select_pid;
		manifest = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);
		
		struct log_list_testing_fail_struct *log_list_testing_fail_struct_ptr = NULL;
		log_list_testing_fail_struct_ptr = new struct log_list_testing_fail_struct;
		if(!(log_list_testing_fail_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_list_testing_fail_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_list_testing_fail_struct_ptr,0x00,sizeof(struct log_list_testing_fail_struct));

		log_list_testing_fail_struct_ptr->log_header.cmd = LOG_REG_LIST_TESTING_FAIL;
		log_list_testing_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_fail_struct_ptr->log_header.pid = self_pid;
		log_list_testing_fail_struct_ptr->log_header.manifest = manifest;
		log_list_testing_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_fail_struct_ptr->log_header.length = sizeof(struct log_list_testing_fail_struct) - sizeof(struct log_header_t);
		log_list_testing_fail_struct_ptr->select_pid = select_pid;
		
		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_fail_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_fail_struct);
	}
	else if(log_mode == LOG_REG_CUT_PK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_cut_pk_struct *log_cut_pk_struct_ptr = NULL;
		log_cut_pk_struct_ptr = new struct log_cut_pk_struct;
		if(!(log_cut_pk_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_cut_pk_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_cut_pk_struct_ptr,0x00,sizeof(struct log_cut_pk_struct));

		log_cut_pk_struct_ptr->log_header.cmd = LOG_REG_CUT_PK;
		log_cut_pk_struct_ptr->log_header.log_time = log_time_dffer;
		log_cut_pk_struct_ptr->log_header.pid = self_pid;
		log_cut_pk_struct_ptr->log_header.manifest = manifest;
		log_cut_pk_struct_ptr->log_header.channel_id = self_channel_id;
		log_cut_pk_struct_ptr->log_header.length = sizeof(struct log_cut_pk_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_cut_pk_struct_ptr);
		buffer_size += sizeof(struct log_cut_pk_struct);
	}
	else if(log_mode == LOG_REG_DATA_COME){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_data_come_struct *log_data_come_struct_ptr = NULL;
		log_data_come_struct_ptr = new struct log_data_come_struct;
		if(!(log_data_come_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_data_come_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_data_come_struct_ptr,0x00,sizeof(struct log_data_come_struct));

		log_data_come_struct_ptr->log_header.cmd = LOG_REG_DATA_COME;
		log_data_come_struct_ptr->log_header.log_time = log_time_dffer;
		log_data_come_struct_ptr->log_header.pid = self_pid;
		log_data_come_struct_ptr->log_header.manifest = manifest;
		log_data_come_struct_ptr->log_header.channel_id = self_channel_id;
		log_data_come_struct_ptr->log_header.length = sizeof(struct log_data_come_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_data_come_struct_ptr);
		buffer_size += sizeof(struct log_data_come_struct);
	}
	else if(log_mode == LOG_RESCUE_TRIGGER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_struct *log_rescue_trigger_struct_ptr = NULL;
		log_rescue_trigger_struct_ptr = new struct log_rescue_trigger_struct;
		if(!(log_rescue_trigger_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_rescue_trigger_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_rescue_trigger_struct_ptr,0x00,sizeof(struct log_rescue_trigger_struct));

		log_rescue_trigger_struct_ptr->log_header.cmd = LOG_RESCUE_TRIGGER;
		log_rescue_trigger_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_struct);
	}
	else if(log_mode == LOG_DELAY_RESCUE_TRIGGER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_struct *log_rescue_trigger_struct_ptr = NULL;
		log_rescue_trigger_struct_ptr = new struct log_rescue_trigger_struct;
		if(!(log_rescue_trigger_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_rescue_trigger_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_rescue_trigger_struct_ptr,0x00,sizeof(struct log_rescue_trigger_struct));

		log_rescue_trigger_struct_ptr->log_header.cmd = LOG_DELAY_RESCUE_TRIGGER;
		log_rescue_trigger_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_struct);
	}
	else if(log_mode == LOG_MERGE_TRIGGER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_struct *log_rescue_trigger_struct_ptr = NULL;
		log_rescue_trigger_struct_ptr = new struct log_rescue_trigger_struct;
		if(!(log_rescue_trigger_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_rescue_trigger_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_rescue_trigger_struct_ptr,0x00,sizeof(struct log_rescue_trigger_struct));

		log_rescue_trigger_struct_ptr->log_header.cmd = LOG_MERGE_TRIGGER;
		log_rescue_trigger_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_struct);
	}
	else if(log_mode == LOG_RESCUE_LIST){
		unsigned long manifest,list_num,connect_num;
		unsigned long *list_ptr, *connect_list_ptr;
		unsigned int pkt_size = 0;
		unsigned int offset = 0, array_size;
		struct log_pkt_format_struct *log_pkt = NULL;
		struct log_list_struct *log_list_struct_ptr = NULL;

		manifest = va_arg(ap, unsigned long);
		list_num = va_arg(ap, unsigned long);
		connect_num = va_arg(ap, unsigned long);
		list_ptr = va_arg(ap, unsigned long *);
		connect_list_ptr = va_arg(ap, unsigned long *);

		pkt_size = sizeof(struct log_header_t) + (2 * sizeof(unsigned long)) + (list_num * sizeof(unsigned long)) + (connect_num * sizeof(unsigned long));
		log_pkt = (struct log_pkt_format_struct *)new unsigned char[pkt_size];
		if(!(log_pkt)){
			handle_error(MALLOC_ERROR, "[ERROR] log_pkt loggerClien new error", __FUNCTION__, __LINE__);
		}
		log_list_struct_ptr = (struct log_list_struct*)log_pkt;

		memset(log_pkt,0x00,pkt_size);

		log_list_struct_ptr->log_header.cmd = LOG_RESCUE_LIST;
		log_list_struct_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_list_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_struct_ptr->log_header.pid = self_pid;
		log_list_struct_ptr->log_header.manifest = manifest; 
		log_list_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_struct_ptr->list_num = list_num;
		log_list_struct_ptr->connect_num = connect_num;

		offset += sizeof(struct log_header_t) + (2 * sizeof(unsigned long));

		if(list_num != 0){
			array_size = list_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,list_ptr,array_size);
			offset += array_size;
		}

		if(connect_num != 0){
			array_size = connect_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,connect_list_ptr,array_size);
			offset += array_size;
		}
		log_buffer.push((struct log_pkt_format_struct *)log_pkt);
		buffer_size += pkt_size;

	}
	else if(log_mode == LOG_RESCUE_TESTING){
		unsigned long manifest,select_pid;
		manifest = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);

		struct log_list_testing_struct *log_list_testing_struct_ptr = NULL;
		log_list_testing_struct_ptr = new struct log_list_testing_struct;
		if(!(log_list_testing_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_list_testing_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_list_testing_struct_ptr,0x00,sizeof(struct log_list_testing_struct));

		log_list_testing_struct_ptr->log_header.cmd = LOG_RESCUE_TESTING;
		log_list_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_struct_ptr->log_header.pid = self_pid;
		log_list_testing_struct_ptr->log_header.manifest = manifest;
		log_list_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_struct_ptr->log_header.length = sizeof(struct log_list_testing_struct) - sizeof(struct log_header_t);
		log_list_testing_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_struct);
	}
	else if(log_mode == LOG_RESCUE_DETECTION_TESTING_SUCCESS){
		unsigned long manifest,testing_result;
		unsigned long select_pid;
		manifest = va_arg(ap, unsigned long);
		testing_result = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);
		
		struct log_list_detection_testing_struct *log_list_detection_testing_struct_ptr = NULL;
		log_list_detection_testing_struct_ptr = new struct log_list_detection_testing_struct;
		if(!(log_list_detection_testing_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_list_detection_testing_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_list_detection_testing_struct_ptr,0x00,sizeof(struct log_list_detection_testing_struct));

		log_list_detection_testing_struct_ptr->log_header.cmd = LOG_RESCUE_DETECTION_TESTING_SUCCESS;
		log_list_detection_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_detection_testing_struct_ptr->log_header.pid = self_pid;
		log_list_detection_testing_struct_ptr->log_header.manifest = manifest;
		log_list_detection_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_detection_testing_struct_ptr->log_header.length = sizeof(struct log_list_detection_testing_struct) - sizeof(struct log_header_t);
		log_list_detection_testing_struct_ptr->testing_result = testing_result;
		log_list_detection_testing_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_list_detection_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_detection_testing_struct);
	}
	else if(log_mode == LOG_RESCUE_LIST_TESTING_FAIL){
		unsigned long manifest;
		unsigned long select_pid;
		manifest = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);

		struct log_list_testing_fail_struct *log_list_testing_fail_struct_ptr = NULL;
		log_list_testing_fail_struct_ptr = new struct log_list_testing_fail_struct;
		if(!(log_list_testing_fail_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_list_testing_fail_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_list_testing_fail_struct_ptr,0x00,sizeof(struct log_list_testing_fail_struct));

		log_list_testing_fail_struct_ptr->log_header.cmd = LOG_RESCUE_LIST_TESTING_FAIL;
		log_list_testing_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_fail_struct_ptr->log_header.pid = self_pid;
		log_list_testing_fail_struct_ptr->log_header.manifest = manifest;
		log_list_testing_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_fail_struct_ptr->log_header.length = sizeof(struct log_list_testing_fail_struct) - sizeof(struct log_header_t);
		log_list_testing_fail_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_fail_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_fail_struct);
	}
	else if(log_mode == LOG_RESCUE_CUT_PK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_cut_pk_struct *log_cut_pk_struct_ptr = NULL;
		log_cut_pk_struct_ptr = new struct log_cut_pk_struct;
		if(!(log_cut_pk_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_cut_pk_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_cut_pk_struct_ptr,0x00,sizeof(struct log_cut_pk_struct));

		log_cut_pk_struct_ptr->log_header.cmd = LOG_RESCUE_CUT_PK;
		log_cut_pk_struct_ptr->log_header.log_time = log_time_dffer;
		log_cut_pk_struct_ptr->log_header.pid = self_pid;
		log_cut_pk_struct_ptr->log_header.manifest = manifest;
		log_cut_pk_struct_ptr->log_header.channel_id = self_channel_id;
		log_cut_pk_struct_ptr->log_header.length = sizeof(struct log_cut_pk_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_cut_pk_struct_ptr);
		buffer_size += sizeof(struct log_cut_pk_struct);
	}
	else if(log_mode == LOG_RESCUE_DATA_COME){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_data_come_struct *log_data_come_struct_ptr = NULL;
		log_data_come_struct_ptr = new struct log_data_come_struct;
		if(!(log_data_come_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_data_come_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_data_come_struct_ptr,0x00,sizeof(struct log_data_come_struct));

		log_data_come_struct_ptr->log_header.cmd = LOG_RESCUE_DATA_COME;
		log_data_come_struct_ptr->log_header.log_time = log_time_dffer;
		log_data_come_struct_ptr->log_header.pid = self_pid;
		log_data_come_struct_ptr->log_header.manifest = manifest;
		log_data_come_struct_ptr->log_header.channel_id = self_channel_id;
		log_data_come_struct_ptr->log_header.length = sizeof(struct log_data_come_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_data_come_struct_ptr);
		buffer_size += sizeof(struct log_data_come_struct);
	}
	else if(log_mode == LOG_START_DELAY){
		unsigned long manifest;
		double	start_delay;
		manifest = va_arg(ap, unsigned long);
		start_delay = va_arg(ap, double);

		struct log_start_delay_struct *log_start_delay_struct_ptr = NULL;
		log_start_delay_struct_ptr = new struct log_start_delay_struct;
		if(!(log_start_delay_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_start_delay_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_start_delay_struct_ptr,0x00,sizeof(struct log_start_delay_struct));

		log_start_delay_struct_ptr->log_header.cmd = LOG_START_DELAY;
		log_start_delay_struct_ptr->log_header.log_time = log_time_dffer;
		log_start_delay_struct_ptr->log_header.pid = self_pid;
		log_start_delay_struct_ptr->log_header.manifest = manifest;
		log_start_delay_struct_ptr->log_header.channel_id = self_channel_id;
		log_start_delay_struct_ptr->log_header.length = sizeof(struct log_start_delay_struct) - sizeof(struct log_header_t);
		log_start_delay_struct_ptr->start_delay = start_delay;

		log_buffer.push((struct log_pkt_format_struct *)log_start_delay_struct_ptr);
		buffer_size += sizeof(struct log_start_delay_struct);
	}
	else if(log_mode == LOG_PERIOD_SOURCE_DELAY){
		unsigned long manifest,sub_number;
		double *delay_list = NULL;
		double max_delay=0;
		unsigned int pkt_size = 0;
		unsigned int offset = 0;

		manifest = va_arg(ap, unsigned long);
		max_delay = va_arg(ap, double);
		sub_number = va_arg(ap, unsigned long);
		delay_list = va_arg(ap, double*);

		pkt_size = sizeof(struct log_header_t) + sizeof(unsigned long)+ sizeof(double)+ (sub_number * sizeof(double));
		struct log_pkt_format_struct *log_pkt_format_struct_ptr = NULL;
		struct log_period_source_delay_struct *log_period_source_delay_struct_ptr = NULL;
		log_pkt_format_struct_ptr = (struct log_pkt_format_struct*)new unsigned char[pkt_size];
		if(!(log_pkt_format_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_pkt_format_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_pkt_format_struct_ptr,0x00,pkt_size);
		log_period_source_delay_struct_ptr = (struct log_period_source_delay_struct *)log_pkt_format_struct_ptr;


		log_period_source_delay_struct_ptr->log_header.cmd = LOG_PERIOD_SOURCE_DELAY;
		log_period_source_delay_struct_ptr->log_header.log_time = log_time_dffer;
		log_period_source_delay_struct_ptr->log_header.pid = self_pid;
		log_period_source_delay_struct_ptr->log_header.manifest = manifest;
		log_period_source_delay_struct_ptr->log_header.channel_id = self_channel_id;
		log_period_source_delay_struct_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_period_source_delay_struct_ptr->max_delay = max_delay;
		log_period_source_delay_struct_ptr->sub_num = sub_number;

		offset = sizeof(struct log_header_t) +  sizeof(unsigned long) +  sizeof(double) ;

		memcpy((char *)log_pkt_format_struct_ptr + offset,delay_list,(sub_number * sizeof(double)));

		log_buffer.push((struct log_pkt_format_struct *)log_pkt_format_struct_ptr);
		buffer_size += pkt_size;
	}
	else if(log_mode == LOG_RESCUE_SUB_STREAM){
		unsigned long manifest,rescue_num;
		manifest = va_arg(ap, unsigned long);
		rescue_num = va_arg(ap, unsigned long);

		struct log_rescue_sub_stream_struct *log_rescue_sub_stream_struct_ptr = NULL;
		log_rescue_sub_stream_struct_ptr = new struct log_rescue_sub_stream_struct;
		if(!(log_rescue_sub_stream_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_rescue_sub_stream_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_rescue_sub_stream_struct_ptr,0x00,sizeof(struct log_rescue_sub_stream_struct));

		log_rescue_sub_stream_struct_ptr->log_header.cmd = LOG_RESCUE_SUB_STREAM;
		log_rescue_sub_stream_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_sub_stream_struct_ptr->log_header.pid = self_pid;
		log_rescue_sub_stream_struct_ptr->log_header.manifest = manifest;
		log_rescue_sub_stream_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_sub_stream_struct_ptr->log_header.length = sizeof(struct log_rescue_sub_stream_struct) - sizeof(struct log_header_t);
		log_rescue_sub_stream_struct_ptr->rescue_num = rescue_num;

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_sub_stream_struct_ptr);
		buffer_size += sizeof(struct log_rescue_sub_stream_struct);
	}
	else if(log_mode == LOG_PEER_LEAVE){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_peer_leave_struct *log_peer_leave_struct_ptr = NULL;
		log_peer_leave_struct_ptr = new struct log_peer_leave_struct;
		if(!(log_peer_leave_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_peer_leave_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_peer_leave_struct_ptr,0x00,sizeof(struct log_peer_leave_struct));

		log_peer_leave_struct_ptr->log_header.cmd = LOG_PEER_LEAVE;
		log_peer_leave_struct_ptr->log_header.log_time = log_time_dffer;
		log_peer_leave_struct_ptr->log_header.pid = self_pid;
		log_peer_leave_struct_ptr->log_header.manifest = manifest;
		log_peer_leave_struct_ptr->log_header.channel_id = self_channel_id;
		log_peer_leave_struct_ptr->log_header.length = sizeof(struct log_peer_leave_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_peer_leave_struct_ptr);
		buffer_size += sizeof(struct log_peer_leave_struct);
	}
	else if(log_mode == LOG_WRITE_STRING){
		unsigned long manifest;
		int str_buffer_size = 300;
		int int_array_size = 12;
		unsigned char *str_buffer = new unsigned char[str_buffer_size];
		if(!(str_buffer)){
			handle_error(MALLOC_ERROR, "[ERROR] str_buffer loggerClien new error ", __FUNCTION__, __LINE__);
		}
		char *inttostr = new char[int_array_size];	//base 4 btes, but it will be increase if not enough (in sprintf)
		if(!(inttostr)){
			handle_error(MALLOC_ERROR, "[ERROR] inttostr loggerClien new error", __FUNCTION__, __LINE__);
		}
		const char *fmt = NULL;
		int str_buffer_offset = 0;
		int d;
		unsigned int u;
		char *s;
		int pkt_size = 0;
		int int_size = 0;

		manifest = va_arg(ap, unsigned long);
		memset(str_buffer,0x00,str_buffer_size);

		for(fmt = va_arg(ap, const char*);*fmt;fmt++){
			switch(*fmt) {
				case 's':           /* string */
					s = va_arg(ap, char *);
					if((str_buffer_offset + (int)strlen(s))>=str_buffer_size){
						break;
					}
					memcpy((char*)str_buffer + str_buffer_offset,s,strlen(s));
					str_buffer_offset += strlen(s);
					break;
				case 'd':           /* int */
					int_size = 0;
					d = va_arg(ap, int);
					memset(inttostr,0x00,int_array_size);
					#ifdef _WIN32
					int_size = _snprintf(inttostr,(unsigned int)int_array_size,"%d",d);
					#else 
					int_size = snprintf(inttostr,(unsigned int)int_array_size,"%d",d);
					#endif
					if((str_buffer_offset + int_size)>=str_buffer_size){
						break;
					}
					memcpy((char*)str_buffer + str_buffer_offset,inttostr,int_size);
					str_buffer_offset += int_size;
					break;
				case 'u':           /* unsigned int */
					int_size = 0;
					u = va_arg(ap, unsigned int);
					memset(inttostr,0x00,int_array_size);
					#ifdef _WIN32
					int_size = _snprintf(inttostr,int_array_size,"%u",u);
					#else 
					int_size = snprintf(inttostr,int_array_size,"%u",u);
					#endif

					if((str_buffer_offset + int_size)>=str_buffer_size){
						break;
					}
					memcpy((char*)str_buffer + str_buffer_offset,inttostr,int_size);
					str_buffer_offset += int_size;
					break;
				default:
					if((str_buffer_offset+1)>=str_buffer_size){
						
					}
					else{
						memcpy((char*)str_buffer + str_buffer_offset,fmt,1);
						str_buffer_offset += 1;
					}
			}
		}

		pkt_size += (sizeof(struct log_header_t) + str_buffer_offset);
		struct log_write_string_struct *log_write_string_struct_ptr = (struct log_write_string_struct *)new unsigned char[pkt_size];
		if(!(log_write_string_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_write_string_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}

		memset(log_write_string_struct_ptr,0x00,pkt_size);

		log_write_string_struct_ptr->log_header.cmd = LOG_WRITE_STRING;
		log_write_string_struct_ptr->log_header.log_time = log_time_dffer;
		log_write_string_struct_ptr->log_header.pid = self_pid;
		log_write_string_struct_ptr->log_header.manifest = manifest;
		log_write_string_struct_ptr->log_header.channel_id = self_channel_id;
		log_write_string_struct_ptr->log_header.length = str_buffer_offset;
		
		memcpy(log_write_string_struct_ptr->buf,str_buffer,str_buffer_offset);

		log_buffer.push((struct log_pkt_format_struct *)log_write_string_struct_ptr);
		buffer_size += pkt_size;
	}
	else if(log_mode == LOG_BEGINE){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_begine_struct *log_begine_struct_ptr = NULL;
		log_begine_struct_ptr = new struct log_begine_struct;
		if(!(log_begine_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_begine_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_begine_struct_ptr,0x00,sizeof(struct log_begine_struct));

		log_begine_struct_ptr->log_header.cmd = LOG_BEGINE;
		log_begine_struct_ptr->log_header.log_time = log_time_dffer;
		log_begine_struct_ptr->log_header.pid = self_pid;
		log_begine_struct_ptr->log_header.manifest = manifest;
		log_begine_struct_ptr->log_header.channel_id = self_channel_id;
		log_begine_struct_ptr->log_header.length = sizeof(struct log_begine_struct) - sizeof(struct log_header_t);
		log_begine_struct_ptr->public_ip = my_public_ip;
		log_begine_struct_ptr->private_port = my_private_port;
		
		_log_ptr->write_log_format("s(u) s:d \n", __FUNCTION__, __LINE__, inet_ntoa(*(struct in_addr *)&my_public_ip), my_private_port);
		
		log_buffer.push((struct log_pkt_format_struct *)log_begine_struct_ptr);
		buffer_size += sizeof(struct log_begine_struct);
	}
	else if(log_mode == LOG_RESCUE_TRIGGER_BACK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_back_struct *log_rescue_trigger_back_struct_ptr = NULL;
		log_rescue_trigger_back_struct_ptr = new struct log_rescue_trigger_back_struct;
		if(!(log_rescue_trigger_back_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_rescue_trigger_back_struct_ptr loggerClien new error ", __FUNCTION__, __LINE__);
		}
		memset(log_rescue_trigger_back_struct_ptr,0x00,sizeof(struct log_rescue_trigger_back_struct));

		log_rescue_trigger_back_struct_ptr->log_header.cmd = LOG_RESCUE_TRIGGER_BACK;
		log_rescue_trigger_back_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_back_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_back_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_back_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_back_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_back_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_back_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_back_struct);
	}
	else if(log_mode == LOG_LIST_EMPTY){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_list_empty_struct *log_list_empty_struct_ptr = NULL;
		log_list_empty_struct_ptr = new struct log_list_empty_struct;
		if(!(log_list_empty_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_list_empty_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_list_empty_struct_ptr,0x00,sizeof(struct log_list_empty_struct));

		log_list_empty_struct_ptr->log_header.cmd = LOG_LIST_EMPTY;
		log_list_empty_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_empty_struct_ptr->log_header.pid = self_pid;
		log_list_empty_struct_ptr->log_header.manifest = manifest;
		log_list_empty_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_empty_struct_ptr->log_header.length = sizeof(struct log_list_empty_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_list_empty_struct_ptr);
		buffer_size += sizeof(struct log_list_empty_struct);
	}
	else if(log_mode == LOG_TEST_DELAY_FAIL){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_test_delay_fail_struct *log_test_delay_fail_struct_ptr = NULL;
		log_test_delay_fail_struct_ptr = new struct log_test_delay_fail_struct;
		if(!(log_test_delay_fail_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_test_delay_fail_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_test_delay_fail_struct_ptr,0x00,sizeof(struct log_test_delay_fail_struct));

		log_test_delay_fail_struct_ptr->log_header.cmd = LOG_TEST_DELAY_FAIL;
		log_test_delay_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_test_delay_fail_struct_ptr->log_header.pid = self_pid;
		log_test_delay_fail_struct_ptr->log_header.manifest = manifest;
		log_test_delay_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_test_delay_fail_struct_ptr->log_header.length = sizeof(struct log_test_delay_fail_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_test_delay_fail_struct_ptr);
		buffer_size += sizeof(struct log_test_delay_fail_struct);
	}
	else if(log_mode == LOG_TEST_DETECTION_FAIL){
		unsigned long manifest;
		unsigned long select_pid;
		manifest = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);

		struct log_test_detection_fail_struct *log_test_detection_fail_struct_ptr = NULL;
		log_test_detection_fail_struct_ptr = new struct log_test_detection_fail_struct;
		if(!(log_test_detection_fail_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_test_detection_fail_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_test_detection_fail_struct_ptr,0x00,sizeof(struct log_test_detection_fail_struct));

		log_test_detection_fail_struct_ptr->log_header.cmd = LOG_TEST_DETECTION_FAIL;
		log_test_detection_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_test_detection_fail_struct_ptr->log_header.pid = self_pid;
		log_test_detection_fail_struct_ptr->log_header.manifest = manifest;
		log_test_detection_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_test_detection_fail_struct_ptr->log_header.length = sizeof(struct log_test_detection_fail_struct) - sizeof(struct log_header_t);
		log_test_detection_fail_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_test_detection_fail_struct_ptr);
		buffer_size += sizeof(struct log_test_detection_fail_struct);
	}
	else if(log_mode == LOG_DATA_COME_PK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_data_come_pk_struct *log_data_come_pk_struct_ptr = NULL;
		log_data_come_pk_struct_ptr = new struct log_data_come_pk_struct;
		if(!(log_data_come_pk_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_data_come_pk_struct_ptr loggerClien new error ", __FUNCTION__, __LINE__);
		}
		memset(log_data_come_pk_struct_ptr,0x00,sizeof(struct log_data_come_pk_struct));

		log_data_come_pk_struct_ptr->log_header.cmd = LOG_DATA_COME_PK;
		log_data_come_pk_struct_ptr->log_header.log_time = log_time_dffer;
		log_data_come_pk_struct_ptr->log_header.pid = self_pid;
		log_data_come_pk_struct_ptr->log_header.manifest = manifest;
		log_data_come_pk_struct_ptr->log_header.channel_id = self_channel_id;
		log_data_come_pk_struct_ptr->log_header.length = sizeof(struct log_data_come_pk_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_data_come_pk_struct_ptr);
		buffer_size += sizeof(struct log_data_come_pk_struct);
	}
	else if(log_mode == LOG_CLIENT_BW){
		unsigned long manifest;
		double temp_should_in_bw;
		double temp_real_in_bw;
		double temp_real_out_bw;
		double temp_quality;

		manifest = va_arg(ap, unsigned long);
		temp_should_in_bw = va_arg(ap, double);
		temp_real_in_bw = va_arg(ap, double);
		temp_real_out_bw = va_arg(ap, double);
		temp_quality = va_arg(ap, double);

		struct log_client_bw_struct *log_client_bw_struct_ptr = NULL;
		log_client_bw_struct_ptr = new struct log_client_bw_struct;
		if(!(log_client_bw_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_client_bw_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_client_bw_struct_ptr,0x00,sizeof(struct log_client_bw_struct));

		log_client_bw_struct_ptr->log_header.cmd = LOG_CLIENT_BW;
		log_client_bw_struct_ptr->log_header.log_time = log_time_dffer;
		log_client_bw_struct_ptr->log_header.pid = self_pid;
		log_client_bw_struct_ptr->log_header.manifest = manifest;
		log_client_bw_struct_ptr->log_header.channel_id = self_channel_id;
		log_client_bw_struct_ptr->log_header.length = sizeof(struct log_client_bw_struct) - sizeof(struct log_header_t);
		log_client_bw_struct_ptr->should_in_bw = temp_should_in_bw;
		log_client_bw_struct_ptr->real_in_bw = temp_real_in_bw;
		log_client_bw_struct_ptr->real_out_bw = temp_real_out_bw;
		log_client_bw_struct_ptr->quality = temp_quality;

		log_buffer.push((struct log_pkt_format_struct *)log_client_bw_struct_ptr);
		buffer_size += sizeof(struct log_client_bw_struct);
	}
	else if(log_mode == LOG_TIME_OUT){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_time_out_struct *log_time_out_struct_ptr = NULL;
		log_time_out_struct_ptr = new struct log_time_out_struct;
		if(!(log_time_out_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_time_out_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_time_out_struct_ptr,0x00,sizeof(struct log_time_out_struct));

		log_time_out_struct_ptr->log_header.cmd = LOG_TIME_OUT;
		log_time_out_struct_ptr->log_header.log_time = log_time_dffer;
		log_time_out_struct_ptr->log_header.pid = self_pid;
		log_time_out_struct_ptr->log_header.manifest = manifest;
		log_time_out_struct_ptr->log_header.channel_id = self_channel_id;
		log_time_out_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_time_out_struct_ptr);
		buffer_size += sizeof(struct log_time_out_struct);
	}
	else if(log_mode == LOG_PKT_LOSE){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_pkt_lose_struct *log_pkt_lose_struct_ptr = NULL;
		log_pkt_lose_struct_ptr = new struct log_pkt_lose_struct;
		if(!(log_pkt_lose_struct_ptr)){
			handle_error(MALLOC_ERROR, "[ERROR] log_pkt_lose_struct_ptr loggerClien new error", __FUNCTION__, __LINE__);
		}
		memset(log_pkt_lose_struct_ptr,0x00,sizeof(struct log_pkt_lose_struct));

		log_pkt_lose_struct_ptr->log_header.cmd = LOG_PKT_LOSE;
		log_pkt_lose_struct_ptr->log_header.log_time = log_time_dffer;
		log_pkt_lose_struct_ptr->log_header.pid = self_pid;
		log_pkt_lose_struct_ptr->log_header.manifest = manifest;
		log_pkt_lose_struct_ptr->log_header.channel_id = self_channel_id;
		log_pkt_lose_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_pkt_lose_struct_ptr);
		buffer_size += sizeof(struct log_pkt_lose_struct);
	}
#endif
	else{
		handle_error(UNKNOWN, "[ERROR] unknown state in log", __FUNCTION__, __LINE__);
	}
	//debug_printf("log_mode = %d, self_channel_id = %u \n", log_mode, self_channel_id);
}

void logger_client::log_clear_buffer()
{
	buffer_clear_flag = 1;
}

void logger_client::log_exit()
{
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr = NULL;
	struct log_pkt_format_struct *log_buffer_element_ptr = NULL;
	Nonblocking_Send_Ctrl_ptr = &(non_log_recv_struct.nonBlockingSendCtrl);
	int chunk_buffer_offset = 0;
	int log_struct_size = 0;
	int _send_byte;

	//blocking send
	while (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING) {
		_send_byte = _net_ptr->nonblock_send(log_server_sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
			
		if (_send_byte <= 0) {
#ifdef _WIN32
			int socketErr = WSAGetLastError();
#else
			int socketErr = errno;
#endif
			exit_code = LOG_SOCKET_ERROR;
			debug_printf("send info to log server error : %d %d \n", _send_byte, socketErr);
			_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__, "send info to log server error :", _send_byte, socketErr);
			*(_net_ptr->_errorRestartFlag) = RESTART;
			PAUSE
		}
	}

	log_to_server(LOG_PEER_LEAVE, 0);
	log_to_server(LOG_TOPO_PEER_LEAVE, 0, 0);
	
	while (buffer_size > 0) {
		memset(chunk_buffer, 0, CHUNK_BUFFER_SIZE + sizeof(struct chunk_header_t));
		chunk_buffer_offset = 0;
	
		log_buffer_element_ptr = log_buffer.front();

		log_struct_size = log_buffer_element_ptr->log_header.length + sizeof(struct log_header_t);
		buffer_size -= log_struct_size;
	
		memcpy((char *)(chunk_buffer->buf) + chunk_buffer_offset, log_buffer_element_ptr, log_struct_size);
		chunk_buffer_offset += log_struct_size;

		if (log_buffer_element_ptr->log_header.cmd == LOG_DATA_PEER_INFO 	|| 
			log_buffer_element_ptr->log_header.cmd == LOG_DATA_START_DELAY 	||
			log_buffer_element_ptr->log_header.cmd == LOG_DATA_BANDWIDTH 	||
			log_buffer_element_ptr->log_header.cmd == LOG_DATA_SOURCE_DELAY ||
			log_buffer_element_ptr->log_header.cmd == LOG_TOPO_PEER_JOIN 	||
			log_buffer_element_ptr->log_header.cmd == LOG_TOPO_TEST_SUCCESS ||
			log_buffer_element_ptr->log_header.cmd == LOG_TOPO_RESCUE_TRIGGER ||
			log_buffer_element_ptr->log_header.cmd == LOG_TOPO_PEER_LEAVE) 
		{
			chunk_buffer->header.cmd = CHNK_CMD_LOG;
		} else {
			chunk_buffer->header.cmd = CHNK_CMD_LOG_DEBUG;
		}

		
		log_buffer.pop();
		if (log_buffer_element_ptr) {
			delete log_buffer_element_ptr;
		}
		log_buffer_element_ptr = NULL;
	
		//chunk_buffer->header.cmd = CHNK_CMD_LOG;
		chunk_buffer->header.rsv_1 = REPLY;
		chunk_buffer->header.length = chunk_buffer_offset;
		chunk_buffer->header.sequence_number = 0;

		//non-bolcking send maybe have problem?
		//_net_ptr->set_blocking(log_server_sock);
		_send_byte = send(log_server_sock, (char *)chunk_buffer, (chunk_buffer->header.length + sizeof(chunk_header_t)), 0);
		//_net_ptr->set_nonblocking(log_server_sock);
		
		if (_send_byte <= 0) {
#ifdef _WIN32
			int socketErr = WSAGetLastError();
#else
			int socketErr = errno;
#endif
			exit_code = LOG_SOCKET_ERROR;
			debug_printf("send info to log server error : %d %d \n", _send_byte, socketErr);
			_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__, "send info to log server error :", _send_byte, socketErr);
			*(_net_ptr->_errorRestartFlag) = RESTART;
			//PAUSE
			break;
		}
	}
	
	list<int>::iterator fd_iter;
	
	// Sleep 0.5 second to flush socket buffer
#ifdef _WIN32
	Sleep(500);
#else
	usleep(500000);
#endif
	
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "close log-server");
	debug_printf("close log-server \n");
	_net_ptr->close(log_server_sock);

	map<int, unsigned long>::iterator map_fd_pid_iter;
	
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
	for(fd_iter = _pk_mgr_ptr->fd_list_ptr->begin(); fd_iter != _pk_mgr_ptr->fd_list_ptr->end(); fd_iter++) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
		if (*fd_iter == log_server_sock) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
			_pk_mgr_ptr->fd_list_ptr->erase(fd_iter);
			break;
		}
	}
}

int logger_client::handle_pkt_in(int sock)
{
	log_to_server(LOG_WRITE_STRING, 0, "s \n", "cannot in this sope in logger_client::handle_pkt_in \n");
	log_exit();
	return RET_OK;
}

int logger_client::handle_pkt_out(int sock)
{
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr = NULL;
	struct log_pkt_format_struct *log_buffer_element_ptr = NULL;
	Nonblocking_Send_Ctrl_ptr = &(non_log_recv_struct.nonBlockingSendCtrl);
	int chunk_buffer_offset = 0;		// offset for accumulate several log messages to be one llp2p packet
	int log_struct_size = 0;
	int _send_byte;
	log_time_differ();
	
	//debug_printf("-------------- %d \n", buffer_size);
	
	/* Modify sending log message mechanism on 2014/01/09 */
	if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY) {
		//if (log_buffer.size() > MAX_STORED_NUM || (log_time_dffer - previous_time_differ) > TIME_PERIOD) {

			previous_time_differ = log_time_dffer;
			
			if (buffer_size > 0) {
				
				memset(chunk_buffer, 0, CHUNK_BUFFER_SIZE + sizeof(struct chunk_header_t));
				do {
					log_buffer_element_ptr = log_buffer.front();

					log_struct_size = log_buffer_element_ptr->log_header.length + sizeof(struct log_header_t);
					if (chunk_buffer_offset + log_struct_size > CHUNK_BUFFER_SIZE) {
						_log_ptr->write_log_format("s(u) s d(d)\n", __FUNCTION__, __LINE__, 
																	"[WARNING] log_buffer will overflow", 
																	chunk_buffer_offset + log_struct_size,
																	CHUNK_BUFFER_SIZE);
						break;
					}
					
					//debug_printf("%d. buffer_length=%d  length=%d(%d)  cmd=%02x \n", log_buffer.size(),
					//																buffer_size, 
					//																log_buffer_element_ptr->log_header.length, 
					//																sizeof(struct log_header_t),
					//																log_buffer_element_ptr->log_header.cmd);
					
				
					buffer_size -= log_struct_size;

					//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "log_header.cmd =", log_buffer_element_ptr->log_header.cmd);
					
					if (buffer_size < 0) {
						handle_error(LOG_BUFFER_ERROR, "[ERROR] buffer_size < 0", __FUNCTION__, __LINE__);
					}
					if (log_buffer.size() > 100) {
						_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[WARNING] log_buffer.size() is more than 100. buffer_size =", buffer_size);
					}

					memcpy((char *)(chunk_buffer->buf)+chunk_buffer_offset, log_buffer_element_ptr, log_struct_size);
					
					if (log_buffer_element_ptr->log_header.cmd == LOG_DATA_PEER_INFO 	|| 
						log_buffer_element_ptr->log_header.cmd == LOG_DATA_START_DELAY 	||
						log_buffer_element_ptr->log_header.cmd == LOG_DATA_BANDWIDTH 	||
						log_buffer_element_ptr->log_header.cmd == LOG_DATA_SOURCE_DELAY ||
						log_buffer_element_ptr->log_header.cmd == LOG_DATA_TOPOLOGY		||
						log_buffer_element_ptr->log_header.cmd == LOG_TOPO_PEER_JOIN 	||
						log_buffer_element_ptr->log_header.cmd == LOG_TOPO_TEST_SUCCESS ||
						log_buffer_element_ptr->log_header.cmd == LOG_TOPO_RESCUE_TRIGGER ||
						log_buffer_element_ptr->log_header.cmd == LOG_TOPO_PEER_LEAVE) 
					{
						chunk_buffer->header.cmd = CHNK_CMD_LOG;
					} else {
						chunk_buffer->header.cmd = CHNK_CMD_LOG_DEBUG;
					}
					
					chunk_buffer_offset += log_struct_size;

					log_buffer.pop();
					if (log_buffer_element_ptr) {
						delete log_buffer_element_ptr;
					}
					log_buffer_element_ptr = NULL;
					
				} while (false);	//while (log_buffer.size() > 0);
				
				//chunk_buffer->header.cmd = CHNK_CMD_LOG;
				chunk_buffer->header.rsv_1 = REPLY;
				chunk_buffer->header.length = chunk_buffer_offset;
				chunk_buffer->header.sequence_number = 0;

				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = chunk_buffer->header.length + sizeof(chunk_header_t);
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = chunk_buffer->header.length + sizeof(chunk_header_t);
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)chunk_buffer;
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_buffer;
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num =  chunk_buffer->header.sequence_number;

				_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));
		
				
				if (_send_byte <= 0) {
#ifdef _WIN32
					int socketErr = WSAGetLastError();
#else
					int socketErr = errno;
#endif
					exit_code = LOG_SOCKET_ERROR;
					//log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "send info to log server error :", _send_byte, socketErr);
					debug_printf("send info to log server error : %d %d \n", _send_byte, socketErr);
					_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__, "send info to log server error :", _send_byte, socketErr);
					//log_exit();
					//PAUSE
					*(_net_ptr->_errorRestartFlag) = RESTART;
				}
				set_out_message_bw(0, _send_byte, PKT_LOGGER);
			}
		//}
	}
	else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == RUNNING) {
		_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));
			
		if (_send_byte <= 0) {
			exit_code = LOG_SOCKET_ERROR;
#ifdef _WIN32
			int socketErr = WSAGetLastError();
#else
			int socketErr = errno;
#endif
			//log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "send info to log server error :", _send_byte, socketErr);
			debug_printf("send info to log server error : %d %d \n", _send_byte, socketErr);
			_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__, "send info to log server error :", _send_byte, socketErr);
			//log_exit();
			//PAUSE
			*(_net_ptr->_errorRestartFlag) = RESTART;
		}
	}
	
	return RET_OK;
}

void logger_client::handle_pkt_error(int sock)
{
	PAUSE
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_pkt_error");
	log_exit();
}

void logger_client::handle_sock_error(int sock, basic_class *bcptr)
{
	PAUSE
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_sock_error");
	log_exit();
}

void logger_client::handle_job_realtime()
{
	PAUSE
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_job_realtime");
	log_exit();
}

void logger_client::handle_job_timer()
{
	PAUSE
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_job_timer");
	log_exit();
}

void logger_client::handle_error(int err_code, const char *err_msg, const char *func, unsigned int line)
{
	exit_code = err_code;
	debug_printf("\n\nERROR:0x%04x  %s  (%s:%u) \n", err_code, err_msg, func, line);
	_log_ptr->write_log_format("s(u) s  (s:u) \n", func, line, err_msg, func, line);
	log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", func, line, err_msg);
	log_exit();
	//PAUSE
	
	*(_net_ptr->_errorRestartFlag) = RESTART;
}