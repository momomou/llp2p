#ifndef _STREAM_HANDLER_H_
#define _STREAM_HANDLER_H_

#include "common.h"
#include "basic_class.h"

class stream_handler:public basic_class {
public:
	stream_handler();
	~stream_handler();

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

private:

};

#endif
