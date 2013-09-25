/*
對於manifest 的概念 ,他只代表sub-stream ID的表示法
每個 peer 應該顧好自己的 manifest table  確保每個sub -straem 都有來源
*/

#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "stunt_mgr.h"
#ifdef _FIRE_BREATH_MOD_
#include "rtsp_viewer.h"
#else
#include "librtsp/rtsp_viewer.h"
#endif

/*	Include STUNT-Server file	*/
#include "stunt/ClientMacro_v2.h"
#include "stunt/tcp_punch.h"

tcp_punch *tcp_punch_ptr = new tcp_punch();

using namespace std;

pk_mgr::pk_mgr(unsigned long html_size, list<int> *fd_list, network *net_ptr , logger *log_ptr , configuration *prep, logger_client * logger_client_ptr, stunt_mgr* stunt_mgr_ptr)
{
	_stunt_mgr_ptr = stunt_mgr_ptr;
	_logger_client_ptr = logger_client_ptr;
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
	//	 childrenSet_ptr = NULL;
	full_manifest = 0;
	threadLockKey = FREE ;
	Xcount = 50;
	pkSendCapacity = false;
	ssDetect_ptr =NULL ;
	statsArryCount_ptr =NULL;
	memset(&lastSynStartclock,0x00,sizeof(struct timerStruct));
	totalMod =0;
	reSynTime=BASE_RESYN_TIME;
	_log_ptr->timerGet(&start);
	fisttimestamp=0 ;
	firstIn =true;
	pkt_count =0 ;
	totalbyte=0;
	sentStartDelay = FALSE;
	synLock=0;

	//
//	pkmgrfile_ptr = fopen("./HEREin" , "wb");
	//	 _log_measureDataPtr =new 
//	performance_filePtr = fopen("./performance_file.txt" , "w");

	_prep->read_key("bucket_size", _bucket_size);


	//主要的大buffer
	//	_chunk_bitstream = (struct chunk_bitstream_t *)malloc(RTP_PKT_BUF_MAX * _bucket_size);
	//	memset( _chunk_bitstream, 0x0, _bucket_size * RTP_PKT_BUF_MAX );

	buf_chunk_t = NULL;
	buf_chunk_t = (struct chunk_t **)malloc(_bucket_size * sizeof(struct chunk_t **) ) ;
	memset( buf_chunk_t, NULL, _bucket_size * sizeof(struct chunk_t **) );

	struct chunk_t* chunk_t_ptr ;


	for (int i = 0; i <_bucket_size; i++) {
		chunk_t_ptr  = (struct chunk_t *)new unsigned char[sizeof(struct chunk_t)];
		if (!chunk_t_ptr) {
			debug_printf("pk_mgr::chunk_t_ptr  new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "chunk_t_ptr new error");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "chunk_t_ptr new error \n");
			PAUSE
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

	clear_map_pid_peer_info();
	clear_map_pid_peerDown_info();
	clear_map_pid_rescue_peer_info();
	clear_delay_table();
	clear_map_streamID_header();
	clear_map_pid_child_peer_info();

	debug_printf("==============deldet pk_mgr success==========\n");
}

//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
void pk_mgr::delay_table_init()
{
	//map<unsigned long, struct source_delay *>::iterator delay_table_iter;
	for (int i = 0; i < sub_stream_num; i++) {
		struct source_delay *source_delay_temp_ptr = NULL;
		source_delay_temp_ptr = new struct source_delay;
		if (!source_delay_temp_ptr) {
			debug_printf("peer_mgr::source_delay_temp_ptr  new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] source_delay_temp_ptr new error");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "source_delay_temp_ptr new error \n");
			PAUSE
		}
		memset(source_delay_temp_ptr, 0, sizeof(struct source_delay)); 

		source_delay_temp_ptr->source_delay_time = -1;
		source_delay_temp_ptr->first_pkt_recv = false;
		source_delay_temp_ptr->end_seq_num = 0;
		source_delay_temp_ptr->end_seq_abs_time = 0;
		source_delay_temp_ptr->rescue_state = 0;
		source_delay_temp_ptr->delay_beyond_count = 0;

		delay_table.insert(pair<unsigned long,struct source_delay *>(i, source_delay_temp_ptr));
		
		//debug_printf("source delay substreamID %d initialization end \n", i);
		_log_ptr->write_log_format("s(u) s d s \n", __FUNCTION__, __LINE__, "source delay substreamID", i, "initialization end");
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
void pk_mgr::send_capacity_init()
{
	peer_start_delay_count = 0;
	//peer_join_send = 0;
}
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
void pk_mgr::quality_source_delay_count(int sock, unsigned long substream_id, unsigned int seq_now)
{
	map<unsigned long,struct source_delay *>::iterator delay_table_iter;

	delay_table_iter = delay_table.find(substream_id);
	if (delay_table_iter == delay_table.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n","error : can not find sub stream struct in quality_source_delay_count\n");
		_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

		_logger_client_ptr->log_exit();
	}

	//if skip packet or before syn  we will not calculate
	if (seq_now > syn_table.start_seq && seq_now >= _current_send_sequence_number) {
		unsigned int detect_source_delay_time;

		_logger_client_ptr->quality_struct_ptr->total_chunk += 1;

		detect_source_delay_time = _log_ptr->diff_TimerGet_ms(&(syn_table.start_clock), &delay_table_iter->second->client_end_time);
		
		if (delay_table_iter->second->end_seq_num == 0) {
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__, __LINE__, "LOG_WRITE_STRING");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "start delay end seq error in source_delay_detection\n");
			_logger_client_ptr->log_exit();
		}
		else {
			unsigned int temp;
			int diff_temp;
			temp = (delay_table_iter->second->end_seq_abs_time) - (syn_table.client_abs_start_time);
			diff_temp = (detect_source_delay_time) - temp;

			if (diff_temp < 0) {
				diff_temp = 0;
			}

			_logger_client_ptr->set_source_delay(substream_id,(unsigned long)diff_temp);
			_log_ptr->write_log_format("s(u) s d(d) s d \n", __FUNCTION__, __LINE__, "Set substream id", substream_id, seq_now, "source delay =", diff_temp);
			//printf("differ : %ld source_delay_detection sub id : %d\n",diff_temp,substream_id);

			/*
			this part count
			*/
			if (((double)detect_source_delay_time - MAX_DELAY) == 0) {
				_logger_client_ptr->quality_struct_ptr->quality_count += 1;
				return;
			}
			double temp_quility = (double)temp/((double)detect_source_delay_time - MAX_DELAY) ;
			
			//printf("detect_source_delay_time: %d, temp: %d, temp_quility: %f \n", detect_source_delay_time, temp, temp_quility);
			
			if ( ((double)detect_source_delay_time - MAX_DELAY) < 0 ) {
				_logger_client_ptr->quality_struct_ptr->quality_count += 1;
			}
			else if (((double)detect_source_delay_time-MAX_DELAY) >= 0 &&  temp_quility >= 1) {
				_logger_client_ptr->quality_struct_ptr->quality_count += 1;
			}
			else {
				_logger_client_ptr->quality_struct_ptr->quality_count += temp_quility ;
			}
		}
	}
}

// Detect each chunk
void pk_mgr::source_delay_detection(int sock, unsigned long sub_id, unsigned int seq_now)
{
	//printf("===== source_delay_detection \n");
	map<unsigned long,struct source_delay *>::iterator delay_table_iter;
	delay_table_iter = delay_table.find(sub_id);
	if (delay_table_iter == delay_table.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : can not find sub stream struct in source_delay_detection\n");
		_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");
		_logger_client_ptr->log_exit();
	}

	// Filtering
	if (seq_now <= delay_table_iter->second->end_seq_num) {
		return ;
	}
	else{
		delay_table_iter->second->end_seq_num = seq_now ;
	}

	if (seq_now > syn_table.start_seq) {
		unsigned int detect_source_delay_time;
		map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
		map<int , unsigned long>::iterator map_fd_pid_iter;
		map<unsigned long, int>::iterator map_out_pid_fd_iter;
		unsigned long detect_peer_id;
		unsigned long testingManifest=0 ;
		unsigned long testingSubStreamID=-1 ;
		//unsigned long afterManifest = 0;
		unsigned long tempManifest=0 ;
		unsigned long peerTestingManifest=0 ;

		map_fd_pid_iter = _peer_ptr->map_fd_pid.find(sock);
		if (map_fd_pid_iter == _peer_ptr->map_fd_pid.end()) {
			debug_printf("[ERROR] can not find fd pid in source_delay_detection\n");
			//return;
			PAUSE
		}

		pid_peerDown_info_iter = map_pid_peerDown_info.find(map_fd_pid_iter->second);
		if (pid_peerDown_info_iter == map_pid_peerDown_info.end()) {
			debug_printf("[ERROR] can not find pid peerinfo in source_delay_detection\n");
			//return;
			PAUSE
		}

		detect_source_delay_time = _log_ptr->diff_TimerGet_ms(&syn_table.start_clock, &delay_table_iter->second->client_end_time);
		
		if (delay_table_iter->second->end_seq_num == 0) {
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","start delay end seq error in source_delay_detection\n");
			_logger_client_ptr->log_exit();
		}
		else{
			UINT32 temp;
			INT32 source_delay;
			temp = delay_table_iter->second->end_seq_abs_time - syn_table.client_abs_start_time;
			source_delay = (detect_source_delay_time) - temp;	// This is source delay time
			
			//debug_printf("detect_source_delay_time: %d, temp: %d, source_delay: %d \n", detect_source_delay_time, temp, source_delay);
			debug_printf2("ssid: %d, source_delay: %d \n", sub_id, source_delay);
			debug_printf2("delay_beyond_count: %d > %d ? \n", delay_table_iter->second->delay_beyond_count, (int)(((double)SOURCE_DELAY_CONTINUOUS*PARAMETER_X*Xcount) /(double)sub_stream_num));
			
			if (source_delay < 0) {
				//_logger_client_ptr->set_source_delay(sub_id,0);


				if(totalMod >= (syn_round_time )){
					//should re_syn
					debug_printf("!!!!!!!!!!!!TotalMod OVER   (syn_round_time )!!!!!!!!!!!!!\n");
					if(reSynTime >5000){
						reSynTime =reSynTime /2;
						if(reSynTime <=5000)
							reSynTime =5000;
						debug_printf("reSynTime Bad change to reSynTime/2  = %d \n",reSynTime);
					}else{
						reSynTime =5000;
						debug_printf("reSynTime Bad change to reSynTime/2  = %d \n",reSynTime);
						//doing nothing it small bound if  reSynTime==10
					}
					//send_syn_token_to_pk(_sock);
				}else{
					debug_printf("diff error in source_delay_detection\n");
					debug_printf("differ : %ld ",source_delay);
					debug_printf("syn_round : %ld\n",syn_round_time);
					debug_printf("syn_table.client_abs_start_time : %lu\n",syn_table.client_abs_start_time);
					syn_table.client_abs_start_time = syn_table.client_abs_start_time + (unsigned long)abs(source_delay);
					totalMod +=  (unsigned long)abs(source_delay) ;
					debug_printf("Total Mod =%d \n",totalMod);
				}

				debug_printf("syn_table.client_abs_start_time : %lu\n",syn_table.client_abs_start_time);
				delay_table_iter->second->delay_beyond_count = 0;
				//PAUSE
				//exit(1);
			}
			else if (source_delay > MAX_DELAY) {
				//_logger_client_ptr->set_source_delay(sub_id,source_delay);
				delay_table_iter->second->delay_beyond_count++;
				
				//觸發條件
				debug_printf("delay_beyond_count: %d > %d ? \n", delay_table_iter->second->delay_beyond_count, (int)(((double)SOURCE_DELAY_CONTINUOUS*PARAMETER_X*Xcount) /(double)sub_stream_num));
				if ((delay_table_iter->second->delay_beyond_count > (int)(((double)SOURCE_DELAY_CONTINUOUS*PARAMETER_X*Xcount) /(double)sub_stream_num) ) && 
					(pid_peerDown_info_iter->second->lastTriggerCount == 0)) {
					
					_log_ptr->write_log_format("s(u) s s d s d \n", __FUNCTION__, __LINE__, "Rescue triggered.",
															   "delay_table_iter->second->delay_beyond_count =", delay_table_iter->second->delay_beyond_count,
															   ">", (int)(((double)SOURCE_DELAY_CONTINUOUS*PARAMETER_X*Xcount) /(double)sub_stream_num));
					
					delay_table_iter->second->delay_beyond_count = 0;
					pid_peerDown_info_iter->second->lastTriggerCount = 1 ;
					
					//找出所有正在測試的substream
					for (unsigned long i = 0; i < sub_stream_num; i++) {
						if (!(check_rescue_state(i,0))) {
							testingManifest |= SubstreamIDToManifest(i);
						}
					}
					
					//找出這個peer 所有正在testing 的substream ID
					peerTestingManifest = (testingManifest & pid_peerDown_info_iter->second->peerInfo.manifest);

					_log_ptr->write_log_format("s =>u s u s u s \n", __FUNCTION__,__LINE__,
																	 "pid ",pid_peerDown_info_iter->second->peerInfo.pid,
																	 "need cut substream id", sub_id, 
																	 "and need rescue");


					// If this substream is from parent, we have no idea to rescue it. Else, find this substream's
					// parent, and cut the testing substream from it(if have testing substream from it). If not found 
					// testing substream, cut other normal substream
					if (pid_peerDown_info_iter->second->peerInfo.pid == PK_PID) {
						debug_printf("Provider is PK can not rescue\n");
						_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__, __LINE__, "Parent is PK can not rescue");
					}
					// If this substream is testing substream
					else if (peerTestingManifest & SubstreamIDToManifest(sub_id)) {
						debug_printf("differ : %ld need to rescue source_delay_detection\n",source_delay);
						debug_printf("sub stream : %ld pid : %d need to rescue source_delay_detection\n",sub_id,pid_peerDown_info_iter->second->peerInfo.pid);

						_logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL, SubstreamIDToManifest(sub_id));
						_logger_client_ptr->log_to_server(LOG_DATA_COME_PK, SubstreamIDToManifest(sub_id));
						_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
																	  "Rescue in testing state. substreamID =", sub_id,
																	  "parentID=", pid_peerDown_info_iter->second->peerInfo.pid,
																	  "parent's testing manifest =", peerTestingManifest);
						
						
						//若有多個正在測試中一次只選擇一個substream cut(最右邊的)  並且重設全部記數器的count
						testingSubStreamID = sub_id; //manifestToSubstreamID (peerTestingManifest);

						(ssDetect_ptr + testingSubStreamID)->isTesting = 0;  //false  

						//set rescue state to normal
						set_rescue_state(testingSubStreamID, 0);

						//should sent to PK select PK ,再把testing 取消偵測的 pk_manifest 設回來
						unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
						pkDownInfoPtr ->peerInfo.manifest |=  SubstreamIDToManifest (testingSubStreamID );
						_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
																	   "PK manifest change from", pk_old_manifest,
																	   "to", pkDownInfoPtr->peerInfo.manifest);

						//should sent to peer cut stream
						unsigned long parent_old_manifest = pid_peerDown_info_iter->second->peerInfo.manifest;
						pid_peerDown_info_iter->second->peerInfo.manifest &= (~ SubstreamIDToManifest (testingSubStreamID )) ;
						_peer_mgr_ptr -> send_manifest_to_parent(pid_peerDown_info_iter->second->peerInfo.manifest ,pid_peerDown_info_iter->second->peerInfo.pid);
						_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
																		   "parent pid", pid_peerDown_info_iter->second->peerInfo.pid,
																		   "manifest change from", parent_old_manifest,
																		   "to", pid_peerDown_info_iter->second->peerInfo.manifest);
																
						/* 0903註解掉，因為還沒送給parent manifest=0這個訊息，等到peer::pkt_out將訊息送出後再刪除
						//如果manifest 是0 就關掉這個peer
						if(pid_peerDown_info_iter->second->peerInfo.manifest == 0 ){
							map<unsigned long, int>::iterator iter = _peer_ptr ->map_in_pid_fd.find(pid_peerDown_info_iter ->first) ;
							if(iter !=_peer_ptr ->map_in_pid_fd.end()){
								_peer_ptr ->data_close(_peer_ptr ->map_in_pid_fd[pid_peerDown_info_iter ->first],"manifest=0",CLOSE_PARENT) ;
								return ;
							}
							else{
								debug_printf("pkmgr"); 
								PAUSE	//PAUSE
							}
						}
						*/

						send_parentToPK(SubstreamIDToManifest(testingSubStreamID), PK_PID+1);
						//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Send updated manifest");
						
						peerTestingManifest &= (~SubstreamIDToManifest(testingSubStreamID));

						while (peerTestingManifest) {
							testingSubStreamID = manifestToSubstreamID(peerTestingManifest);
							(ssDetect_ptr + testingSubStreamID)->testing_count = 0;
							peerTestingManifest &= (~SubstreamIDToManifest(testingSubStreamID));
						}

						//debug_printf("stream testing fail \n");
						//_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"stream testing fail ");
					}
					//normal rescue
					else {
						//debug_printf("differ : %ld need to rescue source_delay_detection\n", source_delay);
						//debug_printf("sub stream : %ld pid : %d need to rescue source_delay_detection\n", sub_id,pid_peerDown_info_iter->second->peerInfo.pid);
						//debug_printf("normal rescue \n");
						_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__, "normal rescue ");
						_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
																	  "Rescue in normal state. substreamID =", sub_id,
																	  "parentID=", pid_peerDown_info_iter->second->peerInfo.pid);
																		   
						_logger_client_ptr->log_to_server(LOG_DELAY_RESCUE_TRIGGER,SubstreamIDToManifest(sub_id));

						unsigned long parent_updated_manifest = (pid_peerDown_info_iter->second->peerInfo.manifest & (~SubstreamIDToManifest(sub_id)));		//manifestFactory (pid_peerDown_info_iter->second->peerInfo.manifest , 1);
						unsigned long parent_old_manifest = pid_peerDown_info_iter->second->peerInfo.manifest;
						
						unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
						pkDownInfoPtr->peerInfo.manifest |= (pid_peerDown_info_iter->second->peerInfo.manifest & (~parent_updated_manifest));
						_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
																	   "PK manifest change from", pk_old_manifest,
																	   "to", pkDownInfoPtr->peerInfo.manifest);
																	   
						//這邊可能因為有些stream 正在testing 而沒有flag 因此傳出去的需要再跟testing 的合起來
						send_rescueManifestToPK(pkDownInfoPtr ->peerInfo.manifest | testingManifest);
						_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Send updated PK's manifest to PK");

						// tempManifest == SubstreamIDToManifest(sub_id)?
						tempManifest =(pid_peerDown_info_iter->second->peerInfo.manifest &(~parent_updated_manifest)) ;

						//把先前的連接的PID存起來
						while(tempManifest){
							testingSubStreamID = manifestToSubstreamID (tempManifest);
							set_rescue_state(testingSubStreamID,1);
							(ssDetect_ptr + testingSubStreamID) ->previousParentPID = pid_peerDown_info_iter->second->peerInfo.pid;
							tempManifest &=  (~SubstreamIDToManifest(testingSubStreamID) );
						}
						
						// Update this peer's manifest after cut off testingSubStreamID(ss needed to rescue)
						pid_peerDown_info_iter->second->peerInfo.manifest = parent_updated_manifest;
						_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
																		   "parent pid", pid_peerDown_info_iter->second->peerInfo.pid,
																		   "manifest change from", parent_old_manifest,
																		   "to", parent_updated_manifest);
																		   
						_peer_mgr_ptr->send_manifest_to_parent(parent_updated_manifest ,pid_peerDown_info_iter->second->peerInfo.pid);
						_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__,
															"Send updated parentID", pid_peerDown_info_iter->second->peerInfo.pid,
															"'s manifest to parentID", pid_peerDown_info_iter->second->peerInfo.pid);
						
						/* 0903註解掉，因為還沒送給parent manifest=0這個訊息，等到peer::pkt_out將訊息送出後再刪除
						if(pid_peerDown_info_iter->second->peerInfo.manifest == 0 ){
							map<unsigned long, int>::iterator iter = _peer_ptr ->map_in_pid_fd.find(pid_peerDown_info_iter ->first) ;
							if(iter !=_peer_ptr ->map_in_pid_fd.end()){
								_peer_ptr ->data_close(_peer_ptr ->map_in_pid_fd[pid_peerDown_info_iter ->first],"manifest=0",CLOSE_PARENT) ;
							}else{
								debug_printf("pkmgr"); 
								PAUSE	//PAUSE
							}

						}
						*/
					}

				}
				else{
				//delay_table_iter->second->delay_beyond_count = 0;
				//printf("differ : %ld source_delay_detection sub id : %d\n",source_delay,sub_id);
				}
			}
			else{
				//_logger_client_ptr->set_source_delay(sub_id,source_delay);
				delay_table_iter->second->delay_beyond_count = 0;
			}
		}
	}
}

