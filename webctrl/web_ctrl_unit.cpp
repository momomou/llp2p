#include "web_ctrl_unit.h"
#include "../network.h"
#include "../logger.h"

using namespace std;

web_ctrl_unit::web_ctrl_unit(network *net_ptr, logger *log_ptr, map<string, unsigned char> *map_stream_name_id)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
    _offset = 0;

    _map_stream_name_id_ptr = map_stream_name_id;
}

web_ctrl_unit::~web_ctrl_unit()
{

}

int web_ctrl_unit::handle_pkt_in(int sock)
{
	int nBytes = 1024, recv_byte; 
	int expect_len;
    int request_type;
    int stream_id;
	struct web_control_info *data = NULL;
    char user_name[64];

	expect_len = sizeof(struct web_control_info);

	recv_byte = recv(sock, recievebuf, nBytes, 0);
    if (recv_byte < 0) {
		data_close(sock, "recv error in pk_mgr::handle_pkt_in");
		printf("recv error in pk_mgr::handle_pkt_in\n");
		DBG_PRINTF("here\n");
		_log_ptr->exit(0, "recv error in pk_mgr::handle_pkt_in");\
	}

	data = (web_control_info *)&recievebuf;
    request_type = data->type;

    if(data->type == WEB_CONNECTED) {
    }
    else if(data->type == WEB_READY) {
        printf("Web site is Ready\n");
        for(iter = _map_stream_name_id_ptr->begin(); iter != _map_stream_name_id_ptr->end(); iter++){
            strcpy(user_name, iter->first.c_str());
            stream_id = (int)iter->second;
            handle_control_stream(user_name, stream_id, sock, NOTIFY_ADD);
        }
    }
    else if(data->type == REQUEST) {
    }

	printf("recieve fin!!\n");

	return RET_OK;
	
}

int web_ctrl_unit::handle_pkt_out(int sock)
{
    int _expect_len, _send_byte;
    unsigned int send_size;
    struct web_control_info *data = NULL;

    if (!sentbuf.size()) {
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);	
		return RET_OK;
	}
    printf("buffer size = %d\n",sentbuf.size());

    data = sentbuf.front();
    sentbuf.pop();
	send_size = sizeof(struct web_control_info);

    _net_ptr->sendto(sock, (char *)data, send_size, 0, (struct sockaddr *)&_cin, sizeof(_cin));

    printf("transfer fin!!\n");

    return RET_OK;
}

void web_ctrl_unit::handle_pkt_error(int sock)
{

}

void web_ctrl_unit::handle_job_realtime()
{

}

void web_ctrl_unit::handle_job_timer()
{

}

void web_ctrl_unit::data_close(int cfd, const char *reason) 
{
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "controller", reason);
	cout << "controller " << cfd << " exit by " << reason << ".." << endl;
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);
	
}

void web_ctrl_unit::set_client_sockaddr(struct sockaddr_in *cin)
{
	if (cin)
		memcpy(&_cin, cin, sizeof(struct sockaddr_in));
}

void web_ctrl_unit::handle_control_stream(string user_name, unsigned char stream_id, int sock, int control_type)
{
    unsigned long reply_size;
    struct web_control_info *data_ptr = NULL;

    reply_size = sizeof(web_control_info);
    data_ptr = (struct web_control_info *) new unsigned char[reply_size];
    memset(data_ptr, 0x0, reply_size);

    printf("sent pkt to web\n");
    data_ptr->type = control_type;
    strcpy(data_ptr->user_name, user_name.c_str());
    data_ptr->stream_id = (int)stream_id;
    sentbuf.push((struct web_control_info *) data_ptr);
    _net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
}