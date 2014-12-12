/*
對於manifest 的概念 ,他只代表sub-stream ID的表示法
每個 peer 應該顧好自己的 manifest table  確保每個sub -straem 都有來源
*/

#include "pk_mgr.h"
#include "network.h"
#include "network_udp.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "stunt_mgr.h"
#ifdef _FIRE_BREATH_MOD_
//#include "rtsp_viewer.h"
#include "librtsp/rtsp_viewer.h"
#else
#include "librtsp/rtsp_viewer.h"
#endif

/*	Include STUNT-Server file	*/
#include "stunt/ClientMacro_v2.h"
#include "stunt/tcp_punch.h"

//tcp_punch *tcp_punch_ptr = new tcp_punch();		// 這行會造成Release版本不能跑

using namespace std;

pk_mgr::pk_mgr(unsigned long html_size, list<int> *fd_list, network *net_ptr, network_udp *net_udp_ptr , logger *log_ptr , configuration *prep, logger_client * logger_client_ptr, stunt_mgr* stunt_mgr_ptr)
{
#ifdef STUNT_FUNC	
	_stunt_mgr_ptr = stunt_mgr_ptr;
#endif
	_logger_client_ptr = logger_client_ptr;
	_net_ptr = net_ptr;
	_net_udp_ptr = net_udp_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_html_size = html_size;
	fd_list_ptr = fd_list;
	level_msg_ptr = NULL;
	_channel_id = 0;
	lane_member = 0;
	bit_rate = 0;
	sub_stream_num = 0;
	my_public_ip = 0;
	my_public_port = 0;
	my_private_ip = 0;
	my_private_port = 0;
	inside_lane_rescue_num = 0;
	outside_lane_rescue_num = 0;
	_manifest = 0;
	current_child_manifest = 0;
	_least_sequence_number = 0;
	stream_number=1;
	peer_start_delay_count = 0;
	_current_send_sequence_number = 0;
	pkDownInfoPtr =NULL ;
	full_manifest = 0;
	Xcount = 50;
	pkSendCapacity = false;
	ssDetect_ptr =NULL ;
	statsArryCount_ptr =NULL;
	memset(&lastSynStartclock,0x00,sizeof(struct timerStruct));
	totalMod =0;
	reSynTime=BASE_RESYN_TIME;
	_log_ptr->timerGet(&start);
	_log_ptr->timerGet(&reconnect_timer);
	_log_ptr->timerGet(&XcountTimer);
	first_timestamp = 0;
	firstIn =true;
	pkt_count = 0;
	pkt_rate = 50;
	totalbyte=0;
	sentStartDelay = false;
	syncLock=0;
	exit_code = PEER_ALIVE;
	first_legal_pkt_received = false;
	my_session = 1;
	sampling_interval = 10;
	capacity_x = 4;
	capacity_n = 0;
	priq_cnt = 0;
	priq_cnt2 = 0;
	avg_queue = 0;
	avg_queue_previous = 100;		// 初始值故意設很高，用意讓第一次計算得到的 avg_queue < avg_queue_previous

	syn_table.first_sync_done = false;
	syn_table.state = SYNC_UNINIT;
	syn_table.client_abs_start_time = 0;
	syn_table.start_seq = 0;
	
	// Record file
	record_file_fp = NULL;
	first_pkt = true;

	_prep->read_key("bucket_size", _bucket_size);

	buf_chunk_t = NULL;
	buf_chunk_t = (struct chunk_t **)malloc(_bucket_size * sizeof(struct chunk_t **) ) ;
	memset( buf_chunk_t, 0, _bucket_size * sizeof(struct chunk_t **) );

	struct chunk_t* chunk_t_ptr ;

	for (int i = 0; i <_bucket_size; i++) {
		chunk_t_ptr  = (struct chunk_t *)new unsigned char[sizeof(struct chunk_t)];
		if (!chunk_t_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::chunk_t_ptr  new error", __FUNCTION__, __LINE__);
		}
		if (chunk_t_ptr == NULL) {
			debug_printf("memory not enough\n");
		}
		memset(chunk_t_ptr, 0, sizeof(struct chunk_t));
		*(buf_chunk_t + i) = chunk_t_ptr ;
	}
}

pk_mgr::~pk_mgr() 
{

//		if(_chunk_bitstream)
	//		free(_chunk_bitstream);
	struct chunk_t* chunk_t_ptr ;

	//free data memory
	for(int i =0 ; i<_bucket_size ; i++){
		chunk_t_ptr =*(buf_chunk_t + i);
		delete chunk_t_ptr;
	}

	if(buf_chunk_t)			free(buf_chunk_t);
	if(ssDetect_ptr)		free(ssDetect_ptr);
	if(statsArryCount_ptr)	free(statsArryCount_ptr);

	clear_map_pid_parent_temp();
	clear_map_pid_parent();
	clear_map_pid_child1();
	clear_delay_table();
	ClearSubstreamTable();
	clear_map_streamID_header();
	clear_map_pid_child_temp();

	debug_printf("==============deldet pk_mgr success==========\n");
}

//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
void pk_mgr::delay_table_init()
{
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		struct source_delay *source_delay_temp_ptr = NULL;
		source_delay_temp_ptr = new struct source_delay;
		if (!source_delay_temp_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] source_delay_temp_ptr  new error", __FUNCTION__, __LINE__);
		}
		memset(source_delay_temp_ptr, 0, sizeof(struct source_delay)); 

		source_delay_temp_ptr->source_delay_time = 1;
		source_delay_temp_ptr->first_pkt_recv = false;
		source_delay_temp_ptr->end_seq_num = 0;
		source_delay_temp_ptr->end_seq_abs_time = 0;
		source_delay_temp_ptr->rescue_state = 0;
		source_delay_temp_ptr->delay_beyond_count = 0;

		delay_table.insert(pair<unsigned long, struct source_delay *>(i, source_delay_temp_ptr));
		
		_log_ptr->write_log_format("s(u) s d s \n", __FUNCTION__, __LINE__, "source delay substreamID", i, "initialization end");
	}
}
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

void pk_mgr::SubstreamTableInit()
{
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		struct substream_info *substream_info_ptr = new struct substream_info;
		if (!substream_info_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] substream_info_ptr  new error", __FUNCTION__, __LINE__);
		}
		
		memset(substream_info_ptr, 0, sizeof(struct substream_info)); 
		
		substream_info_ptr->first_pkt_received = false;
		substream_info_ptr->timeout_flag = false;
		substream_info_ptr->current_parent_pid = -1;		// Maximum value of UINT32
		substream_info_ptr->previous_parent_pid = -1;		// Maximum value of UINT32
		substream_info_ptr->source_delay_table.source_delay_time = 1;
		substream_info_ptr->source_delay_table.first_pkt_recv = false;
		substream_info_ptr->source_delay_table.end_seq_num = 0;
		substream_info_ptr->source_delay_table.end_seq_abs_time = 0;
		substream_info_ptr->source_delay_table.rescue_state = SS_INIT;
		substream_info_ptr->source_delay_table.delay_beyond_count = 0;
		substream_info_ptr->state.state = SS_INIT;
		substream_info_ptr->state.is_testing = false;
		substream_info_ptr->state.dup_src = false;
		substream_info_ptr->state.connecting_peer = false;
		substream_info_ptr->state.rescue_after_fail = true;
		substream_info_ptr->block_rescue = FALSE;
		substream_info_ptr->can_send_block_rescue = TRUE;
		substream_info_ptr->data.testing_count = 0;
		struct timerStruct timer; 
		_log_ptr->timerGet(&timer);
		substream_info_ptr->timer.timer = timer;
		
		ss_table.insert(pair<unsigned long, struct substream_info *>(i, substream_info_ptr));	
	}
}

void pk_mgr::send_source_delay(int pk_sock)
{
	unsigned int pkt_size = sizeof(struct chunk_header_t) + sizeof(UINT32) + sizeof(double)+ sizeof(UINT32) + (sub_stream_num*sizeof(double));
	struct chunk_period_source_delay_struct *chunk_period_source_delay_ptr = (struct chunk_period_source_delay_struct*)new unsigned char[pkt_size];
	int offset = sizeof(struct chunk_header_t) + sizeof(UINT32) + sizeof(double)+ sizeof(UINT32);
	double max_delay = 0;
	double *average_delay = new double[sub_stream_num];
	double *accumulated_delay = new double[sub_stream_num];
	int *count = new int[sub_stream_num];;
	int send_byte;
	char *send_buf = new char[pkt_size];
	
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		accumulated_delay[i] = _logger_client_ptr->max_source_delay[i].accumulated_delay;
		count[i] = _logger_client_ptr->max_source_delay[i].count;
		
		if (count[i] > 0) {
			average_delay[i] = accumulated_delay[i] / (double)(count[i]);
		}
		// If not receive any packet for a while, we still need to calculate the source delay
		else {
			average_delay[i] = LOG_DELAY_SEND_PERIOD + _logger_client_ptr->delay_list[i];
			_log_ptr->write_log_format("s(u) s d s d s \n", __FUNCTION__, __LINE__, "substream id", i, "has received nothing for", LOG_DELAY_SEND_PERIOD, "milliseconds");
		}
		max_delay = max_delay < average_delay[i] ? average_delay[i] : max_delay;
	}
	
	chunk_period_source_delay_ptr->header.cmd = CHNK_CMD_SRC_DELAY;
	chunk_period_source_delay_ptr->header.rsv_1 = REQUEST;
	chunk_period_source_delay_ptr->header.length = pkt_size - sizeof(struct chunk_header_t);
	chunk_period_source_delay_ptr->pid = my_pid;
	chunk_period_source_delay_ptr->max_delay = max_delay;
	chunk_period_source_delay_ptr->sub_num = sub_stream_num;
	
	memcpy(chunk_period_source_delay_ptr->av_delay, &average_delay[0], sub_stream_num*sizeof(double));
	
	
	memcpy(send_buf, chunk_period_source_delay_ptr, pkt_size);
	
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		debug_printf("To PK: ss %d src_delay %d (%d) \n", i, static_cast<int>(average_delay[i]), count[i]);
		_log_ptr->write_log_format("s(u) s d s f s d \n", __FUNCTION__, __LINE__,
																"To PK: substream", i,
																"source delay =", average_delay[i],
																"count =", count[i]);
	}
	debug_printf("Buffers %d data (= %.3f seconds) \n", _least_sequence_number-_current_send_sequence_number, (double)(_least_sequence_number-_current_send_sequence_number)/(Xcount));
	_log_ptr->write_log_format("s(u) s d s f s \n", __FUNCTION__, __LINE__,
															"The program buffers", _least_sequence_number-_current_send_sequence_number,
															"(=", (double)(_least_sequence_number-_current_send_sequence_number)/(Xcount), "seconds");
	
	//_net_ptr->set_blocking(pk_sock);
	send_byte = _net_ptr->send(pk_sock, send_buf, pkt_size, 0);
	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Send", send_byte, "bytes to pk", pk_sock);
	if (send_byte <= 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		exit_code = PK_SOCKET_ERROR;
		debug_printf("[ERROR] Cannot send sync token to pk \n");
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Cannot send sync token to pk", send_byte, socketErr);
		handle_error(PK_SOCKET_ERROR, "[ERROR] Cannot send sync token to pk", __FUNCTION__, __LINE__);
	}
	else {
		if (chunk_period_source_delay_ptr) {
			delete chunk_period_source_delay_ptr;
			delete average_delay;
			delete accumulated_delay;
			delete count;
			delete send_buf;
		}
		//_net_ptr->set_nonblocking(pk_sock);
	}
}
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

void pk_mgr::send_topology_to_log()
{
	unsigned long *parents_list = new unsigned long[sub_stream_num];
	for (unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
		parents_list[ss_id] = ss_table[ss_id]->current_parent_pid;
	}

	_logger_client_ptr->log_to_server(LOG_DATA_TOPOLOGY, 0, sub_stream_num, parents_list);
	delete parents_list;
}

// 與 list 的 candidates 建立連線後的結果告訴 PK
// 目前設計，有 selected pid 的話也只有一個
// return 選中的 parent number
int pk_mgr::SendParentTestToPK(UINT32 my_session)
{
	map<unsigned long, struct manifest_timmer_flag *>::iterator substream_first_reply_peer_iter;
	int packetlen = 0;
	int parent_num = 1;
	int ret_val = 0;
	struct update_test_info *update_test_info_ptr = NULL;
	map<unsigned long, struct mysession_candidates *>::iterator map_mysession_candidates_iter;
	struct peer_info_t *peer_info = NULL;		// 指向被選中的 parent


	map_mysession_candidates_iter = _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.find(my_session);
	if (map_mysession_candidates_iter == _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.end()){
		handle_error(LOGICAL_ERROR, "[ERROR] Not found in map_mysession_candidates", __FUNCTION__, __LINE__);
		return 0;
	}

	// 找出被選中的 parent
	for (int i = 0; i < map_mysession_candidates_iter->second->candidates_num; i++) {
		if (map_mysession_candidates_iter->second->p_candidates_info[i].connection_state == PEER_SELECTED) {
			UINT32 peer_pid = map_mysession_candidates_iter->second->p_candidates_info[i].pid;
			if (map_pid_parent.find(peer_pid) != map_pid_parent.end()) {
				peer_info = &(map_pid_parent.find(peer_pid)->second->peerInfo);
			}
			break;
		}
		else if (map_mysession_candidates_iter->second->n_candidates_info[i].connection_state == PEER_SELECTED) {
			UINT32 peer_pid = map_mysession_candidates_iter->second->n_candidates_info[i].pid;
			if (map_pid_parent.find(peer_pid) != map_pid_parent.end()) {
				peer_info = &(map_pid_parent.find(peer_pid)->second->peerInfo);
			}
			break;
		}
	}
	
	packetlen = parent_num * sizeof (UINT32)+sizeof(struct update_test_info);
	update_test_info_ptr = (struct update_test_info *)new char[packetlen];
	if (!update_test_info_ptr) {
		handle_error(MALLOC_ERROR, "[ERROR] update_test_info_ptr new error", __FUNCTION__, __LINE__);
	}
	memset(update_test_info_ptr, 0, packetlen);

	update_test_info_ptr->header.cmd = CHNK_CMD_PARENT_TEST_INFO;
	update_test_info_ptr->header.length = (packetlen - sizeof(struct chunk_header_t));	//pkt_buf = payload length
	update_test_info_ptr->header.rsv_1 = REQUEST;
	update_test_info_ptr->pid = my_pid;
	update_test_info_ptr->substream_id = manifestToSubstreamID(map_mysession_candidates_iter->second->manifest);
	update_test_info_ptr->sub_num = sub_stream_num;
	if (peer_info != NULL) {
		update_test_info_ptr->choose_parent_pid = peer_info->pid;
		update_test_info_ptr->test_success_parent_num = parent_num;
		update_test_info_ptr->success_parent[0] = peer_info->pid;
	}
	else {
		update_test_info_ptr->choose_parent_pid = PK_PID;
		update_test_info_ptr->test_success_parent_num = 0;
		update_test_info_ptr->success_parent[0] = PK_PID;
	}
	ret_val = update_test_info_ptr->test_success_parent_num;

	_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u u \n", "pid", my_pid, "PARENT_TEST_INFO session", my_session, update_test_info_ptr->choose_parent_pid, update_test_info_ptr->substream_id);
	debug_printf("Send parent test to PK, parent %d \n", update_test_info_ptr->choose_parent_pid);
	_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "Send parent test to PK. parent", update_test_info_ptr->choose_parent_pid, "ssid", update_test_info_ptr->substream_id, "my_session", my_session);


	_net_ptr->set_blocking(_sock);
	_net_ptr->send(_sock, (char*)update_test_info_ptr, packetlen, 0);
	_net_ptr->set_nonblocking(_sock);

	delete[] update_test_info_ptr;

	return ret_val;




	/*
	map<unsigned long, struct manifest_timmer_flag *>::iterator substream_first_reply_peer_iter;
	int packetlen = 0;
	int parent_num = 1;
	struct update_test_info *update_test_info_ptr = NULL;

	if ((substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(session_id)) == _peer_ptr->substream_first_reply_peer.end()) {
		return ;
	}

	packetlen = parent_num * sizeof (UINT32)+sizeof(struct update_test_info);
	update_test_info_ptr = (struct update_test_info *)new char[packetlen];
	if (!update_test_info_ptr) {
		handle_error(MALLOC_ERROR, "[ERROR] update_test_info_ptr new error", __FUNCTION__, __LINE__);
	}
	memset(update_test_info_ptr, 0, packetlen);

	update_test_info_ptr->header.cmd = CHNK_CMD_PARENT_TEST_INFO;
	update_test_info_ptr->header.length = (packetlen - sizeof(struct chunk_header_t));	//pkt_buf = payload length
	update_test_info_ptr->header.rsv_1 = REQUEST;
	update_test_info_ptr->pid = my_pid;
	update_test_info_ptr->substream_id = manifestToSubstreamID(substream_first_reply_peer_iter->second->rescue_manifest);
	update_test_info_ptr->sub_num = sub_stream_num;
	if (substream_first_reply_peer_iter->second->selected_pid != PK_PID) {
		update_test_info_ptr->choose_parent_pid = substream_first_reply_peer_iter->second->selected_pid;
		update_test_info_ptr->test_success_parent_num = parent_num;
		update_test_info_ptr->success_parent[0] = substream_first_reply_peer_iter->second->selected_pid;
	}
	else {
		update_test_info_ptr->choose_parent_pid = PK_PID;
		update_test_info_ptr->test_success_parent_num = 0;
		update_test_info_ptr->success_parent[0] = PK_PID;
	}

	_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u u \n", "pid", my_pid, "PARENT_TEST_INFO session", session_id, update_test_info_ptr->choose_parent_pid, update_test_info_ptr->substream_id);
	debug_printf("Send parent test to PK, parent %d \n", substream_first_reply_peer_iter->second->selected_pid);
	_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Send parent test to PK", substream_first_reply_peer_iter->second->selected_pid);


	_net_ptr->set_blocking(_sock);
	_net_ptr->send(_sock, (char*)update_test_info_ptr, packetlen, 0);
	_net_ptr->set_nonblocking(_sock);

	delete[] update_test_info_ptr;

	return;
	*/
}

void pk_mgr::quality_source_delay_count(int sock, unsigned long ss_id, unsigned int seq_now)
{
	//_log_ptr->write_log_format("s(u) d d d \n", __FUNCTION__, __LINE__, seq_now, _current_send_sequence_number, syn_table.start_seq);
	//if skip packet or before sync  we will not calculate
	//if (seq_now > syn_table.start_seq && seq_now >= _current_send_sequence_number) {
		UINT32 detect_source_delay_time;
		UINT32 temp;
		INT32 source_delay;
		double delay_quality;
		
		_logger_client_ptr->quality_struct_ptr->total_chunk += 1;

		// Calculate source-delay value
		detect_source_delay_time = _log_ptr->diff_TimerGet_ms(&(syn_table.start_clock), &ss_table[ss_id]->latest_pkt_client_time);
		temp = ss_table[ss_id]->latest_pkt_timestamp - syn_table.client_abs_start_time;
		source_delay = detect_source_delay_time - temp;
		
		if (source_delay < 1) {
			source_delay = 1;
		}

		// Accumulate source-delay
		_logger_client_ptr->set_source_delay(ss_id, (UINT32)source_delay);
		
		// Calculate delay_quality value, and accumulate the values
		if ((double)detect_source_delay_time - MAX_DELAY <= 0) {
			delay_quality = 1;
		}
		else {
			delay_quality = (double)temp/((double)detect_source_delay_time - MAX_DELAY);
			if (delay_quality > 1) {
				delay_quality = 1;
			}
		}
		_logger_client_ptr->quality_struct_ptr->accumulated_quality += delay_quality;
		//_log_ptr->write_log_format("s(u) s d s d s d s f s u s u \n", __FUNCTION__, __LINE__, 
		//												"Set substream id", ss_id,
		//												"seq =", seq_now, 
		//												"source delay =", source_delay,
		//												"delay quality =", delay_quality,
		//												"T3-T0 =", detect_source_delay_time,
		//												"T2-T1 =", temp);
	//}
}

