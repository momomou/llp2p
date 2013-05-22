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



class pk_mgr:public basic_class {



public:


	list<int> *fd_list_ptr;
//	list<int> outside_rescue_list;
	list <int> streamID_list;
	struct peer_connect_down_t *pkDownInfoPtr;
	LARGE_INTEGER start,end;
	volatile int Xcount ;

	LARGE_INTEGER teststart,testend;

	multimap <unsigned long, struct peer_info_t *> map_pid_peer_info; 	// <pid, struct peer_info_t *>
	map<unsigned long, struct peer_info_t *> map_pid_rescue_peer_info;		// <pid, struct peer_info_t *>
	map<unsigned long, struct peer_connect_down_t *> map_pid_peerDown_info ; //// <pid, struct peer_connect_down_t *>

	struct chunk_level_msg_t *level_msg_ptr ;
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
	unsigned long inside_lane_rescue_num;
	unsigned long outside_lane_rescue_num;

	unsigned long current_child_manifest;
	unsigned long full_manifest;
	int _sock; 		//PK socket

	////measure
	logger *_log_measureDataPtr ;


	unsigned long syn_round_time;
	struct syn_struct syn_table;
	void syn_table_init(int pk_sock);
	void send_syn_token_to_pk(int pk_sock);
	void syn_recv_handler(struct syn_token_receive* syn_struct_back_token);

	map<unsigned long, struct source_delay *> delay_table;
	void delay_table_init();
	void source_delay_detection(int sock, unsigned long sub_id, unsigned int seq_now);
	void reset_source_delay_detection(unsigned long sub_id);
	void set_rescue_state(unsigned long sub_id,int state);
	int check_rescue_state(unsigned long sub_id,int state);

	int peer_start_delay_count;
	void send_capacity_init();
	void send_capacity_to_pk(int sock);

	
	volatile unsigned int _least_sequence_number;		//最新的seq
	volatile unsigned int _current_send_sequence_number; //最後送給player的seq(還沒送)

	unsigned long stream_number;	//channel 下stream的個數
	map<int, struct update_stream_header *> map_streamID_header;
	
	pk_mgr(unsigned long html_size, list<int> *fd_list, network *net_ptr , logger *log_ptr , configuration *prep);
	~pk_mgr();

	void init();
	int build_connection(string ip, string port); 
	int handle_register(string svc_tcp_port, string svc_udp_port);
	void peer_mgr_set(peer_mgr *peer_mgr_ptr);
	
	void add_stream(int strm_addr, stream *strm, unsigned strm_type);
	void del_stream(int strm_addr, stream *strm, unsigned strm_type);

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

	//可獨立的unitl
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

	void peer_set(peer *peer_ptr);
	void rtsp_viewer_set(rtsp_viewer *rtsp_viewer_ptr);
	void time_handle();

//	void rtmp_sock_set(int sock);

private:
	

	unsigned long _html_size;

	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	rtsp_viewer *_rtsp_viewer_ptr;
	
	FILE *pkmgrfile_ptr ;
	FILE *performance_filePtr ;

	unsigned long _manifest;
	bool pkSendCapacity;

	map<int, stream *> _map_stream_media;	// <strm_addr, stream *>
	map<int, stream *>::iterator _map_stream_iter;	// <strm_addr, stream *>
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

};

#endif
























