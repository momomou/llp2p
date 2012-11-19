#ifndef _STREAM_HANDLER_UDP_H_
#define _STREAM_HANDLER_UDP_H_
#include "common.h"
#include "stream_handler.h"

class network;
class logger;
class stream;

class stream_handler_udp:public stream_handler {
public:
	stream_handler_udp();
	~stream_handler_udp();

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	void add_stream(unsigned long strm_addr, stream *strm);
	void del_stream(unsigned long strm_addr, stream *strm);

private:
	
	network *_net_ptr;
	logger *_log_ptr;

	map<unsigned long, stream *> _map_stream;	// <strm_addr, stream *>
	map<unsigned long, stream *>::iterator _map_stream_iter;	// <strm_addr, stream *>
	
	struct sockaddr_in _cin;
};

#endif