// Detect each chunk
// It must check the first sync is exactly done, then call this function
void pk_mgr::SourceDelayDetection(int sock, unsigned long ss_id, unsigned int seq_now)
{
	// Filtering
	if (ss_table.find(ss_id) == ss_table.end()) {
		handle_error(MACCESS_ERROR, "[ERROR] not found substreamID in delay_table", __FUNCTION__, __LINE__);
	}
	/*
	if (seq_now <= ss_table[ss_id]->latest_pkt_seq) {
		return ;
	}
	*/
	
	ss_table[ss_id]->latest_pkt_seq = seq_now ;
	
	UINT32 detect_source_delay_time;
	UINT32 temp;
	INT32 source_delay;		// This is source delay time
	//map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	//map<int, unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_out_pid_fd_iter;
	struct peer_connect_down_t *parent_info = NULL;
	unsigned long detect_peer_id;
	unsigned long testingManifest = 0;
	unsigned long tempManifest = 0;
	unsigned long peerTestingManifest = 0;

	for (map<unsigned long, struct peer_connect_down_t *>::iterator iter = map_pid_parent.begin(); iter != map_pid_parent.end(); iter++) {
		if (sock == iter->second->peerInfo.sock) {
			//pid_peerDown_info_iter = iter;
			parent_info = iter->second;
		}
	}
	if (parent_info == NULL) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] can not find map_pid_parent", sock);
		handle_error(MACCESS_ERROR, "[ERROR] not found socket in map_pid_parent", __FUNCTION__, __LINE__);
		return;
	}

	/*
	if (_peer_ptr->map_fd_pid.find(sock) != _peer_ptr->map_fd_pid.end()) {
		if (map_pid_parent.find(_peer_ptr->map_fd_pid[sock]) == map_pid_parent.end()) {
			handle_error(MACCESS_ERROR, "[ERROR] not found pid in map_pid_parent", __FUNCTION__, __LINE__);
		}
		parent_info = map_pid_parent[_peer_ptr->map_fd_pid[sock]];
	}
	else if (_peer_ptr->map_udpfd_pid.find(sock) != _peer_ptr->map_udpfd_pid.end()) {
		if (map_pid_parent.find(_peer_ptr->map_udpfd_pid[sock]) == map_pid_parent.end()) {
			handle_error(MACCESS_ERROR, "[ERROR] not found pid in map_pid_parent", __FUNCTION__, __LINE__);
		}
		parent_info = map_pid_parent[_peer_ptr->map_udpfd_pid[sock]];
	}
	else {
		handle_error(MACCESS_ERROR, "[ERROR] not found socket in map_fd_pid", __FUNCTION__, __LINE__);
	}
	*/

	/*
	map_fd_pid_iter = _peer_ptr->map_fd_pid.find(sock);
	if (map_fd_pid_iter == _peer_ptr->map_fd_pid.end()) {
		handle_error(MACCESS_ERROR, "[ERROR] not found socket in map_fd_pid", __FUNCTION__, __LINE__);
	}

	pid_peerDown_info_iter = map_pid_parent.find(map_fd_pid_iter->second);
	if (pid_peerDown_info_iter == map_pid_parent.end()) {
		handle_error(MACCESS_ERROR, "[ERROR] not found pid in map_pid_parent", __FUNCTION__, __LINE__);
	}
	parent_info = pid_peerDown_info_iter->second;
	*/
	detect_source_delay_time = _log_ptr->diff_TimerGet_ms(&syn_table.start_clock, &ss_table[ss_id]->latest_pkt_client_time);
	temp = ss_table[ss_id]->latest_pkt_timestamp - syn_table.client_abs_start_time;
	source_delay = detect_source_delay_time - temp;
	ss_table[ss_id]->source_delay_table.source_delay_time = source_delay < 1 ? 1 : source_delay;
	
	//_log_ptr->write_log_format("s(u) s d s d s d s u s u \n", __FUNCTION__, __LINE__, 
	//													"Set substream id", ss_id,
	//													"seq", seq_now, 
	//													"source delay", source_delay,
	//													"T3-T0", detect_source_delay_time,
	//													"T2-T1", temp);
	
	if (source_delay < 1) {
		if ((int)totalMod >= syn_round_time) {
			//should re_syn
			if (reSynTime > 5000) {
				reSynTime = reSynTime / 2;
				if (reSynTime < 5000) {
					reSynTime = 5000;
				}
				debug_printf("reSynTime Bad change to reSynTime/2  = %d \n",reSynTime);
			}
			else {
				reSynTime = 5000;
				debug_printf("reSynTime Bad change to reSynTime/2  = %d \n",reSynTime);
				//doing nothing it small bound if  reSynTime==10
			}
			//send_syn_token_to_pk(_sock);
		}
		else {
			UINT32 temp = syn_table.client_abs_start_time;
			UINT32 temp2 = totalMod;
			syn_table.client_abs_start_time += abs(source_delay);
			totalMod += abs(source_delay);
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "client_abs_start_time changed from", temp, "to", syn_table.client_abs_start_time);
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "totalMod changed from", temp2, "to", totalMod);
		}
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s u s d s d \n", __FUNCTION__, __LINE__,
																					"[DEBUG] totalMod =", totalMod,
																					"source_delay =", source_delay, 
																					"syn_round_time =", syn_round_time);
		_log_ptr->write_log_format("s(u) s u s d s d \n", __FUNCTION__, __LINE__, __FUNCTION__, __LINE__,
																					"[DEBUG] totalMod =", totalMod,
																					"source_delay =", source_delay, 
																					"syn_round_time =", syn_round_time);																		
		ss_table[ss_id]->source_delay_table.delay_beyond_count = 0;
		
	}
	else if (source_delay > MAX_DELAY) {
		ss_table[ss_id]->source_delay_table.delay_beyond_count++;
		
		//觸發條件(source_delay > MAX_DELAY 的次數上限)
		int source_delay_max_times = (SOURCE_DELAY_CONTINUOUS*Xcount) / sub_stream_num;		// SOURCE_DELAY_CONTINUOUS*一秒中收到的封包數/sub-stream數目
		
		// 在觸發 rescue 的前一秒送 blocl_rescue 給相關的 child-peers
		int threshold = (SOURCE_DELAY_CONTINUOUS - 1) > 0 ? ((SOURCE_DELAY_CONTINUOUS-1)*Xcount) / sub_stream_num : 1;
		if (ss_table[ss_id]->source_delay_table.delay_beyond_count > threshold && ss_table[ss_id]->can_send_block_rescue == TRUE) {
			if (ss_table[ss_id]->block_rescue == FALSE) {
				_peer_mgr_ptr->SendBlockRescue(ss_id, 4);
				_log_ptr->timerGet(&ss_table[ss_id]->block_rescue_interval);
				ss_table[ss_id]->can_send_block_rescue = FALSE;
			}
		}

		if (ss_table[ss_id]->source_delay_table.delay_beyond_count > source_delay_max_times) {
		
			//ss_table[ss_id]->source_delay_table.delay_beyond_count = 0;
			
			// If this substream is from PK, we have no idea to rescue it. Otherwise, find this substream's
			// parent, and cut the testing substream from it(if have testing substream from it). If cannot find 
			// testing substream, cut other normal substream
			if (parent_info->peerInfo.pid == PK_PID) {
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s \n", "pid", my_pid, "[RESCUE_TYPE] 4 Parent is PK. Cannot rescue");
				debug_printf("[RESCUE_TYPE] 4. Parent is PK. Cannot rescue \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 4. Parent is PK. Cannot rescue");
				// Parent 是 PK 的情況下不重置，避免 Timeout Detection 無法偵測，只重置這次相關的變數
				//ResetDetection(ss_id);
				ss_table[ss_id]->source_delay_table.delay_beyond_count = 0;
			}
			else {
				if (ss_table[ss_id]->block_rescue == FALSE) {
					if (ss_table[ss_id]->state.state == SS_STABLE2) {
						if (ss_table[ss_id]->state.rescue_after_fail == true) {
							// RESCUE or MERGE
							UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
							unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
							unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
							unsigned long pk_new_manifest = pk_old_manifest | manifest;
							unsigned long parent_new_manifest = parent_old_manifest & ~manifest;
							bool need_source = true;

							SetSubstreamState(ss_id, SS_RESCUE2);
							debug_printf("[RESCUE_TYPE] 4. Source delay %d > %d \n", source_delay, MAX_DELAY);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_DELAY_RESCUE_TRIGGER, SubstreamIDToManifest(ss_id));
							_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 4 ", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);
							_log_ptr->write_log_format("s(u) s u s u s d s d \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 4. Rescue in STABLE state. substreamID =", ss_id,
								"parentID=", parent_info->peerInfo.pid,
								"delay_beyond_count", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);

							ss_table[ss_id]->data.previousParentPID = parent_info->peerInfo.pid;
							ss_table[ss_id]->state.is_testing = false;

							// Update manifest
							set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
							set_parent_manifest(parent_info, parent_new_manifest);
							SetSubstreamParent(manifest, PK_PID);
							_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);
							if (parent_info->peerInfo.manifest == 0) {
								_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[REASON] manifest = 0 parent", parent_info->peerInfo.pid);
								_peer_ptr->CloseParent(parent_info->peerInfo.pid, false, "[REASON] manifest = 0");
								_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[CHECK POINT] map_pid_parent.size()", map_pid_parent.size());
							}

							//send_parentToPK(manifest, PK_PID+1);
							NeedSourceDecision(&need_source);
							//send_rescueManifestToPK(pk_new_manifest, need_source);
							send_rescueManifestToPK(manifest, need_source);
							ss_table[ss_id]->state.dup_src = need_source;

							// Reset source delay parameters of testing substream of this parent
							ResetDetection(ss_id);
						}
						else {
							// MOVE
							UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
							
							SetSubstreamState(ss_id, SS_STABLE2);
							debug_printf("[RESCUE_TYPE] 4M. Source delay %d > %d \n", source_delay, MAX_DELAY);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 4M.", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);
							_log_ptr->write_log_format("s(u) s u s u s d s d \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 4M. Rescue in STABLE state. substreamID =", ss_id,
								"parentID=", parent_info->peerInfo.pid,
								"delay_beyond_count", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);
							ss_table[ss_id]->state.rescue_after_fail = true;
						}
					}
					else if (ss_table[ss_id]->state.state == SS_CONNECTING2) {
						// No more trigger rescue again
						debug_printf("Should not happen \n");
					}
					else if (ss_table[ss_id]->state.state == SS_RESCUE2) {
						// No more trigger rescue again
						debug_printf("Should not happen \n");
					}
					else if (ss_table[ss_id]->state.state == SS_TEST2) {
						if (ss_table[ss_id]->state.rescue_after_fail == true) {
							// RESCUE or MERGE
							UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
							unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
							unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
							unsigned long pk_new_manifest = pk_old_manifest | manifest;
							unsigned long parent_new_manifest = parent_old_manifest & ~manifest;
							bool need_source = true;

							SetSubstreamState(ss_id, SS_RESCUE2);
							debug_printf("[RESCUE_TYPE] 4. Source delay %d > %d \n", source_delay, MAX_DELAY);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL, SubstreamIDToManifest(ss_id), PK_PID);
							_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 4 ", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);
							_log_ptr->write_log_format("s(u) s u s u s d s d \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 4. Rescue in TEST state. substreamID =", ss_id,
								"parent =", parent_info->peerInfo.pid,
								"delay_beyond_count", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);

							ss_table[ss_id]->state.is_testing = false;

							// Update manifest
							set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
							set_parent_manifest(parent_info, parent_new_manifest);
							SetSubstreamParent(manifest, PK_PID);
							_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);
							if (parent_info->peerInfo.manifest == 0) {
								_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[REASON] manifest = 0 parent", parent_info->peerInfo.pid);
								_peer_ptr->CloseParent(parent_info->peerInfo.pid, false, "[REASON] manifest = 0");
								_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[CHECK POINT] map_pid_parent.size()", map_pid_parent.size());
							}

							//send_parentToPK(manifest, PK_PID+1);
							NeedSourceDecision(&need_source);
							//send_rescueManifestToPK(pk_new_manifest, need_source);
							send_rescueManifestToPK(manifest, need_source);
							ss_table[ss_id]->state.dup_src = need_source;

							// Reset source delay parameters of testing substream of this parent
							ResetDetection(ss_id);
						}
						else {
							// MOVE
							UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream

							SetSubstreamState(ss_id, SS_TEST2);
							debug_printf("[RESCUE_TYPE] 4M. Source delay %d > %d \n", source_delay, MAX_DELAY);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 4M.", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);
							_log_ptr->write_log_format("s(u) s u s u s d s d \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 4M. Rescue in TEST state. substreamID =", ss_id,
								"parent =", parent_info->peerInfo.pid,
								"delay_beyond_count", ss_table[ss_id]->source_delay_table.delay_beyond_count,
								">", source_delay_max_times);
							ss_table[ss_id]->state.rescue_after_fail = true;
						}
					}
					else {
						// Unexpected state
					}
				}
				else {
					UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
					debug_printf("Block Rescue [RESCUE_TYPE] 4. Source delay %d > %d \n", source_delay, MAX_DELAY);
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "Block Rescue [RESCUE_TYPE] 4 ", ss_table[ss_id]->source_delay_table.delay_beyond_count,
						">", source_delay_max_times);
					_log_ptr->write_log_format("s(u) s u s u s d s d \n", __FUNCTION__, __LINE__,
						"Block Rescue [RESCUE_TYPE] 4. Rescue in TEST state. substreamID =", ss_id,
						"parent =", parent_info->peerInfo.pid,
						"delay_beyond_count", ss_table[ss_id]->source_delay_table.delay_beyond_count,
						">", source_delay_max_times);
					ResetDetection(ss_id);
					ss_table[ss_id]->state.rescue_after_fail = true;
				}
			}
		}
	}
	else {
		//_logger_client_ptr->set_source_delay(sub_id,source_delay);
		ss_table[ss_id]->source_delay_table.delay_beyond_count = 0;
	}
}

// 以下情況會送 Capacity
// 1. 收到 PEER_REG (JOIN) 時
// 2. Parent 收到 PEER_SET_MANIFEST 時
// 3. Parent 偵測到 Child 離開時
void pk_mgr::SendCapacityToPK(void)
{
	int packetlen = 0;
	struct rescue_peer_capacity_measurement *chunk_capacity_ptr = NULL;

	packetlen = sizeof(struct rescue_peer_capacity_measurement);
	chunk_capacity_ptr = (struct rescue_peer_capacity_measurement *)new char[packetlen];
	if (!chunk_capacity_ptr) {
		handle_error(MALLOC_ERROR, "[ERROR] update_test_info_ptr new error", __FUNCTION__, __LINE__);
	}
	memset(chunk_capacity_ptr, 0, packetlen);

	chunk_capacity_ptr->header.cmd = CHNK_CMD_PEER_RESCUE_CAPACITY;
	chunk_capacity_ptr->header.rsv_1 = REPLY;
	chunk_capacity_ptr->header.length = packetlen - sizeof(struct chunk_header_t);
	chunk_capacity_ptr->rescue_num = rescueNumAccumulate();
	chunk_capacity_ptr->content_integrity = 1;

	_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u  \n", "pid", my_pid, "capacity", chunk_capacity_ptr->rescue_num, map_pid_child.size());
	_log_ptr->write_log_format("s(u) s u u \n", __FUNCTION__, __LINE__, "Send capacity to PK", chunk_capacity_ptr->rescue_num, map_pid_child.size());
	debug_printf("Send capacity to PK %d %d \n", chunk_capacity_ptr->rescue_num, children_table.size());

	_net_ptr->set_blocking(_sock);
	_net_ptr->send(_sock, (char*)chunk_capacity_ptr, packetlen, 0);
	_net_ptr->set_nonblocking(_sock);

	delete[] chunk_capacity_ptr;

	return;
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

void pk_mgr::init(unsigned short ptop_port, unsigned short public_udp_port, unsigned short private_udp_port)
{
	
	string pk_ip("");
	string pk_port("");
	string svc_tcp_port("");
	string svc_udp_port("");
	
	_prep->read_key("pk_ip", pk_ip);
	_prep->read_key("pk_port", pk_port);
	_prep->read_key("channel_id", _channel_id);
	//_prep->read_key("svc_tcp_port", svc_tcp_port);	//svc_tcp_port is replaced by ptop_port
	//_prep->read_key("svc_udp_port", svc_udp_port);	//svc_tcp_port is replaced by ptop_udp_port

	pkDownInfoPtr = new struct peer_connect_down_t ;
	if (!pkDownInfoPtr) {
		handle_error(MALLOC_ERROR, "[ERROR] pkDownInfoPtr new error", __FUNCTION__, __LINE__);
	}
	memset(pkDownInfoPtr, 0, sizeof(struct peer_connect_down_t));
	
	// set PK-server as one of peer whose PID=999999
	pkDownInfoPtr->peerInfo.pid =PK_PID;
	pkDownInfoPtr->peerInfo.public_ip = inet_addr(pk_ip.c_str());
	//pkDownInfoPtr->peerInfo.tcp_port = htons((unsigned short)atoi(pk_port.c_str()));
	pkDownInfoPtr->peerInfo.tcp_port = htons(ptop_port);

	map_pid_parent[PK_PID] = pkDownInfoPtr;
	
	//web_ctrl_sever_ptr = new web_ctrl_sever(_net_ptr, _log_ptr, fd_list_ptr, &map_stream_name_id); 
	//web_ctrl_sever_ptr->init();

	if ((_sock = build_connection(pk_ip, pk_port)) < 0) {
		handle_error(PK_BUILD_ERROR, "[ERROR] Fail to build connection with pk", __FUNCTION__, __LINE__);
		
		return ;
	}
	debug_printf("Succeed to build connection with pk \n");
	pkDownInfoPtr->peerInfo.sock = _sock;
	
	// Get internal IP address by the connection established with pk
	struct sockaddr_in src_addr;
	int addrLen = sizeof(struct sockaddr_in);
	
	if (getsockname(_sock, (struct sockaddr *)&src_addr, (socklen_t *)&addrLen) == 0) {
		memcpy(&my_private_ip, &src_addr.sin_addr, sizeof(src_addr.sin_addr));
	}
	else {
		my_private_ip = _net_ptr->getLocalIpv4();
	}
	my_private_port = ptop_port;
	debug_printf("private IP: %s:%d \n", inet_ntoa(*(struct in_addr*)&my_private_ip), my_private_port);
			
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "PK_sock =", _sock);

	queue<struct chunk_t*> *queue_out_ctrl_ptr;
	queue<struct chunk_t*> *queue_out_data_ptr;

	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	_peer_ptr->map_in_pid_fd[PK_PID] = _sock;
	_peer_ptr->map_fd_pid[_sock] = PK_PID;
	_peer_ptr->map_fd_out_ctrl[_sock] = queue_out_ctrl_ptr;
	_peer_ptr->map_fd_out_data[_sock] = queue_out_data_ptr;
	
	_net_ptr->pk_fd = _sock;

	Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff ;
	if(!(Nonblocking_Buff_ptr ) || !(queue_out_ctrl_ptr)  || !(queue_out_data_ptr)){
		handle_error(MALLOC_ERROR, "[ERROR] pkDownInfoPtr new error", __FUNCTION__, __LINE__);
	}
	memset(Nonblocking_Buff_ptr, 0x0 , sizeof(Nonblocking_Buff));
	_peer_ptr ->map_fd_nonblocking_ctl[_sock] = Nonblocking_Buff_ptr ;

	if (handle_register(ptop_port, public_udp_port, private_udp_port) < 0) {
		handle_error(PK_BUILD_ERROR, "[ERROR] Fail to register to pk", __FUNCTION__, __LINE__);
		return ;
	}
	
	debug_printf("pk_mgr handle_ register() success \n");
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Register PK complete");

	_net_ptr->set_nonblocking(_sock);	// set to non-blocking
	_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(_sock);
}

// build_connection to (string ip , string port), if failure return -1, else return socket fd
int pk_mgr::build_connection(string ip, string port)
{
	int sock;
	int retVal;
	struct sockaddr_in pk_saddr;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
#ifdef _WIN32
		exit_code = PK_BUILD_ERROR;
		int socketErr = WSAGetLastError();
		debug_printf("[ERROR] Create socket failed %d %d \n", sock, socketErr);
#endif
		return -1;
	}

	memset(&pk_saddr, 0, sizeof(struct sockaddr_in));
	pk_saddr.sin_addr.s_addr = inet_addr(ip.c_str());
	pk_saddr.sin_port = htons((unsigned short)atoi(port.c_str()));
	pk_saddr.sin_family = AF_INET;

	if ((retVal = connect(sock, (struct sockaddr*)&pk_saddr, sizeof(pk_saddr))) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
		if (socketErr == WSAEWOULDBLOCK) {
		
		}
		else {
			exit_code = PK_BUILD_ERROR;
			debug_printf("[ERROR] Building PK connection failed %d %d \n", retVal, socketErr);
			::closesocket(sock);
			return -1;
		}
#else
#endif	
	}
	return sock;

}

//Follow light protocol spec send register message to pk,  HTTP | light | content(request_info_t)
//This function include send() function ,send  register packet to PK Server
int pk_mgr::handle_register(unsigned short ptop_port, unsigned short public_udp_port, unsigned short private_udp_port)
{
	struct chunk_t *chunk_ptr = NULL;
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	int send_byte;
	char *crlf_ptr = NULL;		 // it need to point to   -> \r\n\r\n
	char html_buf[8192];
	unsigned long html_hdr_size; // HTTP protocol  len
	unsigned long buf_len=0;		 // HTTP protocol  len + HTTP content len

	chunk_request_ptr = (struct chunk_request_msg_t *)new unsigned char[sizeof(struct chunk_request_msg_t)];
	if(!(chunk_request_ptr ) ){
		handle_error(MALLOC_ERROR, "[ERROR] chunk_request_ptr new error", __FUNCTION__, __LINE__);
	}
	memset(html_buf, 0x0, _html_size);
	memset(chunk_request_ptr, 0x0, sizeof(struct chunk_request_msg_t));


	strcat(html_buf, "GET / HTTP/1.1\r\nAccept: */*\r\n");
	strcat(html_buf, "User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n\r\n");

	chunk_request_ptr->header.cmd = CHNK_CMD_PEER_REG;
	chunk_request_ptr->header.rsv_1 = REQUEST;
	chunk_request_ptr->header.length = sizeof(struct request_info_t);
	chunk_request_ptr->info.pid = 0;
	chunk_request_ptr->info.channel_id = _channel_id;
	//chunk_request_ptr->info.private_ip = _net_ptr->getLocalIpv4();
	chunk_request_ptr->info.private_ip = my_private_ip;
	chunk_request_ptr->info.tcp_port = ptop_port;
	chunk_request_ptr->info.public_udp_port = public_udp_port;
	chunk_request_ptr->info.private_udp_port = private_udp_port;

	if((crlf_ptr = strstr(html_buf, "\r\n\r\n")) != NULL) {
		crlf_ptr += CRLF_LEN;	
		html_hdr_size = crlf_ptr - html_buf;
		debug_printf("html_hdr_size = %d \n", html_hdr_size);
	} 

	memcpy(html_buf + html_hdr_size, chunk_request_ptr, sizeof(struct chunk_request_msg_t));

	buf_len = html_hdr_size + sizeof(struct chunk_request_msg_t);

	send_byte = _net_ptr->send(_sock, html_buf, buf_len, 0);

	if (chunk_request_ptr) {
		delete chunk_request_ptr;
	}

	if (send_byte <= 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		exit_code = PK_SOCKET_ERROR;
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] send html_buf error", socketErr);
		data_close(_sock, "send html_buf error");
		_log_ptr->exit(0, "send html_buf error");
		return -1;
	}
	else {		//success
		return 1;
	}
}

