#ifndef _RTSP_VIEWER_H_
#define _RTSP_VIEWER_H_

#include "../common.h"
#include "../basic_class.h"

class network;
class logger;
class stream_udp;
class stream_handler_udp;
class pk_mgr;

enum rv_state {STATE_RTSP_INIT = 1, STATE_RTSP_OPTIONS, STATE_RTSP_DESCRIBE, STATE_RTSP_SETUP, STATE_RTSP_PLAY, STATE_RTSP_TEARDOWN};

#define RTSP_PORT_UDP_AUDIO 6970
#define RTSP_PORT_UDP_VIDEO 6971


class rtsp_viewer:public basic_class {
public:

	list<int> *fd_list_ptr;
	int  _sock_udp_audio, _sock_udp_video;
	
	rtsp_viewer(network *net_ptr, logger *log_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list);
	~rtsp_viewer();
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	void init();
	void set_client_sockaddr(struct sockaddr_in *cin);

private:
	network *_net_ptr;
	logger *_log_ptr;
	stream_udp *_strm_audio, *_strm_video;
	stream_handler_udp *_strm_hand_audio, *_strm_hand_video;
	pk_mgr *_pk_mgr_ptr;

	int _send_byte;
	int _expect_len;
	int _offset;

	unsigned short _cln_port_udp_audio, _cln_port_udp_video;
	unsigned short _srv_port_udp_audio, _srv_port_udp_video;
	struct sockaddr_in _cin_tcp, _cin_udp_audio, _cin_udp_video;

	rv_state _cln_state;
	unsigned long _html_size;
	queue<struct chunk_t *> _queue_output_ctrl;

	void data_close(int cfd, const char *reason);
};

#endif