void pk_mgr::send_capacity_to_pk(int sock){

	map<int , unsigned long>::iterator temp_map_fd_pid_iter;
	struct rescue_peer_capacity_measurement *chunk_capacity_ptr = NULL;
	//struct chunk_t * chunk_ptr = NULL;
	int msg_size,send_size;
	map<unsigned long, struct source_delay *>::iterator delay_table_iter;

	msg_size = sizeof(struct rescue_peer_capacity_measurement) + sizeof(unsigned long *)*sub_stream_num;
	send_size = msg_size - sizeof(unsigned long *)*sub_stream_num + sizeof(unsigned long)*sub_stream_num; 

	chunk_capacity_ptr = (struct rescue_peer_capacity_measurement *)new unsigned char[msg_size];
	if (!chunk_capacity_ptr) {
		debug_printf("pk_mgr::chunk_capacity_ptr  new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "chunk_capacity_ptr new error");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "chunk_capacity_ptr new error \n");
		PAUSE
	}
	//chunk_ptr = (struct chunk_t *) new unsigned char[send_size];

	memset(chunk_capacity_ptr, 0, msg_size);
	//memset(chunk_ptr, 0x0, send_size);

	chunk_capacity_ptr->header.cmd = CHNK_CMD_PEER_RESCUE_CAPACITY;
	chunk_capacity_ptr->header.rsv_1 = REPLY;
	chunk_capacity_ptr->header.length = send_size - sizeof(struct chunk_header_t);
	chunk_capacity_ptr->content_integrity = 1;
	//chunk_capacity_ptr->NAT_status = 1;
	chunk_capacity_ptr->rescue_num = rescueNumAccumulate();

	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d\n","Send Capa res num :",chunk_capacity_ptr->rescue_num);

	for (unsigned long i = 0; i < sub_stream_num; i++) {
		if (syn_table.init_flag != 2) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"send capacity error not syn");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "send capacity error not syn \n");
			_logger_client_ptr->log_exit();
		}
		else {
			delay_table_iter = delay_table.find(i);
			if(delay_table_iter == delay_table.end()){
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"[ERROR] can not find source struct in table in send_capacity_to_pk");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","[ERROR] can not find source struct in table in send_capacity_to_pk\n");
				_logger_client_ptr->log_exit();
			}
//			delay_table_iter->second->source_delay_time = (delay_table_iter->second->client_end_time - syn_table.start_clock);//(_log_ptr ->diffTime_ms(syn_table.start_clock,delay_table_iter->second->client_end_time));
			delay_table_iter->second->source_delay_time = _log_ptr->diff_TimerGet_ms(&syn_table.start_clock,&delay_table_iter->second->client_end_time);
			if (delay_table_iter->second->end_seq_num == 0) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"start delay end seq error in send_capacity_to_pk");

				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","start delay end seq error in send_capacity_to_pk\n");
				_logger_client_ptr->log_exit();
			}
			/*else if((delay_table_iter->second->end_seq_num)< (delay_table_iter->second->start_seq_num)){
			debug_printf("start delay end seq < start seq error in send_capacity_to_pk\n");
			PAUSE
			exit(1);
			}
			else if((delay_table_iter->second->end_seq_num) == (delay_table_iter->second->start_seq_num)){
			debug_printf("start delay end seq == start seq warning in send_capacity_to_pk\n");
			PAUSE
			exit(1);
			}*/
			else{
				unsigned int temp;
				int diff_temp;
				temp = (delay_table_iter->second->end_seq_abs_time) - (syn_table.client_abs_start_time);
				diff_temp = (delay_table_iter->second->source_delay_time) - temp;
				//printf("source_delay_time : %ld",(delay_table+i)->source_delay_time);
				//printf(" time : %d\n",temp);
				debug_printf("abs differ : %lu ",temp);
				debug_printf("abs start : %lu ",(syn_table.client_abs_start_time));
				debug_printf("abs end : %lu ",(delay_table_iter->second->end_seq_abs_time));
				debug_printf("relate differ : %lu ",(delay_table_iter->second->source_delay_time));
				debug_printf("differ : %ld\n",diff_temp);
				if(diff_temp < 0){
					debug_printf("diff error in send_capacity_to_pk   ");
					debug_printf("differ : %ld\n",diff_temp);
					delay_table_iter->second->source_delay_time = 0;
					debug_printf("syn_table.client_abs_start_time : %lu   ",syn_table.client_abs_start_time);
					syn_table.client_abs_start_time = syn_table.client_abs_start_time + (unsigned long)abs(diff_temp);
					debug_printf("syn_table.client_abs_start_time : %lu\n",syn_table.client_abs_start_time);
					//PAUSE
					//exit(1);
				}
				else{
					delay_table_iter->second->source_delay_time = (unsigned long)diff_temp;
				}
			}
			//(delay_table+i)->source_delay_time = (delay_table+i)->source_delay_time + (delay_table+i)->start_delay_struct.start_delay;
			chunk_capacity_ptr->source_delay_measur[i] = new (unsigned long);
			if (!chunk_capacity_ptr->source_delay_measur[i]) {
				debug_printf("pk_mgr::chunk_capacity_ptr->source_delay_measur[i]  new error \n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"chunk_capacity_ptr->source_delay_measur[i] new error");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "chunk_capacity_ptr->source_delay_measur[i] new error \n");
				PAUSE
			}
			memset(chunk_capacity_ptr->source_delay_measur[i], 0x0 , sizeof(unsigned long));
			memcpy(chunk_capacity_ptr->source_delay_measur[i],&(delay_table_iter->second->source_delay_time),sizeof(unsigned long));
			debug_printf(" source delay : %lu\n",(*(chunk_capacity_ptr->source_delay_measur[i])));
		}
	}

	//memcpy(chunk_ptr,chunk_capacity_ptr,send_size);

	int send_byte = 0;
	int expect_len = chunk_capacity_ptr->header.length + sizeof(struct chunk_header_t);
	char *send_buf;
	int capacity_chunk_offset = expect_len - sizeof(unsigned long)*sub_stream_num;
	_net_ptr->set_blocking(sock);	// set to blocking

	send_buf = (char *)new char[send_size];
	if (!send_buf) {
		debug_printf("pk_mgr::send_buf new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"send_buf new error");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "send_buf new error \n");
		PAUSE
	}
	memset(send_buf, 0x0, send_size);
	memcpy(send_buf, chunk_capacity_ptr, capacity_chunk_offset);
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		memcpy((send_buf+capacity_chunk_offset),chunk_capacity_ptr->source_delay_measur[i],sizeof(unsigned long));
		capacity_chunk_offset += sizeof(unsigned long);
	}

	send_byte = _net_ptr->send(sock, send_buf, expect_len, 0);
	if( send_byte <= 0 ) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__,__LINE__,"send_capacity_to_pk  error",WSAGetLastError());
		data_close(sock, "send pkt send_capacity_to_pk error");
	}
	else {
		if (send_buf) {
			delete send_buf;
		}

		for (int i = 0; i < sub_stream_num; i++) {
			delete chunk_capacity_ptr->source_delay_measur[i];
		}

		if (chunk_capacity_ptr) {
			delete chunk_capacity_ptr;
		}
		
		_net_ptr->set_nonblocking(sock);	// set to non-blocking
	}
	_log_ptr->write_log_format("s(u) s (d) \n", __FUNCTION__, __LINE__, "Send capacity to PK", send_byte);
	debug_printf("send_capacity_to_pk end\n");
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

void pk_mgr::init(unsigned short ptop_port)
{
	string pk_ip("");
	string pk_port("");
	string svc_tcp_port("");
	string svc_udp_port("");

	_prep->read_key("pk_ip", pk_ip);
	_prep->read_key("pk_port", pk_port);
	_prep->read_key("channel_id", _channel_id);
//	_prep->read_key("svc_tcp_port", svc_tcp_port);	//svc_tcp_port is replaced by ptop_port
	_prep->read_key("svc_udp_port", svc_udp_port);

	cout << "pk_ip=" << pk_ip << endl;
	cout << "pk_port=" << pk_port << endl;
	cout << "channel_id=" << _channel_id << endl;
//	cout << "svc_tcp_port=" << svc_tcp_port << endl;
	cout << "svc_udp_port=" << svc_udp_port << endl;

	pkDownInfoPtr = new struct peer_connect_down_t ;
	if(!(pkDownInfoPtr ) ){
		debug_printf("pk_mgr::pkDownInfoPtr new error \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"pkDownInfoPtr new error");
		PAUSE
	}
	memset(pkDownInfoPtr , 0x0,sizeof( struct peer_connect_down_t));
	
	// set PK-server as one of peer whose PID=999999
	pkDownInfoPtr ->peerInfo.pid =PK_PID;
	pkDownInfoPtr ->peerInfo.public_ip = inet_addr(pk_ip.c_str());
//	pkDownInfoPtr ->peerInfo.tcp_port = htons((unsigned short)atoi(pk_port.c_str()));
	pkDownInfoPtr ->peerInfo.tcp_port = htons(ptop_port);

	map_pid_peerDown_info[PK_PID] =pkDownInfoPtr;
	
	// Map of substream-id and peer-info setting
	//map_substream_peerInfo[0] = pkDownInfoPtr -> peerInfo;
	//map_substream_peerInfo[1] = pkDownInfoPtr -> peerInfo;
	//map_substream_peerInfo[2] = pkDownInfoPtr -> peerInfo;
	//map_substream_peerInfo[3] = pkDownInfoPtr -> peerInfo;

	//web_ctrl_sever_ptr = new web_ctrl_sever(_net_ptr, _log_ptr, fd_list_ptr, &map_stream_name_id); 
	//web_ctrl_sever_ptr->init();

	if (_sock = build_connection(pk_ip, pk_port)) {
		cout << "pk_mgr build_connection() success" << endl;
	} else {
		
//		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","pk_mgr build_connection() fail");
//		_logger_client_ptr->log_exit();
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"pk_mgr build_connection() fail");
		debug_printf("pk_mgr build_connection() fail  \n");
		*(_net_ptr->_errorRestartFlag) =RESTART ;
		PAUSE
	}
	debug_printf("PK_sock = %d \n",_sock);
	_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"PK_sock =",_sock);

	queue<struct chunk_t *> *queue_out_ctrl_ptr;
	queue<struct chunk_t *> *queue_out_data_ptr;

	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	_peer_ptr ->map_in_pid_fd[PK_PID] = _sock;
	_peer_ptr ->map_fd_pid[_sock] = PK_PID;
	_peer_ptr ->map_fd_out_ctrl[_sock] = queue_out_ctrl_ptr;
	_peer_ptr ->map_fd_out_data[_sock] = queue_out_data_ptr;

	Nonblocking_Buff * Nonblocking_Buff_ptr = new Nonblocking_Buff ;
	if(!(Nonblocking_Buff_ptr ) || !(queue_out_ctrl_ptr)  || !(queue_out_data_ptr)){
		debug_printf("pk_mgr::pkDownInfoPtr new error \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"pkDownInfoPtr new error");
		PAUSE
	}
	memset(Nonblocking_Buff_ptr, 0x0 , sizeof(Nonblocking_Buff));
	_peer_ptr ->map_fd_nonblocking_ctl[_sock] = Nonblocking_Buff_ptr ;

//	if (handle_register(svc_tcp_port, svc_udp_port)) {
	if (handle_register(ptop_port, svc_udp_port)) {
		cout << "pk_mgr handle_ register() success" << endl;
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"pk_mgr handle_ register() success");

		_net_ptr->set_nonblocking(_sock);	// set to non-blocking
		_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN);
		_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (this));
		fd_list_ptr->push_back(_sock);

	} else {
		
//		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","pk_mgr handle_ register() fail");
//		_logger_client_ptr->log_exit();
	}
}

// build_connection to (string ip , string port) ,if failure return 0,else return 1
int pk_mgr::build_connection(string ip, string port)
{
	int _sock;
	if((_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		cout << "init create socket failure" << endl;
#ifdef _WIN32
		::WSACleanup();
#endif
		return 0;
	}

	struct sockaddr_in pk_saddr;

	memset(&pk_saddr, 0, sizeof(struct sockaddr_in));

	pk_saddr.sin_addr.s_addr = inet_addr(ip.c_str());
	pk_saddr.sin_port = htons((unsigned short)atoi(port.c_str()));
	pk_saddr.sin_family = AF_INET;

//	_net_ptr->set_nonblocking(_sock);

	if (connect(_sock, (struct sockaddr*)&pk_saddr, sizeof(pk_saddr)) < 0) {
#ifdef _WIN32
//win32
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
//linux

#endif
//		if non-blocking mode waht can i do?
		}
		else {
			cout << "build_connection failure" << endl;
			_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"build_connection failure");

#ifdef _WIN32
			::closesocket(_sock);
			::WSACleanup();
#else
			::close(_sock);
#endif
			return 0;
		}
	}
	return _sock;
}