// CHNK_CMD_PEER_REG
// CHNK_CMD_PEER_SYN
// CHNK_CMD_PEER_RESCUE_LIST
// CHNK_CMD_PEER_DATA
// CHNK_CMD_PEER_SEED
// CHNK_CMD_CHN_UPDATE_DATA
// CHNK_CMD_PARENT_PEER
// CHNK_CMD_CHN_STOP
int pk_mgr::handle_pkt_in(int sock)
{
	unsigned long i;
	//unsigned long buf_len;
	//unsigned long level_msg_size;
	//int recv_byte;
	//int expect_len = 0;
	//int offset = 0;
	int ret = -1;
	unsigned long total_bit_rate = 0;
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, unsigned long>::iterator map_pid_manifest_iter;
	list<int>::iterator outside_rescue_list_iter;

	int offset = 0;
	int recv_byte = 0;
	Nonblocking_Ctl * Nonblocking_Recv_Ctl_ptr =NULL;

	struct chunk_header_t* chunk_header_ptr = NULL;
	struct chunk_t* chunk_ptr = NULL;
	unsigned long buf_len = 0;


	//struct chunk_t *chunk_ptr = NULL;
	//struct chunk_header_t *chunk_header_ptr = NULL;
	//struct peer_info_t *new_peer = NULL;
	//struct peer_info_t *child_peer = NULL;

	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;

	map_fd_nonblocking_ctl_iter = _peer_ptr->map_fd_nonblocking_ctl.find(sock);
	if (map_fd_nonblocking_ctl_iter != _peer_ptr->map_fd_nonblocking_ctl.end()) {
		Nonblocking_Recv_Ctl_ptr = &(map_fd_nonblocking_ctl_iter->second->nonBlockingRecv);
		if(Nonblocking_Recv_Ctl_ptr ->recv_packet_state == 0){
			Nonblocking_Recv_Ctl_ptr ->recv_packet_state =READ_HEADER_READY ;
		}
	}
	else {
		handle_error(MACCESS_ERROR, "[ERROR] PK's socket not found in Nonblocking_Recv_Ctl_ptr", __FUNCTION__, __LINE__);
	}

	for (int i = 0; i < 3; i++) {
		//debug_printf("Nonblocking_Recv_Ctl_ptr->recv_packet_state = %d \n", Nonblocking_Recv_Ctl_ptr->recv_packet_state);

		if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY) {
	
			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			if(!(chunk_header_ptr ) ){
				handle_error(MALLOC_ERROR, "[ERROR] chunk_header_ptr new error", __FUNCTION__, __LINE__);
			}
			memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =0 ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = sizeof(chunk_header_t) ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = sizeof(chunk_header_t) ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_header_ptr ;

		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_RUNNING) {
			//do nothing
		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_OK) {
			buf_len = sizeof(chunk_header_t)+ ((chunk_t *)(Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)) ->header.length ;
			
			chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];
			if (!chunk_ptr) {
				handle_error(MALLOC_ERROR, "[ERROR] chunk_ptr new error", __FUNCTION__, __LINE__);
			}
			//printf("buf_len %d \n",buf_len);

			memset(chunk_ptr, 0, buf_len);
			memcpy(chunk_ptr, Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer, sizeof(chunk_header_t));

			if (Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer) {
				delete [] (unsigned char*)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer ;
			}
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset = sizeof(chunk_header_t) ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length ;
			Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;

			//printf("chunk_ptr->header.length = %d  seq = %d\n",chunk_ptr->header.length,chunk_ptr->header.sequence_number);
			Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_PAYLOAD_READY ;
		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY) {
			//do nothing
		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING) {
			//do nothing
		}
		else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK) {
			break;
		}
		
		
		recv_byte = _net_ptr->nonblock_recv(sock,Nonblocking_Recv_Ctl_ptr);
		
		if (recv_byte < 0) {
			// Although return from network::nonblock_recv() is different, but the results are the same, which returns "RET_SOCK_ERROR"
			if (recv_byte == RET_SOCK_CLOSED_GRACEFUL) {
				
				exit_code = PK_SOCKET_CLOSED;
				
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] socket has been gracefully closed by PK");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d u \n", "[ERROR] socket has been gracefully closed by PK");
				debug_printf("recv_packet_state = %d \n", Nonblocking_Recv_Ctl_ptr->recv_packet_state);
				debug_printf("[ERROR] socket has been gracefully closed by PK \n");
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "recv_packet_state =", Nonblocking_Recv_Ctl_ptr->recv_packet_state);
				_logger_client_ptr->log_exit();
				data_close(_sock, "[ERROR] socket has been gracefully closed by PK");
				
				return RET_SOCK_ERROR;
			}
			else {
#ifdef _WIN32
				int socketErr = WSAGetLastError();
#else
				int socketErr = errno;
#endif
				exit_code = PK_SOCKET_ERROR;
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d u \n", "recv_packet_state =", Nonblocking_Recv_Ctl_ptr->recv_packet_state, __LINE__);
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d u \n", "[ERROR] error occurred in PKMGR recv", socketErr, __LINE__);
				debug_printf("recv_packet_state = %d \n", Nonblocking_Recv_Ctl_ptr->recv_packet_state);
				debug_printf("[ERROR] error occured in PKMGR recv %d \n", socketErr);
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "recv_packet_state =", Nonblocking_Recv_Ctl_ptr->recv_packet_state);
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] error occurred in PKMGR recv", socketErr);
				_logger_client_ptr->log_exit();
				data_close(_sock, "[ERROR] error occured in PKMGR recv");
				//PAUSE
				return RET_SOCK_ERROR;
			}
		}
	}


	if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK) {
			
		chunk_ptr = (chunk_t *)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer;

		Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY;

		buf_len = sizeof(struct chunk_header_t) + chunk_ptr->header.length;
		debug_printf2("Receive packets successfully. buf_len = %d, chunk_ptr->header.length = %d, (%d) \n", buf_len, chunk_ptr->header.length, chunk_ptr->header.cmd);
	}
	else{
		//other stats
		return RET_OK;
	}

	//handle CHNK_CMD_PEER_REG, expect recv  chunk_register_reply_t    from  PK
	//ligh |  pid |  level |   bit_rate|   sub_stream_num |  public_ip |  inside_lane_rescue_num | n*struct level_info_t
	//這邊應該包含整條lane 的peer_info 包含自己
	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_REG) {
		debug_printf("===================CHNK_CMD_PEER_REG=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_REG");

		unsigned long level_msg_size;
		int offset = 0;
		UINT32 peercomm_session = 0;
		struct chunk_register_reply_t *chunk_register_reply = NULL;

		chunk_register_reply = (struct chunk_register_reply_t *)chunk_ptr;
		lane_member = (buf_len - sizeof(struct chunk_header_t) - 9*sizeof(UINT32)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + 2*sizeof(UINT32) + lane_member*sizeof(struct level_info_t *);
		peercomm_session = chunk_register_reply->session;

		// Set up my information
		my_pid = chunk_register_reply->pid;
		my_level = chunk_register_reply->level;
		bit_rate = chunk_register_reply->bit_rate;
		sub_stream_num = chunk_register_reply->sub_stream_num;
		my_public_ip = chunk_register_reply->public_ip;
		inside_lane_rescue_num = chunk_register_reply->inside_lane_rescue_num;
		pkt_rate = chunk_register_reply->pkt_rate;
		sampling_interval = pkt_rate / 5;
		capacity_x = chunk_register_reply->sub_stream_num;
		peercomm_session = chunk_register_reply->session;

		debug_printf("my_pid = %lu \n", my_pid);
		debug_printf("my_level = %lu \n", my_level);
		debug_printf("bit_rate = %lu \n", bit_rate);
		debug_printf("sub_stream_num = %lu \n", sub_stream_num);
		debug_printf("my_public_ip = %s \n", inet_ntoa(*(struct in_addr *)&my_public_ip));
		debug_printf("my_private_ip = %s \n", inet_ntoa(*(struct in_addr *)&my_private_ip));
		debug_printf("inside_lane_rescue_num = %lu \n", inside_lane_rescue_num);
		debug_printf("is_seed = %lu \n", chunk_register_reply->is_seed);
		debug_printf("pkt_rate = %lu \n", pkt_rate);
		_log_ptr->write_log_format("s(u) s u s u s u s u s s s u s u s u s u s u \n", __FUNCTION__, __LINE__,
			"my_pid", my_pid,
			"my_level", my_level,
			"bit_rate", bit_rate,
			"ss_num", sub_stream_num,
			"my_public_ip", inet_ntoa(*(struct in_addr *)&my_public_ip),
			"inside_lane_rescue_num", inside_lane_rescue_num,
			"lane_member", lane_member,
			"is_seed", chunk_register_reply->is_seed,
			"pkt_rate", pkt_rate,
			"peercomm_session", peercomm_session);


		_peer_mgr_ptr->set_up_public_ip(my_public_ip);
		_peer_mgr_ptr->_peer_communication_ptr->set_self_info(my_public_ip);

		//收到sub_stream_num後對rescue 偵測結構做初始化
		init_rescue_detection();
		delay_table_init();
		SubstreamTableInit();
		syn_table_init(_sock);
		_logger_client_ptr->source_delay_struct_init(sub_stream_num);

		for (unsigned long i = 0; i < sub_stream_num; i++) {
			full_manifest |= (1 << i);
		}

		struct chunk_level_msg_t *level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		if (!level_msg_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::level_msg_ptr new error", __FUNCTION__, __LINE__);
		}
		memset(level_msg_ptr, 0, level_msg_size);
		memcpy(level_msg_ptr, chunk_register_reply, sizeof(struct chunk_header_t));
		level_msg_ptr->pid = my_pid;
		level_msg_ptr->manifest = full_manifest;	

		offset += (sizeof(struct chunk_header_t) + 9 * sizeof(UINT32));
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "lane_member ", lane_member);


		// Set PK's manifest
		chunk_register_reply->is_seed == 1 ? set_parent_manifest(pkDownInfoPtr, full_manifest) : set_parent_manifest(pkDownInfoPtr, 0);
		
		_logger_client_ptr->set_self_ip_port(my_public_ip, my_public_port, my_private_ip, my_private_port);
		_logger_client_ptr->set_self_pid_channel(my_pid, _channel_id);
		_logger_client_ptr->log_to_server(LOG_REGISTER, full_manifest);
		_logger_client_ptr->log_to_server(LOG_DATA_PEER_INFO, full_manifest);
		//_logger_client_ptr->log_to_server(LOG_TOPO_PEER_JOIN, full_manifest, PK_PID);
		_peer_mgr_ptr->self_pid = my_pid;

		///////////////////Set pointer and register to STUNT-Server////////////////
		//_stunt_mgr_ptr->_logger_client_ptr = _logger_client_ptr;
		//_stunt_mgr_ptr->_net_ptr = _net_ptr;
		//_stunt_mgr_ptr->_log_ptr = _log_ptr;
		//_stunt_mgr_ptr->_prep_ptr = _prep;
		//_stunt_mgr_ptr->_peer_mgr_ptr = _peer_mgr_ptr;
		//_stunt_mgr_ptr->_peer_ptr = _peer_ptr;
		//_stunt_mgr_ptr->_pk_mgr_ptr = this;
		//_stunt_mgr_ptr->_peer_communication_ptr = _peer_mgr_ptr->_peer_communication_ptr;
		//_peer_mgr_ptr->_peer_communication_ptr->_stunt_mgr_ptr = _stunt_mgr_ptr;

		// Register to STUNT-SERVER
		//int nRet;
		//char myPID[10] = {0};
		//itoa(level_msg_ptr->pid, myPID, 10);
		//nRet = _stunt_mgr_ptr->init(level_msg_ptr->pid);

		//if (nRet == ERR_NONE) {
		//	debug_printf("nRet: %d \n", nRet);
		//	debug_printf("Register to XSTUNT succeeded \n");
		//}
		//else {
		//	debug_printf("Initialization failed. ErrType(%d) \n", nRet);
		//	return 0;
		//}

		//nRet = _stunt_mgr_ptr->tcpPunch_connection(100u);
		///////////////////////////////////////////////////////////////////////////

		// Send Capacity to PK
		SendCapacityToPK();

		// Add each candidate-peer into "map_pid_parent_temp" table
		for (unsigned long i = 0; i < lane_member; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			struct peer_info_t *new_peer = new struct peer_info_t;
			if (!level_msg_ptr->level_info[i] || !new_peer) {
				handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::level_msg_ptr  new_peer new error", __FUNCTION__, __LINE__);
			}
			memset(level_msg_ptr->level_info[i], 0, sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			memset(new_peer, 0, sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));
			offset += sizeof(struct level_info_t);

			//add lane peer_info to map table
			map_pid_parent_temp.insert(pair<unsigned long, peer_info_t *>(new_peer->pid, new_peer));
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "[INSERT] pid", new_peer->pid, "into map_pid_parent_temp size =", map_pid_parent_temp.size());

			new_peer->manifest = full_manifest;
			new_peer->peercomm_session = peercomm_session;
			new_peer->priority = my_session;
		}

		// Start session, and start connecting with candidates
		if (lane_member > 0) {
			_peer_ptr->substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(my_session);
			if (_peer_ptr->substream_first_reply_peer_iter != _peer_ptr->substream_first_reply_peer.end()) {
				handle_error(LOGICAL_ERROR, "[ERROR] Session id conflict", __FUNCTION__, __LINE__);
			}

			_peer_ptr->substream_first_reply_peer[my_session] = new manifest_timmer_flag;
			if (!(_peer_ptr->substream_first_reply_peer[my_session])){
				handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::_peer_ptr->substream_first_reply_peer[session_id]  new error", __FUNCTION__, __LINE__);
			}
			memset(_peer_ptr->substream_first_reply_peer[my_session], 0, sizeof(struct manifest_timmer_flag));

			_peer_ptr->substream_first_reply_peer[my_session]->session_state = FIRST_CONNECTED_RUNNING;
			_peer_ptr->substream_first_reply_peer[my_session]->child_pid = -1;		// Not used
			_peer_ptr->substream_first_reply_peer[my_session]->selected_pid = PK_PID;
			_peer_ptr->substream_first_reply_peer[my_session]->firstReplyFlag = FALSE;
			_peer_ptr->substream_first_reply_peer[my_session]->networkTimeOutFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->connectTimeOutFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->sentTestFlag = FALSE;
			_peer_ptr->substream_first_reply_peer[my_session]->rescue_manifest = full_manifest;
			_peer_ptr->substream_first_reply_peer[my_session]->peer_role = PARENT_PEER;
			_peer_ptr->substream_first_reply_peer[my_session]->allSkipConnFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->firstConnectedFlag = FALSE;

			// Set session timer
			_log_ptr->timerGet(&(_peer_ptr->substream_first_reply_peer[my_session]->connectTimeOut));

			_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
				"Start session", my_session,
				"my role", CHILD_PEER,
				"manifest", level_msg_ptr->manifest,
				"list_number", lane_member);
			_peer_mgr_ptr->_peer_communication_ptr->set_candidates_handler(level_msg_ptr, lane_member, CHILD_PEER, my_session, peercomm_session);

			for (unsigned int ss_id = 0; ss_id < sub_stream_num; ss_id++) {
				SetSubstreamState(ss_id, SS_CONNECTING2);
			}
			my_session++;
		}
		else if (lane_member == 0) {
			_logger_client_ptr->log_to_server(LOG_LIST_EMPTY, full_manifest);
			_logger_client_ptr->log_to_server(LOG_DATA_COME_PK, full_manifest);
		}
		else {
			handle_error(LOGICAL_ERROR, "[ERROR] lane_member < 0", __FUNCTION__, __LINE__);
		}

		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Set manifest", manifestToSubstreamID(level_msg_ptr->manifest), "blocked by connecting peer");

		for (unsigned long i = 0; i < lane_member; i++) {
			if (level_msg_ptr->level_info[i]) {
				delete level_msg_ptr->level_info[i];
			}
		}
		if (level_msg_ptr) {
			delete level_msg_ptr;
		}
		debug_printf("===================CHNK_CMD_PEER_REG DONE=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_REG DONE");
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SYN) {
		// measure start delay
		debug_printf("===================CHNK_CMD_PEER_SYN=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SYN");
		
		
		if (syn_table.state == SYNC_ONGOING) {
			// Already sent request in syn_table_init(), and now receiving reply sync message
			if (chunk_ptr->header.rsv_1 == REPLY) {
				debug_printf("CHNK_CMD_PEER_SYN REPLY \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SYN REPLY");

				struct syn_token_receive* syn_token_receive_ptr;
				syn_token_receive_ptr = (struct syn_token_receive*)chunk_ptr;
				syn_recv_handler(syn_token_receive_ptr);
			}
			else {
				handle_error(UNKNOWN, "[ERROR] Received wrong CHNK_CMD_PEER_SYN ", __FUNCTION__, __LINE__);
			}
		}
	}
	// light | pid | level   | n*struct rescue_peer_info
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_RESCUE_LIST) {
		debug_printf("===================CHNK_CMD_PEER_RESCUE_LIST=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_RESCUE_LIST");

		struct chunk_rescue_list *chunk_rescue_list_ptr = NULL;
		int lane_member = 0;
		int level_msg_size = 0;
		UINT32 peercomm_session = 0;
		struct chunk_level_msg_t *level_msg_ptr = NULL;
		UINT32 ss_id = 0;

		chunk_rescue_list_ptr = (struct chunk_rescue_list *)chunk_ptr;
		lane_member = (buf_len - sizeof(struct chunk_header_t) - 4*sizeof(UINT32)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + 2*sizeof(UINT32) + lane_member*sizeof(struct level_info_t *);
		ss_id = manifestToSubstreamID(chunk_rescue_list_ptr->manifest);
		peercomm_session = chunk_rescue_list_ptr->session;

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		if (!level_msg_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::level_msg_ptr   new error", __FUNCTION__, __LINE__);
		}
		memset(level_msg_ptr, 0, level_msg_size);
		memcpy(level_msg_ptr, chunk_rescue_list_ptr, sizeof(struct chunk_header_t) + 2*sizeof(UINT32));		// Copy header, pid, and manifest

		offset += (sizeof(struct chunk_header_t) + 4*sizeof(UINT32));		// Move offset to rescue_peer_info

		_logger_client_ptr->log_to_server(LOG_RESCUE_TRIGGER_BACK, chunk_rescue_list_ptr->manifest);

		// Merge operation
		if (chunk_rescue_list_ptr->operation == RESCUE_OP) {
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[PEERLIST_TYPE] mypid", my_pid, "RESCUE");
			ss_table[ss_id]->state.rescue_after_fail = true;
		}
		else if (chunk_rescue_list_ptr->operation == MERGE_OP) {
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[PEERLIST_TYPE] mypid", my_pid, "MERGE");
			ss_table[ss_id]->state.rescue_after_fail = true;
		}
		else if (chunk_rescue_list_ptr->operation == MOVE_OP) {
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[PEERLIST_TYPE] mypid", my_pid, "MOVE");
			ss_table[ss_id]->state.rescue_after_fail = false;
		}
		else {
			handle_error(UNKNOWN, "[ERROR] peer-list type error", __FUNCTION__, __LINE__);
		}

		for (int i = 0; i < lane_member; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			struct peer_info_t *new_peer = new struct peer_info_t;
			if (!level_msg_ptr->level_info[i] || !new_peer) {
				handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::level_msg_ptr->level_info[i] new error", __FUNCTION__, __LINE__);
			}
			memset(level_msg_ptr->level_info[i], 0, sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			memset(new_peer, 0x0, sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));
			offset += sizeof(struct level_info_t);

			map_pid_parent_temp.insert(pair<unsigned long, peer_info_t *>(new_peer->pid, new_peer));
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "[INSERT] pid", new_peer->pid, "into map_pid_parent_temp size =", map_pid_parent_temp.size());
			
			new_peer->manifest = chunk_rescue_list_ptr->manifest;
			new_peer->peercomm_session = peercomm_session;
			new_peer->priority = my_session;
		}

		// Start session, and start connecting with candidates
		if (lane_member > 0) {
			_peer_ptr->substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(my_session);
			if (_peer_ptr->substream_first_reply_peer_iter != _peer_ptr->substream_first_reply_peer.end()) {
				handle_error(LOGICAL_ERROR, "[ERROR] Session id conflict", __FUNCTION__, __LINE__);
			}

			_peer_ptr->substream_first_reply_peer[my_session] = new manifest_timmer_flag;
			if (!(_peer_ptr->substream_first_reply_peer[my_session])){
				handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::_peer_ptr->substream_first_reply_peer[session_id]  new error", __FUNCTION__, __LINE__);
			}
			memset(_peer_ptr->substream_first_reply_peer[my_session], 0, sizeof(struct manifest_timmer_flag));

			_peer_ptr->substream_first_reply_peer[my_session]->session_state = FIRST_CONNECTED_RUNNING;
			_peer_ptr->substream_first_reply_peer[my_session]->child_pid = -1;		// Not used
			_peer_ptr->substream_first_reply_peer[my_session]->selected_pid = PK_PID;
			_peer_ptr->substream_first_reply_peer[my_session]->firstReplyFlag = FALSE;
			_peer_ptr->substream_first_reply_peer[my_session]->networkTimeOutFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->connectTimeOutFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->sentTestFlag = FALSE;
			_peer_ptr->substream_first_reply_peer[my_session]->rescue_manifest = level_msg_ptr->manifest;
			_peer_ptr->substream_first_reply_peer[my_session]->peer_role = PARENT_PEER;
			_peer_ptr->substream_first_reply_peer[my_session]->allSkipConnFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->firstConnectedFlag = FALSE;

			// Set session timer
			_log_ptr->timerGet(&(_peer_ptr->substream_first_reply_peer[my_session]->connectTimeOut));

			_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
				"Start my_session", my_session,
				"my role", CHILD_PEER,
				"manifest", level_msg_ptr->manifest,
				"list_number", lane_member);
			_peer_mgr_ptr->_peer_communication_ptr->set_candidates_handler(level_msg_ptr, lane_member, CHILD_PEER, my_session, peercomm_session);

			SetSubstreamState(ss_id, SS_CONNECTING2);
			my_session++;
		}
		else if (lane_member == 0) {
			_logger_client_ptr->log_to_server(LOG_LIST_EMPTY, chunk_rescue_list_ptr->manifest);
			_logger_client_ptr->log_to_server(LOG_DATA_COME_PK, chunk_rescue_list_ptr->manifest);
		}
		else {
			handle_error(LOGICAL_ERROR, "[ERROR] lane_member < 0", __FUNCTION__, __LINE__);
		}

		_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Set manifest", manifestToSubstreamID(level_msg_ptr->manifest), "blocked by connecting peer");

		for (unsigned long i = 0; i < lane_member; i++) {
			if (level_msg_ptr->level_info[i]) {
				delete level_msg_ptr->level_info[i];
			}
		}
		if (level_msg_ptr) {
			delete level_msg_ptr;
		}
	} 
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_DATA) {
		//不刪除 chunk_ptr 全權由handle_stream處理
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_DATA");
		//handle_stream(chunk_ptr, sock);	
		HandleStream(chunk_ptr, sock);	
		return RET_OK;
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SEED) {
		debug_printf("===================CHNK_CMD_PEER_SEED=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SEED");
	
		struct seed_notify *chunk_seed_notify = (struct seed_notify *)chunk_ptr;
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "chunk manifest", chunk_seed_notify->manifest);

		// 將 PK 送來的 manifest 分成 substream 為單位處理
		for (unsigned int ss_id = 0; ss_id < sub_stream_num; ss_id++) {
			UINT32 manifest = SubstreamIDToManifest(ss_id) & chunk_seed_notify->manifest;
			
			// Peer 向 PK 發 rescue，一定會讓 substream 進入 SS_RESCUE 狀態，非此狀態的 substream 就不受此次 SEED CMD 影響 
			// 但還需考慮因為 parent 離開系統而收到的 SEED CMD
			if (ss_table[ss_id]->state.state != SS_RESCUE2) {
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Skip manifest", manifest);
				continue;
			}
			
			unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
			unsigned long pk_new_manifest = pk_old_manifest | manifest;

			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "received manifest =", manifest);

			// If all substreams are not in INIT state, send log message
			if (sentStartDelay == true && pk_old_manifest != pk_new_manifest) {
				_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, pk_new_manifest, PK_PID);
			}

			//Maybe clear previous parent info from map_pid_parent and close socket
			// Update manifest, and clear parent socket if manifest = 0
			set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
			SetSubstreamParent(manifest, PK_PID);

			ss_table[ss_id]->state.is_testing = false;
			SetSubstreamState(ss_id, SS_STABLE2);
			
			/*
			for (map<unsigned long, struct peer_connect_down_t *>::iterator iter = map_pid_parent.begin(); iter != map_pid_parent.end(); iter++) {
				struct peer_connect_down_t *parent_info = iter->second;
				unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
				unsigned long parent_new_manifest = parent_old_manifest & ~manifest;

				_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__, "[CHECK POINT] size() =", map_pid_parent.size(), iter->first);
					
				if (parent_info->peerInfo.pid != PK_PID) {
					set_parent_manifest(parent_info, parent_new_manifest);

					if (parent_info->peerInfo.connection_state == PEER_CONNECTED_PARENT) {
						_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);
					}
						
					if (parent_info->peerInfo.manifest == 0) {
						// Take care the condition if the first element we want to delete
						_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "close at add seed. pid =", iter->first);
						//_peer_ptr->data_close(_peer_ptr->map_in_pid_fd[parent_info->peerInfo.pid], "close at add seed", CLOSE_PARENT);
						_peer_ptr->CloseParent(parent_info->peerInfo.pid, false, "close at add seed");
							
						_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[CHECK POINT] size() =", map_pid_parent.size());

						//delete iter->second;
						//map_pid_parent.erase(iter);
							
						iter = map_pid_parent.begin();
						if (iter == map_pid_parent.end()) {
							break;
						}
					}
				}
			}
			*/

			// Reset substream state
			struct timerStruct timer;
			_log_ptr->timerGet(&timer);
			memcpy(&(ss_table[ss_id]->parent_changed_time), &timer, sizeof(timer));
			
		}

	}
	// for each stream this protocol (only use to  decode to flv )  or (other protocol need some header)  
	// light | streamID_1 (int) | stream_header_1 len (unsigned int) | protocol_1 header | streamID_2 ...... 
	// This CMD only happen in "join" condition or "parent-peer of a peer unnormally terminate connection" condition
	else if (chunk_ptr->header.cmd == CHNK_CMD_CHN_UPDATE_DATA) {		
		debug_printf("===================CHNK_CMD_CHN_UPDATE_DATA=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_CHN_UPDATE_DATA");
		
		int *streamID_ptr = NULL;
		int *len_ptr = NULL;
		//unsigned char *header = NULL;
		update_stream_header *protocol_len_header = NULL ;
		int expected_len = chunk_ptr->header.length;
		int offset = 0;
		
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "expected_len =", expected_len);
		
		while (offset != expected_len) {
			streamID_ptr = (int *)(chunk_ptr->buf + offset);
			len_ptr = (int *)(chunk_ptr->buf + sizeof(int) + offset);
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "len_ptr =", *len_ptr, "streamID_ptr =", *streamID_ptr);
			
			//wait header 
			if (*len_ptr == 0) {
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "wait header streamID =", *streamID_ptr);
				debug_printf("wait header streamID = %d \n", *streamID_ptr);
				offset += sizeof(int) + sizeof(int) ;
				continue;
			}
			//no header
			else if (*len_ptr == -1) {
				protocol_len_header = new update_stream_header;
				if (!protocol_len_header) {
					handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::protocol_len_header new error", __FUNCTION__, __LINE__);
				}
				protocol_len_header->len = *len_ptr;
				map_streamID_header[*streamID_ptr] = (update_stream_header*)protocol_len_header;
				debug_printf("streamID = %d, *len_ptr = %d \n", *streamID_ptr, *len_ptr);
				offset += sizeof(int) + sizeof(int);
			}
			//have header
			else {
				protocol_len_header = (update_stream_header *)new unsigned char[*len_ptr + sizeof(int)];
				if (!protocol_len_header) {
					handle_error(MALLOC_ERROR, "[ERROR] protocol_len_header new error", __FUNCTION__, __LINE__);
				}
				unsigned char *header = (unsigned char *)chunk_ptr->buf + sizeof(int) + offset;
				memcpy(protocol_len_header, header, *len_ptr+sizeof(int));
				map_streamID_header[*streamID_ptr] = (update_stream_header*)protocol_len_header ;
				//debug_printf("streamID = %d  *len_ptr =%d  \n", *streamID_ptr, *len_ptr) ;
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "len_ptr =", *len_ptr);
				offset += sizeof(int) + sizeof(int) + *len_ptr;
			}
		}
		
		stream_number = map_streamID_header.size();
		
		#ifdef RECORD_FILE
		record_file_init(*streamID_ptr);
		#endif
		
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "stream_number =", stream_number);
		debug_printf("\n===================CHNK_CMD_CHN_UPDATE_DATA done=================\n");
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PARENT_PEER){
		debug_printf("===================CHNK_CMD_PARENT_PEER=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PARENT_PEER");
		
		int list_number = 0;
		int level_msg_size = 0;
		unsigned long ss_id = 0;
		UINT32 peercomm_session = 0;
		struct chunk_child_info *child_info_ptr = NULL;
		struct chunk_level_msg_t *level_msg_ptr = NULL;

		//struct peer_info_t *new_peer = NULL;
		
		//set_rescue_state(temp_rescue_sub_id,2);
		child_info_ptr = (struct chunk_child_info*)chunk_ptr;
		list_number = (buf_len - sizeof(struct chunk_header_t) - 3*sizeof(UINT32)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + 3*sizeof(UINT32) + list_number*sizeof(struct level_info_t *);
		ss_id = manifestToSubstreamID(child_info_ptr->manifest);
		peercomm_session = child_info_ptr->session;

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		if (!level_msg_ptr) {
			handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::level_msg_ptr new error", __FUNCTION__, __LINE__);
		}
		memset(level_msg_ptr, 0, level_msg_size);
		memcpy(level_msg_ptr, chunk_ptr, (sizeof(struct chunk_header_t) + 2*sizeof(UINT32)));

		offset += (sizeof(struct chunk_header_t) + 3*sizeof(UINT32));

		_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"list_number ",list_number);

		// list-number only equals to 1
		for (int i = 0; i < list_number; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			struct peer_info_t *new_peer = new struct peer_info_t;
			if (!level_msg_ptr->level_info[i] || !new_peer) {
				handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::level_msg_ptr new error", __FUNCTION__, __LINE__);
			}
			memset(level_msg_ptr->level_info[i], 0, sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			memset(new_peer, 0, sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));
			offset += sizeof(struct level_info_t);

			map_pid_child_temp.insert(pair<unsigned long, peer_info_t *>(new_peer->pid, new_peer));
			_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "[INSERT] pid", new_peer->pid, "into map_pid_child_temp size =", map_pid_child_temp.size());
			_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__, "pid", child_info_ptr->pid, "manifest", child_info_ptr->manifest, "session", child_info_ptr->session, "childpid", child_info_ptr->child_level_info.pid);

			new_peer->manifest = child_info_ptr->manifest;
			new_peer->peercomm_session = peercomm_session;
			new_peer->priority = my_session;
		}

		if((list_number == 0) || (list_number >1)){
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","CHNK_CMD_PARENT_PEER cannot have 0 member or more than 1 members\n");
			_logger_client_ptr->log_exit();
		}
		
		if (list_number == 1) {
			_peer_ptr->substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(my_session);
			if (_peer_ptr->substream_first_reply_peer_iter != _peer_ptr->substream_first_reply_peer.end()) {
				handle_error(LOGICAL_ERROR, "[ERROR] Session id conflict", __FUNCTION__, __LINE__);
			}

			_peer_ptr->substream_first_reply_peer[my_session] = new manifest_timmer_flag;
			if (!(_peer_ptr->substream_first_reply_peer[my_session])){
				handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::_peer_ptr->substream_first_reply_peer[session_id] new error", __FUNCTION__, __LINE__);
			}
			memset(_peer_ptr->substream_first_reply_peer[my_session], 0x0, sizeof(struct manifest_timmer_flag));

			_peer_ptr->substream_first_reply_peer[my_session]->session_state = FIRST_CONNECTED_RUNNING;
			_peer_ptr->substream_first_reply_peer[my_session]->child_pid = child_info_ptr->child_level_info.pid;
			_peer_ptr->substream_first_reply_peer[my_session]->selected_pid = -1;		// Not used
			_peer_ptr->substream_first_reply_peer[my_session]->firstReplyFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->networkTimeOutFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->connectTimeOutFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->sentTestFlag = FALSE;		// Not used
			_peer_ptr->substream_first_reply_peer[my_session]->rescue_manifest = child_info_ptr->manifest;
			_peer_ptr->substream_first_reply_peer[my_session]->peer_role = CHILD_PEER;
			_peer_ptr->substream_first_reply_peer[my_session]->allSkipConnFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[my_session]->firstConnectedFlag = FALSE;

			// Set session timer
			_log_ptr->timerGet(&(_peer_ptr->substream_first_reply_peer[my_session]->connectTimeOut));

			_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
				"Start session", my_session,
				"my role", PARENT_PEER,
				"manifest", level_msg_ptr->manifest,
				"list_number", lane_member);
			_peer_mgr_ptr->_peer_communication_ptr->set_candidates_handler(level_msg_ptr, list_number, PARENT_PEER, my_session, peercomm_session);

			my_session++;
		}
		else {
			handle_error(LOGICAL_ERROR, "[ERROR] lane_member != 1", __FUNCTION__, __LINE__);
		}
		
		for (unsigned long i = 0; i < list_number; i++) {
			if (level_msg_ptr->level_info[i]) {
				delete level_msg_ptr->level_info[i];
			}
		}

		if (level_msg_ptr) {
			delete level_msg_ptr;
		}
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_KICK_PEER) {
		debug_printf("===================CHNK_CMD_KICK_PEER=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_KICK_PEER");
		
		handle_kickout(chunk_ptr, sock);
	}
	else {
		debug_printf("===================CHNK_CMD_ERROR=================\n");
		handle_error(UNKNOWN, "[ERROR] Receive unknown header command", __FUNCTION__, __LINE__);
	}

	if (chunk_ptr) {
		delete [] (unsigned char*)chunk_ptr;
	}

	return RET_OK;
}

