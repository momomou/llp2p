#ifndef _RTMP_SERVER_H_
#define _RTMP_SERVER_H_

#include "../common.h"

#define MAX_POLL_EVENT 100

class network;
class logger;
class amf;
class rtmp;
class rtmp_supplement;
//class rtmp_client;
class rtmp_viewer;
class pk_mgr;

class rtmp_server:public basic_class {
public:
	list<int> *fd_list_ptr;

	rtmp_server(network *net_ptr, logger *log_ptr, amf *amf_ptr, rtmp *rtmp_ptr, rtmp_supplement *rtmp_supplement_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list);
	~rtmp_server();

	void init(int stream_id, string tcp_svc_port);
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	void add_seed(int sock, queue<struct chunk_t *> *queue_out_data_ptr);
	void del_seed(int sock);

private:
	network *_net_ptr;
	logger *_log_ptr;
	amf *_amf_ptr;
	rtmp *_rtmp_ptr;
	rtmp_supplement *_rtmp_supplement_ptr;
	rtmp_viewer *_rtmp_viewer;
	pk_mgr *_pk_mgr_ptr;

	struct sockaddr_in _cin;

	int _sock_tcp;
	int _stream_id;
	void accpet_rtmp_client(int sock);
	void data_close(int cfd, const char *reason);
	map<int, queue<struct chunk_t *> *> _map_seed_out_data;
	
};

#endif
