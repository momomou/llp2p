#ifndef _BIT_STREAM_SERVER_H_
#define _BIT_STREAM_SERVER_H_

#include "common.h"

class network;
class logger;
class bit_stream_out;
class pk_mgr;
class bit_stream_httpout;
class stream;


class bit_stream_server:public basic_class {
public:
	list<int> *fd_list_ptr;

	bit_stream_server(network *net_ptr, logger *log_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list);
	~bit_stream_server();

	unsigned short init(int stream_id, unsigned short bitStreamServerPort);
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
//	void add_seed(int sock, queue<struct chunk_t *> *queue_out_data_ptr);
//	void del_seed(int sock);
	void delBitStreamOut(stream *stream_ptr);

private:
	network *_net_ptr;
	logger *_log_ptr;
	pk_mgr *_pk_mgr_ptr;
	bit_stream_out *_bit_stream_out_ptr;
	bit_stream_httpout *_bit_stream_httpout_ptr;

	struct sockaddr_in _cin;

	int _sock_tcp;
	int _stream_id;
	void data_close(int cfd, const char *reason);
//	map<int, queue<struct chunk_t *> *> _map_seed_out_data;

	
};

#endif