int pk_mgr::handle_pkt_out(int sock)
{
	return RET_OK;
}

void pk_mgr::handle_pkt_error(int sock)
{
	data_close(sock, "handle_pkt_error");
}

void pk_mgr::handle_sock_error(int sock, basic_class *bcptr)
{
	data_close(sock, "handle_sock_error");
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
	if (!request_pkt_ptr) {
		handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::request_pkt_ptr new error", __FUNCTION__, __LINE__);
	}

	memset(html_buf, 0, _html_size);
	memset(request_pkt_ptr, 0, sizeof(struct chunk_request_pkt_t));

	request_pkt_ptr->header.cmd = CHNK_CMD_PEER_REQ_FROM;
	request_pkt_ptr->header.length = sizeof(unsigned long) + sizeof(unsigned int) + sizeof(unsigned int);	//pkt_buf paylod length
	request_pkt_ptr->header.rsv_1 = REQUEST;
	request_pkt_ptr->pid = level_msg_ptr->pid;
	request_pkt_ptr->request_from_sequence_number = req_from;
	request_pkt_ptr->request_to_sequence_number = req_to;

	//printf("request seq %d to %d\n",request_pkt_ptr->request_from_sequence_number,request_pkt_ptr->request_to_sequence_number);

	memcpy(html_buf, request_pkt_ptr, sizeof(struct chunk_request_pkt_t));

	send_byte = _net_ptr->send(_sock, html_buf, sizeof(struct chunk_request_pkt_t), 0);

	if (send_byte <= 0 ) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		exit_code = PK_SOCKET_ERROR;
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] send send_request_sequence_number_to_pk cmd error", socketErr);
		data_close(_sock, "send send_request_sequence_number_to_pk cmd error");
		handle_error(PK_SOCKET_ERROR, "[ERROR] send send_pkt_to_pk error", __FUNCTION__, __LINE__);
		PAUSE
		//_log_ptr->exit(0, "send send_request_sequence_number_to_pk cmd error");
	}
	else {
		if (request_pkt_ptr) {
			delete request_pkt_ptr;
		}
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

	memset(html_buf, 0, _html_size);
	memcpy(html_buf, chunk_ptr, expect_len);

	send_byte = _net_ptr->send(_sock, html_buf, expect_len, 0);

	if( send_byte <= 0 ) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		exit_code = PK_SOCKET_ERROR;
		_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"[ERROR] send send_pkt_to_pk error",socketErr);
		//data_close(_sock, "send send_pkt_to_pk error");
		handle_error(PK_SOCKET_ERROR, "[ERROR] send send_pkt_to_pk error", __FUNCTION__, __LINE__);
		PAUSE
		//_log_ptr->exit(0, "send pkt error");
	} else {
		if (chunk_ptr) {
			delete chunk_ptr;
		}
		_net_ptr->set_nonblocking(_sock);	// set to non-blocking
	}
}

//the main handle steram function 需要處理不同序來源的chunk,		
//送到player 的queue 裡面 必須保證是有方向性的 且最好是依序的
// Parameters:
// 		chunk_ptr	A pointer to P2P packet
// 		sockfd		fd which received this P2P packet
void pk_mgr::handle_stream(struct chunk_t *chunk_ptr, int sockfd)
{

}

void pk_mgr::HandleStream(struct chunk_t *chunk_ptr, int sockfd)
{
	unsigned long i;
	unsigned int seq_ready_to_send = 0;
	unsigned long testingManifest = 0;
	UINT32 parent_pid = -1;
	int leastCurrDiff = 0;
	stream *strm_ptr = NULL;
	struct peer_connect_down_t *parent_info = NULL;
	INT32 ss_id;		// sub-stream ID of this chunk
	UINT32 manifest;		// manifest of this chunk
	//map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	//map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter = map_pid_parent.end();
	struct timerStruct pkt_recv_time;

	//還沒註冊拿到substream num  不做任何偵測和運算
	if (sub_stream_num == 0) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Drop the packet(not get register reply yet)");
		delete[](unsigned char*)chunk_ptr;
		return;
	}

	// 第一次Sync沒完成前不處理任何data
	if (syn_table.first_sync_done == false) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Drop the packet(the first sync not done yet)");
		delete[](unsigned char*)chunk_ptr;
		return;
	}

	// Initialize parameters when the first legal packet arrived
	if (first_legal_pkt_received == false) {

		// Record input bandwidth
		_logger_client_ptr->bw_in_struct_init(chunk_ptr->header.timestamp, chunk_ptr->header.length);
		_logger_client_ptr->log_bw_in_init_flag = 1;

		_least_sequence_number = chunk_ptr->header.sequence_number;
		_current_send_sequence_number = chunk_ptr->header.sequence_number;

		first_timestamp = chunk_ptr->header.timestamp;
		_log_ptr->timerGet(&start);
		/*
		// Record input bandwidth
		if (_logger_client_ptr->log_bw_in_init_flag == 1) {
		_logger_client_ptr->set_in_bw(chunk_ptr->header.timestamp, chunk_ptr->header.length);
		}
		else {
		_logger_client_ptr->log_bw_in_init_flag = 1;
		_logger_client_ptr->bw_in_struct_init(chunk_ptr->header.timestamp, chunk_ptr->header.length);
		}


		if (_current_send_sequence_number == 0) {
		_current_send_sequence_number = chunk_ptr->header.sequence_number;
		}

		// Record the timestamp of the first received packet
		if (first_timestamp == 0) {
		first_timestamp = chunk_ptr->header.timestamp;
		_log_ptr->timerGet(&start);
		}
		*/
		first_legal_pkt_received = true;
	}

	if (_least_sequence_number < chunk_ptr->header.sequence_number) {
		_least_sequence_number = chunk_ptr->header.sequence_number;
	}


	// Record input bandwidth
	_logger_client_ptr->set_in_bw(chunk_ptr->header.timestamp, chunk_ptr->header.length);

	// Get the substream ID and the manifest
	ss_id = chunk_ptr->header.sequence_number % sub_stream_num;
	manifest = SubstreamIDToManifest(ss_id);

	// Get the substream table of this substream ID
	if (ss_table.find(ss_id) == ss_table.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] can not find source struct in table in send_capacity_to_pk\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] can not find source struct in table in send_capacity_to_pk");
		_logger_client_ptr->log_exit();
	}

	// Get parentPid and parentInfo of this chunk
	for (map<unsigned long, struct peer_connect_down_t *>::iterator iter = map_pid_parent.begin(); iter != map_pid_parent.end(); iter++) {
		if (sockfd == iter->second->peerInfo.sock) {
			//pid_peerDown_info_iter = iter;
			parent_info = iter->second;
			parent_pid = iter->second->peerInfo.pid;
		}
	}
	if (parent_info == NULL) {
		// UDP 的話可能會因為已經中斷連線卻收封包，因此忽略
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] can not find map_pid_parent", sockfd);
		delete[](unsigned char*)chunk_ptr;
		return;
	}

	/*
	if (_peer_ptr->map_fd_pid.find(sockfd) != _peer_ptr->map_fd_pid.end()) {
	parent_pid = _peer_ptr->map_fd_pid[sockfd];				// Get parent-pid of this chunk
	pid_peerDown_info_iter = map_pid_parent.find(parent_pid);
	if (pid_peerDown_info_iter == map_pid_parent.end()) {
	// UDP 的話可能會因為已經中斷連線卻收封包，因此忽略
	//handle_error(MACCESS_ERROR, "[ERROR] can not find map_pid_parent", __FUNCTION__, __LINE__);
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] [ERROR] can not find map_pid_parent");
	delete[](unsigned char*)chunk_ptr;
	return;
	}
	parent_info = pid_peerDown_info_iter->second;		// Get parent-info of this chunk
	}
	else if (_peer_ptr->map_udpfd_pid.find(sockfd) != _peer_ptr->map_udpfd_pid.end()) {
	parent_pid = _peer_ptr->map_udpfd_pid[sockfd];				// Get parent-pid of this chunk
	pid_peerDown_info_iter = map_pid_parent.find(parent_pid);
	if (pid_peerDown_info_iter == map_pid_parent.end()) {
	// UDP 的話可能會因為已經中斷連線卻收封包，因此忽略
	//handle_error(MACCESS_ERROR, "[ERROR] can not find map_pid_parent", __FUNCTION__, __LINE__);
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] [ERROR] can not find map_pid_parent");
	delete[](unsigned char*)chunk_ptr;
	return;
	}
	parent_info = pid_peerDown_info_iter->second;		// Get parent-info of this chunk
	}
	else {
	handle_error(MACCESS_ERROR, "[ERROR] can not find map_fd_pid", __FUNCTION__, __LINE__);
	}
	*/


	// If receive the first packet of each substream, send start-delay to PK
	// Record the first packet's sequence number
	if (sentStartDelay == false) {
		// Initialization when the first packet of each substream is received
		if (ss_table[ss_id]->first_pkt_received == false) {
			ss_table[ss_id]->first_pkt_received = true;
			ss_table[ss_id]->first_pkt_seq = chunk_ptr->header.sequence_number;
			ss_table[ss_id]->latest_pkt_seq = chunk_ptr->header.sequence_number;
			peer_start_delay_count++;

			ss_table[ss_id]->data.first_timestamp = chunk_ptr->header.timestamp;
			ss_table[ss_id]->data.last_timestamp = chunk_ptr->header.timestamp;
			ss_table[ss_id]->data.last_seq = chunk_ptr->header.sequence_number;

			ss_table[ss_id]->state.dup_src = false;
			_log_ptr->timerGet(&ss_table[ss_id]->data.lastAlarm);
			_log_ptr->timerGet(&ss_table[ss_id]->data.firstAlarm);

			if (parent_pid == PK_PID) {
				SetSubstreamState(ss_id, SS_STABLE2);
			}
			else {
				SetSubstreamState(ss_id, SS_TEST2);
			}

			SetSubstreamParent(manifest, parent_pid);
		}
		if (peer_start_delay_count == (int)sub_stream_num) {
			sentStartDelay = true;
			_logger_client_ptr->count_start_delay();
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Send start-delay to PK");
			_logger_client_ptr->log_to_server(LOG_TOPO_PEER_JOIN, full_manifest, parent_pid);

		}
	}
	//_log_ptr->write_log_format("s(u) s d s u s d s u \n", __FUNCTION__, __LINE__, 
	//												"peer_start_delay_count =", peer_start_delay_count, 
	//												"start_seq =", syn_table.start_seq,
	//												"pkSendCapacity =", pkSendCapacity,
	//												"first_timestamp =", first_timestamp);

	//↓↓↓↓↓↓↓↓↓↓↓↓任何chunk 都會run↓↓↓↓↓↓↓↓↓↓↓↓↓
	_log_ptr->write_log_format("s(u) s u s d s u s u s u s u s u s u s u s d \n", __FUNCTION__, __LINE__,
		"parent", parent_pid,
		"sock", sockfd,
		"parent_manifest", parent_info->peerInfo.manifest,
		"sID", chunk_ptr->header.stream_id,
		"ssID", ss_id,
		"state", ss_table[ss_id]->state.state,
		"seq", chunk_ptr->header.sequence_number,
		"timestamp", chunk_ptr->header.timestamp,
		"source_delay", ss_table[ss_id]->source_delay_table.source_delay_time,
		"bytes", chunk_ptr->header.length
		);

	// 過濾封包, 如果chunk的manifest跟parent的manifest不一樣
	if ((manifest & parent_info->peerInfo.manifest) == 0) {
		_log_ptr->write_log_format("s(u) s u u \n", __FUNCTION__, __LINE__, "[DEBUG] Drop packet due to manifests are unequal", manifest, parent_info->peerInfo.manifest);
		delete[](unsigned char*)chunk_ptr;
		return;
	}


	_log_ptr->timerGet(&pkt_recv_time);
	if (ss_table[ss_id]->current_parent_pid != PK_PID || ss_table[ss_id]->state.state != SS_TEST2) {
		// SS_TSET 狀態的 substream timer 只記錄從 parent 來的封包，PK 來的封包不記錄，為了能在 SS_TEST 狀態處發 RESCUE 
		ss_table[ss_id]->timer.timer = pkt_recv_time;
	}

	//更新最後的seq 用來做time out
	parent_info->timeOutNewSeq = chunk_ptr->header.sequence_number;
	
	if (parent_pid == PK_PID) {
		pkDownInfoPtr->timeoutPass_flag = 1;
	}
	
	if (parent_pid == PK_PID) {
		if (ss_table[ss_id]->state.state == SS_STABLE2) {
			//
		}
		else if (ss_table[ss_id]->state.state == SS_CONNECTING2) {
			//
		}
		else if (ss_table[ss_id]->state.state == SS_RESCUE2) {
			// 不應該發生
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
		}
		else if (ss_table[ss_id]->state.state == SS_TEST2) {
			//
		}
	}
	else {
		if (ss_table[ss_id]->state.state == SS_STABLE2) {
		
		}
		else if (ss_table[ss_id]->state.state == SS_CONNECTING2) {
			//parent_info->outBuffCount = 0;
			//SetSubstreamState(ss_id, SS_TEST2);
			//ss_table[ss_id]->state.is_testing = true;
		}
		else if (ss_table[ss_id]->state.state == SS_RESCUE2) {
			// 不應該發生
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG]");
		}
		else if (ss_table[ss_id]->state.state == SS_TEST2) {
			_log_ptr->write_log_format("s(u) s d s \n", __FUNCTION__, __LINE__, "ss_id", ss_id, "in SS_TEST state");
			
			ss_table[ss_id]->data.testing_count++;
			if (ss_table[ss_id]->data.testing_count == 1) {
				SetSubstreamParent(manifest, parent_info->peerInfo.pid);
			}
			
			//下面會濾掉慢到的封包 所以在此進入偵測
			RescueDetecion(chunk_ptr);
			ss_table[ss_id]->latest_pkt_timestamp = chunk_ptr->header.timestamp;
			_log_ptr->timerGet(&ss_table[ss_id]->latest_pkt_client_time);
			SourceDelayDetection(sockfd, ss_id, chunk_ptr->header.sequence_number);
		
			
			//測試次數填滿整個狀態  也就是測量了PARAMETER_M  次都沒問題 ( 其中有PARAMETER_M 次計算不會連續觸發)
			//if (ss_table[ss_id]->data.testing_count / ( TEST_DELAY_TIME*Xcount/sub_stream_num ) >= 1) {
			// 經過6秒時間都處在 SS_TEST 狀態而沒處發 RESCUE 進入 SS_RESCUE 狀態，認定此 parent 狀態良好
			if (ss_table[ss_id]->timer.inTest_flag == true) {
				unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
				unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
				unsigned long pk_new_manifest = pk_old_manifest & ~manifest;
				unsigned long parent_new_manifest = parent_info->peerInfo.manifest;
				
				SetSubstreamState(ss_id, SS_STABLE2);
				ss_table[ss_id]->state.is_testing = false;
				ss_table[ss_id]->data.testing_count = 0;
				ss_table[ss_id]->state.rescue_after_fail = true;
				parent_info->outBuffCount = 0;
				
				// Test-delay succeed. Cut the substream from pk
				_logger_client_ptr->log_to_server(LOG_TOPO_TEST_SUCCESS, parent_info->peerInfo.manifest, parent_info->peerInfo.pid);
				_logger_client_ptr->log_to_server(LOG_RESCUE_DETECTION_TESTING_SUCCESS, manifest, 1, parent_info->peerInfo.pid);
				set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
				send_rescueManifestToPKUpdate(pk_new_manifest);
				_logger_client_ptr->log_to_server(LOG_RESCUE_CUT_PK, manifest);
				_logger_client_ptr->log_to_server(LOG_RESCUE_DATA_COME, manifest);

				_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__,
																	"Test-delay succeed. pk manifest", pk_old_manifest,
																	"->", pk_new_manifest,
																	"parent manifest", parent_old_manifest,
																	"->", parent_new_manifest);
																	
			
				ss_table[ss_id]->state.dup_src = false;
				
				//選擇selected peer 送topology
				send_parentToPK(SubstreamIDToManifest(ss_id), parent_info->peerInfo.pid);
				
				SetSubstreamParent(manifest, parent_info->peerInfo.pid);
				
				//testing function
				//reSet_detectionInfo();
				ss_table[ss_id]->timer.inTest_flag = false;
			}
			
		}
	}
	
	//↑↑↑↑↑↑↑↑↑↑↑↑任何chunk 都會run↑↑↑↑↑↑↑↑↑↑↑↑
	
	_log_ptr->write_log_format("s(u) s d u u u u u d d \n", __FUNCTION__, __LINE__, "ss_table",
															ss_id,
															ss_table[ss_id]->first_pkt_received,
															ss_table[ss_id]->first_pkt_seq,
															ss_table[ss_id]->latest_pkt_seq,
															ss_table[ss_id]->latest_pkt_timestamp,
															ss_table[ss_id]->state.state,
															ss_table[ss_id]->state.dup_src,
															ss_table[ss_id]->source_delay_table.delay_beyond_count);
	
	// Compare chunk_ptr(new chunk) with buf_chunk_t[index](chunk buffer)
	// If new chunk's seq > buffer chunk[index]'s seq, update buffer chunk[index]
	// If new chunk's seq = buffer chunk[index]'s seq, drop it
	// If new chunk's seq < buffer chunk[index]'s seq, drop it
	if (chunk_ptr->header.sequence_number > (**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number) {
		delete [] (unsigned char*) *(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size)) ;
		*(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size)) = chunk_ptr;
		//_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__,
		//											"new sequence number in the buffer. seq",
		//											(**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number);
	} 
	else if ((**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number == chunk_ptr->header.sequence_number) {
		//_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__,
		//											"duplicate sequence number in the buffer. seq",
		//											(**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number);
		delete [] (unsigned char*)chunk_ptr ;
		return;
	} 
	else {
		//_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__,
		//											"sequence number smaller than the index in the buffer. seq",
		//											(**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number);
		delete [] (unsigned char*)chunk_ptr ;
		return;
	}

	

	//↓↓↓↓↓↓↓↓↓↓↓↓以下只有先到的chunk才會run(會濾掉重複的和更小的)↓↓↓↓↓↓↓↓↓↓↓↓↓
	
	// Transmit down-stream(UPLOAD) to other peer if SSID match and in map_pid_child(real children)
	HandleRelayStream(chunk_ptr);


	/// Do something when receiving a new sequence chunk from whoever , before put into buffer of player
	
	
	pkt_count++;
	
	/// Check the packets. Divide into two part: 1.from testing-peer, 2.from others(check packets from pk as well)
	// stream from normal parent(pk and non-testing-peer)
	if (ss_table[ss_id]->state.is_testing == false) {
		//丟給rescue_detecion一定是有方向性的
		RescueDetecion(chunk_ptr);
		
		ss_table[ss_id]->latest_pkt_timestamp = chunk_ptr->header.timestamp;
		_log_ptr->timerGet(&ss_table[ss_id]->latest_pkt_client_time);
		
		SourceDelayDetection(sockfd, ss_id, chunk_ptr->header.sequence_number);
	}
	else {
		if (parent_pid == PK_PID) {
			// The chunk is from pk, and whose substream is in testing-state
			// We don't check this chunk which is from pk when it is in testing-state
		} else {
			// The chunk is from testing-peer, and whose substream is in testing-state
			// It is already checked before filtering
		}
	}
	
	//_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sentStartDelay =", sentStartDelay, "pkSendCapacity =", pkSendCapacity);
	
	
	// leastCurrDiff = latest received sequence number so far - latest sequence number sent to player
	// Check whether packet is lost or not in buf_chunk_t[]
	leastCurrDiff = _least_sequence_number - _current_send_sequence_number;
	
	//_log_ptr->write_log_format("s(u) d u u \n", __FUNCTION__, __LINE__, leastCurrDiff, _least_sequence_number, _current_send_sequence_number);
	if (leastCurrDiff < 0) {
		handle_error(UNKNOWN, "[ERROR] leastCurrDiff < 0", __FUNCTION__, __LINE__);
	}
	else if (leastCurrDiff >= _bucket_size) {
		handle_error(UNKNOWN, "[ERROR] leastCurrDiff >= _bucket_size", __FUNCTION__, __LINE__);
	}
	
	if (parent_info->outBuffCount > 0) {
		//ss_table[ss_id]->lost_packet_count -= 1;
	}
	//_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "parent", parent_info->peerInfo.pid, "outBuffCount =", parent_info->outBuffCount);
	/// 封包太晚到，超過BUFF_SIZE秒(與目前為止收到最新的sequence相比)
	// Skip the BUFF_SIZE-second-ago-packet and suppose it is lost.
	for ( ; leastCurrDiff > (int)(Xcount * BUFF_SIZE); _current_send_sequence_number++) {
		leastCurrDiff = _least_sequence_number - _current_send_sequence_number;
		
		_log_ptr->write_log_format("s(u) d d d d d \n", __FUNCTION__, __LINE__,
			leastCurrDiff,
			_least_sequence_number,
			_current_send_sequence_number,
			(**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.sequence_number,
			_current_send_sequence_number);
		//_log_ptr->write_log_format("s(u) u \n", __FUNCTION__, __LINE__, _log_ptr->diff_TimerGet_ms(&pkt_recv_time, &ss_table[ss_id]->parent_changed_time));


		// 為了避免誤判, 對於剛換過parent(從test開始計時)的substream, 給予它一定時間的保護避免觸發type 1的rescue
		if (_log_ptr->diff_TimerGet_ms(&pkt_recv_time, &ss_table[ss_id]->parent_changed_time) < BUFF_SIZE * 2 * 1000) {
			_log_ptr->write_log_format("s(u) s d s d s d s \n", __FUNCTION__, __LINE__,
				"pid", my_pid,
				"parent pid", ss_table[ss_id]->current_parent_pid,
				"is new parent by", BUFF_SIZE, "seconds");
			continue;
		}



		// If BUFF_SIZE-second-ago-packet is lost
		if ((**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number) {

			// The "parent_info" and "ss_id" in this region represents the parent who lost packets and the lost packet ss_id, respectively
			INT32 ss_id = _current_send_sequence_number % sub_stream_num;
			if (map_pid_parent.find(ss_table[ss_id]->current_parent_pid) == map_pid_parent.end()) {
				// This may happens if parent is closed after it lost packet
				continue;
			}
			struct peer_connect_down_t *parent_info = map_pid_parent.find(ss_table[ss_id]->current_parent_pid)->second;




			_logger_client_ptr->quality_struct_ptr->lost_pkt++;
			debug_printf("Packet lost (%d, %d seconds) \n", Xcount*BUFF_SIZE, BUFF_SIZE);
			_log_ptr->write_log_format("s(u) s u s u s u (d)(d) \n", __FUNCTION__, __LINE__,
				"Packet sequence", _current_send_sequence_number,
				"lost from parent", parent_info->peerInfo.pid,
				"lost count", _logger_client_ptr->quality_struct_ptr->lost_pkt,
				leastCurrDiff,
				(Xcount * BUFF_SIZE));

			ss_table[ss_id]->lost_packet_count += 1;
			parent_info->outBuffCount += 1;

			// 在觸發 rescue 的前一秒送 blocl_rescue 給相關的 child-peers
			int threshold = (CHUNK_LOSE - 1) > 0 ? ((CHUNK_LOSE - 1)*Xcount) / sub_stream_num : 1;
			if (ss_table[ss_id]->source_delay_table.delay_beyond_count > threshold && ss_table[ss_id]->can_send_block_rescue == TRUE) {
				// 還沒被凍結 rescue 的話才會發出 Block Rescue，否則不發出
				if (ss_table[ss_id]->block_rescue == FALSE) {
					_peer_mgr_ptr->SendBlockRescue(ss_id, 1);
					_log_ptr->timerGet(&ss_table[ss_id]->block_rescue_interval);
					ss_table[ss_id]->can_send_block_rescue = FALSE;
				}
			}


			if (ss_table[ss_id]->lost_packet_count > CHUNK_LOSE*Xcount / sub_stream_num || parent_info->outBuffCount > CHUNK_LOSE*Xcount / sub_stream_num) {
				// If this substream is from PK, we have no idea to rescue it. Otherwise, find this substream's
				// parent, and cut the testing substream from it(if have testing substream from it). If cannot find 
				// testing substream, cut other normal substream
				if (parent_info->peerInfo.pid == PK_PID) {
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 1 Parent is PK. Cannot rescue", (int)(CHUNK_LOSE*Xcount) / sub_stream_num);
					debug_printf("[RESCUE_TYPE] 1. Parent is PK. Cannot rescue %f \n", (CHUNK_LOSE*Xcount) / sub_stream_num);
					_log_ptr->write_log_format("s(u) s f \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 1. Parent is PK. Cannot rescue", (CHUNK_LOSE*Xcount) / sub_stream_num);
					
					// Parent 是 PK 的情況下不重置，避免 Timeout Detection 無法偵測，只重置 lost_packet_count
					//ResetDetection(ss_id);	
					ss_table[ss_id]->lost_packet_count = 0;
				}
				else {
					if (ss_table[ss_id]->block_rescue == FALSE) {
						if (ss_table[ss_id]->state.state == SS_STABLE2) {
							if (ss_table[ss_id]->state.rescue_after_fail == true) {
								// RESCUE or MERGE
								UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
								unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
								unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
								unsigned long pk_new_manifest = pk_old_manifest | manifest;
								unsigned long parent_new_manifest = parent_old_manifest & ~manifest;
								bool need_source = true;

								SetSubstreamState(ss_id, SS_RESCUE2);
								debug_printf("[RESCUE_TYPE] 1. %d-second-ago-packet is lost \n", BUFF_SIZE);

								// Log the messages
								_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
								_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 1.", BUFF_SIZE, "second-ago-packet is lost", parent_info->peerInfo.pid);
								_log_ptr->write_log_format("s(u) s u s u d s \n", __FUNCTION__, __LINE__,
									"[RESCUE_TYPE] 1. Rescue in STABLE state. substreamID =", ss_id,
									"parentID=", parent_info->peerInfo.pid,
									BUFF_SIZE, "second-ago-packet is lost");

								ss_table[ss_id]->data.previousParentPID = parent_info->peerInfo.pid;
								ss_table[ss_id]->state.is_testing = false;

								// Update manifest
								set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
								set_parent_manifest(parent_info, parent_new_manifest);
								SetSubstreamParent(manifest, PK_PID);
								_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);
								if (parent_info->peerInfo.manifest == 0) {
									_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[REASON] manifest = 0 parent", parent_info->peerInfo.pid);
									_peer_ptr->CloseParent(parent_info->peerInfo.pid, false, "[REASON] manifest = 0");
									_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[CHECK POINT] map_pid_parent.size()", map_pid_parent.size());
								}

								//send_parentToPK(manifest, PK_PID+1);
								NeedSourceDecision(&need_source);
								//send_rescueManifestToPK(pk_new_manifest, need_source);
								send_rescueManifestToPK(manifest, need_source);
								ss_table[ss_id]->state.dup_src = need_source;

								// Reset source delay parameters of testing substream of this parent
								ResetDetection(ss_id);
							}
							else {
								// MOVE
								UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
								
								SetSubstreamState(ss_id, SS_STABLE2);
								debug_printf("[RESCUE_TYPE] 1M. %d-second-ago-packet is lost \n", BUFF_SIZE);

								// Log the messages
								_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 1M.", BUFF_SIZE, "second-ago-packet is lost", parent_info->peerInfo.pid);
								_log_ptr->write_log_format("s(u) s u s u d s \n", __FUNCTION__, __LINE__,
									"[RESCUE_TYPE] 1M. Rescue in STABLE state. substreamID =", ss_id,
									"parentID=", parent_info->peerInfo.pid,
									BUFF_SIZE, "second-ago-packet is lost");
								ss_table[ss_id]->state.rescue_after_fail = true;
							}
						}
						else if (ss_table[ss_id]->state.state == SS_CONNECTING2) {
							// No more trigger rescue again
							debug_printf("Should not happen \n");
						}
						else if (ss_table[ss_id]->state.state == SS_RESCUE2) {
							// No more trigger rescue again
							debug_printf("Should not happen \n");
						}
						else if (ss_table[ss_id]->state.state == SS_TEST2) {
							if (ss_table[ss_id]->state.rescue_after_fail == true) {
								// RESCUE or MERGE
								UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
								unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
								unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
								unsigned long pk_new_manifest = pk_old_manifest | manifest;
								unsigned long parent_new_manifest = parent_old_manifest & ~manifest;
								bool need_source = true;

								SetSubstreamState(ss_id, SS_RESCUE2);
								debug_printf("[RESCUE_TYPE] 1. %d-second-ago-packet is lost \n", BUFF_SIZE);

								// Log the messages
								_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
								_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 1 3", BUFF_SIZE, "second-ago-packet is lost", parent_info->peerInfo.pid);
								_log_ptr->write_log_format("s(u) s u s u d s \n", __FUNCTION__, __LINE__,
									"[RESCUE_TYPE] 1. Rescue in TEST state. substreamID =", ss_id,
									"parentID=", parent_info->peerInfo.pid,
									BUFF_SIZE, "second-ago-packet is lost");

								ss_table[ss_id]->state.is_testing = false;

								// Update manifest
								set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
								set_parent_manifest(parent_info, parent_new_manifest);
								SetSubstreamParent(manifest, PK_PID);
								_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);
								if (parent_info->peerInfo.manifest == 0) {
									_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[REASON] manifest = 0 parent", parent_info->peerInfo.pid);
									_peer_ptr->CloseParent(parent_info->peerInfo.pid, false, "[REASON] manifest = 0");
									_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[CHECK POINT] map_pid_parent.size()", map_pid_parent.size());
								}

								//send_parentToPK(manifest, PK_PID+1);
								NeedSourceDecision(&need_source);
								//send_rescueManifestToPK(pk_new_manifest, need_source);
								send_rescueManifestToPK(manifest, need_source);
								ss_table[ss_id]->state.dup_src = need_source;

								// Reset source delay parameters of testing substream of this parent
								ResetDetection(ss_id);
							}
							else {
								// MOVE
								UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
								
								SetSubstreamState(ss_id, SS_STABLE2);
								debug_printf("[RESCUE_TYPE] 1M. %d-second-ago-packet is lost \n", BUFF_SIZE);

								// Log the messages
								_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 1M.", BUFF_SIZE, "second-ago-packet is lost", parent_info->peerInfo.pid);
								_log_ptr->write_log_format("s(u) s u s u d s \n", __FUNCTION__, __LINE__,
									"[RESCUE_TYPE] 1M. Rescue in TEST state. substreamID =", ss_id,
									"parentID=", parent_info->peerInfo.pid,
									BUFF_SIZE, "second-ago-packet is lost");
								ss_table[ss_id]->state.rescue_after_fail = true;
							}
						}
						else {
							// Unexpected state
						}
					}
					else {
						debug_printf("Block Rescue [RESCUE_TYPE] 1. %d-second-ago-packet is lost \n", BUFF_SIZE);
						_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d s \n", "pid", my_pid, manifest, "Block Rescue[RESCUE_TYPE] 1", BUFF_SIZE, "second-ago-packet is lost");
						_log_ptr->write_log_format("s(u) s u s u d s \n", __FUNCTION__, __LINE__,
							"Block Rescue [RESCUE_TYPE] 1. Rescue in TEST state. substreamID =", ss_id,
							"parentID=", parent_info->peerInfo.pid,
							BUFF_SIZE, "second-ago-packet is lost");
						ResetDetection(ss_id);
						ss_table[ss_id]->state.rescue_after_fail = true;
					}
				}
			}
		}
		else {
			debug_printf("[WARNING] Skip the packet. buffer window %d < %d \n", Xcount*BUFF_SIZE, leastCurrDiff);
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "[WARNING] Skip the packet. buffer window", Xcount*BUFF_SIZE, "<", leastCurrDiff);
		}
	}
	
	// Evaluate the quality
	_log_ptr->timerGet(&ss_table[ss_id]->latest_pkt_client_time);
	quality_source_delay_count(sockfd, ss_id, chunk_ptr->header.sequence_number);
	
	/// Send the stream data to player in order of sequence
	//for ( ; _current_send_sequence_number < (_least_sequence_number-5); ) {
	for ( ; _current_send_sequence_number < (_least_sequence_number); ) {
	
		//_current_send_sequence_number 指向的地方還沒到 ,不做處理等待並return
		if ((**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number) {
			//return ;
			break;
		}
		
		if((**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.stream == STRM_TYPE_MEDIA) {

			//以下為丟給player
			map<int, stream *>::iterator fd_stream_iter;	// <stream_fd, stream *>
			#ifdef RECORD_FILE
			record_file(*(buf_chunk_t + (_current_send_sequence_number % _bucket_size)));
			#endif
			for (fd_stream_iter = _map_stream_fd_stream.begin(); fd_stream_iter != _map_stream_fd_stream.end(); fd_stream_iter++) {
				//per fd mean a player
				strm_ptr = fd_stream_iter->second;
				
				//stream_id 和request 一樣才add chunk
				if ((strm_ptr->_reqStreamID) == (**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.stream_id ) { 
					//if ((_current_send_sequence_number%15) > 0) {
						strm_ptr->add_chunk(*(buf_chunk_t + (_current_send_sequence_number % _bucket_size)));
					//}
					//_net_ptr->epoll_control(fd_stream_iter->first, EPOLL_CTL_MOD, EPOLLOUT);
					//debug_printf("******** %d  %s \n", (*(buf_chunk_t + (_current_send_sequence_number % _bucket_size)))->header.length, (char *)(*(buf_chunk_t + (_current_send_sequence_number % _bucket_size)))->buf);
				}
			}
			_current_send_sequence_number++;
		}
	}
}

void pk_mgr::handle_kickout(struct chunk_t *chunk_ptr, int sockfd)
{
	struct peer_exit* peer_exit_ptr = (struct peer_exit*)chunk_ptr;
	exit_code = peer_exit_ptr->kick_reason;
	
	
	debug_printf("pid %d is kicked. Reason = %d(%d) \n", my_pid, peer_exit_ptr->kick_reason, exit_code);
	_log_ptr->write_log_format("s(u) s u s u(u) \n", __FUNCTION__, __LINE__, "pid", my_pid, "is kicked. Reason =", peer_exit_ptr->kick_reason, exit_code);
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u \n", "pid", my_pid, "is kicked. Reason =", peer_exit_ptr->kick_reason);
	
	// The followings are sensed by PK
	if (exit_code == CLOSE_CHANNEL) {
		debug_printf("_channel_id = %d, peer_exit_ptr->channel_id = %d \n", _channel_id, peer_exit_ptr->channel_id);
		debug_printf("Close channel %u \n", peer_exit_ptr->channel_id);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Close channel", peer_exit_ptr->channel_id);
	}
	else if (exit_code == CLOSE_STREAM) {
		// TODO
	}
	else if (exit_code == BUFFER_OVERFLOW) {
		// TODO
	}
	else if (exit_code == CHANGE_PK) {
		debug_printf("CHANGE_PK  _channel_id = %d, peer_exit_ptr->channel_id = %d \n", _channel_id, peer_exit_ptr->channel_id);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "CHANGE_PK", peer_exit_ptr->channel_id);
	}
	else if (exit_code == FAILED_SERVICE) {
		debug_printf("FAILED_SERVICE  _channel_id = %d, peer_exit_ptr->channel_id = %d \n", _channel_id, peer_exit_ptr->channel_id);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "FAILED_SERVICE", peer_exit_ptr->channel_id);
	}
	else if (exit_code == UNKNOWN) {
	
	}
	else {
		PAUSE
	}
	
	_logger_client_ptr->log_to_server(LOG_PEER_LEAVE, 0);
	
	*(_net_ptr->_errorRestartFlag) = RESTART;
	_logger_client_ptr->log_exit();
	data_close(_sock, "Be kicked by PK");
}

