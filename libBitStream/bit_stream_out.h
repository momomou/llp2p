#ifndef _BIT_STREAM_OUT
#define _BIT_STREAM_OUT

#include "../common.h"
#include "../basic_class.h"
#include "../stream.h"

class network;
class logger;
class pk_mgr;
class bit_stream_server;


class bit_stream_out:public stream {
public:

	list<int> *fd_list_ptr;
	
	bit_stream_out(int stream_id,network *net_ptr, logger *log_ptr,bit_stream_server *bit_stream_server_ptr,pk_mgr *pk_mgr_ptr, list<int> *fd_list);
	~bit_stream_out();
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	void init();
	void set_client_sockaddr(struct sockaddr_in *cin);
	virtual void add_chunk(struct chunk_t *chunk);
    virtual unsigned char get_stream_pk_id();
	void data_close(int cfd, const char *reason) ;


private:
	network *_net_ptr;
	logger *_log_ptr;
	pk_mgr *_pk_mgr_ptr;
	bit_stream_server *_bit_stream_server_ptr;

	struct sockaddr_in _cin_tcp;
	int _stream_id;
	int _send_byte;
	int _expect_len;
	int _offset;
	FILE *file_ptr;
	int first_pkt;

	Network_nonblocking_ctl _send_ctl_info;
	queue<struct chunk_t *> _queue_output_ctrl;
	queue<struct chunk_t *> *_queue_out_data_ptr;

	unsigned long _html_size;
	bool first_Header;

//	void data_close(int cfd, const char *reason);
};

#endif
