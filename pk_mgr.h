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

class pk_mgr:public basic_class {
public:

	list<int> *fd_list_ptr;
	list<struct level_info_t *> rescue_list ;
	list<int> outside_rescue_list;
	list<unsigned int> sequence_number_list;

	map<unsigned long, unsigned long> map_pid_manifest;
    map<unsigned char, int> map_rtmp_chunk_size;
    map<string, unsigned char> map_stream_name_id;
		
	struct chunk_level_msg_t *level_msg_ptr ;
	struct chunk_rescue_list_reply_t *rescue_list_reply_ptr;
	unsigned long lane_member;
	unsigned long peer_list_member;
	struct chunk_rtp_t *_chunk_rtp;
	int _current_pos, _bucket_size;
	unsigned long _channel_id;
	unsigned long bit_rate;
	unsigned long sub_stream_num;
	unsigned long parallel_rescue_num;
	unsigned long inside_lane_rescue_num;
	unsigned long outside_lane_rescue_num;
	unsigned long count;
	unsigned long avg_bandwidth;
	unsigned long current_child_pid;
	unsigned long current_child_manifest;
	unsigned int _least_sequence_number;
	
	map<unsigned long, struct peer_info_t *> map_pid_peer_info;		// <pid, struct peer_info_t *>
	map<unsigned long, struct peer_info_t *> map_pid_rescue_peer_info;		// <pid, struct peer_info_t *>
	
	//unsigned long long bandwidth_bucket[BANDWIDTH_BUCKET];
	
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

	void handle_bandwidth(unsigned long avg_bit_rate);
	void send_rescue(unsigned long manifest);
	void send_rescue_to_pk();
	void send_rescue_to_upstream(unsigned long manifest);
	void send_request_sequence_number_to_pk(unsigned int req_from, unsigned int req_to);
    void send_pkt_to_pk(struct chunk_t *chunk_ptr);
	void handle_rescue(unsigned long pid, unsigned long manifest);
    void handle_latency(struct chunk_t *chunk_ptr, int sockfd);
	void handle_stream(struct chunk_t *chunk_ptr, int sockfd);
    void store_stream_id_map(char user_name[], unsigned char stream_id);
	void data_close(int cfd, const char *reason); 
	int get_sock(void);

	void rtsp_viewer_set(rtsp_viewer *rtsp_viewer_ptr);
	void rtmp_sock_set(int sock);

private:

	int _sock;
	unsigned long _html_size;

	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	rtsp_viewer *_rtsp_viewer_ptr;
	int _rtmp_sock;
	int _time_start;
    int pkt_resent_count;

	unsigned int _current_send_sequence_number;
	unsigned long _recv_byte_count;
	unsigned long _manifest;
	unsigned long _check;

	struct timeb interval_time;	//--!! 0215
	
	map<int, stream *> _map_stream_audio;	// <strm_addr, stream *>
	map<int, stream *> _map_stream_video;	// <strm_addr, stream *>
	map<int, stream *> _map_stream_media;	// <strm_addr, stream *>
	map<int, stream *>::iterator _map_stream_iter;	// <strm_addr, stream *>

};

#endif
























