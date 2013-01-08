#ifndef _WEB_CTRL_SERVER_H_
#define _WEB_CTRL_SERVER_H_

#define BIND_PORT  2000

#include "../common.h"
#include "../basic_class.h"

class network;
class logger;
class web_ctrl_unit;

class web_ctrl_sever:public basic_class {
public:

	list<int> *fd_list_ptr;
	
	web_ctrl_sever(network *net_ptr, logger *log_ptr, list<int> *fd_list, map<string, unsigned char> *map_stream_name_id);
	~web_ctrl_sever();

	int init();
	
	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
    void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
    void handle_control_stream(string user_name, unsigned char stream_id, int control_type);
    void data_close(int cfd, const char *reason);

private:
	network *_net_ptr;
	logger *_log_ptr;
    web_ctrl_unit *_web_ctrl_unit_ptr;

    map<string, unsigned char> *_map_stream_name_id_ptr;

	int servSock;                    /* Socket descriptor for server */ 
    int clntSock;                    /* Socket descriptor for client */ 
    struct sockaddr_in echoServAddr; /* Local address */ 
    struct sockaddr_in echoClntAddr; /* Client address */ 
    unsigned short echoServPort;     /* Server port */ 
    socklen_t clntLen;            /* Length of client address data structure */ 

};

#endif