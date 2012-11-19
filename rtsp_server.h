#ifndef _RTSP_SERVER_H_
#define _RTSP_SERVER_H_

#include "common.h"
#include "basic_class.h"

class network;
class logger;
class pk_mgr;
class rtsp_viewer;

#define RTSP_PORT_TCP 554	// rtsp is binding on port 554

class rtsp_server:public basic_class {
public:

	list<int> *fd_list_ptr;
	
	rtsp_server(network *net_ptr, logger *log_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list);
	~rtsp_server();

	void init(string tcp_svc_port);
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

private:
	network *_net_ptr;
	logger *_log_ptr;
	pk_mgr *_pk_mgr_ptr;
	rtsp_viewer *_rtsp_viewer;

	struct sockaddr_in _sin, _cin;
	unsigned long _html_size;

	int _sock_tcp;

	void accpet_rtsp_client(int sock);
	void data_close(int cfd, const char *reason);
};

#endif
