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
//	list<struct level_info_t *> rescue_list ;
	list<int> outside_rescue_list;
//	list<unsigned int> sequence_number_list;
	list <int> streamID_list;
	struct peer_connect_down_t *pkDownInfoPtr;

	set< int > *childrenSet_ptr;
	map<unsigned long ,set< int > *> map_streamID_childrenSet ;

//	map<unsigned long, unsigned long> map_pid_manifest;

	multimap <unsigned long, struct peer_info_t *> map_pid_peer_info; 	// <pid, struct peer_info_t *>
	map<unsigned long, struct peer_info_t *> map_pid_rescue_peer_info;		// <pid, struct peer_info_t *>
	map<unsigned long, struct peer_connect_down_t *> map_pid_peerDown_info ; //// <pid, struct peer_connect_down_t *>

    map<unsigned char, int> map_rtmp_chunk_size;
//    map<string, unsigned char> map_stream_name_id;
		
	struct chunk_level_msg_t *level_msg_ptr ;
//	struct chunk_rescue_list_reply_t *rescue_list_reply_ptr;
	unsigned long lane_member;
//	unsigned long peer_list_member;
	struct chunk_bitstream_t *_chunk_bitstream;

	//rescue
	struct detectionInfo *ssDetect_ptr;
	int *statsArryCount_ptr ;

//	int _current_pos;
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

	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
	long long syn_round_time;
	struct syn_struct syn_table;
	void syn_table_init(int pk_sock);
	void send_syn_token_to_pk(int pk_sock);
	void syn_recv_handler(struct syn_token_receive* syn_struct_back_token);
	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

	//////////////////////////////////////////////////////////////////////////////////measure start delay
	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
	map<unsigned long, struct source_delay *> delay_table;
	void delay_table_init();
	void source_delay_detection(int sock, unsigned long sub_id, unsigned int seq_now);
	//void source_delay_init(unsigned long init_ssid);
	//void send_start_delay_measure_token(int sock,unsigned long sub_id);
	//void send_back_start_delay_measure_token(int sock,long long peer_start_delay,unsigned long sub_id);
	//////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

	//////////////////////////////////////////////////////////////////////////////////2/20 start delay update
	//void send_start_delay_update(int sock, unsigned long start_delay_manifest, int start_delay_differ);
	//////////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////send capacity
	int peer_start_delay_count;
	//int peer_join_send;
	void send_capacity_init();
	void send_capacity_to_pk(int sock);
	//////////////////////////////////////////////////////////////////////////////////
	
	volatile unsigned int _least_sequence_number;		//最新的seq
	volatile unsigned int _current_send_sequence_number; //最後送給player的seq(還沒送)

	unsigned long stream_number;	//channel 下stream的個數
	
	
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
	void send_request_sequence_number_to_pk(unsigned int req_from, unsigned int req_to);
    void send_pkt_to_pk(struct chunk_t *chunk_ptr);
//	void handle_rescue(unsigned long pid, unsigned long manifest);
//    void handle_latency(struct chunk_t *chunk_ptr, int sockfd);
	void handle_stream(struct chunk_t *chunk_ptr, int sockfd);
//    void store_stream_id_map(char user_name[], unsigned char stream_id);
	void data_close(int cfd, const char *reason); 
	int get_sock(void);



///new rescue function
	void rescue_detecion(struct chunk_t *chunk_ptr);
	void init_rescue_detection();
	void measure();
	void send_rescueManifestToPK(unsigned long manifestValue);
	unsigned long manifestFactory(unsigned long manifestValue,unsigned int ssNumber);
	void threadTimeout();
	static void launchThread(void * arg);
	unsigned int rescueNumAccumulate();
	void handleAppenSelfdPid(struct chunk_t *chunk_ptr );
	void storeChildrenToSet(struct chunk_t *chunk_ptr );
	void send_rescueManifestToPKUpdate(unsigned long manifestValue);
	void send_parentToPK(unsigned long manifestValue ,unsigned long oldPID);
	//可獨立的unitl
	unsigned long manifestToSubstreamID(unsigned long  manifest );
	unsigned long SubstreamIDToManifest(unsigned long  SubstreamID );
	void reSet_detectionInfo();

//clear
	void clear_map_pid_peer_info();
	void clear_map_pid_peer_info(unsigned long manifest);
	void clear_map_pid_peerDown_info();
	void clear_map_pid_rescue_peer_info();



	void peer_set(peer *peer_ptr);
	void rtsp_viewer_set(rtsp_viewer *rtsp_viewer_ptr);

//	void rtmp_sock_set(int sock);

private:
	

	unsigned long _html_size;

	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	rtsp_viewer *_rtsp_viewer_ptr;
//	int _rtmp_sock;
//	int _time_start;
//    int pkt_resent_count;


//	unsigned long _recv_byte_count;
	unsigned long _manifest;
//	unsigned long _check;

//	struct timeb interval_time;	//--!! 0215
	
//	map<int, stream *> _map_stream_audio;	// <strm_addr, stream *>
//	map<int, stream *> _map_stream_video;	// <strm_addr, stream *>
	map<int, stream *> _map_stream_media;	// <strm_addr, stream *>
	map<int, stream *>::iterator _map_stream_iter;	// <strm_addr, stream *>
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

};

#endif
