// Transmit down-stream(UPLOAD) to other peer if SSID match and in map_pid_child(real children)
void pk_mgr::HandleRelayStream(struct chunk_t *chunk_ptr)
{
	UINT32 ss_id = chunk_ptr->header.sequence_number % sub_stream_num;
	UINT32 manifest = SubstreamIDToManifest(ss_id);
	
	for (map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter = map_pid_child.begin(); pid_peer_info_iter != map_pid_child.end(); pid_peer_info_iter++) {
		unsigned long child_pid;
		int child_sock;
		struct peer_info_t *child_peer = NULL;
		queue<struct chunk_t *> *queue_out_data_ptr = NULL;
		bool can_push_data = true;

		//// Test rescue algorithm (隨機掉一個封包)
		srand(time(NULL));
		int ran = rand() % 4;
		if (chunk_ptr->header.sequence_number % (5*pkt_rate + ran) == 0) {
			break;
		}

		child_pid = pid_peer_info_iter->first;		// Get child-Pid
		child_peer = pid_peer_info_iter->second;	// Get child info

		_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "Child", child_pid, "manifest", child_peer->manifest, "state", pid_peer_info_iter->second->connection_state);

		//if (pid_peer_info_iter->second->connection_state == PEER_CONNECTED_CHILD) {
		if (pid_peer_info_iter->second->connection_state == PEER_SELECTED) {
			if (_peer_ptr->map_out_pid_fd.find(child_pid) != _peer_ptr->map_out_pid_fd.end()) {
				child_sock = _peer_ptr->map_out_pid_fd[child_pid];	// Get child socket

				map<int, queue<struct chunk_t *> *>::iterator iter;
				iter = _peer_ptr->map_fd_out_data.find(child_sock);
				if (_peer_ptr->map_fd_out_data.find(child_sock) != _peer_ptr->map_fd_out_data.end()) {
					queue_out_data_ptr = _peer_ptr->map_fd_out_data[child_sock];	// Get queue_out_data_ptr
				}
				else {
					handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_fd but not found in map_fd_out_data", __FUNCTION__, __LINE__);
				}
			}
			else if (_peer_ptr->map_out_pid_udpfd.find(child_pid) != _peer_ptr->map_out_pid_udpfd.end()) {
				child_sock = _peer_ptr->map_out_pid_udpfd[child_pid];	// Get child socket

				map<int, queue<struct chunk_t *> *>::iterator iter;
				iter = _peer_ptr->map_udpfd_out_data.find(child_sock);
				if (_peer_ptr->map_udpfd_out_data.find(child_sock) != _peer_ptr->map_udpfd_out_data.end()) {
					queue_out_data_ptr = _peer_ptr->map_udpfd_out_data[child_sock];	// Get queue_out_data_ptr
				}
				else {
					handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
				}
			}
			else {
				debug_printf("Not found pid %d in map_out_pid_fd \n", pid_peer_info_iter->second->pid);
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Not found pid", pid_peer_info_iter->second->pid, "in map_out_pid_fd");
				//handle_error(UNKNOWN, "[DEBUG] Not found pid in map_out_pid_fd", __FUNCTION__, __LINE__);
				continue;
			}



			/*
			for (list<unsigned long>::iterator iter = _peer_ptr->priority_children.begin(); iter != _peer_ptr->priority_children.end(); iter++) {
				// iterator 只向這次的 sock，可能是因為自己的最高 priority 或是 前面的 child 的 queue 都已經空了
				if (*iter == child_pid) {
					break;
				}
				
				// 檢查那些更高 priority 的 child 的 queue 是否為空
				//list<unsigned long>::reverse_iterator iter = _peer_ptr->priority_children.rbegin();
				int other_child_sock = -1;
				queue<struct chunk_t *> *other_child_queue_out_ctrl_ptr = NULL;
				queue<struct chunk_t *> *other_child_queue_out_data_ptr = NULL;

				if (map_pid_child.find(*iter) != map_pid_child.end()) {
					if (map_pid_child.find(*iter)->second->connection_state == PEER_SELECTED) {
						if (_peer_ptr->map_out_pid_udpfd.find(*iter) != _peer_ptr->map_out_pid_udpfd.end()) {
							other_child_sock = _peer_ptr->map_out_pid_udpfd.find(*iter)->second;
							if (_peer_ptr->map_udpfd_out_data.find(other_child_sock) != _peer_ptr->map_udpfd_out_data.end()) {
								other_child_queue_out_data_ptr = _peer_ptr->map_udpfd_out_data[other_child_sock];	// Get queue_out_data_ptr
							}
							else {
								handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
							}

							if (_peer_ptr->map_udpfd_out_ctrl.find(other_child_sock) != _peer_ptr->map_udpfd_out_ctrl.end()) {
								other_child_queue_out_ctrl_ptr = _peer_ptr->map_udpfd_out_ctrl[other_child_sock];	// Get queue_out_data_ptr
							}
							else {
								handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
							}
						}
					}
					// 找到更高 priority 的 child，檢查它 ctrl queue 和 data queue 是否都為空
					if (other_child_queue_out_ctrl_ptr != NULL && other_child_queue_out_data_ptr != NULL) {
						if (other_child_queue_out_ctrl_ptr->size() + other_child_queue_out_data_ptr->size() > 120) {
							can_push_data = false;
						}
					}
				}
			}
			*/


			if (can_push_data == true) {
				// Check whether child's manifest are equal to chunk_ptr's(new chunk) or not
				if (queue_out_data_ptr != NULL) {
					if (child_peer->manifest & (1 << (chunk_ptr->header.sequence_number % sub_stream_num))) {
						// Put buf_chunk_t[index](chunk buffer) into output queue
						queue_out_data_ptr->push(*(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size)));
						//_net_ptr->epoll_control(child_sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
						_log_ptr->write_log_format("s(u) s u s u s d \n", __FUNCTION__, __LINE__,
							"Transmit packet to child", child_pid,
							"substreamID", ss_id,
							"seq", chunk_ptr->header.sequence_number);


						int udt_max_sndbuf;
						int len1 = sizeof(udt_max_sndbuf);
						int32_t udt_sndbuf;
						int len2 = sizeof(udt_sndbuf);
						int64_t udt_maxbw;
						int len3 = sizeof(udt_maxbw);
						int64_t aaa = 93750;
						//UDT::getsockopt(child_sock, 0, UDT_SNDBUF, &udt_max_sndbuf, &len1);
						//UDT::getsockopt(child_sock, 0, UDT_SNDDATA, &udt_sndbuf, &len2);
						//UDT::setsockopt(child_sock, 0, UDT_MAXBW, &aaa, sizeof(aaa));
						//UDT::getsockopt(child_sock, 0, UDT_MAXBW, &udt_maxbw, &len3);

						//debug_printf("sock %d child %d : UDT_SNDBUF %d UDT_SNDDATA %d  maxbw %d \n", child_sock, child_pid, udt_max_sndbuf, udt_sndbuf, udt_maxbw);

					}
				}
			}
		}
	}

	unsigned int totalRescueNum = 0;
	for (map<unsigned long, struct peer_info_t *>::iterator iter = map_pid_child.begin(); iter != map_pid_child.end(); iter++) {
		if (iter->second->connection_state == PEER_CONNECTED_CHILD) {
			unsigned long tempManifest = iter->second->manifest;
			for (unsigned long i = 0; i < sub_stream_num; i++) {
				if ((1 << i) & tempManifest) {
					totalRescueNum++;
				}
			}
		}
	}

	priq_cnt2 = (priq_cnt2 + 1) % 10;
	if (priq_cnt2 == 0) {
		//_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u d \n", "[CHILD_PRIORITY] pid", my_pid, totalRescueNum);
	}

	for (list<unsigned long>::iterator iter = _peer_ptr->priority_children.begin(); iter != _peer_ptr->priority_children.end(); iter++) {
		// 檢查那些更高 priority 的 child 的 queue 是否為空
		//list<unsigned long>::reverse_iterator iter = _peer_ptr->priority_children.rbegin();
		int other_child_sock = -1;
		queue<struct chunk_t *> *other_child_queue_out_ctrl_ptr = NULL;
		queue<struct chunk_t *> *other_child_queue_out_data_ptr = NULL;

		if (map_pid_child.find(*iter) != map_pid_child.end()) {
			if (map_pid_child.find(*iter)->second->connection_state == PEER_SELECTED) {
				if (_peer_ptr->map_out_pid_udpfd.find(*iter) != _peer_ptr->map_out_pid_udpfd.end()) {
					other_child_sock = _peer_ptr->map_out_pid_udpfd.find(*iter)->second;
					if (_peer_ptr->map_udpfd_out_data.find(other_child_sock) != _peer_ptr->map_udpfd_out_data.end()) {
						other_child_queue_out_data_ptr = _peer_ptr->map_udpfd_out_data[other_child_sock];	// Get queue_out_data_ptr
					}
					else {
						handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
					}

					if (_peer_ptr->map_udpfd_out_ctrl.find(other_child_sock) != _peer_ptr->map_udpfd_out_ctrl.end()) {
						other_child_queue_out_ctrl_ptr = _peer_ptr->map_udpfd_out_ctrl[other_child_sock];	// Get queue_out_data_ptr
					}
					else {
						handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
					}
					
					//_log_ptr->write_log_format("s(u) s\tu\td\td\td \n", __FUNCTION__, __LINE__, "child", *iter, other_child_queue_out_ctrl_ptr->size(), other_child_queue_out_data_ptr->size(), other_child_queue_out_ctrl_ptr->size() + other_child_queue_out_data_ptr->size());
				}
			}
			else {
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] child", *iter);
			}
		}
		else {
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] child", *iter);
		}
	}

	for (list<unsigned long>::reverse_iterator iter = _peer_ptr->priority_children.rbegin(); iter != _peer_ptr->priority_children.rend(); iter++) {
		// 檢查那些更高 priority 的 child 的 queue 是否為空
		//list<unsigned long>::reverse_iterator iter = _peer_ptr->priority_children.rbegin();
		int other_child_sock = -1;
		queue<struct chunk_t *> *other_child_queue_out_ctrl_ptr = NULL;
		queue<struct chunk_t *> *other_child_queue_out_data_ptr = NULL;

		if (map_pid_child.find(*iter) != map_pid_child.end()) {
			if (map_pid_child.find(*iter)->second->connection_state == PEER_SELECTED) {
				if (_peer_ptr->map_out_pid_udpfd.find(*iter) != _peer_ptr->map_out_pid_udpfd.end()) {
					other_child_sock = _peer_ptr->map_out_pid_udpfd.find(*iter)->second;
					if (_peer_ptr->map_udpfd_out_data.find(other_child_sock) != _peer_ptr->map_udpfd_out_data.end()) {
						other_child_queue_out_data_ptr = _peer_ptr->map_udpfd_out_data[other_child_sock];	// Get queue_out_data_ptr
					}
					else {
						handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
					}

					if (_peer_ptr->map_udpfd_out_ctrl.find(other_child_sock) != _peer_ptr->map_udpfd_out_ctrl.end()) {
						other_child_queue_out_ctrl_ptr = _peer_ptr->map_udpfd_out_ctrl[other_child_sock];	// Get queue_out_data_ptr
					}
					else {
						handle_error(UNKNOWN, "[DEBUG] Found child-peer in map_out_pid_udpfd but not found in map_udpfd_out_data", __FUNCTION__, __LINE__);
					}

					_log_ptr->write_log_format("s(u) s\tu\tu\td\td\td \n", __FUNCTION__, __LINE__, "[LOWEST PRIORITY] child", *iter, rescueNumAccumulate(), other_child_queue_out_ctrl_ptr->size(), other_child_queue_out_data_ptr->size(), other_child_queue_out_ctrl_ptr->size() + other_child_queue_out_data_ptr->size());
					break;
				}
			}
			else {
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] child", *iter);
			}
		}
		else {
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] child", *iter);
		}
	}
}

void pk_mgr::record_file_init(int stream_id)
{
	//record_file_fp = fopen("file", "wb");
	
	const char flvHeader[] = { 'F', 'L', 'V', 0x01,
								0x00,	 				// 0x04 == audio, 0x01 == video
								0x00, 0x00, 0x00, 0x09,
								0x00, 0x00, 0x00, 0x00
	};
	/*
	map<int, struct update_stream_header *>::iterator  iter;
	struct update_stream_header *protocol_header = NULL;
	
	if (map_streamID_header.find(stream_id) != map_streamID_header.end()) {
		protocol_header = map_streamID_header.find(stream_id)->second;
		fwrite((char *)flvHeader, 1, sizeof(flvHeader), record_file_fp);
		if (protocol_header->len > 0) {
			fwrite((char *)protocol_header->header, 1, protocol_header->len, record_file_fp);
		}
	}
	*/
}

void pk_mgr::record_file(chunk_t *chunk_ptr)
{
	bool isKeyFrame = false;
	/*
	if (first_pkt) {
		char flvBitFlag;
		if (*(char*)(chunk_ptr->buf) == 0x09) {		//video
			flvBitFlag = *(char*)((chunk_ptr->buf) + 11);  //get first byte
			if ((flvBitFlag & 0xf0) >> 4 == 0x01 ) {
				isKeyFrame = true;
			}
			else {	
				isKeyFrame = false;
			}
		}
		else {		//audio
			isKeyFrame = false;
		}
		if (!isKeyFrame) {
			return ;
		}
		first_pkt = false;
	}
	*/
	fwrite((char *)chunk_ptr->buf, 1, chunk_ptr->header.length, record_file_fp);
}

/****************************************************/
/*		Functions of synchronization				*/
/****************************************************/
void pk_mgr::reset_source_delay_detection(unsigned long sub_id)
{
	set_rescue_state(sub_id,0);
}

//state  /0 detection/ 1rescue/ 2testing
void pk_mgr::set_rescue_state(unsigned long sub_id, int state)
{
	map<unsigned long, struct source_delay *>::iterator delay_table_iter;
	int old_state = -1;
	
	delay_table_iter = delay_table.find(sub_id);
	if (delay_table_iter == delay_table.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] can not find source struct in table in send_capacity_to_pk\n");
		handle_error(UNKNOWN, "[ERROR] can not find source struct in table in send_capacity_to_pk", __FUNCTION__, __LINE__);
	}
	
	old_state = delay_table_iter->second->rescue_state;
	_log_ptr->write_log_format("s(u) s u s d s d \n", __FUNCTION__, __LINE__,
														"substream", sub_id,
														"state changed from", old_state,
														"to", state);
	
	
	if (state == 0) {
		if (old_state == 0 || old_state == 2) {
			delay_table_iter->second->rescue_state = state;
		}
		else {
			debug_printf("[WARNING] set_rescue_state error from %d to %d \n", old_state, state);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d s d \n", "[WARNING] set_rescue_state error from", old_state, "to", state);
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "[WARNING] set_rescue_state error from", old_state, "to", state);
			PAUSE
		}
	}
	else if (state == 1) {
		if (old_state == 0) {
			delay_table_iter->second->rescue_state = state;
		}
		else {
			debug_printf("[WARNING] set_rescue_state error from %d to %d \n", old_state, state);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d s d \n", "[WARNING] set_rescue_state error from", old_state, "to", state);
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "[WARNING] set_rescue_state error from", old_state, "to", state);
			PAUSE
		}
	}
	else if (state == 2) {
		if (old_state == 0 || old_state == 1) {
			delay_table_iter->second->rescue_state = state;
		}
		else {
			debug_printf("[WARNING] set_rescue_state error from %d to %d \n", old_state, state);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d s d \n", "[WARNING] set_rescue_state error from", old_state, "to", state);
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "[WARNING] set_rescue_state error from", old_state, "to", state);
			PAUSE
		}
	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] set_rescue_state error. unknown state", state);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] set_rescue_state error. unknown state", state);
	}
}

int pk_mgr::check_rescue_state(unsigned long sub_id, int state)
{
	map<unsigned long, struct source_delay *>::iterator delay_table_iter;
	
	delay_table_iter = delay_table.find(sub_id);
	if (delay_table_iter == delay_table.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","[ERROR] can not find source struct in table in send_capacity_to_pk\n");
		_logger_client_ptr->log_exit();
	}

	if (state == delay_table_iter->second->rescue_state) {
		return 1;
	}
	else {
		return 0;
	}
}