//Follow light protocol spec send register message to pk,  HTTP | light | content(request_info_t)
//This function include send() function ,send  register packet to PK Server
int pk_mgr::handle_register(unsigned short ptop_port, string svc_udp_port)
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
		debug_printf("pk_mgr::chunk_request_ptr new error \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"chunk_request_ptr new error");
		PAUSE
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
	chunk_request_ptr->info.private_ip = _net_ptr->getLocalIpv4();
	chunk_request_ptr->info.tcp_port = ptop_port;
	chunk_request_ptr->info.udp_port = (unsigned short)atoi(svc_udp_port.c_str());

	if((crlf_ptr = strstr(html_buf, "\r\n\r\n")) != NULL) {
		crlf_ptr += CRLF_LEN;	
		html_hdr_size = crlf_ptr - html_buf;
		cout << "html_hdr_size =" << html_hdr_size << endl;
	} 

	memcpy(html_buf+html_hdr_size, chunk_request_ptr, sizeof(struct chunk_request_msg_t));

	buf_len = html_hdr_size + sizeof(struct chunk_request_msg_t);

	send_byte = _net_ptr->send(_sock, html_buf, buf_len, 0);

	if (chunk_request_ptr) {
		delete chunk_request_ptr;
	}

	if (send_byte <= 0) {
		data_close(_sock, "send html_buf error");
		_log_ptr->exit(0, "send html_buf error");
		return 0;
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
// CHNK_CMD_CHN_UPDATA_DATA
// CHNK_CMD_PARENT_PEER
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
	unsigned long ss_id = 0;
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
	struct peer_info_t *new_peer = NULL;
	struct peer_info_t *child_peer = NULL;

	map<int , Nonblocking_Buff * > ::iterator map_fd_nonblocking_ctl_iter;

	map_fd_nonblocking_ctl_iter = _peer_ptr->map_fd_nonblocking_ctl.find(sock);
	if (map_fd_nonblocking_ctl_iter != _peer_ptr->map_fd_nonblocking_ctl.end()) {
		Nonblocking_Recv_Ctl_ptr = &(map_fd_nonblocking_ctl_iter->second->nonBlockingRecv);
		if(Nonblocking_Recv_Ctl_ptr ->recv_packet_state == 0){
			Nonblocking_Recv_Ctl_ptr ->recv_packet_state =READ_HEADER_READY ;
		}
	}
	else{
		debug_printf("Nonblocking_Recv_Ctl_ptr NOT FIND \n");
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__, __LINE__, "Nonblocking_Recv_Ctl_ptr NOT FIND");
		*(_net_ptr->_errorRestartFlag) =RESTART;
		PAUSE
		return RET_SOCK_ERROR;
	}
	
	//cout << "Nonblocking_Recv_Ctl_ptr->recv_packet_state: " << Nonblocking_Recv_Ctl_ptr->recv_packet_state << "\n";
	for (int i = 0; i < 3; i++) {
		//debug_printf("Nonblocking_Recv_Ctl_ptr->recv_packet_state = %d \n", Nonblocking_Recv_Ctl_ptr->recv_packet_state);
		
		if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY) {
	
			chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
			if(!(chunk_header_ptr ) ){
				debug_printf("pk_mgr::chunk_header_ptr new error \n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"chunk_header_ptr new error");
				PAUSE
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
				debug_printf("pk_mgr::chunk_ptr new error buf_len = %d \n", buf_len);
				_log_ptr->write_log_format("s(u) s s u \n", __FUNCTION__,__LINE__,"[ERROR] chunk_ptr new error", "buf_len", buf_len);
				PAUSE
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
		debug_printf2("recv_byte = %d \n", recv_byte);

		if (recv_byte < 0) {
			debug_printf("Nonblocking_Recv_Ctl_ptr->recv_packet_state = %d \n", Nonblocking_Recv_Ctl_ptr->recv_packet_state);
			debug_printf("recv_byte = %d \n", recv_byte);
			data_close(sock, "[ERROR] error occured in PKMGR recv");
			PAUSE
			return RET_SOCK_ERROR;
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
		debug_printf("\n===================CHNK_CMD_PEER_REG=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_REG");

		unsigned long level_msg_size;
		int offset = 0;
		unsigned long tempManifes = 0;
		struct chunk_register_reply_t *chunk_register_reply;
		
		chunk_register_reply = (struct chunk_register_reply_t *)chunk_ptr;
		lane_member = (buf_len - sizeof(struct chunk_header_t) - 6*sizeof(unsigned long)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + lane_member*sizeof(struct level_info_t *);

		debug_printf("lane_member = %d, level_msg_size = %d \n", lane_member, level_msg_size);

		struct chunk_level_msg_t *level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		if (!level_msg_ptr) {
			debug_printf("pk_mgr::level_msg_ptr new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] level_msg_ptr new error");
			PAUSE
		}
		memset(level_msg_ptr, 0, level_msg_size);
		
		offset += sizeof(struct chunk_header_t);	// offset = 16
		
		// Set up my information
		my_pid = chunk_register_reply->pid;
		my_level = chunk_register_reply->level;
		bit_rate = chunk_register_reply->bit_rate;
		sub_stream_num = chunk_register_reply->sub_stream_num;
		public_ip = chunk_register_reply->public_ip;
		inside_lane_rescue_num = chunk_register_reply->inside_lane_rescue_num;
		
		debug_printf("my_pid = %lu \n", my_pid);
		debug_printf("my_level = %lu \n", my_level);
		debug_printf("bit_rate = %lu \n", bit_rate);
		debug_printf("sub_stream_num = %lu \n", sub_stream_num);
		debug_printf("public_ip = %s \n", inet_ntoa(*(struct in_addr *)&public_ip));
		debug_printf("inside_lane_rescue_num = %lu \n", inside_lane_rescue_num);
		_log_ptr->write_log_format("s(u) s u s u s u s u s s s u s u \n", __FUNCTION__, __LINE__,
																"my_pid =", my_pid,
																"my_level =", my_level,
																"bit_rate =", bit_rate,
																"sub_stream_num =", sub_stream_num,
																"public_ip =", inet_ntoa(*(struct in_addr *)&public_ip),
																"inside_lane_rescue_num =", inside_lane_rescue_num,
																"lane_member =", lane_member);
		
		offset += 6 * sizeof(unsigned long);	// offset = 40
		
		
		_peer_mgr_ptr->set_up_public_ip(public_ip);
		_peer_mgr_ptr->_peer_communication_ptr->set_self_info(public_ip);

		
		//將收到的封包放進  去除掉bit_rate .sub_stream_num .public_ip . inside_lane_rescue_num  ,後放進  chunk_level_msg_t

		
		for (unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
			tempManifes |= (1 << ss_id);
		}

		//收到sub_stream_num後對rescue 偵測結構做初始化
		init_rescue_detection();
		delay_table_init();
		send_capacity_init();
		syn_table_init(_sock);
		_logger_client_ptr->source_delay_struct_init(sub_stream_num);

		_logger_client_ptr->log_to_server(LOG_REGISTER, full_manifest);
		
		level_msg_ptr->pid = my_pid;
		
		//註冊時要的manifest是要全部的substream
		level_msg_ptr->manifest = full_manifest;	// This is manifest needed
		
		///////////////////Set pointer and register to STUNT-Server////////////////
		_stunt_mgr_ptr->_logger_client_ptr = _logger_client_ptr;
		_stunt_mgr_ptr->_net_ptr = _net_ptr;
		_stunt_mgr_ptr->_log_ptr = _log_ptr;
		_stunt_mgr_ptr->_prep_ptr = _prep;
		_stunt_mgr_ptr->_peer_mgr_ptr = _peer_mgr_ptr;
		_stunt_mgr_ptr->_peer_ptr = _peer_ptr;
		_stunt_mgr_ptr->_pk_mgr_ptr = this;
		_stunt_mgr_ptr->_peer_communication_ptr = _peer_mgr_ptr->_peer_communication_ptr;
		_peer_mgr_ptr->_peer_communication_ptr->_stunt_mgr_ptr = _stunt_mgr_ptr;
		/*
		// Register to STUNT-SERVER
		int nRet;
		char myPID[10] = {0};
		itoa(level_msg_ptr->pid, myPID, 10);
		nRet = _stunt_mgr_ptr->init(level_msg_ptr->pid);
		
		if (nRet == ERR_NONE) {
			debug_printf("nRet: %d \n", nRet);
			debug_printf("Register to XSTUNT succeeded \n");
		}
		else {
			debug_printf("Initialization failed. ErrType(%d) \n", nRet);
			//return 0;
		}

		//nRet = _stunt_mgr_ptr->tcpPunch_connection(100u);

		//////////////////////////////////////////////////////////////////
		*/
		
		for (int i = 0; i < lane_member; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			new_peer = new struct peer_info_t;
			if (!level_msg_ptr->level_info[i] || !new_peer) {
				debug_printf("pk_mgr::level_msg_ptr  new_peer new error \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] level_msg_ptr new error");
				PAUSE
			}
			
			memset(level_msg_ptr->level_info[i], 0, sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr+offset, sizeof(struct level_info_t));
			//memcpy(level_msg_ptr->level_info[i], &(chunk_register_reply->level_info[i]), sizeof(struct level_info_t));
			memset(new_peer, 0 ,sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));

			offset += sizeof(struct level_info_t);

			//add lane peer_info to map table
			map_pid_peer_info.insert(pair<unsigned long ,peer_info_t *>(new_peer->pid,new_peer));  

			new_peer->manifest = full_manifest;

			//debug_printf("  pid = %d   ", new_peer->pid);
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "pid =", new_peer->pid, "manifest =", new_peer->manifest);
			
		}
		_log_ptr->write_log_format("\n");

		_peer_mgr_ptr->self_pid = level_msg_ptr->pid ;
		_logger_client_ptr->set_self_pid_channel(level_msg_ptr->pid, _channel_id);

		
		
		//和lane 每個peer 先建立好連線 
		if (lane_member > 0) {
			int session_id = _peer_mgr_ptr->_peer_communication_ptr->set_candidates_handler(full_manifest, level_msg_ptr, lane_member, 0);
			//debug_printf("session_id = %d \n", session_id);
			
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "session_id_start =", session_id);
			_peer_ptr->substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(session_id);
			if (_peer_ptr->substream_first_reply_peer_iter == _peer_ptr->substream_first_reply_peer.end()) {
				_peer_ptr->substream_first_reply_peer[session_id] = new manifest_timmer_flag;
				memset(_peer_ptr->substream_first_reply_peer[session_id], 0, sizeof(struct manifest_timmer_flag));
			}
			else {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "LOG_WRITE_STRING");

				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "this id is already exist in pk_mgr::handle_pkt_in in register \n");
				_logger_client_ptr->log_exit();
			}

			_peer_ptr->substream_first_reply_peer[session_id]->connectTimeOutFlag = TRUE ;
			_peer_ptr->substream_first_reply_peer[session_id]->firstReplyFlag = TRUE ;
			_peer_ptr->substream_first_reply_peer[session_id]->networkTimeOutFlag = TRUE;
			_peer_ptr->substream_first_reply_peer[session_id]->peer_role = 0;
			_peer_ptr->substream_first_reply_peer[session_id]->rescue_manifest = full_manifest;

			//_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);

			_log_ptr->timerGet(& ( _peer_ptr->substream_first_reply_peer[session_id]->connectTimeOut)) ;
			//_peer_mgr_ptr->handle_test_delay(tempManifes);
		}
		else if (lane_member == 0) {
			_logger_client_ptr->log_to_server(LOG_LIST_EMPTY,full_manifest);
			_logger_client_ptr->log_to_server(LOG_DATA_COME_PK,full_manifest);
			pkSendCapacity = true;
			
			// Send pk topology of each substream
			for (unsigned long ss_id = 0; ss_id < sub_stream_num; ss_id++) {
				send_parentToPK(1<<ss_id, PK_PID+1);
			}
		}
		else {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s \n", "[ERROR] Received lane_member =", lane_member, "(cannot less than 0)");
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[ERROR] Received lane_member =", lane_member, "(cannot less than 0)");
		}

		for (int i = 0; i < lane_member; i++) {
			if(level_msg_ptr->level_info[i]) {
				delete level_msg_ptr->level_info[i];
			}
		}
		if (level_msg_ptr) {
			delete level_msg_ptr;
		}
		debug_printf("\n===================CHNK_CMD_PEER_REG DONE=================\n");
	}
	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SYN) {
		// measure start delay
		debug_printf("\n===================CHNK_CMD_PEER_SYN=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SYN");
		
		// Already sent request in syn_table_init(), and now receiving reply syn message
		if (chunk_ptr->header.rsv_1 == REPLY) {
			debug_printf("CHNK_CMD_PEER_SYN REPLY \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SYN REPLY");

			struct syn_token_receive* syn_token_receive_ptr;
			syn_token_receive_ptr = (struct syn_token_receive*)chunk_ptr;
			syn_recv_handler(syn_token_receive_ptr);
		}
		else {
			debug_printf("[ERROR] Received CHNK_CMD_PEER_SYN Request \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] Received CHNK_CMD_PEER_SYN Request");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] Received CHNK_CMD_PEER_SYN Request \n");
			*(_net_ptr->_errorRestartFlag) = RESTART;
			PAUSE
		}
		//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
		
		debug_printf("\n===================CHNK_CMD_PEER_SYN DONE=================\n");
	}
	// light | pid | level   | n*struct rescue_peer_info
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_RESCUE_LIST) {
		debug_printf("\n===================CHNK_CMD_PEER_RESCUE_LIST=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_RESCUE_LIST");

		if (chunk_ptr->header.rsv_1 == REQUEST) {
			_logger_client_ptr->log_to_server(LOG_MERGE_TRIGGER, ((struct chunk_rescue_list*)chunk_ptr)->manifest);
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Merge triggered");
		}
		
		//set recue stat true
		_logger_client_ptr->log_to_server(LOG_RESCUE_TRIGGER_BACK, ((struct chunk_rescue_list*)chunk_ptr)->manifest);
		unsigned long temp_rescue_sub_id = 0;
		int session_id;
		temp_rescue_sub_id = manifestToSubstreamID(((struct chunk_rescue_list*)chunk_ptr)->manifest);
		
		lane_member = (buf_len - sizeof(struct chunk_header_t) - sizeof(unsigned long) - sizeof(unsigned long)) / sizeof(struct level_info_t);
		unsigned long level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + lane_member * sizeof(struct level_info_t *);

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		if (!level_msg_ptr) {
			debug_printf("pk_mgr::level_msg_ptr   new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "level_msg_ptr new error");
			PAUSE
		}
		memset(level_msg_ptr, 0, level_msg_size);
		memcpy(level_msg_ptr, chunk_ptr, (level_msg_size - lane_member * sizeof(struct level_info_t *)));

		offset += (level_msg_size - lane_member * sizeof(struct level_info_t *));

		debug_printf("list peer num %d\n",lane_member);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "lane_member ", lane_member);

		if (chunk_ptr->header.rsv_1 == REQUEST) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "LIST_REQUEST");

			if (check_rescue_state(temp_rescue_sub_id,0)) {
				set_rescue_state(temp_rescue_sub_id,1);
				set_rescue_state(temp_rescue_sub_id,2);
			}
			else{
				debug_printf("why not status 0 in REQUEST , manifest= %d\n",((struct chunk_rescue_list*)chunk_ptr) ->manifest);
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "why not status 0 in REQUEST ", ((struct chunk_rescue_list*)chunk_ptr) ->manifest);
				*(_net_ptr->_errorRestartFlag) = RESTART;
				PAUSE
			}
		}
		else{
			set_rescue_state(temp_rescue_sub_id,2);
		}

		if (lane_member == 0 ) {
			set_rescue_state(temp_rescue_sub_id,0);
		}

		for (i = 0; i < lane_member; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			new_peer = new struct peer_info_t;
			if (!level_msg_ptr->level_info[i] || !new_peer) {
				debug_printf("pk_mgr::level_msg_ptr->level_info[i]    new error \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"level_msg_ptr->level_info[i]  new error");
				PAUSE
			}
			memset(level_msg_ptr->level_info[i], 0 , sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			memset(new_peer, 0x0 , sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));
			offset += sizeof(struct level_info_t);

			map_pid_peer_info.insert(pair<unsigned long ,peer_info_t *>(new_peer->pid,new_peer));  

			new_peer ->manifest = ((struct chunk_rescue_list*)chunk_ptr) ->manifest;
//segmention fault once here  ,lane_member = 4 run once pid=65  manifest=2 and crash !!! WTFFFFF
			debug_printf( "  pid = %d   ",new_peer->pid);
			_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"  pid =   ",new_peer->pid,"manifest=",new_peer ->manifest );

		}
		_log_ptr->write_log_format("\n");

		//和lane 每個peer 先建立好連線	
		//
		if (lane_member > 0) {

			session_id = _peer_mgr_ptr->_peer_communication_ptr->set_candidates_handler(level_msg_ptr->manifest,level_msg_ptr,lane_member, 0);
			_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"session_id_start = ",session_id);
			_peer_ptr->substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(session_id);
			if(_peer_ptr->substream_first_reply_peer_iter == _peer_ptr->substream_first_reply_peer.end()){
				_peer_ptr->substream_first_reply_peer[session_id] = new manifest_timmer_flag;
				if( !(_peer_ptr->substream_first_reply_peer[session_id])){
					debug_printf("pk_mgr::_peer_ptr->substream_first_reply_peer[session_id]  new error \n");
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"_peer_ptr->substream_first_reply_peer[session_id]  new error");
					PAUSE
				}
				memset(_peer_ptr->substream_first_reply_peer[session_id] ,0x0 ,sizeof(struct manifest_timmer_flag));
			}
			else{
				_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","this id is already exist in pk_mgr::handle_pkt_in in rescue\n");
				_logger_client_ptr->log_exit();
			}

			_peer_ptr->substream_first_reply_peer[session_id]->connectTimeOutFlag =TRUE ;
			_peer_ptr->substream_first_reply_peer[session_id]->firstReplyFlag =TRUE ;
			_peer_ptr->substream_first_reply_peer[session_id]->networkTimeOutFlag =TRUE;
			_peer_ptr->substream_first_reply_peer[session_id]->rescue_manifest = level_msg_ptr->manifest;
			_peer_ptr->substream_first_reply_peer[session_id]->peer_role = 0;


		
			//_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);
			//			debug_printf("rescue manifest : %d\n",((struct chunk_rescue_list*)chunk_ptr)->manifest);
			//_peer_mgr_ptr->handle_test_delay( ((struct chunk_rescue_list*)chunk_ptr) ->manifest);

			_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"all peer connect  and sent handle_test_delay ok");

			//get LIST
			_log_ptr->timerGet(&(_peer_ptr->substream_first_reply_peer[session_id]->connectTimeOut) );

		}
		else if (lane_member == 0) {
			_logger_client_ptr->log_to_server(LOG_LIST_EMPTY,((struct chunk_rescue_list*)chunk_ptr) ->manifest);
			_logger_client_ptr->log_to_server(LOG_DATA_COME_PK,((struct chunk_rescue_list*)chunk_ptr) ->manifest);
			
			// Send topology to pk
			send_parentToPK(level_msg_ptr->manifest, PK_PID+1);
		}
		else {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s \n", "[ERROR] Received lane_member =", lane_member, "(cannot less than 0)");
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[ERROR] Received lane_member =", lane_member, "(cannot less than 0)");
		}
		
		for (int i = 0; i < lane_member; i++) {
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
		handle_stream(chunk_ptr, sock);	

		return RET_OK;
	}
	else if (chunk_ptr->header.cmd == CHNK_CMD_PEER_SEED) {
		debug_printf2("\n===================CHNK_CMD_PEER_SEED=================\n");
		struct seed_notify *chunk_seed_notify = (struct seed_notify *)chunk_ptr;
		map<unsigned long, struct peer_connect_down_t *>::iterator iter;
		
		//unsigned long testingManifest = 0;
		unsigned long manifest_temp = 0;
		
		//for (unsigned long i = 0; i < sub_stream_num; i++) {
			//if(  ((ssDetect_ptr) + i) ->isTesting ){
		//	if (!check_rescue_state(i, 0)) {
		//		testingManifest |= SubstreamIDToManifest(i);
		//	}
		//}

		//debug_printf2("testingManifest: %x \n", testingManifest);
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PEER_SEED");
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
														"receive manifest =", chunk_seed_notify->manifest,
														"PK's old manifest =", pkDownInfoPtr->peerInfo.manifest);

		manifest_temp = chunk_seed_notify->manifest;
		
		/*	rewrite 20130915 below 	*/
		for (iter = map_pid_peerDown_info.begin(); iter != map_pid_peerDown_info.end(); iter++) {
			if (iter->first != PK_PID) {
				unsigned long temp = iter->second->peerInfo.manifest;
				iter->second->peerInfo.manifest &= ~manifest_temp;
				_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
														"parent pid", iter->first,
														"manifest is changed from", temp,
														"to", iter->second->peerInfo.manifest);
														
				if (iter->second->peerInfo.manifest == 0) {
					unsigned long parent_pid = iter->first;
					int old_size = map_pid_peerDown_info.size();
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "close at add seed. pid =", iter->first);
					iter--;
					_peer_ptr->data_close(_peer_ptr->map_in_pid_fd[parent_pid], "close at add seed", CLOSE_PARENT);
					_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "map_pid_peerDown_info.size() is changed from", old_size, "to", map_pid_peerDown_info.size());
				}
			}
		}
		/*	rewrite 20130915 above 	*/
		
		while (manifest_temp) {
			unsigned long substream_id = manifestToSubstreamID(manifest_temp);
			unsigned long manifest = SubstreamIDToManifest(substream_id);
			map<unsigned long, struct peer_connect_down_t *>::iterator iter;
			
			//erase other peer stream
			//Maybe clear previous parent info from map_pid_peerDown_info and close socket
			/*	Commented on 20130915, rewrite it
			for (iter = map_pid_peerDown_info.begin(); iter != map_pid_peerDown_info.end(); iter++) {
				if (iter->first != PK_PID  && (iter->second->peerInfo.manifest & manifest)) {
					 iter->second->peerInfo.manifest &= ~manifest;
				}
			}
			
			////////這邊會發生iter segmentation fault的問題
			////////20130915 已修改
			for (iter = map_pid_peerDown_info.begin(); iter != map_pid_peerDown_info.end(); ) {
				_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "pid =", iter->first, "map_pid_peerDown_info.size =", map_pid_peerDown_info.size());
				if (iter->first != PK_PID && iter->second->peerInfo.manifest == 0) {
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "close at add seed. pid =", iter->first);
					_peer_ptr->data_close(_peer_ptr->map_in_pid_fd[iter->first], "close at add seed", CLOSE_PARENT);	// 這行還有用嗎? close parent已經在別處處理了
					
					iter = map_pid_peerDown_info.begin();
				}
				else {
					iter++;
				}
			}
			*/
			////////
			//maybe need resend manifest to my parent peer

			//set recue state to nornal  state = 0
			if (check_rescue_state(substream_id, 0)) {
				//donothing
			}
			else if (check_rescue_state(substream_id, 1)) {
				set_rescue_state(substream_id, 2);
				set_rescue_state(substream_id, 0);
			}
			else if (check_rescue_state(substream_id, 2)) {
				set_rescue_state(substream_id, 0);
			}

			ssDetect_ptr[substream_id].isTesting = FALSE;

			manifest_temp &= ~SubstreamIDToManifest(substream_id);
		}

		pkDownInfoPtr->peerInfo.manifest |= chunk_seed_notify->manifest ;

		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Change PK's manifest to =", pkDownInfoPtr->peerInfo.manifest);
		debug_printf2("\n===================CHNK_CMD_PEER_SEED DONE=================\n");
	}
	// for each stream this protocol (only use to  decode to flv )  or (other protocol need some header)  
	// light | streamID_1 (int) | stream_header_1 len (unsigned int) | protocol_1 header | streamID_2 ...... 
	// This CMD only happen in "join" condition or "parent-peer of a peer unnormally terminate connection" condition
	else if(chunk_ptr->header.cmd == CHNK_CMD_CHN_UPDATA_DATA){		
		debug_printf("\n===================CHNK_CMD_CHN_UPDATA_DATA=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_CHN_UPDATA_DATA");
		
		int *streamID_ptr = NULL ;
		int *len_ptr = NULL;
		unsigned char *header = NULL;
		update_stream_header *protocol_len_header = NULL ;
		int expected_len = chunk_ptr->header.length;
		int ooffset = 0;
		
		while (true) {
			if (ooffset == expected_len) {
				break;
			}
			streamID_ptr = (int *)((char*)chunk_ptr->buf + ooffset);
			len_ptr = (int *)((char *)chunk_ptr->buf + sizeof(int) + ooffset);

			//debug_printf("len_ptr = %d \n", *len_ptr);
			
			//wait header 
			if (*len_ptr == 0) {
				debug_printf("wait header streamID = %d ",*streamID_ptr);
				ooffset += sizeof(int) + sizeof(int) ;
				continue ;
			}
			//no header
			else if (*len_ptr == -1) {
				//ooffset += sizeof(int) + sizeof( int) ;
				protocol_len_header = new update_stream_header;
				if (!protocol_len_header) {
					debug_printf("pk_mgr::protocol_len_header new error \n");
					_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] protocol_len_header  new error");
					PAUSE
				}
				protocol_len_header ->len = *len_ptr;
				map_streamID_header[*streamID_ptr] = (update_stream_header*)protocol_len_header;
				debug_printf("streamID = %d, *len_ptr = %d \n", *streamID_ptr, *len_ptr);
				ooffset += sizeof(int) + sizeof( int) ;
			}
			//have header
			else {
				protocol_len_header = (update_stream_header *)new unsigned char[*len_ptr + sizeof( int)];
				if (!protocol_len_header) {
					debug_printf("pk_mgr::protocol_len_header new error \n");
					_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] protocol_len_header  new error");
					PAUSE
				}
				header = ((unsigned char *)chunk_ptr->buf + sizeof(int) + ooffset);
				memcpy(protocol_len_header, header, *len_ptr + sizeof( int));
				map_streamID_header[*streamID_ptr] = (update_stream_header*)protocol_len_header ;
				debug_printf("streamID = %d  *len_ptr =%d  \n", *streamID_ptr, *len_ptr) ;
				ooffset += sizeof(int) + sizeof( int) + *len_ptr;
			}
		}
		stream_number = map_streamID_header.size();
		
		debug_printf("stream_number = %d \n", stream_number);
		debug_printf("===================CHNK_CMD_CHN_UPDATA_DATA done=================\n");
		//PAUSE
	}
	else if(chunk_ptr->header.cmd == CHNK_CMD_PARENT_PEER){
		debug_printf("\n===================CHNK_CMD_PARENT_PEER=================\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "CHNK_CMD_PARENT_PEER");

		//set recue stat true
		unsigned long temp_rescue_sub_id = 0;
		unsigned long list_number;
		unsigned long level_msg_size;
		int i,session_id;
		struct chunk_child_info *child_info_ptr = NULL;
		struct chunk_level_msg_t *level_msg_ptr = NULL;
		//struct peer_info_t *new_peer = NULL;

		temp_rescue_sub_id = manifestToSubstreamID(((struct chunk_child_info*)chunk_ptr) ->manifest);
		//set_rescue_state(temp_rescue_sub_id,2);
		child_info_ptr = (struct chunk_child_info*)chunk_ptr;



		list_number = (buf_len - sizeof(struct chunk_header_t) - sizeof(unsigned long) - sizeof(unsigned long)) / sizeof(struct level_info_t);
		level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + list_number * sizeof(struct level_info_t *);

		level_msg_ptr = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
		if( !(level_msg_ptr)){
			debug_printf("pk_mgr::level_msg_ptr new error \n");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"level_msg_ptr  new error");
			PAUSE
		}
		memset(level_msg_ptr, 0x0, level_msg_size);
		memcpy(level_msg_ptr, chunk_ptr, (level_msg_size - list_number * sizeof(struct level_info_t *)));

		offset += (level_msg_size - list_number * sizeof(struct level_info_t *));

		debug_printf("list peer num %d\n",list_number);
		_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"list_number ",list_number);

		if((list_number == 0) || (list_number >1)){
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","CHNK_CMD_PARENT_PEER cannot have 0 member or more than 1 members\n");
			_logger_client_ptr->log_exit();
		}
		
		// list-number only equals to 1
		for (int i = 0; i < list_number; i++) {
			level_msg_ptr->level_info[i] = new struct level_info_t;
			new_peer = new struct peer_info_t;
			if( !(level_msg_ptr->level_info[i]) || !(new_peer)){
				debug_printf("pk_mgr::level_msg_ptr new error \n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"level_msg_ptr  new error");
				PAUSE
			}
			memset(level_msg_ptr->level_info[i], 0x0 , sizeof(struct level_info_t));
			memcpy(level_msg_ptr->level_info[i], (char *)chunk_ptr + offset, sizeof(struct level_info_t));
			memset(new_peer, 0x0 , sizeof(struct peer_info_t));
			memcpy(new_peer, level_msg_ptr->level_info[i], sizeof(struct level_info_t));
			offset += sizeof(struct level_info_t);
			
			// map_pid_child_peer_info: map of temp child-peer
			map_pid_child_peer_info.insert(pair<unsigned long ,peer_info_t *>(new_peer->pid, new_peer));  

			new_peer ->manifest = ((struct chunk_rescue_list*)chunk_ptr) ->manifest;
			debug_printf( " child pid = %d   ",child_info_ptr->child_level_info.pid);
			_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__," child  pid =   ",child_info_ptr->child_level_info.pid,"manifest=",child_info_ptr->manifest );

		}
		_log_ptr->write_log_format("\n");

		session_id = _peer_mgr_ptr->_peer_communication_ptr->set_candidates_handler(level_msg_ptr->manifest,level_msg_ptr,list_number, 1);
		_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"session_id_start = ",session_id);
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"call set_candidates_handler\n");
		/*substream_first_reply_peer_iter = substream_first_reply_peer.find(((struct chunk_rescue_list*)chunk_ptr) ->manifest);
		if(_peer_ptr->substream_first_reply_peer_iter == _peer_ptr->substream_first_reply_peer.end()){
			_peer_ptr->substream_first_reply_peer[((struct chunk_rescue_list*)chunk_ptr) ->manifest] = new manifest_timmer_flag;
			memset(_peer_ptr->substream_first_reply_peer[((struct chunk_rescue_list*)chunk_ptr) ->manifest] ,0x0 ,sizeof(struct manifest_timmer_flag));
		}

		_peer_ptr->substream_first_reply_peer[((struct chunk_rescue_list*)chunk_ptr) ->manifest] ->connectTimeOutFlag =true ;
		_peer_ptr->substream_first_reply_peer[((struct chunk_rescue_list*)chunk_ptr) ->manifest] ->firstReplyFlag =true ;
		_peer_ptr->substream_first_reply_peer[((struct chunk_rescue_list*)chunk_ptr) ->manifest] ->networkTimeOutFlag =true;*/
		_peer_ptr->substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer.find(session_id);	// substream_first_reply_peer is a map
		if(_peer_ptr->substream_first_reply_peer_iter == _peer_ptr->substream_first_reply_peer.end()){
			_peer_ptr->substream_first_reply_peer[session_id] = new manifest_timmer_flag;
			if( !(_peer_ptr->substream_first_reply_peer[session_id] )){
				debug_printf("pk_mgr::_peer_ptr->substream_first_reply_peer[session_id] new error \n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"_peer_ptr->substream_first_reply_peer[session_id]   new error");
				PAUSE
			}
			memset(_peer_ptr->substream_first_reply_peer[session_id] ,0x0 ,sizeof(struct manifest_timmer_flag));
		}
		else{
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");
	
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","this id is already exist in peer::handle_pkt_in\n");
			_logger_client_ptr->log_exit();
		}

		_peer_ptr->substream_first_reply_peer[session_id]->connectTimeOutFlag =TRUE ;
		_peer_ptr->substream_first_reply_peer[session_id]->firstReplyFlag =FALSE ;
		_peer_ptr->substream_first_reply_peer[session_id]->networkTimeOutFlag =TRUE;
		_peer_ptr->substream_first_reply_peer[session_id]->rescue_manifest = child_info_ptr->manifest;
		_peer_ptr->substream_first_reply_peer[session_id]->peer_role = 1;
		_peer_ptr->substream_first_reply_peer[session_id]->pid =child_info_ptr->child_level_info.pid;

		_log_ptr->timerGet(&(_peer_ptr->substream_first_reply_peer[session_id]->connectTimeOut));
		//和lane 每個peer 先建立好連線	
		//
		/*if(lane_member >= 1){
			_peer_mgr_ptr->connect_peer(level_msg_ptr, level_msg_ptr->pid);
			//			debug_printf("rescue manifest : %d\n",((struct chunk_rescue_list*)chunk_ptr)->manifest);
//			_peer_mgr_ptr->handle_test_delay( ((struct chunk_rescue_list*)chunk_ptr) ->manifest);

			_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"all peer connect  and sent handle_test_delay ok");

//get LIST
			_log_ptr ->getTickTime(&_peer_ptr->substream_first_reply_peer[((struct chunk_rescue_list*)chunk_ptr) ->manifest]->connectTimeOut ) ;


		}*/



		for (int i = 0; i < list_number; i++) {
			if (level_msg_ptr->level_info[i]) {
				delete level_msg_ptr->level_info[i];
			}
		}

		if (level_msg_ptr) {
			delete level_msg_ptr;
		}

	}
	else {
		debug_printf("\n===================CHNK_CMD_ERROR=================\n");
		debug_printf("Receive unknown heaher command %d \n", chunk_ptr->header.cmd);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Receive unknown heaher command", chunk_ptr->header.cmd);
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "Receive unknown heaher command", chunk_ptr->header.cmd);
		*(_net_ptr->_errorRestartFlag) = RESTART;
		PAUSE
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
	data_close(sock, "handle_pkt_error");
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
		debug_printf("pk_mgr::request_pkt_ptr new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "request_pkt_ptr  new error");
		PAUSE
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

	if( send_byte <= 0 ) {
		data_close(_sock, "send send_request_sequence_number_to_pk cmd error");
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
		_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"send_pkt_to_pk  error",WSAGetLastError());
		data_close(_sock, "send send_pkt_to_pk error");
		//_log_ptr->exit(0, "send pkt error");
	} else {
		if (chunk_ptr) {
			delete chunk_ptr;
		}
		_net_ptr->set_nonblocking(_sock);	// set to non-blocking
	}
}

