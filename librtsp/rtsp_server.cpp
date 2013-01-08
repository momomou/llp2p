#include "rtsp_server.h"
#include "../network.h"
#include "../logger.h"
#include "rtsp_viewer.h"
#include <sstream>

rtsp_server::rtsp_server(network *net_ptr, logger *log_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	fd_list_ptr = fd_list;
	_sock_tcp = 0; 
	_rtsp_viewer = NULL;
	_html_size = HTML_SIZE;

	_rtsp_viewer = new rtsp_viewer(_net_ptr, _log_ptr, _pk_mgr_ptr, fd_list_ptr);

	if (!_rtsp_viewer) {
		cout << "can't allocate rtsp viewer!!!" << endl;
		return;
	}
	
	printf("new rtsp_viewer successfully\n");
	
}

rtsp_server::~rtsp_server()
{
	if(_rtsp_viewer)
		delete _rtsp_viewer;

}

void rtsp_server::init(string tcp_svc_port)
{
	struct sockaddr_in sin;
	unsigned short port_tcp;
	stringstream ss_tmp;
	int enable = 1;

	ss_tmp << tcp_svc_port;
	ss_tmp >> port_tcp;
	DBG_PRINTF("port_tcp = %hu\n", port_tcp);
	
	_sock_tcp = socket(AF_INET, SOCK_STREAM, 0);

	if(_sock_tcp < 0)
		throw "create tcp srv socket fail";

	memset(&sin, 0 ,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port_tcp);

	if (bind(_sock_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1) {
		throw "can't bind socket: _sock_tcp";
	} else {
		printf("Server bind at TCP port: %d\n", port_tcp);
	}	 

	if (listen(_sock_tcp, MAX_POLL_EVENT) == -1) {
		printf("TCP PORT :%d   listen error! \n",port_tcp);
		throw "can't listen socket: _sock_tcp";
	}
	setsockopt(_sock_tcp, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int));

	_net_ptr->set_nonblocking(_sock_tcp);	// set to non-blocking
	_net_ptr->epoll_control(_sock_tcp, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->set_fd_bcptr_map(_sock_tcp, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(_sock_tcp);
	
}

int rtsp_server::handle_pkt_in(int sock)
{
	int new_fd;
	socklen_t sock_len;

	if (sock != _sock_tcp)
		return RET_SOCK_ERROR;

	sock_len = sizeof(_cin);
	new_fd = _net_ptr->accept(sock, (struct sockaddr *)&_cin, &sock_len);
	if(new_fd < 0) {
		printf("Error occured in accept\n");
		return RET_SOCK_ERROR;
	}
	printf("accept client successfully\n");

	_rtsp_viewer->init();
	
	_rtsp_viewer->set_client_sockaddr(&_cin);
	_net_ptr->set_nonblocking(new_fd);
	_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->set_fd_bcptr_map(new_fd, dynamic_cast<basic_class *> (_rtsp_viewer));
	fd_list_ptr->push_back(new_fd);

	return RET_OK;
}


int rtsp_server::handle_pkt_out(int sock)
{
	return RET_OK;
}

void rtsp_server::handle_pkt_error(int sock)
{

}

void rtsp_server::handle_job_realtime()
{

}


void rtsp_server::handle_job_timer()
{

}

void rtsp_server::accpet_rtsp_client(int sock)
{
	

}

void rtsp_server::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;
	
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "rtsp_server", reason);
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);	

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
	
}
