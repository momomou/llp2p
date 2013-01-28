#ifndef __PEER_H__
#define __PEER_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network;
class logger;
class pk_mgr;
class peer_mgr;

class peer:public basic_class {
public:

//	unsigned long count;
//	unsigned long avg_bandwidth;
//    unsigned long parent_bandwidth;
	bool first_reply_peer;

	//unsigned long long bandwidth_bucket[BANDWIDTH_BUCKET];

	queue<struct chunk_t *> *queue_out_ctrl_ptr;
	queue<struct chunk_t *> *queue_out_data_ptr;
	
	map<int, queue<struct chunk_t *> *> map_fd_out_ctrl;	// <fd, queue of chunk pointer which store outgoing control packet(chunk) >
	map<int, queue<struct chunk_t *> *> map_fd_out_data;		// <fd, queue of chunk pointer which store outgoin data packet(chunk) >
	map<unsigned long, int> map_in_pid_fd;
	map<unsigned long, int> map_out_pid_fd;
	map<int , unsigned long> map_fd_pid;

	list<int> *fd_list_ptr;

	peer(list<int> *fd_list);
	~peer();
	void peer_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr *pk_mgr_ptr, peer_mgr *peer_mgr_ptr);
	void handle_connect(int sock, struct chunk_t *chunk_ptr, struct sockaddr_in cin);
	int handle_connect_request(int sock, struct level_info_t *level_info_ptr, unsigned long pid);
	void clear_map();
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();



	void data_close(int cfd, const char *reason); 

private:
	
	int _fd;
	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	pk_mgr * _pk_mgr_ptr;
	peer_mgr * _peer_mgr_ptr;
	int _send_byte;
	int _expect_len;
	int _offset;
//	int _time_start;
//	unsigned long _recv_byte_count;
    unsigned long _recv_parent_byte_count;
    unsigned long parent_manifest;

	list<int>::iterator fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator map_fd_queue_iter;
	map<int , unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;
	map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;

	struct peer_info_t *peerInfoPtr ;
	struct peer_connect_down_t *peerDownInfoPtr;

	struct chunk_t *_chunk_ptr;
	struct sockaddr_in _sin, _cin;
//	struct timeb interval_time;	//--!! 0215

};

#endif





























