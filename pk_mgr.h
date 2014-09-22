#ifndef __PK_MGR_H__
#define __PK_MGR_H__

#include "common.h"
#include "basic_class.h"
#include "stream_udp.h"
#include <iostream>
#include <map>

class network;
class network_udp;
class logger;
class peer_mgr;
class rtsp_viewer;
class stream;
class peer;
class logger_client;
class stunt_mgr;


class pk_mgr:public basic_class {



public:
	FILE *record_file_fp;		// For save the file
	bool first_pkt;				// Flag for check keyframe
	list<int> *fd_list_ptr;
//	list<int> outside_rescue_list;
	list <int> streamID_list;
	struct peer_connect_down_t *pkDownInfoPtr;
	struct timerStruct start;
	struct timerStruct end;
	struct timerStruct reconnect_timer;	// Test for reconnect
	volatile unsigned long Xcount;		// Number of average received packets per unit time. Xcount:一秒鐘收到的packet數量
	unsigned long totalMod ;
	unsigned long reSynTime;
	struct timerStruct lastSynStartclock;
	unsigned long pkt_count ;			// 一次 XCOUNT_INTERVAL 時間內收到的chunk數量(有過濾過)
	unsigned long totalbyte;
	int syncLock;					// set 1 if send sync token to pk and not yet receive the response
	unsigned char exit_code;		// Error code (for program exit)

	UINT32 first_timestamp;		// 第一個收到的封包的timestampd
	bool firstIn;
	struct timerStruct LastTimer;
	struct timerStruct sleepTimer;
	struct timerStruct reSynTimer;
	struct timerStruct XcountTimer;
	struct timerStruct programStartTimer;

//	LARGE_INTEGER teststart,testend;
//	LARGE_INTEGER syn_round_start;
	
	// The main table storing peer state
	multimap <unsigned long, struct peer_info_t *> map_pid_parent_temp;		// My temp parent (life: session start <-----> session stop)
	multimap <unsigned long, struct peer_info_t *> map_pid_child_temp;		// My temp child
	map<unsigned long, struct peer_connect_down_t *> map_pid_parent;		// My real parent-peer (第一個回test reply的peer會塞進去) (life: session start <-----> session stop)
	map<unsigned long, struct peer_info_t *> map_pid_child;					// My real child-peer (life: session start <-----> session stop)
	map<unsigned long, struct peer_info_t *> map_pid_child2;				// My real child-peer ????

	// These two tables are to prevent more than 2 connections (upstream + downstream)
	map<unsigned long, int> parents_table;									// <pid, state> table, to prevent too much connections(no more than 2)
	map<unsigned long, int> children_table;									// <pid, state> table, to prevent too much connections(no more than 2)

	//about player  ,delete by bit_stream_server
	map<int, stream *> _map_stream_fd_stream;					// <stream_fd, stream *>
	map<int, stream *>::iterator _map_stream_fd_stream_iter;	// <stream_fd, stream *>

	//substreamID,delay
	map<unsigned long, struct source_delay *> delay_table;

	//substreamID, substream info
	map<unsigned long, struct substream_info *> ss_table;

	//streamID , media_header
	map<int, struct update_stream_header *> map_streamID_header;

	// substreamID, peer-info
	map<unsigned long, struct peer_info_t> map_substream_peerInfo;

	struct chunk_level_msg_t *level_msg_ptr;
	unsigned long lane_member;

//	struct chunk_bitstream_t *_chunk_bitstream;
	struct chunk_t ** buf_chunk_t;

	//rescue
	struct detectionInfo *ssDetect_ptr;
	unsigned long *statsArryCount_ptr ;

	int	_bucket_size;
	unsigned long _channel_id;
	unsigned long bit_rate;
	unsigned long sub_stream_num;
	unsigned long pkt_rate;

	unsigned long inside_lane_rescue_num;
	unsigned long outside_lane_rescue_num;

	unsigned long current_child_manifest;
	unsigned long full_manifest;
	int _sock; 		//PK socket


	// Variables of synchronization
	struct timerStruct syn_round_start;
	int syn_round_time;
	struct syn_struct syn_table;



	void syn_table_init(int pk_sock);
	void send_syn_token_to_pk(int pk_sock);
	void syn_recv_handler(struct syn_token_receive* syn_struct_back_token);
	void delay_table_init();
	void SubstreamTableInit();
	void source_delay_detection(int sock, unsigned long sub_id, unsigned int seq_now);
	void SourceDelayDetection(int sock, unsigned long sub_id, unsigned int seq_now);
	void quality_source_delay_count(int sock, unsigned long substream_id, unsigned int seq_now);	// Calculation of quality and source-delay
	void reset_source_delay_detection(unsigned long sub_id);
	void set_rescue_state(unsigned long sub_id,int state);
	int check_rescue_state(unsigned long sub_id,int state);
	void SetSubstreamState(unsigned long ss_id, int state);
	void SetSubstreamParent(unsigned long manifest, unsigned long pid);		// Set new selected parent in these substream
	void SetSubstreamBlockRescue(unsigned long ss_id, unsigned long type);
	void SetSubstreamUnblockRescue(unsigned long ss_id);

