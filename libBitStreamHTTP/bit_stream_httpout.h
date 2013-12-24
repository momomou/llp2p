#ifndef _BIT_STREAM_HTTPOUT
#define _BIT_STREAM_HTTPOUT

#include "../common.h"
#include "../basic_class.h"
#include "../stream.h"

class network;
class logger;
class pk_mgr;
class bit_stream_server;


class bit_stream_httpout:public stream {
public:

	list<int> *fd_list_ptr;
	map<int, struct update_stream_header *>::iterator  map_streamID_header_iter;

	bit_stream_httpout(int stream_id , network *net_ptr, logger *log_ptr,bit_stream_server *bit_stream_server_ptr,pk_mgr *pk_mgr_ptr, list<int> *fd_list ,int acceptfd);
	~bit_stream_httpout();
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	void send_header_to_player(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	void init();
	void set_client_sockaddr(struct sockaddr_in *cin);
	virtual void add_chunk(struct chunk_t *chunk);
    virtual unsigned char get_stream_pk_id();
//	int _reqStreamID;


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
	int firstRecvReqSreamID;

	int a;
	int b;

	FILE *file_ptr;
	FILE *file_ptr_test;

	bool first_pkt;
	bool first_HTTP_Header;		// If send HTTP header and FLV header send to player, this value becomes false

	Network_nonblocking_ctl _send_ctl_info;
	queue<struct chunk_t *> _queue_output_ctrl;
	queue<struct chunk_t *> *_queue_out_data_ptr;

	unsigned long _html_size;

	//void data_close(int cfd, const char *reason);
	bool isStreamID_inChannel(int streamid);
	int getStreamID_FromHTTP_Request(int sock,char *httpBuffer,unsigned long BufferSize );
	bool isKeyFrame(struct chunk_t *chunk_ptr);
	unsigned int getFlvTimeStamp(struct chunk_t *chunk_ptr);
	void data_close(int cfd, const char *reason) ;


};

#endif