//handle_latency hidden at 2013/01/16


//
//the main handle steram function 需要處理不同序來源的chunk,		
//送到player 的queue 裡面 必須保證是有方向性的 且最好是依序的
// Parameters:
// 		chunk_ptr	A pointer to P2P packet
// 		sockfd		fd which received this P2P packet
void pk_mgr::handle_stream(struct chunk_t *chunk_ptr, int sockfd)
{
	unsigned long i;
	unsigned int seq_ready_to_send = 0;
	unsigned long parent_pid = -1;
	//int downStreamSock = -1;
	//int downStreamPid = -1;
	stream *strm_ptr = NULL;
	//struct peer_info_t *peer = NULL;
	struct peer_connect_down_t *parent_info = NULL;
	unsigned long substream_id = 0;
	//map<int, queue<struct chunk_t *> *>::iterator iter;		//fd_downstream
	map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	int leastCurrDiff = 0;
	//queue<struct chunk_t *> *queue_out_data_ptr;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	unsigned long testingManifest = 0;
	map<unsigned long, struct source_delay *>::iterator delay_table_iter;

	//還沒註冊拿到substream num  不做任何偵測和運算
	if (sub_stream_num == 0) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG](is this condition happen?)receive chunk_t packet but not yet regester in pk");
		delete [] (unsigned char*)chunk_ptr ;
		return;
	}
	
	//↓↓↓↓↓↓↓↓↓↓↓↓任何chunk 都會run↓↓↓↓↓↓↓↓↓↓↓↓↓

	/*
	this part is used for lo bw 
	*/

	if (!_logger_client_ptr->log_bw_in_init_flag) {
		_logger_client_ptr->log_bw_in_init_flag = 1;
		_logger_client_ptr->bw_in_struct_init(chunk_ptr->header.timestamp, chunk_ptr->header.length);
	}
	else {
		_logger_client_ptr->set_in_bw(chunk_ptr->header.timestamp, chunk_ptr->header.length);
	}

	// Get the substream ID
	substream_id = chunk_ptr->header.sequence_number % sub_stream_num;

	// Get the source_dalay table of this substream ID
	delay_table_iter = delay_table.find(substream_id);
	if (delay_table_iter == delay_table.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] can not find source struct in table in send_capacity_to_pk\n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] can not find source struct in table in send_capacity_to_pk");
		_logger_client_ptr->log_exit();
	}

	// _least_sequence_number is the latest sequence number
	if (chunk_ptr->header.sequence_number > _least_sequence_number) {
		_least_sequence_number = chunk_ptr->header.sequence_number;
	}
	
	if (_current_send_sequence_number == -1) {
		_current_send_sequence_number = chunk_ptr->header.sequence_number;
	}

	// Get parentPid and parentInfo of this chunk
	fd_pid_iter = _peer_ptr->map_fd_pid.find(sockfd);
	if (fd_pid_iter != _peer_ptr->map_fd_pid.end()) {
		parent_pid = fd_pid_iter->second;						// Get parent-pid of this chunk
		pid_peerDown_info_iter = map_pid_peerDown_info.find(parent_pid);
		if (pid_peerDown_info_iter != map_pid_peerDown_info.end()) {
			parent_info = pid_peerDown_info_iter->second;		// Get parent-info of this chunk
		}
	}

	_log_ptr->write_log_format("s(u) s u s u s u s u s u s u s u \n", __FUNCTION__, __LINE__, 
																	  "parentPid =", parent_pid, 
																	  "parent manifest =", parent_info->peerInfo.manifest,
																	  "substreamID =", substream_id,
																	  "state =", delay_table_iter->second->rescue_state,
																	  "pkt seqnum =", chunk_ptr->header.sequence_number,
																	  "bytes =", chunk_ptr->header.length,
																	  "timestamp =", chunk_ptr->header.timestamp);

	//更新最後的seq 用來做time out
	parent_info->timeOutNewSeq = chunk_ptr->header.sequence_number;

	/*	rewrite 20130914 in here below	*/
	// If receive packets of each substream, send start-delay to PK.
	// Also record the first packet's sequence number
	if (!sentStartDelay && syn_table.init_flag == 2 && chunk_ptr->header.sequence_number > syn_table.start_seq) {
		if (!delay_table[substream_id]->first_pkt_recv) {
			peer_start_delay_count++;
			delay_table[substream_id]->first_pkt_recv = true;
			delay_table[substream_id]->end_seq_num = chunk_ptr->header.sequence_number;
		}
		if (peer_start_delay_count == sub_stream_num) {
			sentStartDelay = TRUE;
			_logger_client_ptr->count_start_delay();
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Send start-delay to PK");
	
		}
	}
	_log_ptr->write_log_format("s(u) s d s u s d \n", __FUNCTION__, __LINE__, 
													"peer_start_delay_count =", peer_start_delay_count, 
													"start_seq =", syn_table.start_seq,
													"pkSendCapacity =", pkSendCapacity);
	
	// Send capacity to PK(send capacity condition 2), only happen in JOIN state
	if (pkSendCapacity) {
		if (syn_table.init_flag == 2 && chunk_ptr->header.sequence_number > syn_table.start_seq) {
			if (peer_start_delay_count == sub_stream_num) {
				send_capacity_to_pk(_sock);
				pkSendCapacity = false;
			}
		}
	}
	/*	rewrite 20130914 in here above	*/
	
	
	//如果這個 peer 來的chunk 裡的substream 從pk和這個peer都有來 且在rescue的狀態  (if rescue testing stream)
	// 這條substream準備開始testing
	if ((SubstreamIDToManifest(substream_id) & parent_info->peerInfo.manifest) &&  
		(SubstreamIDToManifest(substream_id) & pkDownInfoPtr->peerInfo.manifest) && 
		parent_pid != PK_PID && 
		check_rescue_state(substream_id, 2)) {

		parent_info->outBuffCount = 0;
		ssDetect_ptr[substream_id].isTesting = 1;	//true
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "start testing stream SSID =", substream_id);

		//這邊只是暫時改變PK的substream 實際上還是有串流下來
		unsigned long pk_oldmanifest = pkDownInfoPtr->peerInfo.manifest;
		//_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"PK old manifest",pkDownInfoPtr->peerInfo.manifest);
		pkDownInfoPtr->peerInfo.manifest &= (~SubstreamIDToManifest(substream_id));
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "for testing stream PK manifest temp change from", pk_oldmanifest, "to", pkDownInfoPtr->peerInfo.manifest);

		//開始testing 送topology   ,0627 一律不送previousParentPID
		//send_parentToPK ( SubstreamIDToManifest (temp_sub_id) , (ssDetect_ptr + temp_sub_id)->previousParentPID ); 
		send_parentToPK(SubstreamIDToManifest(substream_id), PK_PID+1); 

		//testing function
		reSet_detectionInfo();
	}
	
	//如果這個 peer 來的chunk 裡的substream 從pk和這個peer都有來 且pk 為full_stream狀態 且在join的狀態  //狀態 0
	// join狀態且testing結束，正準備切斷pk所有substream
	if ((SubstreamIDToManifest(substream_id) & parent_info->peerInfo.manifest) &&
		(SubstreamIDToManifest(substream_id) & pkDownInfoPtr->peerInfo.manifest) &&
		parent_pid != PK_PID &&
		pkDownInfoPtr->peerInfo.manifest == full_manifest &&
		check_rescue_state(substream_id,0)) {
		
		_logger_client_ptr->log_to_server(LOG_REG_LIST_DETECTION_TESTING_SUCCESS,parent_info->peerInfo.manifest,1);
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "JOIN state, testing successes. Change PK manifest from 15 to 0, and inform PK");
		pkDownInfoPtr->peerInfo.manifest =0;
		
		// right now cut off all substreams from PK
		send_rescueManifestToPKUpdate(0);

		_logger_client_ptr->log_to_server(LOG_REG_CUT_PK, parent_info->peerInfo.manifest);
		_logger_client_ptr->log_to_server(LOG_REG_DATA_COME, parent_info->peerInfo.manifest);
	}
	
	//如果這個substream正在測試中 且不是從PK來 (從peer 來) 
	//只是把從PK來的flag取消 但實際上PK還是有下串流進來
	if (ssDetect_ptr[substream_id].isTesting && 
		parent_pid != PK_PID &&  
		check_rescue_state(substream_id, 2)) {

		ssDetect_ptr[substream_id].testing_count++;

		//下面會濾掉慢到的封包 所以在此進入偵測

		rescue_detecion(chunk_ptr);
		//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
		if (syn_table.init_flag == 2) {
			delay_table_iter->second->end_seq_abs_time = chunk_ptr ->header.timestamp;
			_log_ptr->timerGet(& delay_table_iter->second->client_end_time );
			source_delay_detection(sockfd, substream_id, chunk_ptr->header.sequence_number);
		}
		//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

		//測試次數填滿兩次整個狀態  也就是測量了PARAMETER_M  次都沒問題 ( 其中有PARAMETER_M 次計算不會連續觸發)
		if ((ssDetect_ptr[substream_id].testing_count / (PARAMETER_M * Xcount ))  >= 2) {
			
			(ssDetect_ptr + substream_id)->isTesting = 0;  //false
			(ssDetect_ptr + substream_id)->testing_count = 0;
			parent_info->outBuffCount = 0;

			//找出所有正在測試的substream
			for (unsigned long i = 0; i < sub_stream_num; i++) {
				if (!check_rescue_state(i,0)) {
					testingManifest |= SubstreamIDToManifest(i);
				}
			}

			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__,__LINE__,"all testing manifest ",testingManifest);
			//testing ok should cut this substream from pk
			testingManifest  &=  ~SubstreamIDToManifest(substream_id) ;
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__,__LINE__,"after cut the testing substream",testingManifest,"sent to PK manifest=",pkDownInfoPtr->peerInfo.manifest | testingManifest);

			_logger_client_ptr->log_to_server(LOG_RESCUE_DETECTION_TESTING_SUCCESS,SubstreamIDToManifest(substream_id),1);
			send_rescueManifestToPKUpdate(pkDownInfoPtr->peerInfo.manifest | testingManifest);	// [QUESTION] send_rescueManifestToPKUpdate(0)??
			_logger_client_ptr->log_to_server(LOG_RESCUE_CUT_PK,SubstreamIDToManifest(substream_id));
			_logger_client_ptr->log_to_server(LOG_RESCUE_DATA_COME,SubstreamIDToManifest(substream_id));

			//debug_printf("testing ok  cut pk substream= %d  manifest=%d sent new topology \n",substream_id,pkDownInfoPtr->peerInfo.manifest);
			_log_ptr->write_log_format("s(u) s u s u s \n", __FUNCTION__,__LINE__,"testing ok  cut pk substream=  ",substream_id,"manifest=",pkDownInfoPtr->peerInfo.manifest,"sent new topology");

			//set recue stat true
			set_rescue_state(substream_id,0);

			//選擇selected peer 送topology
			send_parentToPK(SubstreamIDToManifest (substream_id) ,PK_PID +1 );

			//testing function
			reSet_detectionInfo();
		}
	}
	//↑↑↑↑↑↑↑↑↑↑↑↑任何chunk 都會run↑↑↑↑↑↑↑↑↑↑↑↑
	
	// Compare chunk_ptr(new chunk) with buf_chunk_t[index](chunk buffer)
	// If new chunk's seq > buffer chunk[index]'s seq, update buffer chunk[index]
	// If new chunk's seq = buffer chunk[index]'s seq, drop it
	// If new chunk's seq < buffer chunk[index]'s seq, drop it
	if (chunk_ptr->header.sequence_number > (**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number) {
		delete [] (unsigned char*) *(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size)) ;
		*(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size)) = chunk_ptr;
		//debug_printf2("new packets sequence number = %d \n", chunk_ptr->header.sequence_number);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__,
													"new sequence number in the buffer. seq",
													(**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number);
	} 
	else if ((**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number == chunk_ptr->header.sequence_number) {
		//debug_printf2("duplicate packets sequence number = %d \n", chunk_ptr->header.sequence_number);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__,
													"duplicate sequence number in the buffer. seq",
													(**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number);
		delete [] (unsigned char*)chunk_ptr ;
		return;
	} 
	else {
		//debug_printf2("duplicate packets sequence number = %d \n", chunk_ptr->header.sequence_number);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__,
													"sequence number smaller than the index in the buffer. seq",
													(**(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size))).header.sequence_number);
		delete [] (unsigned char*)chunk_ptr ;
		return;
	}

	//↓↓↓↓↓↓↓↓↓↓↓↓以下只有先到的chunk才會run(會濾掉重複的和更小的)↓↓↓↓↓↓↓↓↓↓↓↓↓
	
	if (!fisttimestamp) {
		fisttimestamp = chunk_ptr->header.timestamp;
		_log_ptr->timerGet(&start);
	}

	_log_ptr->timerGet(&end);

	pkt_count++;
	debug_printf2("pkt_count = %d, (%d ms), Xcount = %d \n", pkt_count, _log_ptr->diff_TimerGet_ms(&start, &end), Xcount);
	
	// Calculate Xcount per 10 seconds
	if (_log_ptr->diff_TimerGet_ms(&start, &end) >= 10000) {
		int temp = Xcount;
		Xcount = pkt_count / (PARAMETER_X*10);
		//low bound = 5
		if (Xcount < 5) {
			Xcount = 5;
		}
		debug_printf("pkt_count = %lu, Xcount = %lu (packets received per %d ms) \n", pkt_count, Xcount, 1000/PARAMETER_X);
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Xcount changed from", temp, "to", Xcount);
		start = end;
		pkt_count = 0;
	}

	//send down stream(UPLOAD) to other peer if SSID match and in map_pid_rescue_peer_info(real children)
	for (pid_peer_info_iter = map_pid_rescue_peer_info.begin(); pid_peer_info_iter !=map_pid_rescue_peer_info.end(); pid_peer_info_iter++) {

		unsigned long child_pid;	
		int child_sock;
		struct peer_info_t *child_peer = NULL;
		queue<struct chunk_t *> *queue_out_data_ptr;
		
		child_pid = pid_peer_info_iter->first;		//get child-Pid
		child_peer = pid_peer_info_iter->second;	//get child info
		
		map_pid_fd_iter = _peer_ptr->map_out_pid_fd.find(child_pid) ;
		if (map_pid_fd_iter != _peer_ptr->map_out_pid_fd.end()) {
			child_sock = map_pid_fd_iter->second;	//get child socket

			map<int, queue<struct chunk_t *> *>::iterator iter;
			iter = _peer_ptr->map_fd_out_data.find(child_sock);
			if (iter != _peer_ptr ->map_fd_out_data.end()) {
				queue_out_data_ptr = iter->second;	//get queue_out_data_ptr
			}
			else {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] Found child-peer in map_out_pid_fd but not found in map_fd_out_data");
			}
		}
		else {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] Found child-peer in map_pid_rescue_peer_info but not found in map_out_pid_fd");
		}
		
		// Check whether child's manifest are equal to chunk_ptr's(new chunk) or not
		if ((child_peer->manifest & (1 << (chunk_ptr->header.sequence_number % sub_stream_num)))) {
			//queue_out_data_ptr->push((struct chunk_t *)(_chunk_bitstream + (chunk_ptr->header.sequence_number % _bucket_size)));
			// Put buf_chunk_t[index](chunk buffer) into output queue
			queue_out_data_ptr->push( *(buf_chunk_t + (chunk_ptr->header.sequence_number % _bucket_size)) ) ;

			_net_ptr->epoll_control(child_sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
			_log_ptr->write_log_format("s(u) s u s u s d \n", __FUNCTION__, __LINE__,
													"Send chunk to child", child_pid,
													"substreamID", substream_id,
													"chunk's sequence number", chunk_ptr->header.sequence_number);
		}
	}


	//如果這個sustream 正在測試中，並且從pk來的不進入測試
	if (ssDetect_ptr[substream_id].isTesting && parent_pid == PK_PID) {
		// do nothing
	}
	//正常串流在此進入  (非teting)
	else if (!ssDetect_ptr[substream_id].isTesting && check_rescue_state(substream_id,0)) {          
		//丟給rescue_detecion一定是有方向性的
		if (syn_table.init_flag == 2) {

			rescue_detecion(chunk_ptr);

			delay_table_iter->second->end_seq_abs_time = chunk_ptr->header.timestamp;
			_log_ptr->timerGet(&delay_table_iter->second->client_end_time);

			source_delay_detection(sockfd, substream_id, chunk_ptr->header.sequence_number);
		}
	}

	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sentStartDelay =", sentStartDelay, "pkSendCapacity =", pkSendCapacity);
	
	
	
	/*	Commented 20130914 below, rewrite it	*/
	/*
	if (!sentStartDelay) {
		if (!delay_table_iter->second->first_pkt_recv && syn_table.init_flag == 2) {
			if (chunk_ptr->header.sequence_number > syn_table.start_seq) {
				
				peer_start_delay_count++;
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "peer_start_delay_count =", peer_start_delay_count);
				delay_table_iter->second->first_pkt_recv = 1;
				delay_table_iter->second->end_seq_num = chunk_ptr->header.sequence_number;
				if (peer_start_delay_count == sub_stream_num) {
					_logger_client_ptr->count_start_delay();
					for (unsigned long i = 0; i < sub_stream_num; i++) {
						delay_table[i]->first_pkt_recv = 0;
						peer_start_delay_count = 0;
						sentStartDelay = TRUE;
					}
				}
			}
		}
	}
	
	//pk
	if (parent_pid == PK_PID) {
		//若SYN完了
		if (pkSendCapacity) {
			if (!delay_table_iter->second->first_pkt_recv && syn_table.init_flag == 2) {

				if (chunk_ptr->header.sequence_number > syn_table.start_seq) {
					peer_start_delay_count++;
					_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "peer_start_delay_count =", peer_start_delay_count);
					delay_table_iter->second->first_pkt_recv = 1;
					delay_table_iter->second->end_seq_num = chunk_ptr ->header.sequence_number;
					if (peer_start_delay_count == sub_stream_num) {
						send_capacity_to_pk(_sock);
						//_logger_client_ptr->count_start_delay();
					}
				}
			}
		}
	}
	//peers
	else {
		//若SYN完了
		if (!delay_table_iter->second->first_pkt_recv && syn_table.init_flag == 2) {

			if (chunk_ptr ->header.sequence_number > syn_table.start_seq) {
				peer_start_delay_count++;
				delay_table_iter->second->first_pkt_recv = 1;
				delay_table_iter->second->end_seq_num = chunk_ptr ->header.sequence_number;
				if (peer_start_delay_count == sub_stream_num) {
					send_capacity_to_pk(_sock);
					//_logger_client_ptr->count_start_delay();
				}
			}
		}
	}
	*/
	/*	Commented 20130914 above, rewrite it	*/
	
	
	//差值在BUFF_SIZE 之外可能某個些seq都不到 ,跳過那些seq直到差值在BUFF_SIZE內
	// leastCurrDiff: latest received sequence number so far - latest sequence number sent to player
	// Check whether packet is lost or not in buf_chunk_t[]
	leastCurrDiff = (int)( (int)_least_sequence_number - (int)_current_send_sequence_number);
	debug_printf2("_least_sequence_number = %d, _current_send_sequence_number = %d, (%d) \n", _least_sequence_number, _current_send_sequence_number, Xcount*BUFF_SIZE*PARAMETER_X);
	if (leastCurrDiff < 0) {
		debug_printf("leastCurrDiff < 0    = %d \n" ,leastCurrDiff);
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"leastCurrDiff < 0  ");
		*(_net_ptr->_errorRestartFlag) =RESTART;
		PAUSE
	}
	if ( (int)(Xcount * BUFF_SIZE * PARAMETER_X) <= leastCurrDiff && leastCurrDiff < _bucket_size) {
		debug_printf("Start sending packets to player \n");
		_log_ptr->write_log_format("s(u) s (d) \n", __FUNCTION__, __LINE__, "Start sending packets to player", (int)(Xcount * BUFF_SIZE * PARAMETER_X));
		for ( ; ; _current_send_sequence_number++) {
			
			//代表有封包還沒到.略過
			if ((**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number) {
				debug_printf("buf_chunk_t[%d].header.sequence_number %d is not correct, it should be %d \n", _current_send_sequence_number%_bucket_size, 
																											(**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.sequence_number,
																											_current_send_sequence_number);
				
				(_logger_client_ptr->quality_struct_ptr->loss_pkt)++;
				debug_printf("Packet sequence number %d lost. loss_pkt = %d \n", _current_send_sequence_number, _logger_client_ptr->quality_struct_ptr->loss_pkt);
				_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__,
																"Packet sequence number", _current_send_sequence_number,
																"lost. loss_pkt", _logger_client_ptr->quality_struct_ptr->loss_pkt);
				//debug_printf("here1 leastCurrDiff =%d  _current= %d SSID =%d\n ",leastCurrDiff ,_current_send_sequence_number,_current_send_sequence_number%sub_stream_num);
				//_log_ptr->write_log_format("s =>u s u s u s u\n", __FUNCTION__,__LINE__,"here1 leastCurrDiff =",leastCurrDiff,"_current=",_current_send_sequence_number,"SSID =",_current_send_sequence_number%sub_stream_num);

				//add rescue algo 只救最後離觸發點最近沒到的substream
				unsigned long lost_Manifest = SubstreamIDToManifest(_current_send_sequence_number%sub_stream_num) ;
				unsigned long  testingSubStreamID = 0;
				map<unsigned long, int>::iterator map_out_pid_fd_iter;
				struct peer_connect_down_t *lost_ParentInfo = NULL ;

				for (pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter != map_pid_peerDown_info.end(); pid_peerDown_info_iter++) {
					lost_ParentInfo = pid_peerDown_info_iter->second;

					if (lost_ParentInfo->peerInfo.manifest & lost_Manifest) {
						lost_ParentInfo->outBuffCount ++;
						debug_printf("lastTriggerCount %d, outBuffCount %d > %d ?, pid %d \n", lost_ParentInfo->lastTriggerCount,
																								lost_ParentInfo->outBuffCount,
																								(CHUNK_LOSE*PARAMETER_X*Xcount)/sub_stream_num,
																								lost_ParentInfo->peerInfo.pid);
						_log_ptr->write_log_format("s(u) s d s u \n", __FUNCTION__, __LINE__,
																		"lost packet sequence number", _current_send_sequence_number,
																		"from Parent pid", lost_ParentInfo->peerInfo.pid);
																		
						/* Rescue triggered because packet lost is too much */
						// Parent is PK
						if ((lost_ParentInfo ->lastTriggerCount == 0) && 
							(lost_ParentInfo->outBuffCount > ((CHUNK_LOSE*PARAMETER_X*Xcount)/sub_stream_num)) && 
							(lost_ParentInfo->peerInfo.pid == PK_PID && 
							check_rescue_state(testingSubStreamID,0))) {
							
							_log_ptr->write_log_format("s(u) s s d s d \n", __FUNCTION__, __LINE__, "Rescue triggered.",
															   "lost_ParentInfo->outBuffCount =", lost_ParentInfo->outBuffCount,
															   ">", (CHUNK_LOSE*PARAMETER_X*Xcount)/sub_stream_num);
							_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[PROBLEM] Parent is PK");
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s d \n", __FUNCTION__, __LINE__, "packets lost too much from PK, upper bound =", ((CHUNK_LOSE*PARAMETER_X*Xcount)/sub_stream_num));
							lost_ParentInfo->outBuffCount = 0;
						}
						// Parent is not PK
						else if ((lost_ParentInfo ->lastTriggerCount == 0) && 
								(lost_ParentInfo->outBuffCount > ((CHUNK_LOSE*PARAMETER_X*Xcount)/sub_stream_num)) && 
								(lost_ParentInfo->peerInfo.pid != PK_PID && 
								check_rescue_state(testingSubStreamID,0))) {
							
							_log_ptr->write_log_format("s(u) s s d s d \n", __FUNCTION__, __LINE__, "Rescue triggered.",
															   "lost_ParentInfo->outBuffCount =", lost_ParentInfo->outBuffCount,
															   ">", (CHUNK_LOSE*PARAMETER_X*Xcount)/sub_stream_num);
					
							_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s d \n", __FUNCTION__, __LINE__, "packets lost too much from other parent, upper bound =", ((CHUNK_LOSE*PARAMETER_X*Xcount)/sub_stream_num));
							
							
							lost_ParentInfo->lastTriggerCount = 1 ;
							lost_ParentInfo->outBuffCount = 0;

							//找出所有正在測試的substream
							for (unsigned long i = 0; i < sub_stream_num; i++) {
								//if ((ssDetect_ptr + i) ->isTesting ){
								if (!( check_rescue_state(i,0))) {
									testingManifest |= SubstreamIDToManifest(i);
								}
							}

							//testing fail
							if (((testingManifest & lost_ParentInfo->peerInfo.manifest) & lost_Manifest)) {
								//LOG_PKT_LOSE
								_logger_client_ptr->log_to_server(LOG_PKT_LOSE, lost_Manifest);
								_logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL, lost_Manifest);
								_logger_client_ptr->log_to_server(LOG_DATA_COME_PK, lost_Manifest);
								debug_printf("this lose manifest is in testing it should from PK and peer why lose \n");
								_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"this lose manifest is in testing it should from PK and peer why lose");
								lost_ParentInfo->outBuffCount = 0;
							}
							//normal rescue
							else{
								lost_ParentInfo->peerInfo.manifest &= (~lost_Manifest) ;
								pkDownInfoPtr->peerInfo.manifest |= lost_Manifest ;

								_log_ptr->write_log_format("s =>u s u s u \n", __FUNCTION__, __LINE__,
																			"Parent PID", lost_ParentInfo->peerInfo.pid,
																			"outBuffCount", lost_ParentInfo->outBuffCount);
								_log_ptr->write_log_format("s =>u s u s u s u\n", __FUNCTION__, __LINE__,
																			"buffoutbound rescue SSID=",manifestToSubstreamID(lost_Manifest),
																			"sent TO PK manifest", pkDownInfoPtr->peerInfo.manifest | testingManifest,
																			"parent manifest=", lost_ParentInfo->peerInfo.manifest);

								_logger_client_ptr->log_to_server(LOG_RESCUE_TRIGGER, lost_Manifest);
								send_rescueManifestToPK(pkDownInfoPtr ->peerInfo.manifest | testingManifest);

								while (lost_Manifest) {
									testingSubStreamID = manifestToSubstreamID (lost_Manifest);
									
									//set recue stat true
									if (check_rescue_state(testingSubStreamID,0)) {
										set_rescue_state(testingSubStreamID,1);
									}
									else if (check_rescue_state(testingSubStreamID,2)) {
										set_rescue_state(testingSubStreamID,0);
										set_rescue_state(testingSubStreamID,1);
										//send_parentToPK(SubstreamIDToManifest(testingSubStreamID),PK_PID +1);
									}
									else if (check_rescue_state(testingSubStreamID,1)) {
										//do nothing
									}

									(ssDetect_ptr + testingSubStreamID)->previousParentPID = lost_ParentInfo->peerInfo.pid;
									lost_Manifest &= (~SubstreamIDToManifest(testingSubStreamID));
								}

								_peer_mgr_ptr->send_manifest_to_parent(lost_ParentInfo ->peerInfo.manifest ,lost_ParentInfo ->peerInfo.pid);
								/* 0903註解掉，因為還沒送給parent manifest=0這個訊息，等到peer::pkt_out將訊息送出後再刪除
								if(pid_peerDown_info_iter->second->peerInfo.manifest == 0 ){
									map_out_pid_fd_iter = _peer_ptr ->map_in_pid_fd.find(pid_peerDown_info_iter ->first) ;
									if(map_out_pid_fd_iter !=_peer_ptr ->map_in_pid_fd.end()){
										_peer_ptr ->data_close(_peer_ptr ->map_in_pid_fd[pid_peerDown_info_iter ->first],"manifest=0",CLOSE_PARENT) ;
										pid_peerDown_info_iter = map_pid_peerDown_info.begin() ;
										if(pid_peerDown_info_iter ==map_pid_peerDown_info.end()){
											break;
										}
									}else{
										debug_printf("pkmgr1446"); //PAUSE
									}

								reSet_detectionInfo();
								}
								*/
							}
						}
					}
					debug_printf("should from %d  manifest %d\n",pid_peerDown_info_iter ->first,pid_peerDown_info_iter->second->peerInfo.manifest);
					_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__,"should from ",pid_peerDown_info_iter ->first,"manifest",pid_peerDown_info_iter->second->peerInfo.manifest);
				}
				//PAUSE
				continue;
			}
			else{
				leastCurrDiff = _least_sequence_number - _current_send_sequence_number;
				debug_printf("leastCurrDiff    = %d \n", leastCurrDiff);
				if (leastCurrDiff < 0) {
					debug_printf("leastCurrDiff < 0   =%d\n",leastCurrDiff);
					_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"leastCurrDiff < 0   ");
					*(_net_ptr->_errorRestartFlag) =RESTART;
					PAUSE
				}
			}

			if ((leastCurrDiff < (int)( PARAMETER_X * Xcount *BUFF_SIZE)) && _current_send_sequence_number <= _least_sequence_number) {
				//debug_printf("break! \n");
				break;
			}
			//printf("_least_sequence_number++ \n");
		}
		debug_printf("Start sending packets to player \n");
		_log_ptr->write_log_format("s(u) s (d) \n", __FUNCTION__, __LINE__, "Finish sending packets to player", (int)( PARAMETER_X * Xcount *BUFF_SIZE));
		debug_printf("here3 leastCurrDiff = %d \n", leastCurrDiff);
		_log_ptr->write_log_format("s =>u s u s u s u \n", __FUNCTION__, __LINE__,
															"here3 leastCurrDiff =", leastCurrDiff,
															"_current=", _current_send_sequence_number,
															"SSID =", _current_send_sequence_number%sub_stream_num);
		//PAUSE
	}
	//可能某個subtream 追過_bucket_size,直接跳到最後一個 (應該不會發生)
	else if (leastCurrDiff >= _bucket_size) {

		debug_printf("i think not go here leastCurrDiff =%d\n",leastCurrDiff);
		_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"i think not go here leastCurrDiff ",leastCurrDiff);

		_current_send_sequence_number = _least_sequence_number;  
		_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"i think not go here leastCurrDiff  ");
		*(_net_ptr->_errorRestartFlag) =RESTART;
		PAUSE

			//在正常範圍內 正常傳輸丟給player 留給下面處理
	}
	else {

	}

	//after skip packet will be batter 
	if (syn_table.init_flag == 2) {
		quality_source_delay_count(sockfd, substream_id,chunk_ptr->header.sequence_number);
	}

	//正常傳輸丟給player 在這個for 底下給的seq下的一定是有方向性的
	for ( ; _current_send_sequence_number < _least_sequence_number; ) {

		//_current_send_sequence_number 指向的地方還沒到 ,不做處理等待並return
		if ((**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.sequence_number != _current_send_sequence_number) {
			//printf("wait packet,seq= %d  SSID =%d\n",_current_send_sequence_number,_current_send_sequence_number%sub_stream_num);
			//_log_ptr->write_log_format("s u s u s u s u\n","seq_ready_to_send",seq_ready_to_send,"_least_sequence_number",_least_sequence_number);
			return ;

			//為連續,丟給player
		}
		else if((**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.stream == STRM_TYPE_MEDIA) {

			//以下為丟給player
			for (_map_stream_iter = _map_stream_media.begin(); _map_stream_iter != _map_stream_media.end(); _map_stream_iter++) {
				//per fd mean a player   
				strm_ptr = _map_stream_iter->second;
				//stream_id 和request 一樣才add chunk
				if ((strm_ptr->_reqStreamID) == (**(buf_chunk_t + (_current_send_sequence_number % _bucket_size))).header.stream_id ) { 
					strm_ptr->add_chunk(*(buf_chunk_t + (_current_send_sequence_number % _bucket_size)));
					_net_ptr->epoll_control(_map_stream_iter->first, EPOLL_CTL_MOD, EPOLLOUT);
				}
			}
			_current_send_sequence_number++;
		}

	}
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
		_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] can not find source struct in table in send_capacity_to_pk\n");
		_logger_client_ptr->log_exit();
	}
	
	old_state = delay_table_iter->second->rescue_state;
	_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
														"rescue substream id =", sub_id,
														"state(0 normal 1 rescue trigger 2 testing) =", state,
														"old state =", old_state);
	
	if (state == 1) {
		if (old_state == 1 || old_state == 0) {
			delay_table_iter->second->rescue_state = state;
		}
		else {
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d s d s \n", "[ERROR] set_rescue_state error from", old_state, "to", state, "\n");
			_logger_client_ptr->log_exit();
		}
	}
	else if (state == 2) {
		if (old_state == 2 || old_state == 1) {
			delay_table_iter->second->rescue_state = state;
		}
		else {
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d s d s \n", "[ERROR] set_rescue_state error from", old_state, "to", state, "\n");
			_logger_client_ptr->log_exit();
		}
	}
	else if (state == 0) {
		if (old_state == 0 || old_state == 2) {
			delay_table_iter->second->rescue_state = state;
		}
		else {
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d s d s \n", "[ERROR] set_rescue_state error from", old_state, "to", state, "\n");
			_logger_client_ptr->log_exit();
		}
	}
	else {
		_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"LOG_WRITE_STRING");

		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d s \n", "[ERROR] set_rescue_state error. unknown state", state, "\n");
		_logger_client_ptr->log_exit();
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
		//_log_ptr->write_log_format("s =>u s u s u s u\n", __FUNCTION__,__LINE__," rescue substream id : ",sub_id," state(0 normal 1 rescue trigger 2 testing) =",delay_table_iter->second->rescue_state," check ok",state);
		return 1;
	}
	else {
		//_log_ptr->write_log_format("s =>u s u s u s u\n", __FUNCTION__,__LINE__," rescue substream id : ",sub_id," state(0 normal 1 rescue trigger 2 testing) =",delay_table_iter->second->rescue_state," check fail",state);
		return 0;
	}
}