	void set_parent_manifest(struct peer_connect_down_t* parent_info, UINT32 manifest);

	int peer_start_delay_count;		// If received first packet of each substream, peer_start_delay_count++
	void send_capacity_init();
	void send_capacity_to_pk(int sock);
	void send_source_delay(int sock);		// Send source-delay info to pk
	void send_topology_to_log();			// Send topology to log server
	void SendParentTestToPK(unsigned long session_id);

	volatile UINT32 _least_sequence_number;			//收到目前為止最新的seq
	volatile UINT32 _current_send_sequence_number;	//最後送給player的seq(還沒送)

	unsigned long stream_number;	//channel 下stream的個數
	
	pk_mgr(unsigned long html_size, list<int> *fd_list, network *net_ptr,  network_udp *net_udp_ptr , logger *log_ptr , configuration *prep , logger_client * logger_client_ptr, stunt_mgr *stunt_mgr);
	~pk_mgr();

	void init(unsigned short ptop_port, unsigned short ptop_udp_port);
	int build_connection(string ip, string port); 
	int handle_register(unsigned short ptop_port, unsigned short svc_udp_port);
	void peer_mgr_set(peer_mgr *peer_mgr_ptr);
	
	void add_stream(int stream_fd, stream *strm, unsigned strm_type);
	void del_stream(int stream_fd, stream *strm, unsigned strm_type);

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

//	void handle_bandwidth(unsigned long avg_bit_rate);
//	void send_rescue(unsigned long manifest);
//	void send_rescue_to_pk();
//	void send_rescue_to_upstream(unsigned long manifest);
//	void handle_rescue(unsigned long pid, unsigned long manifest);
//    void handle_latency(struct chunk_t *chunk_ptr, int sockfd);
//    void store_stream_id_map(char user_name[], unsigned char stream_id);
	void data_close(int cfd, const char *reason); 
	int get_sock(void);
	void send_request_sequence_number_to_pk(unsigned int req_from, unsigned int req_to);
    void send_pkt_to_pk(struct chunk_t *chunk_ptr);


	void handle_stream(struct chunk_t *chunk_ptr, int sockfd);
	void HandleStream(struct chunk_t *chunk_ptr, int sockfd);
	void handle_kickout(struct chunk_t *chunk_ptr, int sockfd);
	void handle_error(int exit_code, const char *msg, const char *func, unsigned int line);

///new rescue function
	//	void handleAppenSelfdPid(struct chunk_t *chunk_ptr );
	//	void storeChildrenToSet(struct chunk_t *chunk_ptr );
	void rescue_detecion(struct chunk_t *chunk_ptr);
	void RescueDetecion(struct chunk_t *chunk_ptr);
	void init_rescue_detection();
	void measure();
	void Measure();
	void send_rescueManifestToPK(unsigned long manifestValue, bool need_source);
	unsigned long manifestFactory(unsigned long manifest, int ss_num);

	unsigned int rescueNumAccumulate();
	void send_rescueManifestToPKUpdate(unsigned long manifestValue);
	void send_parentToPK(unsigned long manifest, unsigned long parent_pid);
	void reSet_detectionInfo();
	void ResetDetection(unsigned long ss_id);
	void ResetDetection();
	void NeedSourceDecision(bool *need_source);

	// Record file
	void record_file(chunk_t *chunk_ptr);
	void record_file_init(int stream_id);

	//可獨立的unitl
	unsigned long manifestToSubstreamID(unsigned long  manifest );
	unsigned long SubstreamIDToManifest(unsigned long  SubstreamID );
	unsigned long manifestToSubstreamNum(unsigned long  manifest );

	void HandleRescueManifest(unsigned long rescue_manifest, unsigned long *manifest_A, unsigned long *manifest_B);
	void ArrangeResource();

//clear
	void clear_map_pid_parent_temp();
	void clear_map_pid_parent_temp(unsigned long manifest);
	void clear_map_pid_parent();
	void clear_map_pid_child1();
	void clear_map_pid_child_temp(unsigned long pid,unsigned long manifest);
	void clear_map_pid_child_temp();

	void clear_delay_table();
	void ClearSubstreamTable();
	void clear_map_streamID_header();

	void peer_set(peer *peer_ptr);
	void rtsp_viewer_set(rtsp_viewer *rtsp_viewer_ptr);
	void time_handle();

//	void rtmp_sock_set(int sock);

	bool pkSendCapacity;
	unsigned long my_pid;
	unsigned long my_level;
	unsigned long _manifest;
	unsigned long my_public_ip;
	unsigned short my_public_port;
	unsigned long my_private_ip;
	unsigned short my_private_port;

	int session_id;

private:
	

	unsigned long _html_size;

	stunt_mgr *_stunt_mgr_ptr;
	logger_client * _logger_client_ptr;
	network *_net_ptr;
	network_udp *_net_udp_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	rtsp_viewer *_rtsp_viewer_ptr;
	
	
	unsigned long lastPKtimer;		// Record the time that pk receives peer's sync token
	bool sentStartDelay;
	bool first_legal_pkt_received;

	//map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

};

#endif
