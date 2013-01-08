#ifndef _RTMP_VIEWER_H_
#define _RTMP_VIEWER_H_

#include "../common.h"
#include "../basic_class.h"
#include "rtmp_supplement.h"
#include "../stream_udp.h"
#include "../stream_handler_udp.h"

class network;
class logger;
class rtmp_client;
class rtmp_server;
class pk_mgr;
class stream_udp;
class stream_handler_udp;

enum rv_state {STATE_RTSP_INIT = 1, STATE_RTSP_OPTIONS, STATE_RTSP_DESCRIBE, STATE_RTSP_SETUP, STATE_RTSP_PLAY, STATE_RTSP_TEARDOWN};

enum
{
  STREAMING_ACCEPTING,
  HANDSHAKING_1,
  HANDSHAKING_2,
  INVOKEMETADATA,    
  STREAMING_IN_PROGRESS,
  STREAMING_STOPPING,
  STREAMING_STOPPED
};

typedef struct
{
  int socket;
  int state;
  int streamID;
  char *connect;

} STREAMING_SERVER;

#define RTMP_PORT_UDP 6971

class rtmp_viewer:public stream {
public:
	list<int> *fd_list_ptr;
	int  _sock_udp_video;

	rtmp_viewer(int stream_id, network *net_ptr, logger *log_ptr, rtmp_server *rtmp_server, amf *amf_ptr, rtmp *rtmp_ptr, rtmp_supplement *rtmp_supplement_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list);
	~rtmp_viewer();
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	virtual void add_chunk(struct chunk_t *chunk);
    virtual unsigned char get_stream_pk_id();

	void set_client_sockaddr(struct sockaddr_in *cin);

	//based on libRTMP function -------------------->
	bool	SHandShake1(RTMP *r);
	bool	SHandShake2(RTMP *r);
	int ServeInvoke(STREAMING_SERVER *server, RTMP * r, RTMPPacket *packet, unsigned int offset);
	int ServePacket(STREAMING_SERVER *server, RTMP *r, RTMPPacket *packet);
	//based on libRTMP function<--------------------
		
private:
	network *_net_ptr;
	logger *_log_ptr;
	rtmp_client *_rtmp_client;
	rtmp_server *_rtmp_server;
	amf *_amf_ptr;
	rtmp *_rtmp_ptr;
	rtmp_supplement *_rtmp_supplement_ptr;
	pk_mgr *_pk_mgr_ptr;
	stream_udp *_strm_video;
	stream_handler_udp *_strm_hand_video;

	STREAMING_SERVER *server;

	int _send_byte;
	int _expect_len;
	int _offset;
	int  _sock_tcp;
	int control_pkt;
	FILE *file_ptr ;
	
	unsigned short _cln_port_udp_video;
	unsigned short _srv_port_udp_video;
	struct sockaddr_in _cin_tcp;
	unsigned long _html_size;
	
	queue<struct chunk_t *> _queue_output_ctrl;
	queue<struct chunk_t *> *_queue_out_data_ptr;
	
	RTMPPacket packet;
	RTMP cRtmp;
	int _stream_id;
    unsigned char _stream_pk_id;
	
	char serverbuf[RTMP_SIG_SIZE + 1], *serversig;
       char clientsig[RTMP_SIG_SIZE];

	Network_nonblocking_ctl _send_ctl_info;

	map<int, bool> _channel_map;

	void data_close(int cfd, const char *reason);
};

#endif