void pk_mgr::syn_table_init(int pk_sock)
{
	syn_table.client_abs_start_time = -1;
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
	synLock = 1;
	struct syn_token_send *syn_token_send_ptr = new struct syn_token_send;
	if (!syn_token_send_ptr) {
		debug_printf("pk_mgr::syn_token_send_ptr new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] syn_token_send_ptr  new error");
		PAUSE
	}
	memset(syn_token_send_ptr, 0, sizeof(struct syn_token_send));

	syn_token_send_ptr->header.cmd = CHNK_CMD_PEER_SYN;
	syn_token_send_ptr->header.rsv_1 = REQUEST;
	syn_token_send_ptr->header.length = sizeof(struct syn_token_send) - sizeof(struct chunk_header_t);
	syn_token_send_ptr->reserve = 0;
	
	debug_printf("sizeof struct syn_token_send: %d, sizeof struct chunk_header_t: %d \n", sizeof(struct syn_token_send), sizeof(struct chunk_header_t));
	debug_printf("Send synchronization packets to PK \n");
	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Send synchronization packets to PK");
	
	int send_byte;
	int expected_len = sizeof(struct syn_token_send);
	char send_buf[sizeof(struct syn_token_send)] = {0};
	
	memcpy(send_buf, syn_token_send_ptr, expected_len);
	
	_net_ptr->set_blocking(pk_sock);	// set to blocking

	_log_ptr->timerGet(&lastSynStartclock);
	_log_ptr->timerGet(&syn_round_start);
	//_log_ptr->getTickTime(&syn_round_start);

	send_byte = _net_ptr->send(pk_sock, send_buf, expected_len, 0);
	if (send_byte <= 0) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "send_syn_token_to_pk  error", WSAGetLastError());
		debug_printf("send send_syn_token_to_pk error \n");
		data_close(pk_sock, "send send_syn_token_to_pk error");
		//_log_ptr->exit(0, "send pkt error");
	}
	else {
		if (syn_token_send_ptr) {
			delete syn_token_send_ptr;
		}
		_net_ptr->set_nonblocking(pk_sock);	// set to non-blocking
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
void pk_mgr::syn_recv_handler(struct syn_token_receive* syn_struct_back_token)
{
	//LARGE_INTEGER syn_round_end;
	synLock = 0;
	struct timerStruct syn_round_end;

	_log_ptr->timerGet(&syn_round_end);
	//_log_ptr->getTickTime(&syn_round_end);
	//_log_ptr->diffTime_ms(syn_round_start,syn_round_end);

	if (syn_table.init_flag == 0) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] in syn_recv_handler() syn not send");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n", "[ERROR] in syn_recv_handler() syn not send\n");
		_logger_client_ptr->log_exit();
	}
	// Need to set syn_table.start_clock and syn_table.client_abs_start_time
	else if (syn_table.init_flag == 1) {
		debug_printf("syn_table.init_flag == 1 \n");
		
		syn_round_time = _log_ptr->diff_TimerGet_ms(&syn_round_start, &syn_round_end) - (syn_struct_back_token->pk_SendTime - syn_struct_back_token->pk_RecvTime) ;
		
		//syn_table.start_clock = lastSynStartclock;
		memcpy(&syn_table.start_clock, &lastSynStartclock, sizeof(struct timerStruct));
		
		syn_table.client_abs_start_time = syn_struct_back_token->pk_RecvTime - (syn_round_time/2);
		if (syn_table.client_abs_start_time < 0) {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "warning syn error\n");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "client_abs_start_time : %ld\n", syn_table.client_abs_start_time);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "pk_time : %ld\n", syn_struct_back_token->pk_RecvTime);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "(syn_round_time/2) : %ld\n", (syn_round_time/2));
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "syn_round_time : %ld\n", syn_round_time);
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[QUESTION] Why syn_table.client_abs_start_time < 0 ?");
			_logger_client_ptr->log_exit();
		}
		else{
			debug_printf("client_abs_start_time: %lu \n", syn_table.client_abs_start_time);
			debug_printf("pk_time: %lu \n", syn_struct_back_token->pk_RecvTime);
			debug_printf("(syn_round_time/2): %lu \n", (syn_round_time/2));
			debug_printf("syn_round_time: %lu \n", syn_round_time);
			_log_ptr->write_log_format("s(u) s d s u s u s u \n", __FUNCTION__, __LINE__,
																"init_flag", syn_table.init_flag,
																"client_abs_start_time =", syn_table.client_abs_start_time,
																"pk_RecvTime =", syn_struct_back_token->pk_RecvTime,
																"syn_round_time =", syn_round_time);
		}
		syn_table.init_flag = 2;
		syn_table.start_seq = syn_struct_back_token->seq_now;
		lastPKtimer = syn_struct_back_token->pk_RecvTime;
	
	//already syn and re-syn need to  reset syn_table.start_clock  and 	syn_table.client_abs_start_time and synround
	} else if(syn_table.init_flag == 2) {
		debug_printf("syn_table.init_flag == 2 \n");
		volatile unsigned long secondSyn_absTime = 0;
		volatile unsigned long serverSynPeriod = 0;
		volatile unsigned long serverPacketSynPeriod = 0;
		volatile unsigned long clockPeriodTime = 0;
		volatile unsigned long tickPeriodTime = 0;
		int clockPeriodDiff = 0;
		int tickPeriodDiff = 0;

		syn_round_time = _log_ptr->diff_TimerGet_ms(&syn_round_start,&syn_round_end)- (unsigned long)abs((int)(syn_struct_back_token->pk_SendTime-syn_struct_back_token->pk_RecvTime)) ;

		secondSyn_absTime = syn_struct_back_token->pk_RecvTime - (syn_round_time/2);

		serverSynPeriod = secondSyn_absTime -syn_table.client_abs_start_time;

		//select a good timeMod
		_log_ptr->timerMod =MOD_TIME__CLOCK;
		clockPeriodTime=_log_ptr->diff_TimerGet_ms(&syn_table.start_clock,&lastSynStartclock);

		_log_ptr->timerMod =MOD_TIME_TICK;
		tickPeriodTime=_log_ptr->diff_TimerGet_ms(&syn_table.start_clock,&lastSynStartclock);
		//tickPeriodTime=_log_ptr->diffTime_ms(syn_table.start_clock.tickTime ,lastSynStartclock.tickTime);

		debug_printf("serverSynPeriod =%d clockPeriodTime =%d  tickPeriodTime = %d \n",serverSynPeriod,clockPeriodTime,tickPeriodTime);
		
		serverPacketSynPeriod = syn_struct_back_token->pk_RecvTime - lastPKtimer;
		debug_printf("packet  period timer = %d \n", serverPacketSynPeriod);
		_log_ptr->write_log_format("s =>u s u s u s u \n", __FUNCTION__, __LINE__,
															"serverSynPeriod =", serverSynPeriod,
															"clockPeriodTime =", clockPeriodTime,
															"tickPeriodTime =", tickPeriodTime);
		
		clockPeriodDiff = abs((int)(serverPacketSynPeriod - clockPeriodTime));
		tickPeriodDiff = abs((int)(serverPacketSynPeriod - tickPeriodTime));

		debug_printf("clockPeriodDiff =%d  tickPeriodDiff =%d \n",clockPeriodDiff,tickPeriodDiff);

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
			debug_printf("re_syn select MOD_TIME_TICK \n") ;
			_log_ptr->timerMod = MOD_TIME_TICK;
		}
		else{
			debug_printf("re_syn select MOD_TIME__CLOCK \n") ;
			_log_ptr->timerMod =MOD_TIME__CLOCK;
		}

		//reset set all syn_table
		syn_table.client_abs_start_time = secondSyn_absTime;
		//syn_table.start_clock = lastSynStartclock;
		memcpy(&syn_table.start_clock, &lastSynStartclock, sizeof(struct timerStruct));
		//syn_table.start_seq = syn_struct_back_token->seq_now;
		totalMod = 0;
		lastPKtimer = syn_struct_back_token->pk_RecvTime ;
		
	}
}
/****************************************************/
/*		Functions of synchronization end			*/
/****************************************************/

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

	_log_ptr->write_log_format("s(u) s (s)\n", __FUNCTION__, __LINE__, "close PK", reason);
	debug_printf("close PK by %s \n", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".. " << endl;
	//_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);

	map<int, unsigned long>::iterator map_fd_pid_iter;
	for (map_fd_pid_iter = _peer_ptr->map_fd_pid.begin(); map_fd_pid_iter != _peer_ptr->map_fd_pid.end(); map_fd_pid_iter++) {
		if (map_fd_pid_iter->first == cfd) {
			debug_printf("pid = %lu \n", map_fd_pid_iter->second);
		}
	}
	
	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if (*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}

	*(_net_ptr->_errorRestartFlag) = RESTART;
	PAUSE
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
	for (unsigned long i = 0; i < sub_stream_num; i++) {
		full_manifest |= (1<<i);
	}

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
	int substreamID;
	struct timerStruct	newAlarm; 
	unsigned long sourceTimeDiffOne;
	unsigned long localTimeDiffOne;
	unsigned long sourceTimeDiffTwo;
	unsigned long localTimeDiffTwo;
	double sourceBitrate;
	double localBitrate;

	substreamID = (chunk_ptr ->header.sequence_number) % sub_stream_num;

	//int X = Xcount;
	
	for (int i = 0; i < sub_stream_num; i++) {
		debug_printf2("ssDetect_ptr[%d].count_X = %d  (%d) \n", i, ssDetect_ptr[i].count_X, Xcount);
	}
	
	//如果是第一個封包，初始化
	if (!ssDetect_ptr[substreamID].last_seq) {  
		ssDetect_ptr[substreamID].last_timestamp = chunk_ptr->header.timestamp;
		ssDetect_ptr[substreamID].last_seq = chunk_ptr->header.sequence_number;
		ssDetect_ptr[substreamID].first_timestamp = chunk_ptr->header.timestamp;
		_log_ptr->timerGet(&ssDetect_ptr[substreamID].lastAlarm);
		_log_ptr->timerGet(&ssDetect_ptr[substreamID].firstAlarm);
	}
	
	//開始計算偵測
	//只有是 比上次記錄新的sequence_number 才做處理
	else if (ssDetect_ptr[substreamID].last_seq < chunk_ptr->header.sequence_number) {
		_log_ptr->timerGet(&newAlarm);

		//////////////////////////////////////利用頻寬判斷(測量方法一)////////////////////////////////////////////
		// Check bandwidth per Xcount numbers of chucks
		ssDetect_ptr[substreamID].count_X++;
		ssDetect_ptr[substreamID].total_byte += chunk_ptr->header.length;

		//只有第一次計算會跑
		if (ssDetect_ptr[substreamID].count_X == 1) {
			ssDetect_ptr[substreamID].first_timestamp = chunk_ptr->header.timestamp;
		}

		if ( ssDetect_ptr[substreamID].count_X >= (Xcount -1)) {
			_log_ptr->timerGet(&ssDetect_ptr[substreamID].previousAlarm) ;
		}

		//累積X個封包後做判斷
		if (ssDetect_ptr[substreamID].count_X >= Xcount) {

			(ssDetect_ptr[substreamID].measure_N)++;  //從1開始計

			sourceTimeDiffOne = chunk_ptr->header.timestamp - ssDetect_ptr[substreamID].first_timestamp;
			//localTimeDiffOne=	(newAlarm - ssDetect_ptr[substreamID] ->firstAlarm);//_log_ptr ->diffTime_ms((ssDetect_ptr + substreamID) ->firstAlarm ,newAlarm);
			localTimeDiffOne = _log_ptr->diff_TimerGet_ms( &(ssDetect_ptr[substreamID].firstAlarm), &newAlarm);
			if (localTimeDiffOne < 1) {
				localTimeDiffOne = 1;
			}
			if (sourceTimeDiffOne < 1) {
				sourceTimeDiffOne = 1;
			}
			sourceBitrate = ( ( double)(ssDetect_ptr[substreamID].total_byte ) /(double)sourceTimeDiffOne )*8*1000 ;
			localBitrate  = ( ( double)(ssDetect_ptr[substreamID].total_byte ) /(double)localTimeDiffOne  )*8*1000 ;

			ssDetect_ptr[substreamID].last_sourceBitrate = sourceBitrate ;
			ssDetect_ptr[substreamID].last_localBitrate = localBitrate;

			//做每個peer substream的加總 且判斷需不需要救
			measure();
			
			ssDetect_ptr[substreamID].total_byte = chunk_ptr->header.length;
			ssDetect_ptr[substreamID].count_X = 1;
			ssDetect_ptr[substreamID].firstAlarm = ssDetect_ptr[substreamID].previousAlarm ;
			ssDetect_ptr[substreamID].first_timestamp = chunk_ptr->header.timestamp;
		}
		//////////////////////////////////////(測量方法一結束)///////////////////////////////////////////////////////////

		////////////////////////////////////單看兩個連續封包的delay取max (測量方法二)///////////////////////////////////
		sourceTimeDiffTwo = chunk_ptr->header.timestamp - ssDetect_ptr[substreamID].last_timestamp;
		//localTimeDiffTwo=	(newAlarm - ssDetect_ptr[substreamID] ->lastAlarm);//_log_ptr ->diffTime_ms((ssDetect_ptr + substreamID) ->lastAlarm ,newAlarm);
		localTimeDiffTwo = _log_ptr->diff_TimerGet_ms(&ssDetect_ptr[substreamID].lastAlarm, &newAlarm);
		if (localTimeDiffTwo > sourceTimeDiffTwo) { 
			if (ssDetect_ptr[substreamID].total_buffer_delay < (localTimeDiffTwo - sourceTimeDiffTwo)) {
				ssDetect_ptr[substreamID].total_buffer_delay = (localTimeDiffTwo - sourceTimeDiffTwo);
				//printf("SSID=%d Max total_buffer_delay %u\n",substreamID,(ssDetect_ptr + substreamID) ->total_buffer_delay);
			}
		}
		else {
			//	debug_printf("on time \n");
		}		

		debug_printf2("5 clockTime = %d, tickTime = %llu, initClockFlag = %d, initTickFlag = %d \n", ssDetect_ptr[substreamID].lastAlarm.clockTime, ssDetect_ptr[substreamID].lastAlarm.tickTime, ssDetect_ptr[substreamID].lastAlarm.initClockFlag, ssDetect_ptr[substreamID].lastAlarm.initTickFlag);
		
		ssDetect_ptr[substreamID].lastAlarm = newAlarm;		// (ssDetect_ptr + substreamID)->lastAlarm = newAlarm;
		ssDetect_ptr[substreamID].last_timestamp = chunk_ptr->header.timestamp;		// (ssDetect_ptr + substreamID)->last_timestamp = chunk_ptr->header.timestamp;
		
		debug_printf2("5 clockTime = %d, tickTime = %llu, initClockFlag = %d, initTickFlag = %d \n", ssDetect_ptr[substreamID].lastAlarm.clockTime, ssDetect_ptr[substreamID].lastAlarm.tickTime, ssDetect_ptr[substreamID].lastAlarm.initClockFlag, ssDetect_ptr[substreamID].lastAlarm.initTickFlag);
		
		//////////////////////////////////////(測量方法二結束)///////////////////////////////////////////////////////////

		ssDetect_ptr[substreamID].last_seq = chunk_ptr->header.sequence_number;
	}
	else if (ssDetect_ptr[substreamID].last_seq == chunk_ptr->header.sequence_number) {
		//doing nothing
	}
	else {
		//在某些特定的情況下會近來  但不影響運算
		//PAUSE
		//printf("why here old packet here??\n");
	}

	return ;
}