void pk_mgr::SetSubstreamState(unsigned long ss_id, int state)
{
	_log_ptr->write_log_format("s(u) s u s d s d \n", __FUNCTION__, __LINE__,
														"Substream", ss_id, 
														"state changed from", ss_table[ss_id]->state.state,
														"->", state);
	ss_table[ss_id]->state.state = state;
}

void pk_mgr::SetSubstreamBlockRescue(unsigned long ss_id, unsigned long type)
{
	ss_table[ss_id]->block_rescue = TRUE;
	_log_ptr->timerGet(&(ss_table[ss_id]->block_rescue_timer));
	_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "Substream", ss_id, "Block rescue, type", type);
}

void pk_mgr::SetSubstreamUnblockRescue(unsigned long ss_id)
{
	ss_table[ss_id]->block_rescue = FALSE;
	_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Substream", ss_id, "Unblock rescue");
}
         
void pk_mgr::SetSubstreamParent(unsigned long manifest, unsigned long pid)
{
	for (unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
		if ((1 << ss_id) & manifest) {
			ss_table[ss_id]->previous_parent_pid = ss_table[ss_id]->current_parent_pid;
			ss_table[ss_id]->current_parent_pid = pid;
			_log_ptr->write_log_format("s(u) s d s u s u \n", __FUNCTION__, __LINE__,
														"ss_id", ss_id,
														"current parent", ss_table[ss_id]->current_parent_pid, 
														"previous parent", ss_table[ss_id]->previous_parent_pid);
		}
	}
}

void pk_mgr::set_parent_manifest(struct peer_connect_down_t *parent_info, UINT32 manifest)
{
	int old_manifest = parent_info->peerInfo.manifest;
	parent_info->peerInfo.manifest = manifest;
	_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "parent pid", parent_info->peerInfo.pid,
																			  "manifest change from", old_manifest,
																			  "to", parent_info->peerInfo.manifest);
}

void pk_mgr::syn_table_init(int pk_sock)
{
	syn_table.client_abs_start_time = 0;
	syn_table.init_flag = 1;
	syn_table.start_seq = 0;
	send_syn_token_to_pk(pk_sock);
}


// Send SYN request to pk
//struct syn_token_send:
//		struct chunk_header_t header
//		unsigned long reserve
void pk_mgr::send_syn_token_to_pk(int pk_sock)
{
	syncLock = 1;
	syn_table.state = SYNC_ONGOING;
	
	struct syn_token_send *syn_token_send_ptr = new struct syn_token_send;
	if (!syn_token_send_ptr) {
		exit_code = MALLOC_ERROR;
		debug_printf("Malloc syn_token_send_ptr failed \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] Malloc syn_token_send_ptr failed");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u \n", "[ERROR] Malloc syn_token_send_ptr failed", __LINE__);
		_logger_client_ptr->log_exit();
		data_close(_sock, "[ERROR] Malloc syn_token_send_ptr failed");
		//PAUSE
		return ;
	}
	memset(syn_token_send_ptr, 0, sizeof(struct syn_token_send));

	syn_token_send_ptr->header.cmd = CHNK_CMD_PEER_SYN;
	syn_token_send_ptr->header.rsv_1 = REQUEST;
	syn_token_send_ptr->header.length = sizeof(struct syn_token_send) - sizeof(struct chunk_header_t);
	syn_token_send_ptr->reserve = 0;
	
	_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Send synchronization packets to PK", syn_token_send_ptr->header.length);
	
	int send_byte;
	char send_buf[sizeof(struct syn_token_send)] = {0};
	
	memcpy(send_buf, syn_token_send_ptr, sizeof(struct syn_token_send));
	
	_net_ptr->set_blocking(pk_sock);

	_log_ptr->timerGet(&lastSynStartclock);
	_log_ptr->timerGet(&syn_round_start);

	debug_printf("%s \n", __FUNCTION__);
	send_byte = _net_ptr->send(pk_sock, send_buf, sizeof(struct syn_token_send), 0);
	_log_ptr->write_log_format("s(u) d d \n", __FUNCTION__, __LINE__, send_byte, sizeof(struct syn_token_send));

	if (send_byte <= 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		exit_code = PK_SOCKET_ERROR;
		debug_printf("[ERROR] Cannot send sync token to pk \n");
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Cannot send sync token to pk", send_byte, socketErr);
		handle_error(MALLOC_ERROR, "[ERROR] Cannot send sync token to pk", __FUNCTION__, __LINE__);
	}
	else {
		if (syn_token_send_ptr) {
			delete syn_token_send_ptr;
		}
		_net_ptr->set_nonblocking(pk_sock);
	}
}

//syn_round froce to using tickTime
//struct syn_token_receive
//		struct chunk_header_t header
//		unsigned long seq_now
//		unsigned long pk_RecvTime
//		unsigned long pk_SendTime
//
// send_syn_token_to_pk() <-> syn_recv_handler()
// Call the function only when syn_table.state == SYNC_ONGOING
void pk_mgr::syn_recv_handler(struct syn_token_receive* syn_struct_back_token)
{
	struct timerStruct syn_round_end;
	
	_log_ptr->timerGet(&syn_round_end);
	
	// Divide into first time sync and other syncs
	if (syn_table.first_sync_done == true) {
		volatile unsigned long secondSyn_absTime = 0;
		volatile unsigned long serverSynPeriod = 0;			// Time period of the estimated time that peer in pk's time line
		volatile unsigned long serverPacketSynPeriod = 0;	// The time interval that pk receives peer's sync token
		volatile unsigned long clockPeriodTime = 0;
		volatile unsigned long tickPeriodTime = 0;
		int clockPeriodDiff = 0;
		int tickPeriodDiff = 0;

		
		syn_round_time = _log_ptr->diff_TimerGet_ms(&syn_round_start, &syn_round_end) -(syn_struct_back_token->pk_SendTime - syn_struct_back_token->pk_RecvTime);
		if (syn_round_time < 0) {
			syn_round_time = 0;
		}
		
		secondSyn_absTime = syn_struct_back_token->pk_RecvTime - (syn_round_time/2);

		serverSynPeriod = secondSyn_absTime - syn_table.client_abs_start_time;
		serverPacketSynPeriod = syn_struct_back_token->pk_RecvTime - lastPKtimer;
		
		//select a good timeMod
		_log_ptr->timerMod = MOD_TIME__CLOCK;
		clockPeriodTime = _log_ptr->diff_TimerGet_ms(&syn_table.start_clock, &lastSynStartclock);
		_log_ptr->timerMod = MOD_TIME_TICK;
		tickPeriodTime = _log_ptr->diff_TimerGet_ms(&syn_table.start_clock, &lastSynStartclock);

		
		debug_printf("packet  period timer = %d \n", serverPacketSynPeriod);
		_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__,
															"serverSynPeriod =", serverSynPeriod,
															"clockPeriodTime =", clockPeriodTime,
															"tickPeriodTime =", tickPeriodTime,
															"serverPacketSynPeriod =", serverPacketSynPeriod);
		
		clockPeriodDiff = abs((int)(serverPacketSynPeriod - clockPeriodTime));
		tickPeriodDiff = abs((int)(serverPacketSynPeriod - tickPeriodTime));

		if (clockPeriodDiff <= 50 || tickPeriodDiff <= 50) {
			reSynTime *= 2;
			debug_printf("Good synchronization. Change reSynTime from %d to %d \n", reSynTime/2, reSynTime);
			_log_ptr->write_log_format("s(u) s s d s d \n", __FUNCTION__, __LINE__, "Good synchronization.",
																			"Change reSynTime from", reSynTime/2,
																			"(ms) to", reSynTime);
		}
		else if (clockPeriodDiff >= 250 && tickPeriodDiff>= 250) {
			reSynTime /= 2;
			if (reSynTime < 5000) {
				reSynTime = 5000;
			}
			debug_printf("Bad synchronization. Change reSynTime from %d(ms) to %d(ms) \n", reSynTime*2, reSynTime);
			_log_ptr->write_log_format("s(u) s s d s d \n", __FUNCTION__, __LINE__, "Bad synchronization.",
																			"Change reSynTime from", reSynTime*2,
																			"(ms) to", reSynTime);
		}
		else {
			debug_printf("Just so-so synchronization. reSynTime = %d unchanged \n", reSynTime);
			_log_ptr->write_log_format("s(u) s s d s \n", __FUNCTION__, __LINE__, "Just so-so synchronization.",
																			"reSynTime =", reSynTime, "unchanged");
		}
		
		//select a smaller PeriodDiff , means timer is close to PK server 
		if (clockPeriodDiff >= tickPeriodDiff) {
			debug_printf("Select MOD_TIME_TICK \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Select MOD_TIME_TICK");
			_log_ptr->timerMod = MOD_TIME_TICK;
		}
		else {
			debug_printf("Select MOD_TIME__CLOCK \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Select MOD_TIME__CLOCK");
			_log_ptr->timerMod = MOD_TIME__CLOCK;
		}

		//reset set all syn_table
		syn_table.client_abs_start_time = secondSyn_absTime;
		memcpy(&syn_table.start_clock, &lastSynStartclock, sizeof(struct timerStruct));
		totalMod = 0;
		lastPKtimer = syn_struct_back_token->pk_RecvTime ;
	}
	else {
		syn_round_time = _log_ptr->diff_TimerGet_ms(&syn_round_start, &syn_round_end) - (syn_struct_back_token->pk_SendTime - syn_struct_back_token->pk_RecvTime);
		if (syn_round_time < 0) {
			syn_round_time = 0;
		}
		
		memcpy(&syn_table.start_clock, &lastSynStartclock, sizeof(struct timerStruct));
		
		syn_table.client_abs_start_time = syn_struct_back_token->pk_RecvTime - (syn_round_time/2);
		
		syn_table.start_seq = syn_struct_back_token->seq_now;
		lastPKtimer = syn_struct_back_token->pk_RecvTime;
		syn_table.first_sync_done = true;
		
		_log_ptr->write_log_format("s(u) s u s u s d s u \n", __FUNCTION__, __LINE__,
															"client_abs_start_time =", syn_table.client_abs_start_time,
															"pk_RecvTime =", syn_struct_back_token->pk_RecvTime,
															"syn_round_time =", syn_round_time,
															"syn_table.start_seq =", syn_table.start_seq);
		
	}
	
	syncLock = 0;
	syn_table.state = SYNC_FINISH;
}
/****************************************************/
/*		Functions of synchronization end			*/
/****************************************************/

void pk_mgr::add_stream(int stream_fd, stream *strm, unsigned strm_type)
{
	if (strm_type == STRM_TYPE_MEDIA) {
		_map_stream_fd_stream[stream_fd] = strm;
	}
}


void pk_mgr::del_stream(int stream_fd, stream *strm, unsigned strm_type)
{
	if (strm_type == STRM_TYPE_MEDIA) {
		_map_stream_fd_stream.erase(stream_fd);
	}
}

void pk_mgr::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;

	_log_ptr->write_log_format("s(u) s (s) \n", __FUNCTION__, __LINE__, "close PK", reason);
	debug_printf("close PK by %s \n", reason);
	_net_ptr->close(cfd);

	map<int, unsigned long>::iterator map_fd_pid_iter;
	for (map_fd_pid_iter = _peer_ptr->map_fd_pid.begin(); map_fd_pid_iter != _peer_ptr->map_fd_pid.end(); map_fd_pid_iter++) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
		if (map_fd_pid_iter->first == cfd) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
			debug_printf("pid = %lu \n", map_fd_pid_iter->second);
		}
	}
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
		if (*fd_iter == cfd) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[CHECK POINT]");
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}

	*(_net_ptr->_errorRestartFlag) = RESTART;
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
	//Detect substream 的buff
	ssDetect_ptr = (struct detectionInfo *)malloc(sizeof(struct detectionInfo) * sub_stream_num);
	memset(ssDetect_ptr, 0, sizeof(struct detectionInfo) * sub_stream_num); 
	for(unsigned long i = 0; i < sub_stream_num; i++) {
		ssDetect_ptr->previousParentPID = PK_PID + 1;
	}

	statsArryCount_ptr = (unsigned long *)malloc(sizeof(unsigned long) * (sub_stream_num+1));
	memset(statsArryCount_ptr, 0, sizeof(unsigned long) * (sub_stream_num+1)); 

	//	_beginthread(launchThread, 0,this );
}

//必須保證進入這個function 的substream 是依序的
void pk_mgr::rescue_detecion(struct chunk_t *chunk_ptr)
{

}

//必須保證進入這個function 的substream 是依序的
void pk_mgr::RescueDetecion(struct chunk_t *chunk_ptr)
{
	struct timerStruct newAlarm;
	UINT32 sourceTimeDiffOne;
	UINT32 localTimeDiffOne;
	UINT32 sourceTimeDiffTwo;
	UINT32 localTimeDiffTwo;
	double sourceBitrate;
	double localBitrate;
	int ss_id;
	
	ss_id = chunk_ptr->header.sequence_number % sub_stream_num;
	
	/*
	//如果是第一個封包，初始化
	if (!ssDetect_ptr[ss_id].last_seq) {  
		ssDetect_ptr[ss_id].last_timestamp = chunk_ptr->header.timestamp;
		ssDetect_ptr[ss_id].last_seq = chunk_ptr->header.sequence_number;
		ssDetect_ptr[ss_id].first_timestamp = chunk_ptr->header.timestamp;
		_log_ptr->timerGet(&ssDetect_ptr[ss_id].lastAlarm);
		_log_ptr->timerGet(&ssDetect_ptr[ss_id].firstAlarm);
		
		return;
	}
	*/
	
	//開始計算偵測
	//只有是 比上次記錄新的sequence_number 才做處理
	if (ss_table[ss_id]->data.last_seq < chunk_ptr->header.sequence_number) {
		_log_ptr->timerGet(&newAlarm);

		//////////////////////////////////////利用頻寬判斷(測量方法一)////////////////////////////////////////////
		// Check bandwidth per Xcount numbers of chucks
		ss_table[ss_id]->data.count_X++;
		ss_table[ss_id]->data.total_byte += chunk_ptr->header.length;
		_log_ptr->write_log_format("s(u) s d s d (d) \n", __FUNCTION__, __LINE__, "ss_table[", ss_id, "].total_byte =", ss_table[ss_id]->data.total_byte, chunk_ptr->header.length);

		//累積Xcount個封包後做判斷
		if (ss_table[ss_id]->data.count_X > Xcount) {

			sourceTimeDiffOne = chunk_ptr->header.timestamp - ss_table[ss_id]->data.first_timestamp;
			localTimeDiffOne = _log_ptr->diff_TimerGet_ms( &(ss_table[ss_id]->data.firstAlarm), &newAlarm);
			
			if (localTimeDiffOne < 1) {
				localTimeDiffOne = 1;
			}
			if (sourceTimeDiffOne < 1) {
				sourceTimeDiffOne = 1;
			}
			sourceBitrate = ((double)(ss_table[ss_id]->data.total_byte) / (double)sourceTimeDiffOne) * 8 * 1000 ;
			localBitrate  = ((double)(ss_table[ss_id]->data.total_byte) / (double)localTimeDiffOne) * 8 * 1000 ;

			ss_table[ss_id]->data.total_source_bitrate = sourceBitrate ;
			ss_table[ss_id]->data.total_local_bitrate = localBitrate;

			//做每個peer substream的加總 且判斷需不需要救
			//Measure();
			
			ss_table[ss_id]->data.measure_N++;  //從1開始計
			ss_table[ss_id]->data.total_byte = chunk_ptr->header.length;
			ss_table[ss_id]->data.count_X = 1;
			ss_table[ss_id]->data.firstAlarm = newAlarm;
			ss_table[ss_id]->data.first_timestamp = chunk_ptr->header.timestamp;
		}
		//////////////////////////////////////(測量方法一結束)///////////////////////////////////////////////////////////

		////////////////////////////////////單看兩個連續封包的delay取max (測量方法二)///////////////////////////////////
		sourceTimeDiffTwo = chunk_ptr->header.timestamp - ss_table[ss_id]->data.last_timestamp;
		localTimeDiffTwo = _log_ptr->diff_TimerGet_ms(&ss_table[ss_id]->data.lastAlarm, &newAlarm);
		if (localTimeDiffTwo > sourceTimeDiffTwo) {
			if (ss_table[ss_id]->data.total_buffer_delay < (int)(localTimeDiffTwo - sourceTimeDiffTwo)) {
				ss_table[ss_id]->data.total_buffer_delay = localTimeDiffTwo - sourceTimeDiffTwo;
			}
		}
		else {
			// do nothing
		}		

		ss_table[ss_id]->data.lastAlarm = newAlarm;		// (ss_table + ss_id)->lastAlarm = newAlarm;
		ss_table[ss_id]->data.last_timestamp = chunk_ptr->header.timestamp;		// (ss_table + ss_id)->last_timestamp = chunk_ptr->header.timestamp;
		//////////////////////////////////////(測量方法二結束)///////////////////////////////////////////////////////////

		ss_table[ss_id]->data.last_seq = chunk_ptr->header.sequence_number;
	}
	else if (ss_table[ss_id]->data.last_seq == chunk_ptr->header.sequence_number) {
		//doing nothing
	}
	else {
		//在某些特定的情況下會近來  但不影響運算
		//PAUSE
		//printf("why here old packet here??\n");
	}

	return;
}

//累積Xcount個packets後呼叫此function，計算每個parent-peer擁有的substream後判斷需不需要rescue
void pk_mgr::measure()
{	
	
}

//累積Xcount個packets後呼叫此function，計算每個parent-peer擁有的substream後判斷需不需要rescue
void pk_mgr::Measure()
{	
	unsigned long peerTestingManifest = 0;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	
	//for each parent-peer
	for (pid_peerDown_info_iter = map_pid_parent.begin(); pid_peerDown_info_iter != map_pid_parent.end(); pid_peerDown_info_iter++) {

		struct peer_connect_down_t *parent_info = NULL;
		unsigned long tempManifest = 0;
		unsigned long afterManifest = 0;
		unsigned long perPeerSS_num = 0;	// The number of substream of a parent-peer 
		int peerHighestSSID = -1;			// Max substream ID of the parent-peer
		double totalSourceBitrate = 0;
		double totalLocalBitrate = 0;
		unsigned int count_N = 0;
		unsigned int continuous_P = 0;
		unsigned int rescueSS = 0;
		int tempMax = 0;
		int testingSubStreamID = -1;
		unsigned long testingManifest = 0;
		
		memset(statsArryCount_ptr, 0, sizeof(unsigned long)*(sub_stream_num+1)); 
		
		parent_info = pid_peerDown_info_iter->second;
		
		tempManifest = parent_info->peerInfo.manifest;
		if (tempManifest == 0) {
			//_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] PID", parent_info->peerInfo.pid, "manifest = 0");
			continue;
		}
		
		// Get parent-peer's information, and calculate bit-rate
		for (unsigned long i = 0; i < sub_stream_num; i++) {
			if (tempManifest & (1<<i)) {
				perPeerSS_num++;
				peerHighestSSID = i;
				totalSourceBitrate += ss_table[i]->data.total_source_bitrate;
				totalLocalBitrate += ss_table[i]->data.total_local_bitrate;
			}
		}
		
		for (unsigned long i = 0; i < sub_stream_num; i++) {
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "ss_table[", i, "].total_byte =", ss_table[i]->data.total_byte);
		}
		
		_log_ptr->write_log_format("s(u) s d s d s f s f \n", __FUNCTION__, __LINE__,
														"pid ", parent_info->peerInfo.pid,
														"manifest =", parent_info->peerInfo.manifest,
														"totalSourceBitrate =", totalSourceBitrate,
														"totalLocalBitrate =", totalLocalBitrate);
		
		
		double ratio = totalLocalBitrate / totalSourceBitrate;
		
		if (ratio < (double)1/(double)(2*perPeerSS_num)) {
			parent_info->rescueStatsArry[ss_table[peerHighestSSID]->data.measure_N%PARAMETER_M] = perPeerSS_num;
		}
		else {
			for (int i = perPeerSS_num; i > 0; i--) {
				if (ratio > (double)(2*i-1)/(double)(2*perPeerSS_num)) {
					parent_info->rescueStatsArry[ss_table[peerHighestSSID]->data.measure_N%PARAMETER_M] = perPeerSS_num - i;
					break;
				}
			}
		}
		
		_log_ptr->write_log_format("s(u) s d s f s d (d)(d)(d) \n", __FUNCTION__, __LINE__,
																"[DEBUG] parent PID", parent_info->peerInfo.pid,
																"ratio =", ratio,
																"perPeerSS_num =", perPeerSS_num,
																ss_table[peerHighestSSID]->data.measure_N%PARAMETER_M,
																parent_info->rescueStatsArry[ss_table[peerHighestSSID]->data.measure_N%PARAMETER_M],
																ss_table[peerHighestSSID]->data.measure_N);
		
		// 根據rescueStatsArry 來決定要不要觸發rescue
		// 近PARAMETER_M發生N次
		for (int i = 0; i < PARAMETER_M; i++) {
			statsArryCount_ptr[parent_info->rescueStatsArry[i]]++;
			if (parent_info->rescueStatsArry[i] > 0) {
				count_N++;
			}
		}
		// 近PARAMETER_P次 發生 P次
		for (int j = 0; j < PARAMETER_P; j++) {
			if (parent_info->rescueStatsArry[(ss_table[peerHighestSSID]->data.measure_N-j+PARAMETER_M)%PARAMETER_M] > 0) {
				continuous_P++ ;
			}
		}

		for (int i = 0; i < PARAMETER_M; i++) {
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__,
														"parent_info->rescueStatsArry[", i, 
														"] =", parent_info->rescueStatsArry[i]);
		}
		for (unsigned long i = 0; i < sub_stream_num+1; i++) {
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__,
														"statsArryCount_ptr[", i, 
														"] =", statsArryCount_ptr[i]);
		}
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__,
														"count_N =", count_N, 
														"continuous_P =", continuous_P);
		
		// 找出需要rescue的substream數量
		for (unsigned long i = 1; i < sub_stream_num+1; i++) {
			if (tempMax < (int)statsArryCount_ptr[i]) {
				tempMax = statsArryCount_ptr[i];
				rescueSS = i;
			}
		}

		//符合條件觸發rescue 需要救rescue_ss 個
		if (count_N >= PARAMETER_N  || continuous_P == PARAMETER_P) {
			
			_log_ptr->write_log_format("s(u) s s d s d s d \n", __FUNCTION__, __LINE__, "Rescue triggered.",
																					"count_N ", count_N,
																					">= PARAMETER_N", PARAMETER_N,
																					"or continuous_P ==", continuous_P);
							
			//PID是PK的有問題 (代表是這個peer下載能力有問題)
			if (parent_info->peerInfo.pid == PK_PID) {
				debug_printf("[RESCUE_TYPE] 2. Peer download bandwidth has problem \n");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s \n", "pid", my_pid, "[RESCUE_TYPE] 2 Peer download bandwidth has problem");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 2. Peer download bandwidth has problem");
			}
			else {
				// 根據計算出來的rescueSS切斷peer的substream
				for (unsigned long i = 0; i < rescueSS; i++) {
					unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
					unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
					unsigned long parent_new_manifest = manifestFactory(parent_old_manifest, 1);
					unsigned long pk_new_manifest = pk_old_manifest | (parent_old_manifest ^ parent_new_manifest);
					int ss_id = manifestToSubstreamID(parent_old_manifest^parent_new_manifest);		// The substream needed rescue
					UINT32 manifest = SubstreamIDToManifest(ss_id);
					bool need_source = true;
				
					if (ss_table[ss_id]->block_rescue == FALSE) {
						if (ss_table[ss_id]->state.state == SS_STABLE2) {

							SetSubstreamState(ss_id, SS_RESCUE2);
							debug_printf("[RESCUE_TYPE] 2. %d %d \n", count_N, continuous_P);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_RESCUE_TRIGGER, manifest);
							_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 2", count_N, continuous_P);
							_log_ptr->write_log_format("s(u) s u s u d d \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 2. Rescue in STABLE state. substreamID =", ss_id,
								"parentID=", parent_info->peerInfo.pid,
								count_N, continuous_P);


							ss_table[ss_id]->data.previousParentPID = parent_info->peerInfo.pid;

							// Update manifest
							set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
							set_parent_manifest(parent_info, parent_new_manifest);
							SetSubstreamParent(manifest, PK_PID);
							_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);

							//send_parentToPK(manifest, PK_PID+1);
							NeedSourceDecision(&need_source);
							//send_rescueManifestToPK(pk_new_manifest, need_source);
							send_rescueManifestToPK(manifest, need_source);
							ss_table[ss_id]->state.dup_src = need_source;

						}
						else if (ss_table[ss_id]->state.state == SS_CONNECTING2) {
							// No more trigger rescue again
							debug_printf("Should not happen \n");
						}
						else if (ss_table[ss_id]->state.state == SS_RESCUE2) {
							// No more trigger rescue again
							debug_printf("Should not happen \n");
						}
						else if (ss_table[ss_id]->state.state == SS_TEST2) {

							SetSubstreamState(ss_id, SS_RESCUE2);
							debug_printf("[RESCUE_TYPE] 2. %d %d \n", count_N, continuous_P);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 2", count_N, continuous_P);
							_log_ptr->write_log_format("s(u) s u s u d d \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 2. Rescue in TEST state. substreamID =", ss_id,
								"parentID=", parent_info->peerInfo.pid,
								count_N, continuous_P);

							ss_table[ss_id]->state.is_testing = false;

							// Update manifest
							set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
							set_parent_manifest(parent_info, parent_new_manifest);
							SetSubstreamParent(manifest, PK_PID);
							_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);

							//send_parentToPK(manifest, PK_PID+1);
							NeedSourceDecision(&need_source);
							//send_rescueManifestToPK(pk_new_manifest, need_source);
							send_rescueManifestToPK(manifest, need_source);
							ss_table[ss_id]->state.dup_src = need_source;

							// Reset source delay parameters of testing substream of this parent
							// Restart other testing substreams of this parent
							for (unsigned long i = 0; i < sub_stream_num; i++) {
								if ((1 << i) & parent_new_manifest) {
									//ss_table[i]->data.delay_beyond_count = 0;
									ss_table[i]->source_delay_table.delay_beyond_count = 0;
								}
							}
						}
						else {
							// Unexpected state
						}
					}
				}
			}
		}
	}
}



