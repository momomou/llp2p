#ifndef __WEB_CTRL_UNIT_H__
#define __WEB_CTRL_UNIT_H__

#include "common.h"
#include "basic_class.h"

class network;
class logger;

class web_ctrl_unit:public basic_class {
public:

	web_ctrl_unit(network *net_ptr, logger *log_ptr, map<string, unsigned char> *map_stream_name_id);
	~web_ctrl_unit();

	void init();
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	void data_close(int cfd, const char *reason);
    void set_client_sockaddr(struct sockaddr_in *cin);
    void handle_control_stream(string user_name, unsigned char stream_id, int sock, int control_type);

private:

	network *_net_ptr;
	logger *_log_ptr;

    map<string, unsigned char> *_map_stream_name_id_ptr;
    map<string, unsigned char>::const_iterator iter;

	char recievebuf[1024];
    int _offset;
    queue<struct web_control_info *>  sentbuf;

    struct sockaddr_in _cin;

};

#endif