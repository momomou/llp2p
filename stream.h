#ifndef _STREAM_H_
#define _STREAM_H_
#include "common.h"
#include "basic_class.h"

class stream:public basic_class {

public:
	stream();
	~stream();
	int _reqStreamID;
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	virtual void add_chunk(struct chunk_t *chunk);
	virtual void set_client_sockaddr(struct sockaddr_in *cin);
    virtual unsigned char get_stream_pk_id();

private:
};

#endif