//struct peer_connect_down_t
//	struct peer_info_t peerInfo;
//	int rescueStatsArry[PARAMETER_M];
//	volatile unsigned int timeOutLastSeq;
//	volatile unsigned int timeOutNewSeq;
//	volatile unsigned int lastTriggerCount;
//	volatile unsigned int outBuffCount;
//	
// ssDetect_ptr:
// struct detectionInfo 
//	//timer
//	struct timerStruct	lastAlarm;
//	struct timerStruct	firstAlarm;
//	struct timerStruct	previousAlarm;
//
//	unsigned int	last_timestamp;
//	unsigned int	first_timestamp;
//	unsigned long	last_seq;
//
//	unsigned int	measure_N;		//第N次測量
//	unsigned int	count_X;		//X個封包量一次
//
//	unsigned int	total_buffer_delay;
//	double			last_sourceBitrate;
//	double			last_localBitrate;
//	unsigned int	total_byte;
//	int				isTesting;
//	unsigned int	testing_count;	//用來測試rescue 的計數器
//	unsigned		previousParentPID;
//
//累積Xcount個packets後呼叫此function，計算每個parent-peer擁有的substream後判斷需不需要rescue
void pk_mgr::measure()
{	
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	//map<unsigned long, int>::iterator map_out_pid_fd_iter;
	struct peer_connect_down_t *connectPeerInfo = NULL;
	unsigned long tempManifest=0 ;
	unsigned long afterManifest=0 ;
	//	unsigned long sentToPKManifest=0 ;
	unsigned long perPeerSS_num = 0;		//針對一個peer sub stream 的個數 
	int peerHighestSSID = -1;	//用來確保是跟這次測量做加總,而不是和上一次
	double totalSourceBitrate =0;
	double totalLocalBitrate =0 ;
	unsigned int count_N=0;
	unsigned int continuous_P=0;
	unsigned int rescueSS=1;
	int tempMax=-1;

	int testingSubStreamID=-1 ;
	unsigned long testingManifest=0 ;
	unsigned long peerTestingManifest= 0;

	debug_printf2("********** pk_mgr::measure() \n");

	//for each peer
	for (pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter != map_pid_peerDown_info.end(); pid_peerDown_info_iter++) {

		peer_connect_down_t *connectPeerInfo = NULL;
		tempManifest=0 ;
		afterManifest=0 ;
		//unsigned long sentToPKManifest=0 ;
		perPeerSS_num = 0;		//針對一個peer sub stream 的個數 
		peerHighestSSID = -1;	//用來確保是跟這次測量做加總,而不是和上一次
		totalSourceBitrate =0;
		totalLocalBitrate =0 ;
		count_N=0;
		continuous_P=0;
		rescueSS=1;
		tempMax=-1;
		testingSubStreamID=-1 ;
		testingManifest=0 ;
		memset( statsArryCount_ptr , 0x00 ,sizeof(unsigned long) * (sub_stream_num+1)  ); 
		
		connectPeerInfo = pid_peerDown_info_iter->second;
		debug_printf("pid %d, manifest %d \n", connectPeerInfo->peerInfo.pid, connectPeerInfo->peerInfo.manifest);
		
		tempManifest = connectPeerInfo->peerInfo.manifest;
		if (tempManifest == 0) {
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "[DEBUG] PID", connectPeerInfo->peerInfo.pid, "manifest = 0");
			continue;
		}
		
		// Get parent-peer's information, and calculate bit-rate
		for (int i = 0; i < sub_stream_num; i++) {
			//i=substreamID
			if (tempManifest & (1<<i)) {
				perPeerSS_num++;
				peerHighestSSID = i;
				totalSourceBitrate += (ssDetect_ptr + i)->last_sourceBitrate;
				totalLocalBitrate += (ssDetect_ptr + i)->last_localBitrate;
			}
		}
		debug_printf("totalSourceBitrate=%.5f, totalLocalBitrate=%.5f \n", totalSourceBitrate, totalLocalBitrate);
		_log_ptr->write_log_format("s(u) s f s f\n", __FUNCTION__, __LINE__,
														"totalSourceBitrate =", totalSourceBitrate,
														"totalLocalBitrate =", totalLocalBitrate);
		
		//設定最近偵測的狀態到rescueStatsArry
		for (int i = 0; i <= perPeerSS_num; i++) {
			//介在需要rescue i 個substream之間
			if (totalLocalBitrate <= (1 - (double)(2*i-1)/(double)(2*perPeerSS_num) )*totalSourceBitrate && 
				totalLocalBitrate > (1 - (double)(2*(i+1)-1)/(double)(2*perPeerSS_num) )*totalSourceBitrate) {
				//rescue i substream
				debug_printf("(ssDetect_ptr + peerHighestSSID)->measure_N-1 = %d \n", ((ssDetect_ptr + peerHighestSSID)->measure_N-1));
				debug_printf("((ssDetect_ptr + peerHighestSSID)->measure_N-1) % PARAMETER_M = %d \n", ((ssDetect_ptr + peerHighestSSID)->measure_N-1) % PARAMETER_M);
				debug_printf("i = %d \n", i);
				connectPeerInfo->rescueStatsArry[((ssDetect_ptr + peerHighestSSID)->measure_N-1) % PARAMETER_M] = i;
			}
		}
		if( (totalLocalBitrate > (1 - (double)(-1)/(double)(2*perPeerSS_num) )*totalSourceBitrate )){
			connectPeerInfo ->rescueStatsArry[( (ssDetect_ptr + peerHighestSSID)->measure_N -1)% PARAMETER_M ] =0;
		}
		_log_ptr->write_log_format("s =>u s u s d\n", __FUNCTION__, __LINE__, 
													"[DEBUG] PID", pid_peerDown_info_iter->second->peerInfo.pid, 
													"rescue substream number =", connectPeerInfo->rescueStatsArry[( (ssDetect_ptr + peerHighestSSID)->measure_N -1)% PARAMETER_M ]);

		//根據rescueStatsArry 來決定要不要觸發rescue
		for (int i = 0; i < PARAMETER_M; i++) {
			//做統計
			( *(statsArryCount_ptr + connectPeerInfo->rescueStatsArry[i]) )++;	// statsArryCount_ptr[connectPeerInfo->rescueStatsArry[i]]++;
			
			if (connectPeerInfo->rescueStatsArry[i] > 0) {
				count_N++;
			}
			//printf("%d  ",connectPeerInfo ->rescueStatsArry[i]);
			//_log_ptr->write_log_format("u  ",connectPeerInfo ->rescueStatsArry[i]);
		}
		//printf("\n");
		//_log_ptr->write_log_format("\n");

		//近PARAMETER_P次 發生 P次
		for(int j=0 ; j<PARAMETER_P ;j++){
			if ( connectPeerInfo ->rescueStatsArry[ (PARAMETER_M +( (ssDetect_ptr + peerHighestSSID)->measure_N -1)-j )% PARAMETER_M ] >0 ){
				continuous_P++ ;
			}
		}


		for(unsigned long k=0 ; k<( (ssDetect_ptr + peerHighestSSID)->measure_N -1)% PARAMETER_M ;k++){
			//printf("   ");
			//_log_ptr->write_log_format("   ");
		}

		//printf("↑\n");
		//_log_ptr->write_log_format("↑\n");


		//找出統計最多的值
		for (unsigned long k = 1; k < sub_stream_num+1; k++) {
			//printf("substream%d = %d   \n",k,*(statsArryCount_ptr+ k) );
			if (k != 0 && tempMax < (*(statsArryCount_ptr+ k)) ) {
				tempMax = *(statsArryCount_ptr+ k);	// tempMax = statsArryCount_ptr[k];
				rescueSS = k ;
			}
		}

		//符合條件觸發rescue 需要救rescue_ss 個
		if( (count_N >= PARAMETER_N  || continuous_P == PARAMETER_P)  && connectPeerInfo ->lastTriggerCount  == 0){

			connectPeerInfo->lastTriggerCount = 1;
			_log_ptr->write_log_format("s(u) s s d s d s d \n", __FUNCTION__, __LINE__, "Rescue triggered.",
																					"count_N ", count_N,
																					">= PARAMETER_N", PARAMETER_N,
																					"or continuous_P ==", continuous_P);
															   
			//找出所有正在測試的substream
			for (unsigned long i = 0; i < sub_stream_num; i++) {
				if (!check_rescue_state(i,0)) {
					testingManifest |= SubstreamIDToManifest(i);
				}
			}
	
			//找出這個peer 所有正在testing 的substream ID
			peerTestingManifest = (testingManifest & connectPeerInfo->peerInfo.manifest);

			debug_printf("continuous_P = %d \n pid = %d need cut %d substream and need rescue \n", continuous_P, connectPeerInfo->peerInfo.pid,rescueSS);
			_log_ptr->write_log_format("s =>u s u s u s \n", __FUNCTION__,__LINE__,"pid ",connectPeerInfo ->peerInfo.pid ," need cut",rescueSS,"substream and need rescue");

			//PID是PK的有問題 (代表是這個peer下載能力有問題)
			if (connectPeerInfo->peerInfo.pid == PK_PID) {
				debug_printf("download have problem , peer need set dead\n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[PROBLEM] Parent is PK");
			}
			//PID是正在測試的peer 測試失敗
			else if (peerTestingManifest) {
				debug_printf("stream testing fail \n");
				//若有多個正在測試中一次只選擇一個substream cut(最右邊的)  並且重設全部記數器的count

				//if( (ssDetect_ptr +manifestToSubstreamID (peerTestingManifest)) ->isTesting){

				
					testingSubStreamID = manifestToSubstreamID(peerTestingManifest);	// return the right side substreamID

					_logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL,SubstreamIDToManifest (testingSubStreamID ));
					_logger_client_ptr->log_to_server(LOG_DATA_COME_PK,SubstreamIDToManifest (testingSubStreamID ));

					_log_ptr->write_log_format("s =>u s u s u s u \n", __FUNCTION__, __LINE__,
																	   "stream testing fail peerTestingManifest ", peerTestingManifest,
																	   "select manifest = ", manifestToSubstreamID (peerTestingManifest),
																	   "testingSubStreamID",testingSubStreamID);

					(ssDetect_ptr + testingSubStreamID)->isTesting = 0;  //false  

					//set rescue stat zero
					set_rescue_state(testingSubStreamID, 0);

					//should sent to PK select PK ,再把testing 取消偵測的 pk_manifest 設回來
					unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
					pkDownInfoPtr->peerInfo.manifest |= SubstreamIDToManifest(testingSubStreamID ) ;
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
																	   "PK manifest change from", pk_old_manifest,
																	   "to", pkDownInfoPtr->peerInfo.manifest);
 
					//should sent to peer cut stream												   
					unsigned long parent_old_manifest = connectPeerInfo->peerInfo.manifest;			
					connectPeerInfo->peerInfo.manifest &= (~SubstreamIDToManifest(testingSubStreamID)) ;
					_peer_mgr_ptr->send_manifest_to_parent(connectPeerInfo->peerInfo.manifest, connectPeerInfo->peerInfo.pid);
					_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
																		   "parent pid", pid_peerDown_info_iter->second->peerInfo.pid,
																		   "manifest change from", parent_old_manifest,
																		   "to", connectPeerInfo->peerInfo.manifest);								
					
					/* 0903註解掉，因為還沒送給parent manifest=0這個訊息，等到peer::pkt_out將訊息送出後再刪除
					if (pid_peerDown_info_iter->second->peerInfo.manifest == 0) {
						map<unsigned long, int>::iterator iter = _peer_ptr->map_in_pid_fd.find(pid_peerDown_info_iter->first) ;
						if (iter !=_peer_ptr->map_in_pid_fd.end()) {
							_peer_ptr->data_close(_peer_ptr->map_in_pid_fd[pid_peerDown_info_iter ->first], "manifest=0", CLOSE_PARENT) ;
							pid_peerDown_info_iter = map_pid_peerDown_info.begin();
							if(pid_peerDown_info_iter == map_pid_peerDown_info.end()) {
								break;
							}
						}
						else{
							debug_printf("pkmgr2016"); //PAUSE
						}
					}
					*/
					
					send_parentToPK(SubstreamIDToManifest(testingSubStreamID), PK_PID+1);

					peerTestingManifest &= (~SubstreamIDToManifest(testingSubStreamID));

					//重設其他正在testing 的substream
					while(peerTestingManifest){
						testingSubStreamID = manifestToSubstreamID(peerTestingManifest);
						(ssDetect_ptr + testingSubStreamID)->testing_count = 0;
						peerTestingManifest &= (~SubstreamIDToManifest(testingSubStreamID));

					}

					reSet_detectionInfo();
				//}

			}
			//PID 是其他peer
			else {

				debug_printf("normal rescue \n");
				_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"normal rescue ");

				afterManifest = manifestFactory(connectPeerInfo->peerInfo.manifest, rescueSS);	// return manifest which has cut off rescueSS
				pkDownInfoPtr ->peerInfo.manifest |= (connectPeerInfo->peerInfo.manifest & (~afterManifest));

				//這邊可能因為有些stream 正在testing 而沒有flag 因此傳出去的需要再跟testing 的合起來
				_logger_client_ptr->log_to_server(LOG_RESCUE_TRIGGER,(connectPeerInfo ->peerInfo.manifest &(~afterManifest)));
				_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__," old PK manifest ",pkDownInfoPtr ->peerInfo.manifest);
				send_rescueManifestToPK(pkDownInfoPtr->peerInfo.manifest | testingManifest);
				_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"PK manifest +testing manifest sent to PK ",pkDownInfoPtr ->peerInfo.manifest);

				
				debug_printf("original manifest %d after cut manifest %d  PK manifest=%d \n",connectPeerInfo ->peerInfo.manifest,afterManifest,pkDownInfoPtr ->peerInfo.manifest );
				_log_ptr->write_log_format("s =>u s u s u s u\n", __FUNCTION__,__LINE__,"original manifest",connectPeerInfo ->peerInfo.manifest,"after cut manifest",afterManifest," PK manifest= ",pkDownInfoPtr ->peerInfo.manifest);

				tempManifest = (connectPeerInfo->peerInfo.manifest & (~afterManifest)) ;

				//把先前的連接的PID存起來
				while (tempManifest) {
					testingSubStreamID = manifestToSubstreamID(tempManifest);
					//set recue stat true
					set_rescue_state(testingSubStreamID,1);
					(ssDetect_ptr + testingSubStreamID) ->previousParentPID = connectPeerInfo ->peerInfo.pid;
					tempManifest &=  (~SubstreamIDToManifest(testingSubStreamID) );
				}
				
				// Send manifest to parent
				connectPeerInfo ->peerInfo.manifest = afterManifest ;
				_peer_mgr_ptr->send_manifest_to_parent(connectPeerInfo ->peerInfo.manifest ,connectPeerInfo ->peerInfo.pid);
				_log_ptr->write_log_format("s =>u s u s u\n", __FUNCTION__,__LINE__," sent to parent PID ",pkDownInfoPtr ->peerInfo.pid,"manifest =",pkDownInfoPtr ->peerInfo.manifest);
				/* 0903註解掉，因為還沒送給parent manifest=0這個訊息，等到peer::pkt_out將訊息送出後再刪除
				if (pid_peerDown_info_iter->second->peerInfo.manifest == 0) {
					map<unsigned long, int>::iterator iter = _peer_ptr ->map_in_pid_fd.find(pid_peerDown_info_iter ->first) ;
					if (iter != _peer_ptr ->map_in_pid_fd.end()) {
						_peer_ptr->data_close(_peer_ptr->map_in_pid_fd[pid_peerDown_info_iter ->first], "manifest=0", CLOSE_PARENT) ;
						pid_peerDown_info_iter = map_pid_peerDown_info.begin() ;
						if (pid_peerDown_info_iter == map_pid_peerDown_info.end()) {
							break;
						}
					}
					else{
						debug_printf("pkmgr"); //PAUSE
					}
				}
				*/
				reSet_detectionInfo();
			}
		}
		else{
			//if trigger
			if (connectPeerInfo->lastTriggerCount >= 1) {
				connectPeerInfo->lastTriggerCount ++ ;
			}
			if (connectPeerInfo ->lastTriggerCount >= PARAMETER_M){
				connectPeerInfo ->lastTriggerCount = 0 ;
			}
		}
	}
}