// Send rescue message to pk so that peer will get rescue list
// Parameters:
//   manifestValue[in]: The manifest should from pk. The value is total substreams' manifest
//   need_source: If set it true, which means the peer needs the source from pk
void pk_mgr::send_rescueManifestToPK(unsigned long manifestValue, bool need_source)
{
	struct rescue_pkt_from_server *chunk_rescueManifestPtr = NULL;

	chunk_rescueManifestPtr = new struct rescue_pkt_from_server;
	if (!chunk_rescueManifestPtr) {
		handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::chunk_rescueManifestPtr new error ", __FUNCTION__, __LINE__);
	}

	memset(chunk_rescueManifestPtr, 0, sizeof(struct rescue_pkt_from_server));

	chunk_rescueManifestPtr->header.cmd = CHNK_CMD_PEER_RESCUE;
	chunk_rescueManifestPtr->header.length = sizeof(struct rescue_pkt_from_server) - sizeof(struct chunk_header_t);	//pkt_buf paylod length
	chunk_rescueManifestPtr->header.rsv_1 = REQUEST;
	chunk_rescueManifestPtr->pid = _peer_mgr_ptr->self_pid;
	chunk_rescueManifestPtr->manifest = manifestValue;
	chunk_rescueManifestPtr->rescue_seq_start = _current_send_sequence_number - (sub_stream_num*5);
	chunk_rescueManifestPtr->need_source = need_source == true ? 1 : 0;

	_net_ptr->set_blocking(_sock);
	_net_ptr->send(_sock, (char*)chunk_rescueManifestPtr, sizeof(struct rescue_pkt_from_server), 0);
	_net_ptr->set_nonblocking(_sock);

	//debug_printf("sent rescue to PK manifest = %d  start from %d\n",manifestValue,_current_send_sequence_number -(sub_stream_num*5) );
	_log_ptr->write_log_format("s(u) s u s u s d \n", __FUNCTION__, __LINE__,
														"sent rescue to PK manifest =", manifestValue,
														"start from", _current_send_sequence_number-sub_stream_num*5,
														"need_source =", need_source) ;

	delete chunk_rescueManifestPtr;
	return ;
}

void pk_mgr::clear_map_pid_parent_temp()
{
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(pid_peer_info_iter =map_pid_parent_temp.begin();pid_peer_info_iter!=map_pid_parent_temp.end(); pid_peer_info_iter++){
		peerInfoPtr=pid_peer_info_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_parent_temp.clear();
}

void pk_mgr::clear_delay_table()
{
	map<unsigned long, struct source_delay *>::iterator source_delay_iter;	
	for (source_delay_iter= delay_table.begin(); source_delay_iter !=delay_table.end(); source_delay_iter++) {
		delete source_delay_iter->second;
	}
	delay_table.clear();
}

void pk_mgr::ClearSubstreamTable()
{
	map<unsigned long, struct substream_info*>::iterator ss_table_iter;	
	for (ss_table_iter = ss_table.begin(); ss_table_iter != ss_table.end(); ss_table_iter++) {
		delete ss_table_iter->second;
	}
	ss_table.clear();
}

void pk_mgr::clear_map_streamID_header(){

	map<int, struct update_stream_header *>::iterator update_stream_header_iter;

	for(update_stream_header_iter = map_streamID_header.begin() ;update_stream_header_iter !=map_streamID_header.end();update_stream_header_iter++){
		
		delete update_stream_header_iter->second;

	}
	map_streamID_header.clear();
}



void pk_mgr::clear_map_pid_parent_temp(unsigned long manifest){

	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	multimap <unsigned long, struct peer_info_t *>::iterator temp_pid_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;


	for(pid_peer_info_iter =map_pid_parent_temp.begin();pid_peer_info_iter!=map_pid_parent_temp.end(); ){
		peerInfoPtr=pid_peer_info_iter ->second;

		if(peerInfoPtr ->manifest == manifest){

			debug_printf("clear map_pid_parent_temp pid = %d \n",pid_peer_info_iter->first);
			_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"clear map_pid_parent_temp pid ",pid_peer_info_iter->first) ;

			delete peerInfoPtr;

			temp_pid_peer_info_iter =pid_peer_info_iter;
			pid_peer_info_iter ++;

			map_pid_parent_temp.erase(temp_pid_peer_info_iter);
//			pid_peer_info_iter =map_pid_parent_temp.begin() ;
			if(pid_peer_info_iter == map_pid_parent_temp.end())
				break;

		}else{
			pid_peer_info_iter++ ;
		}
	}
	_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__," clear_map_pid_parent_temp  end manifest =",manifest) ;

	debug_printf("clear_map_pid_parent_temp  end manifest = %d \n",manifest);

}



void pk_mgr::clear_map_pid_parent()
{
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;

	for(pid_peerDown_info_iter =map_pid_parent.begin();pid_peerDown_info_iter!= map_pid_parent.end(); pid_peerDown_info_iter++){
		peerDownInfoPtr=pid_peerDown_info_iter ->second;
		delete peerDownInfoPtr;
	}

	map_pid_parent.clear();
}

void pk_mgr::clear_map_pid_child1()
{
	map<unsigned long, struct peer_info_t *>::iterator map_pid_child1_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(map_pid_child1_iter =map_pid_child.begin();map_pid_child1_iter!= map_pid_child.end(); map_pid_child1_iter++){
		peerInfoPtr=map_pid_child1_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_child.clear();
}

void pk_mgr::clear_map_pid_child_temp()
{
	multimap<unsigned long, struct peer_info_t *>::iterator map_pid_child_temp_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(map_pid_child_temp_iter =map_pid_child_temp.begin();map_pid_child_temp_iter!= map_pid_child_temp.end(); map_pid_child_temp_iter++){
		peerInfoPtr=map_pid_child_temp_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_child.clear();
}

void pk_mgr::clear_map_pid_child_temp(unsigned long pid,unsigned long manifest)
{
	multimap<unsigned long, struct peer_info_t *>::iterator map_pid_child_temp_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(map_pid_child_temp_iter =map_pid_child_temp.begin();map_pid_child_temp_iter!= map_pid_child_temp.end(); map_pid_child_temp_iter++){
		peerInfoPtr=map_pid_child_temp_iter ->second;
		if(peerInfoPtr ->pid == pid && peerInfoPtr ->manifest==manifest){
			delete peerInfoPtr;
			map_pid_child_temp.erase(map_pid_child_temp_iter);
			return;
		}
	}
	return;
}

//回傳cut掉ssNumber 數量的manifestValue
unsigned long pk_mgr::manifestFactory(unsigned long manifest, int ss_num)
{
	unsigned long temp_manifest = 0;
	int cnt = 0;

	for (unsigned long i = 0; i < sub_stream_num; i++) {
		if ((1 << i) & manifest) {
			temp_manifest |= (1 << i) ;
			cnt++;
			if (cnt == ss_num) {
				break;
			}
		}
	}

	return manifest & ~temp_manifest;
}

unsigned int pk_mgr::rescueNumAccumulate()
{
	unsigned int totalRescueNum = 0;
	for (map<unsigned long, struct peer_info_t *>::iterator iter = map_pid_child.begin(); iter != map_pid_child.end(); iter++) {
		if (iter->second->connection_state == PEER_SELECTED) {
			unsigned long tempManifest = iter->second->manifest;
			for (unsigned long i = 0; i < sub_stream_num; i++) {
				if ((1 << i) & tempManifest) {
					totalRescueNum++;
				}
			}
		}
	}

	//debug_printf("sent capacity totalRescueNum = %d \n", totalRescueNum);
	_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__, __LINE__, "sent capacity totalRescueNum", totalRescueNum);

	return totalRescueNum;
}

//若對應到多個則只會回傳最小的(最右邊 最低位的)
unsigned long  pk_mgr::manifestToSubstreamID(unsigned long  manifest)
{
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		if ((1 << i) & manifest) {
			return i;
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



unsigned long  pk_mgr::manifestToSubstreamNum(unsigned long  manifest)
{
	unsigned long substreamNum = 0 ;
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		if (manifest & 1<<i) {
			substreamNum++ ;
		}
	}
	return substreamNum ;
}

// This function only called for cutting off all substreams from PK, which means that the 
// parameter manifestValue always 0, send_rescueManifestToPKUpdate(0)
void pk_mgr::send_rescueManifestToPKUpdate(unsigned long manifestValue)
{
	struct rescue_update_from_server  *chunk_rescueManifestPtr = NULL;

	chunk_rescueManifestPtr = new struct rescue_update_from_server;
	if (!chunk_rescueManifestPtr) {
		handle_error(MALLOC_ERROR, "[ERROR] pk_mgr::chunk_rescueManifestPtr new error", __FUNCTION__, __LINE__);
		PAUSE
	}
	memset(chunk_rescueManifestPtr, 0, sizeof(struct rescue_update_from_server));

	chunk_rescueManifestPtr->header.cmd = CHNK_CMD_PEER_RESCUE_UPDATE ;
	chunk_rescueManifestPtr->header.length = (sizeof(struct rescue_update_from_server)-sizeof(struct chunk_header_t)) ;	//pkt_buf paylod length
	chunk_rescueManifestPtr->header.rsv_1 = REQUEST ;
	chunk_rescueManifestPtr->pid = _peer_mgr_ptr ->self_pid ;
	chunk_rescueManifestPtr->manifest = manifestValue ;

	_net_ptr->set_blocking(_sock);
	_net_ptr ->send(_sock , (char*)chunk_rescueManifestPtr ,sizeof(struct rescue_update_from_server),0) ;
	_net_ptr->set_nonblocking(_sock);

	_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "send_rescueManifestToPKUpdate =",manifestValue) ;

	delete chunk_rescueManifestPtr;

	return ;
}


// 這邊的 manifestValue 只會有一個 sunstream ID  // 若送一個空的list 給PK 代表選PK當PARENT
// header  | manifest | parent_num  |pareentPID  | pareentPID | ... 
// Send topology to PK
void pk_mgr::send_parentToPK(unsigned long manifestValue,unsigned long parent_pid)
{
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct update_topology_info *parentListPtr = NULL;
	unsigned long packetlen = 0;
	int parent_num = 1;
	int ret = 0;

	packetlen = parent_num * sizeof (unsigned long)+sizeof(struct update_topology_info);
	parentListPtr = (struct update_topology_info *) new char [packetlen];
	if (!parentListPtr) {
		handle_error(MALLOC_ERROR, "[ERROR] parentListPtr new error", __FUNCTION__, __LINE__);
		PAUSE
	}
	memset(parentListPtr, 0, packetlen);

	parentListPtr->header.cmd = CHNK_CMD_TOPO_INFO ;
	parentListPtr->header.length = ( packetlen-sizeof(struct chunk_header_t)) ;	//pkt_buf = payload length
	parentListPtr->header.rsv_1 = REQUEST ;
	parentListPtr->parent_num = 1 ; 
	parentListPtr->manifest = manifestValue ;
	parentListPtr->parent_pid[0] = parent_pid;

	debug_printf("Send parent %d to PK \n", parent_pid);
	_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Send parent", parent_pid, "to PK");

	_net_ptr->set_blocking(_sock);
	ret = _net_ptr->send(_sock , (char*)parentListPtr, packetlen , 0);
	_net_ptr->set_nonblocking(_sock);

	if (ret != packetlen) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u d u \n", "[CHNK_CMD_TOPO_INFO] mypid :", my_pid, ret, packetlen);
		debug_printf("ret %d  packetlen %d \n", ret, packetlen);
		PAUSE
	}



	delete [] parentListPtr;

	return ;
}

void pk_mgr::reSet_detectionInfo()
{
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
	
	pkDownInfoPtr->timeOutNewSeq = 0;
	pkDownInfoPtr->timeOutLastSeq = 0;

	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		ssDetect_ptr[i].count_X = 0;
		ssDetect_ptr[i].first_timestamp = 0;
		ssDetect_ptr[i].last_localBitrate = 0;
		ssDetect_ptr[i].last_seq = 0;
		ssDetect_ptr[i].last_sourceBitrate = 0;
		ssDetect_ptr[i].last_timestamp = 0;
		ssDetect_ptr[i].measure_N = 1;
		ssDetect_ptr[i].total_buffer_delay = 0;
		ssDetect_ptr[i].total_byte = 0;
	}
	
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		ss_table[i]->data.count_X = 0;
		ss_table[i]->data.first_timestamp = 0;
		ss_table[i]->data.total_local_bitrate = 0;
		ss_table[i]->data.last_seq = 0;
		ss_table[i]->data.total_source_bitrate = 0;
		ss_table[i]->data.last_timestamp = 0;
		ss_table[i]->data.measure_N = 1;
		ss_table[i]->data.total_buffer_delay = 0;
		ss_table[i]->data.total_byte = 0;
	}

	for (pid_peerDown_info_iter = map_pid_parent.begin(); pid_peerDown_info_iter!= map_pid_parent.end(); pid_peerDown_info_iter++) {
		for (int i = 0; i < PARAMETER_M; i++) {
			pid_peerDown_info_iter->second->rescueStatsArry[i] = 0;
		}
	}
}

// Reset all 5 rescue detection of this substream
void pk_mgr::ResetDetection(unsigned long ss_id)
{
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);

	pkDownInfoPtr->timeOutNewSeq = 0;
	pkDownInfoPtr->timeOutLastSeq = 0;

	// Type 1: packet lost
	if (map_pid_parent.find(ss_table[ss_id]->current_parent_pid) != map_pid_parent.end()) {
		map_pid_parent.find(ss_table[ss_id]->current_parent_pid)->second->outBuffCount = 0;
	}
	ss_table[ss_id]->lost_packet_count = 0;

	// Type 2: bandwidth detection
	if (map_pid_parent.find(ss_table[ss_id]->current_parent_pid) != map_pid_parent.end()) {
		for (int i = 0; i < PARAMETER_M; i++) {
			map_pid_parent.find(ss_table[ss_id]->current_parent_pid)->second->rescueStatsArry[i] = 0;
		}
	}

	// Type 3: timeout
	struct timerStruct timer;
	_log_ptr->timerGet(&timer);
	ss_table[ss_id]->timer.timer = timer;

	// Type 4: over MAX delay
	ss_table[ss_id]->source_delay_table.delay_beyond_count = 0;

	// Type 5: connect timeout

}

// Reset all 5 rescue detection of all substreams
void pk_mgr::ResetDetection()
{
	_log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);

	pkDownInfoPtr->timeOutNewSeq = 0;
	pkDownInfoPtr->timeOutLastSeq = 0;

	for (unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
		// Type 1: packet lost
		if (map_pid_parent.find(ss_table[ss_id]->current_parent_pid) != map_pid_parent.end()) {
			map_pid_parent.find(ss_table[ss_id]->current_parent_pid)->second->outBuffCount = 0;
		}

		// Type 2: bandwidth detection
		if (map_pid_parent.find(ss_table[ss_id]->current_parent_pid) != map_pid_parent.end()) {
			for (int i = 0; i < PARAMETER_M; i++) {
				map_pid_parent.find(ss_table[ss_id]->current_parent_pid)->second->rescueStatsArry[i] = 0;
			}
		}
		ss_table[ss_id]->lost_packet_count = 0;

		// Type 3: timeout
		struct timerStruct timer;
		_log_ptr->timerGet(&timer);
		ss_table[ss_id]->timer.timer = timer;

		// Type 4: over MAX delay
		ss_table[ss_id]->source_delay_table.delay_beyond_count = 0;

		// Type 5: connect timeout
	}
}

// Decide the peer is able to not get source from pk or not
void pk_mgr::NeedSourceDecision(bool *need_source)
{
	double cnt = 0;
	for (UINT32 i = 0; i < sub_stream_num; i++) {
		if (ss_table[i]->state.state == SS_STABLE2) {
			cnt += 1;
		}
		else {
			if (ss_table[i]->state.dup_src == true) {
				cnt += 1;
			}
		}
	}
	_log_ptr->write_log_format("s(u) s f s u (f) \n", __FUNCTION__, __LINE__, "cnt", cnt, "sub_stream_num", sub_stream_num, cnt/sub_stream_num);
	//*need_source = cnt/sub_stream_num > NEEDSOURCE_THRESHOLD ? false : true;
	// NeedSourceDecision 有誤判，還在找bug中，先暫時"每次都會要求 source"
	*need_source = true;
}

