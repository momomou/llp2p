#include "bit_stream_server.h"
#include "network.h"
#include "logger.h"
#include "pk_mgr.h"
#include "bit_stream_out.h"
#include "bit_stream_httpout.h"
#include <sstream>

#define mode 5 // mod_HTTP

//need to init a server to listen
//after accept a client need call pk_mgr -> add_stream 


bit_stream_server::bit_stream_server(network *net_ptr, logger *log_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list )
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	fd_list_ptr = fd_list;
	_sock_tcp = 0; 
	_bit_stream_out_ptr = NULL;

	

}

bit_stream_server::~bit_stream_server()
{

}

//init socket to listen , set nob-locking ,start to epoll LIN
void bit_stream_server::init(int stream_id, unsigned short bitStreamServerPort)
{
	struct sockaddr_in sin;
	int enable = 1;
	int iMode = 1;
	_stream_id = stream_id;


	DBG_PRINTF("bitStreamServerPort = %hu\n", bitStreamServerPort);
	_sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if(_sock_tcp < 0)
		throw "create tcp srv socket fail";
	memset(&sin, 0 ,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(bitStreamServerPort);

	if (::bind(_sock_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1) {
		throw "can't bind socket: _sock_tcp";
	} else {
		printf("bitStreamServer bind at TCP port: %d\n", bitStreamServerPort);
	}	 

	if (::listen(_sock_tcp, MAX_POLL_EVENT) == -1) {
		printf("bitStreamServer :%d   listen error! \n",bitStreamServerPort);
		throw "can't listen socket: _sock_tcp";
	}

// for windows
#ifdef _WIN32
	if (::ioctlsocket(_sock_tcp,FIONBIO,(u_long FAR*)&iMode) == SOCKET_ERROR){
		throw "socket error: _sock_tcp";
	}
#endif


	setsockopt(_sock_tcp, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(int));
	
	

	_net_ptr->set_nonblocking(_sock_tcp);	// set to non-blocking
	_net_ptr->epoll_control(_sock_tcp, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->fd_bcptr_map_set(_sock_tcp, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(_sock_tcp);
	
}

int bit_stream_server::handle_pkt_in(int sock)
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
	

//mode select mode == mode_BitStream
	if(mode == mode_BitStream){


		_bit_stream_out_ptr = new bit_stream_out(_stream_id, _net_ptr, _log_ptr, this, _pk_mgr_ptr, fd_list_ptr);
	if (!_bit_stream_out_ptr) {
		cout << "can not allocate _bit_stream_out!!!" << endl;
		_net_ptr -> close (new_fd);
		return RET_ERROR;
	}
	printf("new bit_stream_out successfully\n");
	_pk_mgr_ptr ->add_stream( new_fd,(stream*)_bit_stream_out_ptr, STRM_TYPE_MEDIA);


	_bit_stream_out_ptr->set_client_sockaddr(&_cin);
	_net_ptr->set_nonblocking(new_fd);
	_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->fd_bcptr_map_set(new_fd, dynamic_cast<basic_class *> (_bit_stream_out_ptr));
	fd_list_ptr->push_back(new_fd);

	return RET_OK;

//mode == mode_HTTP
	}else if (mode == mode_HTTP){

		_bit_stream_httpout_ptr = new bit_stream_httpout(_stream_id , _net_ptr, _log_ptr, this, _pk_mgr_ptr, fd_list_ptr ,new_fd);
	
	if (!_bit_stream_httpout_ptr) {
		cout << "can not allocate _bit_stream_out!!!" << endl;
		_net_ptr -> close (new_fd);
		return RET_ERROR;
	}
	printf("\nnew bit_stream_httpout successfully\n");
	_pk_mgr_ptr ->add_stream( new_fd,(stream*)_bit_stream_httpout_ptr, STRM_TYPE_MEDIA);


	_bit_stream_httpout_ptr->set_client_sockaddr(&_cin);
	_net_ptr->set_nonblocking(new_fd);
	_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->fd_bcptr_map_set(new_fd, dynamic_cast<basic_class *> (_bit_stream_httpout_ptr));
	fd_list_ptr->push_back(new_fd);

	return RET_OK;
	}else if (mode==mode_RTMP){
	// other mode  RTSP RTMP FILE TE_SG
	}



}


int bit_stream_server::handle_pkt_out(int sock)
{
	return RET_OK;
}

void bit_stream_server::handle_pkt_error(int sock)
{

}

void bit_stream_server::handle_sock_error(int sock, basic_class *bcptr)
{
	delete dynamic_cast<bit_stream_out *> (bcptr);
	_net_ptr->fd_del_hdl_map_delete(sock);
	del_seed(sock);
	data_close(sock, "client closed!!");
	_pk_mgr_ptr->del_stream(sock,(stream *)bcptr, STRM_TYPE_MEDIA);
}

void bit_stream_server::handle_job_realtime()
{

}


void bit_stream_server::handle_job_timer()
{

}

void bit_stream_server::delBitStreamOut(stream *stream_ptr)
{
	delete stream_ptr;
}


void bit_stream_server::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;

	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "bit_stream_server", reason);
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);	

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
}

void bit_stream_server:: add_seed(int sock, queue<struct chunk_t *> *queue_out_data_ptr)
{
	_map_seed_out_data[sock] = queue_out_data_ptr;
}
void bit_stream_server:: del_seed(int sock)
{
	_map_seed_out_data.erase(sock);
	cout << "del_seed success" << endl;

}