void pk_mgr::send_rescueManifestToPK(unsigned long manifestValue)
{
	struct rescue_pkt_from_server  *chunk_rescueManifestPtr = NULL;

	chunk_rescueManifestPtr = new struct rescue_pkt_from_server;
	if (!chunk_rescueManifestPtr) {
		debug_printf("pk_mgr::chunk_rescueManifestPtr new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"chunk_rescueManifestPtr  new error");
		PAUSE
	}

	memset(chunk_rescueManifestPtr, 0, sizeof(struct rescue_pkt_from_server));

	chunk_rescueManifestPtr->header.cmd = CHNK_CMD_PEER_RESCUE ;
	chunk_rescueManifestPtr->header.length = (sizeof(struct rescue_pkt_from_server)-sizeof(struct chunk_header_t)) ;	//pkt_buf paylod length
	chunk_rescueManifestPtr->header.rsv_1 = REQUEST ;
	chunk_rescueManifestPtr->pid = _peer_mgr_ptr ->self_pid ;
	chunk_rescueManifestPtr->manifest = manifestValue ;
	chunk_rescueManifestPtr->rescue_seq_start =_current_send_sequence_number -(sub_stream_num*5) ;

	_net_ptr->set_blocking(_sock);

	_net_ptr ->send(_sock , (char*)chunk_rescueManifestPtr ,sizeof(struct rescue_pkt_from_server),0) ;

	_net_ptr->set_nonblocking(_sock);

	//debug_printf("sent rescue to PK manifest = %d  start from %d\n",manifestValue,_current_send_sequence_number -(sub_stream_num*5) );
	_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__,__LINE__, "sent rescue to PK manifest =", manifestValue, "start from", _current_send_sequence_number -(sub_stream_num*5)) ;

	delete chunk_rescueManifestPtr;

	return ;
}

void pk_mgr::clear_map_pid_peer_info()
{
	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(pid_peer_info_iter =map_pid_peer_info.begin();pid_peer_info_iter!=map_pid_peer_info.end(); pid_peer_info_iter++){
		peerInfoPtr=pid_peer_info_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_peer_info.clear();
}

void pk_mgr::clear_delay_table()
{
	map<unsigned long, struct source_delay *>::iterator source_delay_iter;	
	for (source_delay_iter= delay_table.begin(); source_delay_iter !=delay_table.end(); source_delay_iter++) {
		delete source_delay_iter->second;
	}
	delay_table.clear();
}

void pk_mgr::clear_map_streamID_header(){

	map<int, struct update_stream_header *>::iterator update_stream_header_iter;

	for(update_stream_header_iter = map_streamID_header.begin() ;update_stream_header_iter !=map_streamID_header.end();update_stream_header_iter++){
		
		delete update_stream_header_iter->second;

	}
	map_streamID_header.clear();
}



void pk_mgr::clear_map_pid_peer_info(unsigned long manifest){

	multimap <unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	multimap <unsigned long, struct peer_info_t *>::iterator temp_pid_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;


	for(pid_peer_info_iter =map_pid_peer_info.begin();pid_peer_info_iter!=map_pid_peer_info.end(); ){
		peerInfoPtr=pid_peer_info_iter ->second;

		if(peerInfoPtr ->manifest == manifest){

			debug_printf("clear map_pid_peer_info pid = %d \n",pid_peer_info_iter->first);
			_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__,"clear map_pid_peer_info pid ",pid_peer_info_iter->first) ;

			delete peerInfoPtr;

			temp_pid_peer_info_iter =pid_peer_info_iter;
			pid_peer_info_iter ++;

			map_pid_peer_info.erase(temp_pid_peer_info_iter);
//			pid_peer_info_iter =map_pid_peer_info.begin() ;
			if(pid_peer_info_iter == map_pid_peer_info.end())
				break;

		}else{
			pid_peer_info_iter++ ;
		}
	}
	_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__,__LINE__," clear_map_pid_peer_info  end manifest =",manifest) ;

	debug_printf("clear_map_pid_peer_info  end manifest = %d \n",manifest);

}



void pk_mgr::clear_map_pid_peerDown_info()
{
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct peer_connect_down_t *peerDownInfoPtr = NULL;

	for(pid_peerDown_info_iter =map_pid_peerDown_info.begin();pid_peerDown_info_iter!= map_pid_peerDown_info.end(); pid_peerDown_info_iter++){
		peerDownInfoPtr=pid_peerDown_info_iter ->second;
		delete peerDownInfoPtr;
	}

	map_pid_peerDown_info.clear();
}

