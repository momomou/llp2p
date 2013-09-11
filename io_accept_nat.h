#ifndef __IO_ACCEPT_NAT_H__
#define __IO_ACCEPT_NAT_H__

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
class logger_client;
class stunt_mgr;

class io_accept_nat:public basic_class{
public:
	io_accept_nat(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr , logger_client * logger_client_ptr, stunt_mgr *stunt_mgr_ptr);
	~io_accept_nat();

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	struct sockaddr_in _cin;

	//map<int, int> map_fd_unknown;
	//map<int, int>::iterator map_fd_unknown_iter;
	list<int> map_fd_unknown;

	logger_client * _logger_client_ptr;
	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	pk_mgr * _pk_mgr_ptr;
	peer_communication *_peer_communication_ptr;
	stunt_mgr *_stunt_mgr_ptr;
};

#endif