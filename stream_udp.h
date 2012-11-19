#ifndef _STREAM_UDP_H_
#define _STREAM_UDP_H_
#include "common.h"
#include "stream.h"

class network;
class logger;

class stream_udp:public stream {
public:
	stream_udp(network *net_ptr, logger *log_ptr);
	~stream_udp();

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	virtual void add_chunk(struct chunk_t *chunk);

	void set_client_sockaddr(struct sockaddr_in *cin);

private:
	
	network *_net_ptr;
	logger *_log_ptr;

	queue<struct chunk_t *> _chunk_q;

	struct sockaddr_in _cin;
};

#endif