// handle timeout  connect time out
// 1. Connect Timeout
// 2. Log periodically sending(source-delay, quality, and bandwidth)
// 3. Streaming Timeout
// 4. Re-Sync Timeout
void pk_mgr::time_handle()
{
	struct timerStruct new_timer;
	_log_ptr->timerGet(&new_timer);
	
	map<unsigned long, manifest_timmer_flag *>::iterator temp_substream_first_reply_peer_iter;
	map<unsigned long, struct manifest_timmer_flag *>::iterator substream_first_reply_peer_iter;
	
	// If this function is first called
	if (firstIn) {
		_log_ptr->timerGet(&programStartTimer);		// Record program start time
		_log_ptr->timerGet(&LastTimer);		// Using for streaming timeout
		_log_ptr->timerGet(&sleepTimer);
		_log_ptr->timerGet(&reSynTimer);	// Using for re-Sync timeout
		firstIn = false;
	}
	
	/*
	// Reconnect test
	if (_log_ptr->diff_TimerGet_ms(&reconnect_timer, &new_timer) >= 10000) {
		
		*(_net_ptr->_errorRestartFlag) = RESTART;
	}
	*/

	
	// Timer for some test
	if (_log_ptr->diff_TimerGet_ms(&reconnect_timer, &new_timer) >= 200) {

		reconnect_timer = new_timer;

		for (map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter = map_pid_child.begin(); pid_peer_info_iter != map_pid_child.end(); pid_peer_info_iter++) {
			
			unsigned long child_pid;
			int child_sock;
			struct peer_info_t *child_peer = NULL;

			// 計算這次 children 總共的 ss 數量和 queue 數量
			child_pid = pid_peer_info_iter->first;
			if (map_pid_child.find(child_pid) != map_pid_child.end()) {
				child_peer = map_pid_child.find(child_pid)->second;		// Get child info
				if (child_peer->connection_state == PEER_CONNECTED_CHILD) {
					if (_peer_ptr->map_out_pid_fd.find(child_pid) != _peer_ptr->map_out_pid_fd.end()) {
						child_sock = _peer_ptr->map_out_pid_fd[child_pid];	// Get child socket
					}
					else if (_peer_ptr->map_out_pid_udpfd.find(child_pid) != _peer_ptr->map_out_pid_udpfd.end()) {
						child_sock = _peer_ptr->map_out_pid_udpfd[child_pid];	// Get child socket
					}
					else {
						continue;
					}
					
					// 累加每個 queue 的 pending packets
					int pendingpkt_size;
					int len = sizeof(pendingpkt_size);
					UDT::getsockopt(child_sock, 0, UDT_SNDDATA, &pendingpkt_size, &len);
					_log_ptr->write_log_format("s(u) s\td\td \n", __FUNCTION__, __LINE__, "child pid", child_pid, pendingpkt_size);
				}
			}
		}
	}
	

	// Handle timer of Block Rescue
	for (unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
		if (ss_table[ss_id]->can_send_block_rescue == FALSE) {
			// 至少須經過 BLOCK_RESCUE_INTERVAL 這麼多秒才能再次發送 Block Rescue Message
			if (_log_ptr->diff_TimerGet_ms(&(ss_table[ss_id]->block_rescue_interval), &new_timer) >= BLOCK_RESCUE_INTERVAL) {
				ss_table[ss_id]->can_send_block_rescue = TRUE;
			}
		}
		if (ss_table[ss_id]->block_rescue == TRUE) {
			// 收到 Block Rescue Message 的話會被凍結 Rescue 一段時間
			if (_log_ptr->diff_TimerGet_ms(&(ss_table[ss_id]->block_rescue_timer), &new_timer) >= BLOCK_RESCUE_TIME) {
				SetSubstreamUnblockRescue(ss_id);
				ResetDetection(ss_id);
			}
		}
	}

	// Timer for building UDP connection
	for (list<struct delay_build_connection>::iterator iter = _peer_mgr_ptr->_peer_communication_ptr->list_build_connection.begin(); iter != _peer_mgr_ptr->_peer_communication_ptr->list_build_connection.end(); iter++) {
		if (_log_ptr->diff_TimerGet_ms(&(iter->timer), &new_timer) >= NAT_WAITING_TIME) {
			_peer_mgr_ptr->_peer_communication_ptr->non_blocking_build_connection_udp_now(iter->candidates_info, iter->caller, iter->manifest, iter->peer_pid, iter->flag, iter->my_session, iter->peercomm_session);
			_peer_mgr_ptr->_peer_communication_ptr->list_build_connection.erase(iter);
			iter = _peer_mgr_ptr->_peer_communication_ptr->list_build_connection.begin();
			if (iter == _peer_mgr_ptr->_peer_communication_ptr->list_build_connection.end()) {
				break;
			}
		}
	}
	
	// Handle each session of map_mysession_candidates
	for (map<unsigned long, struct mysession_candidates *>::iterator iter = _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.begin(); iter != _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.end(); iter++) {
		//_log_ptr->write_log_format("s(u) s u d \n", __FUNCTION__, __LINE__, "my_session", iter->first, _log_ptr->diff_TimerGet_ms(&(iter->second->timer), &new_timer));
		UINT32 my_session = iter->first;
		if (iter->second->session_state == SESSION_CONNECTING) {
			// 2秒的建立時間
			if (_log_ptr->diff_TimerGet_ms(&(iter->second->timer), &new_timer) >= CONNECT_TIME_OUT) {
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "my_session", iter->first, "state change to SESSION_SELECTING");
				iter->second->session_state = SESSION_SELECTING;
				int ss_id = manifestToSubstreamID(iter->second->manifest);
				if (ss_table[ss_id]->state.state == SS_CONNECTING2) {
					SetSubstreamState(ss_id, SS_TEST2);
					ss_table[ss_id]->state.is_testing = true;
					_log_ptr->timerGet(&(ss_table[ss_id]->timer.inTest_timer));
				}
				else {
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] ss_id", ss_id);
				}
			}
		}
		else if (iter->second->session_state == SESSION_SELECTING) {
			if (iter->second->myrole == CHILD_PEER) {
				// Selecting parent strategy 
				_peer_mgr_ptr->_peer_communication_ptr->SelectStrategy(my_session);		// 每個 my_session 只會進來一次
				_peer_mgr_ptr->_peer_communication_ptr->SendPeerCon(my_session);			// 每個 my_session 只會進來一次
				int n = SendParentTestToPK(my_session);

				if (n == 0) {
					// Not get any parent in this session
					unsigned long rescue_manifest = iter->second->manifest;

					unsigned long ss_id = manifestToSubstreamID(rescue_manifest);
					UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
					unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
					unsigned long pk_new_manifest = pk_old_manifest | manifest;
					bool need_source = true;


					if (ss_table[ss_id]->state.rescue_after_fail == true) {
						// RESCUE or MERGE
						SetSubstreamState(ss_id, SS_RESCUE2);
						debug_printf("[RESCUE_TYPE] 5 Connect all fail \n");

						// Log the messages
						_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
						_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s \n", "my_pid", my_pid, manifest, "[RESCUE_TYPE] 5 Connect all fail");
						_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 5. Connect all fail");

						// Update manifest
						set_parent_manifest(pkDownInfoPtr, pk_new_manifest);

						//send_parentToPK(manifest, PK_PID+1);
						NeedSourceDecision(&need_source);
						//send_rescueManifestToPK(pk_new_manifest, need_source);
						send_rescueManifestToPK(manifest, need_source);
						ss_table[ss_id]->state.dup_src = need_source;

						rescue_manifest &= ~SubstreamIDToManifest(ss_id);
					}
					else {
						// MOVE
						SetSubstreamState(ss_id, SS_STABLE2);
						debug_printf("[RESCUE_TYPE] 5 Connect all fail \n");

						// Log the messages
						_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s \n", "my_pid", my_pid, manifest, "[RESCUE_TYPE] 5M. Connect all fail");
						_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 5M. Connect all fail");
						ss_table[ss_id]->state.rescue_after_fail = true;
					}


					//_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "manifest timeout and all connect fail", substream_first_reply_peer_iter->second->rescue_manifest);

				}
				else {
					// 有和 peer 建立連線
					if (_peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.find(my_session) != _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.end()) {
						if (_peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates[my_session]->all_behind_nat == TRUE) {
							_logger_client_ptr->add_nat_success_times();
						}
					}
				}

			}
			iter->second->session_state = SESSION_END;
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "my_session", iter->first, "state change to SESSION_END");
		}
		else if (iter->second->session_state == SESSION_END) {
			// // 保留一段時間後刪除，因為可能同時有同一個 parent 在多個正在處理的 my_session 中，不要因 my_session 處理完畢就馬上關閉 socket
			//if (_log_ptr->diff_TimerGet_ms(&(iter->second->timer), &new_timer) >= 5000) {
				//_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "my_session", my_session, "diff_time", _log_ptr->diff_TimerGet_ms(&(iter->second->timer), &new_timer));
			if (iter->second->myrole == CHILD_PEER) {
				// Child 端馬上刪除 table
				_peer_mgr_ptr->_peer_communication_ptr->StopSession(my_session);
			}
			else {
				// Parent 端等待一段時間再刪除 table，為了等待 child 送來的 CHNK_CMD_PEER_CON
				if (_log_ptr->diff_TimerGet_ms(&(iter->second->timer), &new_timer) >= CONNECT_TIME_OUT+2000) {
					_peer_mgr_ptr->_peer_communication_ptr->StopSession(my_session);
				}
			}
			//}
			break;
		}

	}

	/*
	// Handle each session
	for (substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.begin(); substream_first_reply_peer_iter != _peer_ptr->substream_first_reply_peer.end(); ) {
		unsigned long session_id = substream_first_reply_peer_iter->first;
		
		if (substream_first_reply_peer_iter->second->peer_role == PARENT_PEER) {
			// If receive CHNK_CMD_PEER_CON, regarding that peer as first connected peer


			if (substream_first_reply_peer_iter->second->session_state == FIRST_CONNECTED_READY) {

			}
			else if (substream_first_reply_peer_iter->second->session_state == FIRST_CONNECTED_RUNNING) {

			}
			else if (substream_first_reply_peer_iter->second->session_state == FIRST_CONNECTED_OK) {
				substream_first_reply_peer_iter->second->session_state = FIRST_REPLY_READY;
			}
			else if (substream_first_reply_peer_iter->second->session_state == FIRST_REPLY_READY) {
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "session", session_id, "handle_test_delay");
				if (_peer_mgr_ptr->handle_test_delay(session_id) > 0) {
					substream_first_reply_peer_iter->second->session_state = FIRST_REPLY_RUNNING;

				}
			}
			else if (substream_first_reply_peer_iter->second->session_state == FIRST_REPLY_RUNNING) {

			}
			else if (substream_first_reply_peer_iter->second->session_state == FIRST_REPLY_OK) {
				// If receive CHNK_CMD_PEER_TEST_DELAY REPLY, regarding that peer as real parent
				
				// 會進到這個 state 表示有拿到 first reply peer，立刻告訴 PK 這次 session 的結果
				if (substream_first_reply_peer_iter->second->sentTestFlag == FALSE) {
					SendParentTestToPK(session_id);
					substream_first_reply_peer_iter->second->sentTestFlag = TRUE;
				}
			}


			// Stop the session when timeout
			if (_log_ptr->diff_TimerGet_ms(&(substream_first_reply_peer_iter->second->connectTimeOut), &new_timer) >= CONNECT_TIME_OUT) {
				
				// 如果全部都連線失敗，2秒後會告訴 PK 這次 session 的結果，順序要在 rescue 之前
				if (substream_first_reply_peer_iter->second->sentTestFlag == FALSE) {
					SendParentTestToPK(session_id);
					substream_first_reply_peer_iter->second->sentTestFlag = TRUE;
				}

				if (substream_first_reply_peer_iter->second->session_state != FIRST_REPLY_OK) {
					// Not get any parent in this session
					unsigned long rescue_manifest = substream_first_reply_peer_iter->second->rescue_manifest;

					while (rescue_manifest > 0) {
						unsigned long ss_id = manifestToSubstreamID(rescue_manifest);
						UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
						unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
						unsigned long pk_new_manifest = pk_old_manifest | manifest;
						bool need_source = true;
						int candidate_num = -1;
						int candidate_pid = -1;
						int candidate_pid2 = -1;

						if (_peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.find(session_id) != _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.end()) {
							candidate_num = _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.find(session_id)->second->candidates_num;
							candidate_pid = _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.find(session_id)->second->candidates_info[0].pid;
							if (candidate_num == 2) {
								candidate_pid2 = _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.find(session_id)->second->candidates_info[0].pid;
							}
						}

						
						if (ss_table[ss_id]->state.rescue_after_fail == true) {
							// RESCUE or MERGE
							SetSubstreamState(ss_id, SS_RESCUE2);
							debug_printf("[RESCUE_TYPE] 5 Connect all fail %d %d %d \n", candidate_num, candidate_pid, candidate_pid2);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d d d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 5 Connect all fail", candidate_num, candidate_pid, candidate_pid2);
							_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 5. Connect all fail");

							// Update manifest
							set_parent_manifest(pkDownInfoPtr, pk_new_manifest);

							//send_parentToPK(manifest, PK_PID+1);
							NeedSourceDecision(&need_source);
							//send_rescueManifestToPK(pk_new_manifest, need_source);
							send_rescueManifestToPK(manifest, need_source);
							ss_table[ss_id]->state.dup_src = need_source;

							rescue_manifest &= ~SubstreamIDToManifest(ss_id);
						}
						else {
							// MOVE
							SetSubstreamState(ss_id, SS_STABLE2);
							debug_printf("[RESCUE_TYPE] 5 Connect all fail %d %d %d \n", candidate_num, candidate_pid, candidate_pid2);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s d d d \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 5M. Connect all fail", candidate_num, candidate_pid, candidate_pid2);
							_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 5M. Connect all fail");
							ss_table[ss_id]->state.rescue_after_fail = true;
						}
					}

					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "manifest timeout and all connect fail", substream_first_reply_peer_iter->second->rescue_manifest);

				}
				else {
					// 有和 peer 建立連線
					if (_peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.find(session_id) != _peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates.end()) {
						if (_peer_mgr_ptr->_peer_communication_ptr->map_mysession_candidates[session_id]->all_behind_nat == TRUE) {
							_logger_client_ptr->add_nat_success_times();
						}
					}
				}
				

				
				

				int ss_id = manifestToSubstreamID(substream_first_reply_peer_iter->second->rescue_manifest);
				ss_table[ss_id]->state.connecting_peer = false;
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Set substream", ss_id, "unblock by connecting peer");

				unsigned long selected_pid = substream_first_reply_peer_iter->second->selected_pid;		// selected_pid 如果是 PK 代表這次 session 的 candidate 都沒建立成功

				_peer_mgr_ptr->_peer_communication_ptr->StopSession(session_id);
				_peer_ptr->StopSession(session_id);
				for (multimap <unsigned long, struct peer_info_t *>::iterator iter = map_pid_parent_temp.begin(); iter != map_pid_parent_temp.end(); ) {
					_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__,
						"size()", map_pid_parent_temp.size(),
						"pid", iter->second->pid,
						"manifest", iter->second->manifest,
						"count", map_pid_parent_temp.count(iter->second->pid));

					if (iter->second->priority == session_id) {
						
						for (map<unsigned long, struct peer_connect_down_t *>::iterator iter = map_pid_parent.begin(); iter != map_pid_parent.end(); iter++) {
							_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "pid", iter->first, "manifest", iter->second->peerInfo.manifest);
						}
						// Update temp-parent's manifest and erase when session stop, except the selected peer
						if (iter->second->pid != selected_pid) {
							map<unsigned long, struct peer_connect_down_t *>::iterator parent_iter = map_pid_parent.find(iter->first);
							if (parent_iter != map_pid_parent.end()) {
								set_parent_manifest(parent_iter->second, parent_iter->second->peerInfo.manifest & ~(iter->second->manifest));
								_peer_mgr_ptr->send_manifest_to_parent(parent_iter->second->peerInfo.manifest, parent_iter->second->peerInfo.pid);
								_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "manifest", parent_iter->second->peerInfo.manifest);
								if (parent_iter->second->peerInfo.manifest == 0) {
									_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "manifest", parent_iter->second->peerInfo.manifest);
									_peer_ptr->CloseParent(parent_iter->first, false, "Session stop and manifest = 0");
									//delete parent_iter->second;
									//map_pid_parent.erase(parent_iter);
									
								}
							}
							else {
								_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "Not found pid", iter->first, "in map_pid_parent");
							}
						}
						delete iter->second;
						iter = map_pid_parent_temp.erase(iter);
					}
					else {
						iter++;
					}
				}
				substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.begin();
			}
			else {
				substream_first_reply_peer_iter++;
			}
		}
		else if (substream_first_reply_peer_iter->second->peer_role == CHILD_PEER) {
			if (_log_ptr->diff_TimerGet_ms(&(substream_first_reply_peer_iter->second->connectTimeOut), &new_timer) >= CONNECT_TIME_OUT) {
				
				_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
					"session", substream_first_reply_peer_iter->first,
					"state", substream_first_reply_peer_iter->second->session_state);

				unsigned long session_state = substream_first_reply_peer_iter->second->session_state;
				_peer_mgr_ptr->_peer_communication_ptr->StopSession(session_id);
				_peer_ptr->StopSession(session_id);
				
				substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.begin();
			}
			else {
				substream_first_reply_peer_iter++;
			}
			
		}
	}
	*/
	/////////////////for connect timeout  END/////////////////////////////

	
	/*	Log periodically sending(source-delay, quality, and bandwidth)	*/
	struct timerStruct log_now_time;
	_log_ptr->timerGet(&log_now_time);
	
	// send source-delay
	if (_logger_client_ptr->log_source_delay_init_flag) {
		if (_log_ptr->diff_TimerGet_ms(&(_logger_client_ptr->log_period_source_delay_start),&log_now_time) > LOG_DELAY_SEND_PERIOD) {
			_logger_client_ptr->log_period_source_delay_start = log_now_time;
			send_source_delay(_sock);		// Send souce-delay to pk
			_logger_client_ptr->send_max_source_delay();		// Send source-delay to log-server
			send_topology_to_log();				// Send topology to log-server
			
			_net_ptr->nin = 0;

			//*(_net_ptr->_errorRestartFlag) = RESTART;	
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "--- Peer Information ---");
			for (map<unsigned long, struct peer_connect_down_t *>::iterator iter = map_pid_parent.begin(); iter != map_pid_parent.end(); iter++) {
				//debug_printf("parent %d  manifest %d  state %d \n", iter->first, iter->second->peerInfo.manifest, iter->second->peerInfo.state);
				_log_ptr->write_log_format("s(u) s u s u s u s u \n", __FUNCTION__, __LINE__, "parent", iter->first, "fd", iter->second->peerInfo.sock, "manifest", iter->second->peerInfo.manifest, "state", iter->second->peerInfo.connection_state);
			}
			for (map<unsigned long, struct peer_info_t *>::iterator iter = map_pid_child.begin(); iter != map_pid_child.end(); iter++) {
				//debug_printf("child %d  manifest %d  state %d \n", iter->first, iter->second->manifest, iter->second->state);
				_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__, "child", iter->first, "manifest", iter->second->manifest, "state", iter->second->connection_state);
			}
			for (map<unsigned long, int>::iterator iter = _peer_ptr->map_in_pid_fd.begin(); iter != _peer_ptr->map_in_pid_fd.end(); iter++) {
				//debug_printf("parent-pid %d  fd %d \n", iter->first, iter->second);
				_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "parent-pid", iter->first, "fd", iter->second);
			}
			for (map<unsigned long, int>::iterator iter = _peer_ptr->map_in_pid_udpfd.begin(); iter != _peer_ptr->map_in_pid_udpfd.end(); iter++) {
				debug_printf("parent-pid %d  fd %d \n", iter->first, iter->second);
				_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "parent-pid", iter->first, "fd", iter->second);
			}
			for (map<unsigned long, int>::iterator iter = _peer_ptr->map_out_pid_fd.begin(); iter != _peer_ptr->map_out_pid_fd.end(); iter++) {
				//debug_printf("child-pid %d  fd %d \n", iter->first, iter->second);
				_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "child-pid", iter->first, "fd", iter->second);
			}
			for (map<unsigned long, int>::iterator iter = _peer_ptr->map_out_pid_udpfd.begin(); iter != _peer_ptr->map_out_pid_udpfd.end(); iter++) {
				//debug_printf("child-pid %d  fd %d \n", iter->first, iter->second);
				_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "child-pid", iter->first, "fd", iter->second);
			}
			for (int ss_id = 0; ss_id < sub_stream_num; ss_id++) {
				debug_printf("ss %d  parent %d  state %d \n", ss_id, ss_table[ss_id]->current_parent_pid, ss_table[ss_id]->state.state);
				_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "ss_id", ss_id, "parent", ss_table[ss_id]->current_parent_pid);
			}
			for (list<unsigned long>::iterator iter = _peer_ptr->priority_children.begin(); iter != _peer_ptr->priority_children.end(); iter++) {
				//debug_printf("priority child %d \n", *iter);
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "priority child", *iter);
			}
			for (map<int, basic_class *>::iterator iter = _net_udp_ptr->_map_fd_bc_tbl.begin(); iter != _net_udp_ptr->_map_fd_bc_tbl.end(); iter++) {
				_log_ptr->write_log_format("s(u) s d [d] = d s d \n", __FUNCTION__, __LINE__, "size", _net_udp_ptr->_map_fd_bc_tbl.size(), iter->first, iter->second, "state", UDT::getsockstate(iter->first));
			}
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "_peer_ptr->substream_first_reply_peer.size()", _peer_ptr->substream_first_reply_peer.size());
			
			
		}
	}
	
	// send quality and bandwidth
	if (_logger_client_ptr->log_bw_in_init_flag) {
		if (_log_ptr->diff_TimerGet_ms(&(_logger_client_ptr->log_period_bw_start),&log_now_time) > LOG_BW_SEND_PERIOD) {
			_logger_client_ptr->log_period_bw_start = log_now_time;
			_logger_client_ptr->send_bw();
		}
	}

	// Substream Timeout Detection
	for (unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
		// 在觸發 rescue 的前一秒送 blocl_rescue 給相關的 child-peers
		int threshold = (DATA_TIMEOUT - 1000) > 0 ? DATA_TIMEOUT - 1000 : DATA_TIMEOUT;
		if (_log_ptr->diff_TimerGet_ms(&ss_table[ss_id]->timer.timer, &new_timer) >= threshold && ss_table[ss_id]->can_send_block_rescue == TRUE) {
			// 還沒被凍結 rescue 的話才會發出 Block Rescue，否則不發出
			if (ss_table[ss_id]->block_rescue == FALSE) {
				_peer_mgr_ptr->SendBlockRescue(ss_id, 3);
				_log_ptr->timerGet(&ss_table[ss_id]->block_rescue_interval);
				ss_table[ss_id]->can_send_block_rescue = FALSE;
			}
		}

		if (_log_ptr->diff_TimerGet_ms(&ss_table[ss_id]->timer.timer, &new_timer) >= DATA_TIMEOUT) {

			map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
			struct peer_connect_down_t *parent_info = NULL;
			unsigned long parent_pid = -1;

			// Get neccessary info
			// 這部分因為可能找不到 substream 的 parent，先暫時分兩部分寫
			parent_pid = ss_table[ss_id]->current_parent_pid;
			if ((pid_peerDown_info_iter = map_pid_parent.find(parent_pid)) == map_pid_parent.end()) {
				// Timeout 發生時沒有找到 parent-peer
				debug_printf("parent pid %d \n", parent_pid);
				//handle_error(MACCESS_ERROR, "[ERROR] not found pid in map_pid_parent", __FUNCTION__, __LINE__);


				if (ss_table[ss_id]->block_rescue == FALSE) {
						UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
						unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
						unsigned long pk_new_manifest = pk_old_manifest | manifest;
						bool need_source = true;

						SetSubstreamState(ss_id, SS_RESCUE2);
						debug_printf("[RESCUE_TYPE] 3 substreamID %d timeout \n", ss_id);

						// Log the messages
						_logger_client_ptr->log_to_server(LOG_TIME_OUT, SubstreamIDToManifest(ss_id));
						_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
						_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 3 Not found parent");
						_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__,
							"[RESCUE_TYPE] 3 Rescue in STABLE state. ss_id =", ss_id,
							"Not found parent");

						ss_table[ss_id]->state.is_testing = false;

						// Update manifest
						set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
						SetSubstreamParent(manifest, PK_PID);

						//send_parentToPK(manifest, PK_PID+1);
						NeedSourceDecision(&need_source);
						//send_rescueManifestToPK(pk_new_manifest, need_source);
						send_rescueManifestToPK(manifest, need_source);
						ss_table[ss_id]->state.dup_src = need_source;

						// Reset source delay parameters of testing substream of this parent
						ResetDetection(ss_id);
				}
				else {
					UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
					debug_printf("Block Rescue [RESCUE_TYPE] 3 substreamID %d timeout \n", ss_id);
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s \n", "pid", my_pid, manifest, "Block Rescue [RESCUE_TYPE] 3 Not found parent");
					_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__,
						"Block Rescue [RESCUE_TYPE] 3 Rescue in STABLE state. ss_id =", ss_id,
						"Not found parent");
					ResetDetection(ss_id);
				}
			}
			else {
				// Timeout 發生時有找到 parent-peer
				parent_info = pid_peerDown_info_iter->second;
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Rescue triggered. timeout. ss_id =", ss_id);

				if (parent_pid == PK_PID) {
					UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
					unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
					unsigned long pk_new_manifest = pk_old_manifest | manifest;
					bool need_source = true;

					SetSubstreamState(ss_id, SS_RESCUE2);
					debug_printf("[RESCUE_TYPE] 3 Parent is PK \n");

					// Log the messages
					_logger_client_ptr->log_to_server(LOG_TIME_OUT, SubstreamIDToManifest(ss_id));
					_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s \n", "pid", my_pid, "[RESCUE_TYPE] 3 Parent is PK");
					_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[RESCUE_TYPE] 3 Parent is PK");

					ss_table[ss_id]->state.is_testing = false;

					// Update manifest
					set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
					SetSubstreamParent(manifest, PK_PID);

					//send_parentToPK(manifest, PK_PID+1);
					NeedSourceDecision(&need_source);
					//send_rescueManifestToPK(pk_new_manifest, need_source);
					send_rescueManifestToPK(manifest, need_source);
					ss_table[ss_id]->state.dup_src = need_source;

					//handle_error(RECV_NODATA, "[ERROR] Rescue triggered. PK timeout", __FUNCTION__, __LINE__);

					// Reset source delay parameters of testing substream of this parent
					ResetDetection(ss_id);
				}
				else {
					debug_printf("parent pid %d ss_id %d Time out \n", parent_pid, ss_id);
					_log_ptr->write_log_format("s(u) u s u \n", __FUNCTION__, __LINE__, my_pid, "Parent-peer Timeout. pid =", parent_pid);
					//_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u u \n", "my_pid", my_pid, "Parent-peer Timeout. pid =", parent_pid, __LINE__);
					if (ss_table[ss_id]->block_rescue == FALSE) {
						if (ss_table[ss_id]->state.state == SS_STABLE2 || ss_table[ss_id]->state.state == SS_INIT) {
							UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
							unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
							unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
							unsigned long pk_new_manifest = pk_old_manifest | manifest;
							unsigned long parent_new_manifest = parent_old_manifest & ~manifest;
							bool need_source = true;

							SetSubstreamState(ss_id, SS_RESCUE2);
							debug_printf("[RESCUE_TYPE] 3 substreamID %d timeout \n", ss_id);

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_TIME_OUT, SubstreamIDToManifest(ss_id));
							_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s u s \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 3 Parent", parent_info->peerInfo.pid, "timeout");
							_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 3 Rescue in STABLE state. ss_id =", ss_id,
								"parent pid =", parent_info->peerInfo.pid);

							ss_table[ss_id]->data.previousParentPID = parent_info->peerInfo.pid;
							ss_table[ss_id]->state.is_testing = false;

							// Update manifest
							set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
							set_parent_manifest(parent_info, parent_new_manifest);
							SetSubstreamParent(manifest, PK_PID);
							_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);
							if (parent_info->peerInfo.manifest == 0) {
								_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[REASON] manifest = 0 parent", parent_info->peerInfo.pid);
								_peer_ptr->CloseParent(parent_info->peerInfo.pid, false, "[REASON] manifest = 0");
								_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[CHECK POINT] map_pid_parent.size()", map_pid_parent.size());
							}

							//send_parentToPK(manifest, PK_PID+1);
							NeedSourceDecision(&need_source);
							//send_rescueManifestToPK(pk_new_manifest, need_source);
							send_rescueManifestToPK(manifest, need_source);
							ss_table[ss_id]->state.dup_src = need_source;

							// Reset source delay parameters of testing substream of this parent
							ResetDetection(ss_id);
						}
						else if (ss_table[ss_id]->state.state == SS_CONNECTING2) {
							// No more trigger rescue again
							debug_printf("Should not happen \n");
						}
						else if (ss_table[ss_id]->state.state == SS_RESCUE2) {
							// No more trigger rescue again
							debug_printf("Should not happen \n");
						}
						else if (ss_table[ss_id]->state.state == SS_TEST2) {
							UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
							unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
							unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
							unsigned long pk_new_manifest = pk_old_manifest | manifest;
							unsigned long parent_new_manifest = parent_old_manifest & ~manifest;
							bool need_source = true;

							SetSubstreamState(ss_id, SS_RESCUE2);
							debug_printf("[RESCUE_TYPE] 3 Parent timeout \n");

							// Log the messages
							_logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL, SubstreamIDToManifest(ss_id), PK_PID);
							_logger_client_ptr->log_to_server(LOG_TOPO_RESCUE_TRIGGER, manifest, PK_PID);
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s u s \n", "pid", my_pid, manifest, "[RESCUE_TYPE] 3 Parent", parent_info->peerInfo.pid, "timeout");
							_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
								"[RESCUE_TYPE] 3 Rescue in TEST state. ss_id =", ss_id,
								"parent pid =", parent_info->peerInfo.pid);

							ss_table[ss_id]->state.is_testing = false;

							// Update manifest
							set_parent_manifest(pkDownInfoPtr, pk_new_manifest);
							set_parent_manifest(parent_info, parent_new_manifest);
							SetSubstreamParent(manifest, PK_PID);
							_peer_mgr_ptr->send_manifest_to_parent(parent_info->peerInfo.pid, parent_new_manifest, TOTAL_OP);
							if (parent_info->peerInfo.manifest == 0) {
								_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[REASON] manifest = 0 parent", parent_info->peerInfo.pid);
								_peer_ptr->CloseParent(parent_info->peerInfo.pid, false, "[REASON] manifest = 0");
								_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[CHECK POINT] map_pid_parent.size()", map_pid_parent.size());
							}

							//send_parentToPK(manifest, PK_PID+1);
							NeedSourceDecision(&need_source);
							//send_rescueManifestToPK(pk_new_manifest, need_source);
							send_rescueManifestToPK(manifest, need_source);
							ss_table[ss_id]->state.dup_src = need_source;

							// Reset source delay parameters of testing substream of this parent
							ResetDetection(ss_id);
						}
						else {
							// Unexpected state
						}
					}
					else {
						UINT32 manifest = SubstreamIDToManifest(ss_id);		// Manifest of this substream
						debug_printf("Block Rescue [RESCUE_TYPE] 3 substreamID %d timeout \n", ss_id);
						_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u(u) s \n", "pid", my_pid, manifest, "Block Rescue [RESCUE_TYPE] 3 Not found parent");
						_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__,
							"Block Rescue [RESCUE_TYPE] 3 Rescue in STABLE state. ss_id =", ss_id,
							"Not found parent");
						ResetDetection(ss_id);
					}
				}
			}
		}

		if (ss_table[ss_id]->state.state == SS_TEST2 && _log_ptr->diff_TimerGet_ms(&ss_table[ss_id]->timer.inTest_timer, &new_timer) >= TEST_DELAY_TIME) {
			ss_table[ss_id]->timer.inTest_flag = true;
		}
		
	}
	
	////////////////////for RE-SYN time START////////////////////////////////
	if (_log_ptr->diff_TimerGet_ms(&reSynTimer,&new_timer) >= reSynTime && syncLock ==0) {
		debug_printf("_log_ptr ->diff_TimerGet_ms = %u, reSynTime = %u \n", _log_ptr->diff_TimerGet_ms(&reSynTimer, &new_timer), reSynTime);
		send_syn_token_to_pk(_sock);
		reSynTimer = new_timer;
	}
	////////////////////for RE-SYN time END////////////////////////////////

	// Calculate Xcount every XCOUNT_INTERVAL seconds
	if (_log_ptr->diff_TimerGet_ms(&XcountTimer, &new_timer) >= XCOUNT_INTERVAL) {
		int temp = Xcount;
		//Xcount = 0.5*(pkt_count / (PARAMETER_X*(XCOUNT_INTERVAL/1000))) + 0.5*pkt_rate / PARAMETER_X;
		Xcount = 0.5*(pkt_count / (XCOUNT_INTERVAL/1000)) + 0.5*pkt_rate;
		// Set Minimum
		if (Xcount < 1) {
			Xcount = 1;
		}
		debug_printf("Xcount(pkt_rate) %lu \n", Xcount);
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Xcount changed from", temp, "to", Xcount);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s d d (d) \n", "pid", my_pid, "Xcount:", Xcount, temp, pkt_rate);
		XcountTimer = new_timer;
		//start = end;
		pkt_count = 0;
	}
	

	//to avoid CPU 100%
//	if((new_timer - sleepTimer) >= 1){
//		Sleep(1);
//		sleepTimer=new_timer ;
////		debug_printf("hello\n");
//	}

	static unsigned long runCount = 0;
	runCount++;
	if (runCount %10 ==0) {
#ifdef _WIN32
		//Sleep(1);
#else
		usleep(1000);
#endif
	}
}

void pk_mgr::handle_error(int err_code, const char *err_msg, const char *func, unsigned int line)
{
	exit_code = err_code;
	debug_printf("\n\nERROR:0x%04x  %s  (%s:%u)   %s \n", err_code, err_msg, func, line, _log_ptr->get_now_time());
	_log_ptr->write_log_format("s(u) s  (s:u) \n", func, line, err_msg, func, line);
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", func, line, err_msg);

	switch (err_code) {
	case MALLOC_ERROR:
		break;
	default:
		_logger_client_ptr->log_exit();
		//data_close(_sock, err_msg);
		PAUSE
		break;
	}
	
	//exit_code = err_code;
	//debug_printf("\n\nERROR:0x%04x  %s  (%s:%u)   %s \n", err_code, err_msg, func, line, _log_ptr->get_now_time());
	//_log_ptr->write_log_format("s(u) s  (s:u) \n", func, line, err_msg, func, line);
	//_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", func, line, err_msg);
	//_logger_client_ptr->log_exit();
	//data_close(_sock, err_msg);
	//PAUSE
	
	//*(_net_ptr->_errorRestartFlag) = RESTART;
}

void pk_mgr::ArrangeResource()
{

}










