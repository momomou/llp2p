#ifndef __IO_NONBLOCKING_H__
#define __IO_NONBLOCKING_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class peer_communication;
class logger_client;
class network;
class logger;


class io_nonblocking:public basic_class{
public:
	io_nonblocking(network *net_ptr,logger *log_ptr ,peer_communication* peer_communication_ptr, logger_client * logger_client_ptr );
	~io_nonblocking();

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	void data_close(int sock);

	peer_communication *_peer_communication_ptr;
	logger_client * _logger_client_ptr;
	network *_net_ptr;
	logger *_log_ptr;
};

#endif