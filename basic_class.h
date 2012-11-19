#ifndef __BASIC_CLASS_H__
#define __BASIC_CLASS_H__

class basic_class {
public:
	
	basic_class();
	~basic_class();
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

private:

	
};

#endif



































