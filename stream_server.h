#ifndef __STREAM_SERVER_H__
#define __STREAM_SERVER_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network;
class logger;

class stream_server:public basic_class {
public:

	stream_server();
	~stream_server();
	void stream_server_set(network *net_ptr , logger *log_ptr , configuration *prep);
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	void data_close(int cfd, const char *reason); 

private:
	
	int _fd;
	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;

	struct sockaddr_in _sin, _cin;

};

#endif


























