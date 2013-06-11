#ifndef __NAT_INTERFACE_H__
#define __NAT_INTERFACE_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network;
class logger;
class peer_mgr;
class peer;
class pk_mgr;
class peer_communication;
class io_nat_punch;

class nat_interface:public basic_class{
public:
	nat_interface(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr);
	~nat_interface();

	void nat_register(unsigned long self_pid);
	void nat_stop(unsigned long stop_session_id);
	void nat_start(int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id);

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	map<int, int> map_nat_fd_unknown;
	map<int, int>::iterator map_nat_fd_unknown_iter;

	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	pk_mgr * _pk_mgr_ptr;
	io_nat_punch * _io_nat_punch_ptr;
	peer_communication *_peer_communication_ptr;
};

#endif