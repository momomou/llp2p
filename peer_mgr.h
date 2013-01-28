#ifndef __PEER_MGR_H__
#define __PEER_MGR_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network;
class logger;
class pk_mgr;
class peer;

class peer_mgr:public basic_class {
public:

	peer *peer_ptr;
	list<int> *fd_list_ptr;
//	list<unsigned long> rescue_pid_list;
//	map<int, int> map_rescue_fd_count;	// <fd, count>
//	map<int, unsigned long> map_fd_pid;	// <fd, pid>
//	map<int, queue<struct chunk_t *> *> _map_fd_downstream;		// <downstream_fd, downstream_out_data_queue>
	unsigned long self_pid;
	
	peer_mgr(list<int> *fd_list);
	~peer_mgr();
	void peer_mgr_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr * pk_mgr_ptr);
	//void pk_mgr_set(pk_mgr * pk_mgr_ptr);
	
	void connect_peer(struct chunk_level_msg_t *level_msg_ptr, unsigned long pid);
	
//	int connect_other_lane_peer(struct chunk_rescue_list_reply_t *rescue_list_reply_ptr, unsigned long peer_list_member, unsigned long pid, unsigned long outside_lane_rescue_num);
	int build_connection(struct level_info_t *level_info_ptr, unsigned long pid);

//	void send_bandwidth(unsigned long pid, unsigned long avg_bit_rate);
	void send_rescue(unsigned long pid, unsigned long self_pid, unsigned long manifest);
	void handle_cut_peer(unsigned long pid, int sock);
	void send_cut_peer(unsigned long pid, int sock);
	void rescue_reply(unsigned long pid, unsigned long manifest);
	void add_downstream(unsigned long pid, struct chunk_t *chunk_ptr);
//	void add_rescue_downstream(unsigned long pid);
	void add_rescue_fd(unsigned long pid);
	void cut_rescue_peer(int sock);
	void del_rescue_downstream();
	void cut_rescue_downstream(unsigned long pid);
    void set_up_public_ip(struct chunk_level_msg_t *level_msg_ptr);
    void clear_ouput_buffer(unsigned long pid);
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	
	///2013/01/23
	void send_test_delay(int _sock);
	void handle_test_delay();
	void send_manifest_to_parent(unsigned long manifestValue,unsigned long parentPid);
	void handle_manifestSet(struct chunk_manifest_set_t *chunk_ptr);

	void data_close(int cfd, const char *reason); 

private:
	
	int _sock;
	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	pk_mgr *_pk_mgr_ptr;
	unsigned long _peer_list_member;
    unsigned long self_public_ip;

	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

	struct sockaddr_in _sin, _cin;
	

};

#endif






























