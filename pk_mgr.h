#ifndef __PK_MGR_H__
#define __PK_MGR_H__

#include "common.h"
#include "basic_class.h"
#include "stream_udp.h"
#include <iostream>
#include <map>

class network;
class logger;
class peer_mgr;
class rtsp_viewer;
class stream;
class peer;
class logger_client;
class stunt_mgr;


class pk_mgr:public basic_class {



public:
	FILE *fp;
	list<int> *fd_list_ptr;
//	list<int> outside_rescue_list;
	list <int> streamID_list;
	struct peer_connect_down_t *pkDownInfoPtr;
	struct timerStruct start,end;
	volatile unsigned long Xcount;						// Number of average received packets per unit time
	unsigned long totalMod ;
	unsigned long reSynTime;
	struct timerStruct lastSynStartclock;
	unsigned long pkt_count ;			// Ω XCOUNT_INTERVAL 丁ずΜchunk计秖(Τ筁耾筁)
	unsigned long totalbyte;
	int synLock;
	unsigned char exit_code;		// Error code (for program exit)

	unsigned long first_timestamp;		// 材Μtimestampd
	bool firstIn;
	struct timerStruct LastTimer;
	struct timerStruct sleepTimer;
	struct timerStruct reSynTimer;
	struct timerStruct XcountTimer;
	struct timerStruct programStartTimer;

//	LARGE_INTEGER teststart,testend;
//	LARGE_INTEGER syn_round_start;
	

	//temp parent 
	multimap <unsigned long, struct peer_info_t *> map_pid_peer_info;
	//temp child
	multimap <unsigned long, struct peer_info_t *> map_pid_child_peer_info;

	map<unsigned long, struct peer_info_t *> map_pid_rescue_peer_info;		 // real child-peer
	map<unsigned long, struct peer_connect_down_t *> map_pid_peerDown_info ; // real parent-peer

	//about player  ,delete by bit_stream_server
	map<int, stream *> _map_stream_fd_stream;					// <stream_fd, stream *>
	map<int, stream *>::iterator _map_stream_fd_stream_iter;	// <stream_fd, stream *>

	//substreamID,delay
	map<unsigned long, struct source_delay *> delay_table;

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

	unsigned long public_ip;
	unsigned short my_private_port;
	unsigned long inside_lane_rescue_num;
	unsigned long outside_lane_rescue_num;

	unsigned long current_child_manifest;
	unsigned long full_manifest;
	int _sock; 		//PK socket


	// Variables of synchronization
	struct timerStruct syn_round_start;
	unsigned long syn_round_time;
	struct syn_struct syn_table;



	void syn_table_init(int pk_sock);
	void send_syn_token_to_pk(int pk_sock);
	void syn_recv_handler(struct syn_token_receive* syn_struct_back_token);


	// For debug
	void PrintSubstreamInfo();

	void delay_table_init();
	void source_delay_detection(int sock, unsigned long sub_id, unsigned int seq_now);
	void quality_source_delay_count(int sock, unsigned long substream_id, unsigned int seq_now);
	void reset_source_delay_detection(unsigned long sub_id);
	void set_rescue_state(unsigned long sub_id,int state);
	int check_rescue_state(unsigned long sub_id,int state);

	void set_parent_manifest(struct peer_connect_down_t* parent_info, UINT32 manifest);

	int peer_start_delay_count;		// If received first packet of each substream, peer_start_delay_count++
	void send_capacity_init();
	void send_capacity_to_pk(int sock);

	
	volatile unsigned int _least_sequence_number;			//Μヘ玡ゎ程穝seq
	volatile unsigned int _current_send_sequence_number;	//程癳倒playerseq(临⊿癳)

	unsigned long stream_number;	//channel stream计
	
	pk_mgr(unsigned long html_size, list<int> *fd_list, network *net_ptr , logger *log_ptr , configuration *prep , logger_client * logger_client_ptr, stunt_mgr *stunt_mgr);
	~pk_mgr();

	void init(unsigned short ptop_port);
	int build_connection(string ip, string port); 
	int handle_register(unsigned short ptop_port, string svc_udp_port);
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


///new rescue function
	//	void handleAppenSelfdPid(struct chunk_t *chunk_ptr );
	//	void storeChildrenToSet(struct chunk_t *chunk_ptr );
	void rescue_detecion(struct chunk_t *chunk_ptr);
	void init_rescue_detection();
	void measure();
	void send_rescueManifestToPK(unsigned long manifestValue);
	unsigned long manifestFactory(unsigned long manifestValue,unsigned int ssNumber);

	unsigned int rescueNumAccumulate();
	void send_rescueManifestToPKUpdate(unsigned long manifestValue);
	void send_parentToPK(unsigned long manifestValue ,unsigned long oldPID);
	void reSet_detectionInfo();

	//縒ミunitl
	unsigned long manifestToSubstreamID(unsigned long  manifest );
	unsigned long SubstreamIDToManifest(unsigned long  SubstreamID );
	unsigned long manifestToSubstreamNum(unsigned long  manifest );

	volatile int threadLockKey ;
	void threadTimeout();
	static void launchThread(void * arg);
	void threadLock(int locker,unsigned long sleepTime);
	void threadFree(int locker);


//clear
	void clear_map_pid_peer_info();
	void clear_map_pid_peer_info(unsigned long manifest);
	void clear_map_pid_peerDown_info();
	void clear_map_pid_rescue_peer_info();
	void clear_map_pid_child_peer_info(unsigned long pid,unsigned long manifest);
	void clear_map_pid_child_peer_info();

	void clear_delay_table();
	void clear_map_streamID_header();

	void peer_set(peer *peer_ptr);
	void rtsp_viewer_set(rtsp_viewer *rtsp_viewer_ptr);
	void time_handle();

//	void rtmp_sock_set(int sock);

private:
	

	unsigned long _html_size;

	stunt_mgr *_stunt_mgr_ptr;
	logger_client * _logger_client_ptr;
	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	rtsp_viewer *_rtsp_viewer_ptr;
	
	FILE *pkmgrfile_ptr ;
	FILE *performance_filePtr ;

	unsigned long my_pid;
	unsigned long my_level;
	unsigned long _manifest;
	
	
	bool pkSendCapacity;
	unsigned long lastPKtimer;
	int sentStartDelay;

	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

};

#endif
























