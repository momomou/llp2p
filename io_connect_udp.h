#ifndef __IO_CONNECT_UDP_H__
#define __IO_CONNECT_UDP_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network_udp;
class logger;
class peer_mgr;
class peer;
class pk_mgr;
class peer_communication;
class logger_client;

class io_connect_udp:public basic_class{
public:
	io_connect_udp(network_udp *net_udp_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr, logger_client * logger_client_ptr);
	~io_connect_udp();

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_in_udp(int sock);
	virtual int handle_pkt_out(int sock);
	virtual int handle_pkt_out_udp(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_pkt_error_udp(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	void data_close(int cfd, const char *reason) ;

	logger_client * _logger_client_ptr;
	network_udp *_net_udp_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	pk_mgr * _pk_mgr_ptr;
	peer_communication *_peer_communication_ptr;
};

#endif