void pk_mgr::clear_map_pid_rescue_peer_info()
{
	map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(map_pid_rescue_peer_info_iter =map_pid_rescue_peer_info.begin();map_pid_rescue_peer_info_iter!= map_pid_rescue_peer_info.end(); map_pid_rescue_peer_info_iter++){
		peerInfoPtr=map_pid_rescue_peer_info_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_rescue_peer_info.clear();
}

void pk_mgr::clear_map_pid_child_peer_info()
{
	multimap<unsigned long, struct peer_info_t *>::iterator map_pid_child_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(map_pid_child_peer_info_iter =map_pid_child_peer_info.begin();map_pid_child_peer_info_iter!= map_pid_child_peer_info.end(); map_pid_child_peer_info_iter++){
		peerInfoPtr=map_pid_child_peer_info_iter ->second;
		delete peerInfoPtr;
	}

	map_pid_rescue_peer_info.clear();
}

void pk_mgr::clear_map_pid_child_peer_info(unsigned long pid,unsigned long manifest)
{
	multimap<unsigned long, struct peer_info_t *>::iterator map_pid_child_peer_info_iter;
	struct peer_info_t *peerInfoPtr =NULL;

	for(map_pid_child_peer_info_iter =map_pid_child_peer_info.begin();map_pid_child_peer_info_iter!= map_pid_child_peer_info.end(); map_pid_child_peer_info_iter++){
		peerInfoPtr=map_pid_child_peer_info_iter ->second;
		if(peerInfoPtr ->pid == pid && peerInfoPtr ->manifest==manifest){
			delete peerInfoPtr;
			map_pid_child_peer_info.erase(map_pid_child_peer_info_iter);
			return;
		}
	}
	return;
}

//回傳cut掉ssNumber 數量的manifestValue
unsigned long pk_mgr::manifestFactory(unsigned long manifestValue,unsigned int ssNumber)
{
	unsigned long afterManifestValue=0;
	unsigned int countss=0;

	for (unsigned long i = 0; i < sub_stream_num; i++) {
		if ((1 << i) & manifestValue) {
			afterManifestValue |= (1 << i) ;
			countss++;
			if (countss == ssNumber) {
				break;
			}
		}

	}
	manifestValue &= (~afterManifestValue);
	
	return manifestValue;
}

//close socket and sent rescue to pk

void pk_mgr::threadTimeout()
{
	debug_printf("thread start  hello\n");
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

	struct peer_connect_down_t *parentPeerPtr=NULL;
	unsigned long parentPid=-1;
	unsigned long testingManifest = 0;
	unsigned long tempManifest=0;
	unsigned long testingSubStreamID = -1 ;
	int sock=-1;

	while(1){

		parentPeerPtr=NULL;
		parentPid=-1;
		testingManifest = 0;
		tempManifest=0;
		testingSubStreamID = -1 ;
		sock=-1;

		threadLock(TIMEOUT_LOCKER,10);

		for(pid_peerDown_info_iter =map_pid_peerDown_info.begin(); pid_peerDown_info_iter!= map_pid_peerDown_info.end() ;pid_peerDown_info_iter++)	{

			parentPeerPtr = pid_peerDown_info_iter ->second;	//get parent peer info 
			parentPid = parentPeerPtr ->peerInfo.pid;			//get parent pid

			map_pid_fd_iter = _peer_ptr ->map_in_pid_fd.find(parentPid);
			if(map_pid_fd_iter!= _peer_ptr ->map_in_pid_fd.end())
				sock=map_pid_fd_iter ->second;					//get parent sock

			//timeout 發生
			if(parentPeerPtr->peerInfo.pid!=PK_PID && parentPeerPtr ->timeOutLastSeq == parentPeerPtr->timeOutNewSeq && parentPeerPtr->peerInfo.manifest!=0){



				//找出所有正在測試的substream
				for(unsigned long i =0  ; i < sub_stream_num;i++){
//					if ((ssDetect_ptr + i) ->isTesting ){
					if(!( check_rescue_state(i,0))){
						testingManifest |= SubstreamIDToManifest(i);
					}
				}

				debug_printf("Pid =%d Time out\n",parentPid);

				pkDownInfoPtr->peerInfo.manifest |= parentPeerPtr->peerInfo.manifest ;
				send_rescueManifestToPK(parentPeerPtr->peerInfo.manifest | testingManifest);

				tempManifest =  parentPeerPtr->peerInfo.manifest ;

				//把先前的連接的PID存起來
				while(tempManifest){
					testingSubStreamID = manifestToSubstreamID (tempManifest);
					//set recue stat true
					if(check_rescue_state(testingSubStreamID,0)){
						set_rescue_state(testingSubStreamID,1);
					}else if (check_rescue_state(testingSubStreamID,2)){
						set_rescue_state(testingSubStreamID,0);
						set_rescue_state(testingSubStreamID,1);
					}else if (check_rescue_state(testingSubStreamID,1)){
						//do nothing
					}
					(ssDetect_ptr + testingSubStreamID) ->previousParentPID = parentPeerPtr ->peerInfo.pid;
					tempManifest &=  (~SubstreamIDToManifest(testingSubStreamID) );
				}


				reSet_detectionInfo();

				_peer_ptr ->data_close(sock ,"time out data_close ",CLOSE_PARENT);
				pid_peerDown_info_iter =map_pid_peerDown_info.begin();
				if(pid_peerDown_info_iter ==map_pid_peerDown_info.end()){
					break;
				}



				//更新timeOutNewSeq
			}else{


				parentPeerPtr ->timeOutLastSeq =parentPeerPtr ->timeOutNewSeq ;

			}

		}

		threadFree(TIMEOUT_LOCKER);
		Sleep(3000);	

	}



}



//int pk_mgr::threadLock(int locker)
void pk_mgr::threadLock(int locker,unsigned long sleepTime)
{

	//Locker Want to LOCK

	//if LOCK
	if(threadLockKey){

		//other LOCK wait until  free
		if(threadLockKey != locker){

			while(1){
				if(threadLockKey == FREE ){
					threadLockKey = locker ;
					break ;
				}
				debug_printf("wait for free");
				Sleep(sleepTime);
			}

		}else if(threadLockKey == locker){
			printf ("Locker Lock  %d!?\n",locker); PAUSE
		}

		//if FREE
	}else{

		threadLockKey = locker ;

	}

	return ;
	//	return Locker;

}


//int pk_mgr::threadFree(int locker)
void pk_mgr::threadFree(int locker)
{

	//Locker Want to FREE

	//if LOCK
	if(threadLockKey){

		//other LOCK wait until  free
		if(threadLockKey != locker){

			debug_printf("Can't FREE other Locker\n");
			PAUSE

		}else if(threadLockKey == locker){
			threadLockKey=FREE;
		}

		//if FREE
	}else{
		//just FERR

	}

	return ;
	//	return locker;

}


void pk_mgr::launchThread(void * arg)
{
	pk_mgr * pk_mgr_ptr = NULL ;

	pk_mgr_ptr = static_cast<pk_mgr *>(arg);
	pk_mgr_ptr ->threadTimeout();

	debug_printf("not go to here\n");
}


unsigned int pk_mgr::rescueNumAccumulate(){

	map<unsigned long, struct peer_info_t *>::iterator iter;
	unsigned long tempManifest = 0;
	unsigned int totalRescueNum = 0;

	for (iter = map_pid_rescue_peer_info.begin(); iter != map_pid_rescue_peer_info.end(); iter++) {
		tempManifest = iter->second->manifest;
		for (unsigned long i=0; i < sub_stream_num; i++) {
			if((1<<i) & tempManifest) {
				totalRescueNum++;
				_log_ptr->write_log_format("s =>u s u s u s u \n", __FUNCTION__, __LINE__,
																   "pid =", iter->first,
																   "manifest =", tempManifest,
																   "substreamID =", i) ;
			}
		}
	}

	debug_printf("sent capacity totalRescueNum = %d \n",totalRescueNum);
	_log_ptr->write_log_format("s =>u s u \n", __FUNCTION__, __LINE__, "sent capacity totalRescueNum =", totalRescueNum);

	return totalRescueNum ;

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



unsigned long  pk_mgr::manifestToSubstreamNum(unsigned long  manifest )
{
	unsigned long substreamNum = 0 ;

	for(unsigned long i = 0 ;i< sub_stream_num ;i++){
		if(manifest & 1<<i){
			substreamNum ++ ;
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
		debug_printf("pk_mgr::chunk_rescueManifestPtr new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "chunk_rescueManifestPtr  new error");
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
// if (oldPID == PK_PID+1 )  沒有舊的parent  for testing stream [QUESTION]
// Send topology to PK
void pk_mgr::send_parentToPK(unsigned long manifestValue,unsigned long oldPID)
{
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	struct update_topology_info *parentListPtr = NULL;
	unsigned long packetlen = 0;
	int i = 0;

	unsigned long count = 0;

	if (oldPID != PK_PID + 1) {
		count ++ ;
	} 

	for (pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter != map_pid_peerDown_info.end(); pid_peerDown_info_iter++) {
		//這個parent 有傳給自己且不是從PK來
		if ((pid_peerDown_info_iter->second->peerInfo.manifest & manifestValue ) && pid_peerDown_info_iter->first !=PK_PID) {
			count ++;
		}
	}

	/*
	if(count == 0){
	debug_printf("parent is PK do nothing\n");
	_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"parent is PK do nothing") ;

	return ;
	}
	*/

	packetlen = count * sizeof (unsigned long ) + sizeof(struct update_topology_info) ;
	parentListPtr = (struct update_topology_info *) new char [packetlen];
	if (!parentListPtr) {
		debug_printf("pk_mgr::parentListPtr new error \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] parentListPtr  new error");
		PAUSE
	}
	memset(parentListPtr, 0, packetlen);

	parentListPtr->header.cmd = CHNK_CMD_TOPO_INFO ;
	parentListPtr->header.length = ( packetlen-sizeof(struct chunk_header_t)) ;	//pkt_buf = payload length
	parentListPtr->header.rsv_1 = REQUEST ;
	parentListPtr->parent_num = count ; 
	parentListPtr->manifest = manifestValue ;

	if (oldPID != PK_PID+1) {
		parentListPtr->parent_pid [i] = oldPID ;
		debug_printf("SSID =%d my old parent = %d  ",manifestToSubstreamID(manifestValue),oldPID);
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "SSID =", manifestToSubstreamID(manifestValue), "my old parent =", oldPID) ;
		i++ ;
	} 

	for (pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter != map_pid_peerDown_info.end(); pid_peerDown_info_iter++) {
		//這個parent 有傳給自己且不是從PK來
		if ((pid_peerDown_info_iter->second->peerInfo.manifest & manifestValue) && pid_peerDown_info_iter->first !=PK_PID) {

			parentListPtr->parent_pid[i] = pid_peerDown_info_iter->first;
			debug_printf("SSID =%d  my new parent = %d  \n", manifestToSubstreamID(manifestValue), pid_peerDown_info_iter->first);
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
															"SSID =", manifestToSubstreamID(manifestValue),
															"my new parent =", pid_peerDown_info_iter ->first) ;

			i++ ;
		}
	}

	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,manifestValue, "s d \n", "Send Topo num :", count);

	_net_ptr->set_blocking(_sock);
	_net_ptr ->send(_sock , (char*)parentListPtr, packetlen , 0) ;
	_net_ptr->set_nonblocking(_sock);

	_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "SEND Parent topology OK!") ;


	delete [] parentListPtr;

	return ;
}

void pk_mgr::reSet_detectionInfo()
{
	pkDownInfoPtr ->timeOutNewSeq =0;
	pkDownInfoPtr ->timeOutLastSeq=0;

	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	//	memset( ssDetect_ptr , 0x00 ,sizeof(struct detectionInfo) * sub_stream_num ); 

	for (unsigned long i = 0; i < sub_stream_num; i++) {
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

	for (pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter!= map_pid_peerDown_info.end(); pid_peerDown_info_iter++) {
		for (int i = 0; i < PARAMETER_M; i++) {
			pid_peerDown_info_iter->second->rescueStatsArry[i] = 0;
		}
	}
}


// handle timeout  connect time out
// 1. Connect Timeout
// 2. Log interval
// 3. Streaming Timeout
// 4. Re-Sync Timeout
void pk_mgr::time_handle()
{
	struct timerStruct new_timer;
	_log_ptr->timerGet(&new_timer) ;
	
	map<unsigned long, manifest_timmer_flag *>::iterator temp_substream_first_reply_peer_iter;

	// If this function is first called
	if (firstIn) {
		_log_ptr->timerGet(&LastTimer);		// Using for streaming timeout
		_log_ptr->timerGet(&sleepTimer);
		_log_ptr->timerGet(&reSynTimer);	// Using for re-Sync timeout
		firstIn = false;
	}

	/////////////////for connect timeout START/////////////////////////////
	for (_peer_ptr->substream_first_reply_peer_iter =_peer_ptr->substream_first_reply_peer.begin(); _peer_ptr->substream_first_reply_peer_iter !=_peer_ptr->substream_first_reply_peer.end(); ) {

		if (_peer_ptr->substream_first_reply_peer_iter->second->connectTimeOutFlag == TRUE) {

			//if(/*_log_ptr ->diffTime_ms(_peer_ptr->substream_first_reply_peer_iter ->second->connectTimeOut,new_timer)*/(new_timer - _peer_ptr->substream_first_reply_peer_iter ->second->connectTimeOut) >= CONNECT_TIME_OUT){
			if (_log_ptr->diff_TimerGet_ms(&(_peer_ptr->substream_first_reply_peer_iter ->second->connectTimeOut), &new_timer) >= CONNECT_TIME_OUT) {

				if (_peer_ptr->substream_first_reply_peer_iter->second->peer_role == 0) {

					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "session manifest timeout", _peer_ptr->substream_first_reply_peer_iter ->second->rescue_manifest) ;
					_peer_ptr->substream_first_reply_peer_iter ->second->connectTimeOutFlag=FALSE ;
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "session_id_stop =", _peer_ptr->substream_first_reply_peer_iter->first);
					_peer_mgr_ptr->_peer_communication_ptr->stop_attempt_connect(_peer_ptr->substream_first_reply_peer_iter->first);

					if ( _peer_mgr_ptr->handle_test_delay(_peer_ptr->substream_first_reply_peer_iter->second->rescue_manifest) > 0) {
						_peer_ptr->substream_first_reply_peer_iter++;	
					}
					//connect all fail 
					else{
						pkSendCapacity = true;
						_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "manifest timeout and all connect fail", _peer_ptr->substream_first_reply_peer_iter ->second->rescue_manifest) ;

						temp_substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer_iter ;
					
						_peer_ptr->substream_first_reply_peer_iter++;

						clear_map_pid_peer_info(temp_substream_first_reply_peer_iter ->second->rescue_manifest);

						delete [] (unsigned char*)temp_substream_first_reply_peer_iter ->second;

						_peer_ptr ->substream_first_reply_peer.erase(temp_substream_first_reply_peer_iter);

						if (_peer_ptr->substream_first_reply_peer_iter == _peer_ptr->substream_first_reply_peer.end()) {
							break;
						}			
					}
				}
				else if (_peer_ptr->substream_first_reply_peer_iter->second->peer_role == 1) {

					_peer_ptr->substream_first_reply_peer_iter->second->connectTimeOutFlag = FALSE ;
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "session_id_stop =", _peer_ptr->substream_first_reply_peer_iter->first);
					_peer_mgr_ptr->_peer_communication_ptr->stop_attempt_connect(_peer_ptr->substream_first_reply_peer_iter->first);

					temp_substream_first_reply_peer_iter = _peer_ptr->substream_first_reply_peer_iter ;
					
					_peer_ptr->substream_first_reply_peer_iter++;

					clear_map_pid_child_peer_info(temp_substream_first_reply_peer_iter->second->pid,temp_substream_first_reply_peer_iter->second->rescue_manifest);
					delete [] (unsigned char*)temp_substream_first_reply_peer_iter ->second;
					_peer_ptr ->substream_first_reply_peer.erase(temp_substream_first_reply_peer_iter);
				
					if(_peer_ptr->substream_first_reply_peer_iter ==_peer_ptr->substream_first_reply_peer.end()){
						break;
					}
				}
				else {
					debug_printf("what !?\n");
					PAUSE
				}
			}
			else {
				_peer_ptr->substream_first_reply_peer_iter++;
			}
		}
		else {
			_peer_ptr->substream_first_reply_peer_iter++;
		}
	}
	/////////////////for connect timeout  END/////////////////////////////

	/*
	this part is used for log period send
	*/
	//LARGE_INTEGER log_now_time;
	struct timerStruct log_now_time;
	_log_ptr->timerGet(&log_now_time);

	if (_logger_client_ptr->log_source_delay_init_flag) {
		if (_log_ptr->diff_TimerGet_ms(&(_logger_client_ptr->log_period_source_delay_start),&log_now_time) > LOG_DELAY_SEND_PERIOD) {
			_logger_client_ptr->log_period_source_delay_start = log_now_time;
			_logger_client_ptr->send_max_source_delay();
		}
	}
	
	if (_logger_client_ptr->log_bw_in_init_flag) {
		if (_log_ptr->diff_TimerGet_ms(&(_logger_client_ptr->log_period_bw_start),&log_now_time) > LOG_BW_SEND_PERIOD) {
			_logger_client_ptr->log_period_bw_start = log_now_time;
			_logger_client_ptr->send_bw();
		}
	}

	////////////////////for streaming timeout  START//////////////////////////////// 
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	
	struct peer_connect_down_t *parent_info = NULL;
	unsigned long parent_pid = -1;
	unsigned long testingManifest = 0;
	unsigned long tempManifest=0;
	unsigned long testingSubStreamID = -1 ;
	unsigned long needrescueManifest=0;
	int sock=-1;
	unsigned long peerTestingManifest=0;

	//if(/*_log_ptr ->diffTime_ms(LastTimer,new_timer)*/(new_timer - LastTimer) >= NETWORK_TIMEOUT){
	if (_log_ptr->diff_TimerGet_ms(&LastTimer, &new_timer) >= NETWORK_TIMEOUT) {

		LastTimer = new_timer ;

		for (pid_peerDown_info_iter = map_pid_peerDown_info.begin(); pid_peerDown_info_iter != map_pid_peerDown_info.end(); ) {

			parent_info = pid_peerDown_info_iter->second;	//get parent peer info 
			parent_pid = parent_info->peerInfo.pid;			//get parent pid

			map_pid_fd_iter = _peer_ptr->map_in_pid_fd.find(parent_pid);
			if (map_pid_fd_iter != _peer_ptr ->map_in_pid_fd.end()) {
				sock = map_pid_fd_iter->second;				//get parent sock
			}
			else {
				debug_printf("map_in_pid_fd not found \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] map_in_pid_fd not found");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] map_in_pid_fd not found \n");
				*(_net_ptr->_errorRestartFlag) = RESTART;
				PAUSE
			}

			//timeout 發生
			if (parent_info->timeOutLastSeq == parent_info->timeOutNewSeq && 
				parent_info->peerInfo.manifest != 0 && 
				parent_info->timeOutLastSeq != 0) {

				
				_log_ptr->write_log_format("s(u) s s u \n", __FUNCTION__, __LINE__, "Rescue triggered.",
															   "timeout. parent pid =", parent_pid);
					
				
				if (parent_pid == PK_PID) {
					debug_printf("PK timeOut \n");
					_log_ptr->write_log_format("\ns =>u s \n", __FUNCTION__,__LINE__,"[PROBLEM] PK timeOut");
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "[DEBUG] PK timeout \n");
					//*(_net_ptr->_errorRestartFlag) =RESTART;
					//PAUSE
					//exit(0);
					pid_peerDown_info_iter++;
				}
				else {
					debug_printf("parent pid %d Time out \n", parent_pid);
					_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Parent-peer TimeOut. pid =", parent_pid) ;
					
					unsigned long pk_old_manifest = pkDownInfoPtr->peerInfo.manifest;
					pkDownInfoPtr->peerInfo.manifest |= parent_info->peerInfo.manifest;
					_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
																	   "PK manifest change from", pk_old_manifest,
																	   "to", pkDownInfoPtr->peerInfo.manifest);
					
					unsigned long parent_old_manifest = parent_info->peerInfo.manifest;
					//tempManifest = parent_info->peerInfo.manifest;
					parent_info->peerInfo.manifest = 0;
					_log_ptr->write_log_format("s(u) s u s u s u \n", __FUNCTION__, __LINE__,
																		   "parent pid", parent_info->peerInfo.pid,
																		   "manifest change from", parent_old_manifest,
																		   "to", parent_info->peerInfo.manifest);				

					reSet_detectionInfo();
					
					pid_peerDown_info_iter++;
					_peer_ptr->data_close(sock, "time out data_close", CLOSE_PARENT);
					
					needrescueManifest = 0;
					
					while (parent_old_manifest) {
						testingSubStreamID = manifestToSubstreamID(parent_old_manifest);

						_logger_client_ptr->log_to_server(LOG_TIME_OUT, (SubstreamIDToManifest(testingSubStreamID)));
						
						// set recue stat true
						// State 0: the substream does not trigger rescue mechanism
						if (check_rescue_state(testingSubStreamID, 0)) {
							set_rescue_state(testingSubStreamID, 1);
							needrescueManifest |= SubstreamIDToManifest(testingSubStreamID);
						}
						// state 1: the substream triggered rescue mechanism
						else if (check_rescue_state(testingSubStreamID, 1)) {
							debug_printf("check_rescue_state(testingSubStreamID,1 parent should only PK  eror\n");
							_log_ptr->write_log_format("s =>u s\n", __FUNCTION__,__LINE__,"check_rescue_state(testingSubStreamID,1 parent should only PK  eror  ");
							*(_net_ptr->_errorRestartFlag) = RESTART;
							PAUSE
							//do nothing
						}
						// state 2: the substream is being tested
						else if (check_rescue_state(testingSubStreamID, 2)) {
							//already start testing stream
							//run testing fail direct from PK server sent topology
							_logger_client_ptr->log_to_server(LOG_TEST_DETECTION_FAIL, SubstreamIDToManifest(testingSubStreamID));
							_logger_client_ptr->log_to_server(LOG_DATA_COME_PK, SubstreamIDToManifest(testingSubStreamID));
							set_rescue_state(testingSubStreamID, 0);
							//set_rescue_state(testingSubStreamID,1);
							(ssDetect_ptr + testingSubStreamID)->isTesting = 0 ; 
							send_parentToPK(SubstreamIDToManifest(testingSubStreamID), PK_PID+1);
						}
						parent_old_manifest &= (~SubstreamIDToManifest(testingSubStreamID) );
					}

					if (needrescueManifest) {
						//找出所有正在測試的substream
						for(unsigned long i = 0; i < sub_stream_num; i++) {
							if (!check_rescue_state(i,0)) {
								testingManifest |= SubstreamIDToManifest(i);
							}
						}

						_logger_client_ptr->log_to_server(LOG_RESCUE_TRIGGER,needrescueManifest);
						
						// The rescue-manifest sent to PK = original substream from PK + testingManifest
						send_rescueManifestToPK(pkDownInfoPtr->peerInfo.manifest | testingManifest);

						if (pid_peerDown_info_iter == map_pid_peerDown_info.end()) {
							break;
						}
					}
					reSet_detectionInfo();
				}
			}
			// update timeOutNewSeq
			else {
				parent_info->timeOutLastSeq = parent_info->timeOutNewSeq;
				pid_peerDown_info_iter++;
			}
		}
	}
	////////////////////for streaming timeout END////////////////////////////////
	
	
	////////////////////for RE-SYN time START////////////////////////////////
	if (_log_ptr->diff_TimerGet_ms(&reSynTimer,&new_timer) >= reSynTime && synLock ==0) {
		debug_printf("_log_ptr ->diff_TimerGet_ms = %u, reSynTime = %u \n", _log_ptr->diff_TimerGet_ms(&reSynTimer, &new_timer), reSynTime);
		send_syn_token_to_pk(_sock);
		reSynTimer =new_timer;
	}
	////////////////////for RE-SYN time END////////////////////////////////


	//to avoid CPU 100%
//	if((new_timer - sleepTimer) >= 1){
//		Sleep(1);
//		sleepTimer=new_timer ;
////		debug_printf("hello\n");
//	}

	static unsigned long runCount = 0;
	runCount++;
	if (runCount %10 ==0) {
		Sleep(1);
	}
}

void pk_mgr::PrintSubstreamInfo()
{
	map<unsigned long, struct peer_info_t>::iterator itr;
	for ( itr=map_substream_peerInfo.begin(); itr!=map_substream_peerInfo.end(); itr++ ) {
		debug_printf("substream %d \n", itr->first);
		debug_printf("parent pid: %d, public IP: %s \n", itr->second.pid, inet_ntoa( *(in_addr*)&(itr->second.public_ip) ));
	}
}









