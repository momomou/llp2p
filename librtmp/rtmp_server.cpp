#include "rtmp_server.h"
#include "../network.h"
#include "../logger.h"
#include "../pk_mgr.h"
#include "rtmp_viewer.h"
#include <sstream>


rtmp_server::rtmp_server(network *net_ptr, logger *log_ptr, amf *amf_ptr, rtmp *rtmp_ptr, rtmp_supplement *rtmp_supplement_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_amf_ptr = amf_ptr;
	_rtmp_ptr = rtmp_ptr;
	_rtmp_supplement_ptr = rtmp_supplement_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	fd_list_ptr = fd_list;

	_sock_tcp = 0; 
	_rtmp_viewer = NULL;
	

}

rtmp_server::~rtmp_server()
{

}

void rtmp_server::init(int stream_id, string tcp_svc_port)
{
	struct sockaddr_in sin, sin_audio, sin_video;
	unsigned short port_tcp;
	stringstream ss_tmp;
	int enable = 1;
	int iMode = 1;

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

	if (::bind(_sock_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1) {
		throw "can't bind socket: _sock_tcp";
	} else {
		printf("Server bind at TCP port: %d\n", port_tcp);
	}	 

	if (::listen(_sock_tcp, MAX_POLL_EVENT) == -1) {
		printf("TCP PORT :%d   listen error! \n",port_tcp);
		throw "can't listen socket: _sock_tcp";
	}

#ifdef _WIN32
	if (::ioctlsocket(_sock_tcp,FIONBIO,(u_long FAR*)&iMode) == SOCKET_ERROR){
		throw "socket error: _sock_tcp";
	}
#endif

	setsockopt(_sock_tcp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	_stream_id = stream_id;
	//_rtmp_client->add_map_server_to_viewer_queue(stream_id, &_map_seed_out_data);
	_net_ptr->set_nonblocking(_sock_tcp);	// set to non-blocking
	_net_ptr->epoll_control(_sock_tcp, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->fd_bcptr_map_set(_sock_tcp, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(_sock_tcp);

}

int rtmp_server::handle_pkt_in(int sock)
{
	int new_fd;
	socklen_t sock_len;


	if (sock != _sock_tcp)
		return RET_ERROR;

	sock_len = sizeof(_cin);
	new_fd = _net_ptr->accept(sock, (struct sockaddr *)&_cin, &sock_len);
	if(new_fd < 0) {
		printf("Error occured in accept\n");
		return RET_ERROR;
	}
	


	_rtmp_viewer = new rtmp_viewer(_stream_id, _net_ptr, _log_ptr, this, _amf_ptr, _rtmp_ptr, _rtmp_supplement_ptr, _pk_mgr_ptr, fd_list_ptr);
	if (!_rtmp_viewer) {
		cout << "can not allocate rtmp viewer!!!" << endl;
		::close(new_fd);
		return RET_ERROR;
	}


	
	printf("new rtmp_viewer successfully\n");

	//_beginthread(threadSoc, 0, (void*)fd_list_ptr);
	_rtmp_viewer->set_client_sockaddr(&_cin);
	_net_ptr->set_nonblocking(new_fd);
	_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN);
	printf("Rtmp_server epoll new_fd=>%d\n", new_fd);
	_net_ptr->fd_bcptr_map_set(new_fd, dynamic_cast<basic_class *> (_rtmp_viewer));
	_net_ptr->fd_del_hdl_map_set(new_fd, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(new_fd);

	return RET_OK;
}


int rtmp_server::handle_pkt_out(int sock)
{
	return RET_OK;
}

void rtmp_server::handle_pkt_error(int sock)
{

}

void rtmp_server::handle_sock_error(int sock, basic_class *bcptr)
{
	delete dynamic_cast<rtmp_viewer *> (bcptr);
	_net_ptr->fd_del_hdl_map_delete(sock);
	del_seed(sock);
	data_close(sock, "client closed!!");
	_pk_mgr_ptr->del_stream(sock,(stream *)bcptr, STRM_TYPE_MEDIA);
}

void rtmp_server::handle_job_realtime()
{

}


void rtmp_server::handle_job_timer()
{

}

void rtmp_server::accpet_rtmp_client(int sock)
{
	

}

void rtmp_server::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;

	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "rtmp_server", reason);
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);	

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
}

void rtmp_server:: add_seed(int sock, queue<struct chunk_t *> *queue_out_data_ptr)
{
	_map_seed_out_data[sock] = queue_out_data_ptr;
}
void rtmp_server:: del_seed(int sock)
{
	_map_seed_out_data.erase(sock);
	cout << "del_seed success" << endl;

}











