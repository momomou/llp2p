#ifndef __REGISTER_MGR_H__
#define __REGISTER_MGR_H__

#include "common.h"
#include "network.h"
#include "pk_mgr.h"

class register_mgr:public basic_class{	
public:

	register_mgr();
	~register_mgr();

	void build_connect(int channel_id);
	
	/*
	this part just implement handle pkt out
	*/
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	network *_net_ptr;
	pk_mgr *_pk_mgr_ptr;
	configuration *_prep;
	logger *_log_ptr;
	
	string pk_ip;
	string pk_port;
	bool got_pk;

};

#endif

