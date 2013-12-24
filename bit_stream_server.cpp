#include "bit_stream_server.h"
#include "network.h"
#include "logger.h"
#include "logger_client.h"
#include "pk_mgr.h"
#include <sstream>
#include "common.h"
#ifdef _FIRE_BREATH_MOD_
//#include "bit_stream_out.h"
//#include "bit_stream_httpout.h"
#include "libBitStream/bit_stream_out.h"
#include "libBitStreamHTTP/bit_stream_httpout.h"
#else
#include "libBitStream/bit_stream_out.h"
#include "libBitStreamHTTP/bit_stream_httpout.h"
#endif
//need to init a server to listen
//after accept a client need call pk_mgr -> add_stream 


bit_stream_server::bit_stream_server(network *net_ptr, logger *log_ptr, logger_client *logger_client_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_logger_client_ptr = logger_client_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	fd_list_ptr = fd_list;
	_sock_tcp = 0; 
	_bit_stream_out_ptr = NULL;
}

bit_stream_server::~bit_stream_server()
{
	map<int, stream *>::iterator _map_stream_iter;	// <strm_addr, stream *>
	for(_map_stream_iter =_pk_mgr_ptr->_map_stream_fd_stream.begin() ; _map_stream_iter!=_pk_mgr_ptr->_map_stream_fd_stream.end(); _map_stream_iter++){

		if(MODE == MODE_BitStream){
			delete dynamic_cast<bit_stream_out *> (_map_stream_iter->second);
		}else 	if(MODE == MODE_HTTP){
			delete dynamic_cast<bit_stream_httpout *> (_map_stream_iter->second);
		}else 	if(MODE == MODE_RTMP){
			//delete dynamic_cast<bit_stream_out *> (_map_stream_iter->second);
		}

		data_close(_map_stream_iter ->first, "player obj closed!!");
	}
	_pk_mgr_ptr->_map_stream_fd_stream.clear();

	debug_printf("==============delete bit_stream_server success==========\n");
}

//init socket to listen , set nob-locking ,start to epoll LIN
// Return an available port for player
unsigned short bit_stream_server::init(int stream_id, unsigned short bitStreamServerPort)
{
	struct sockaddr_in sin;
	int enable = 1;
	int iMode = 1;
	_stream_id = stream_id;

	debug_printf("bitStreamServerPort = %hu \n", bitStreamServerPort);
	
	if ((_sock_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		debug_printf("[ERROR] Create socket failed \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed");
		PAUSE
	}
	/*
	if (setsockopt(_sock_tcp, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(int)) != 0) {
		_log_ptr->write_log_format("s(u) s s d \n", __FUNCTION__, __LINE__,"[ERROR] Set SO_REUSEADDR failed", ". Socket error", WSAGetLastError());
		debug_printf("[ERROR] Set SO_REUSEADDR failed. socket error %d \n", WSAGetLastError());
		PAUSE
	}
	*/
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	// Find a available port. Default is 3000
	for ( ; ; bitStreamServerPort++) {
		sin.sin_port = htons(bitStreamServerPort);
		int n = bind(_sock_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
		printf("bind n: %d \n", n);
		if (n != 0) {
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Socket bind failed at port", bitStreamServerPort, ". Socket error", WSAGetLastError());
			debug_printf("Socket bind failed at port %d. Socket error %d \n", bitStreamServerPort, WSAGetLastError());
			continue;
		}
		n = listen(_sock_tcp, MAX_POLL_EVENT);
		printf("listen n: %d \n", n);
		if (n != 0) {
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Socket listen failed at port", bitStreamServerPort, ". Socket error", WSAGetLastError());
			debug_printf("Socket listen failed at port %d. Socket error %d \n", bitStreamServerPort, WSAGetLastError());
			continue;
		}
		break;
	}
	
	_net_ptr->set_nonblocking(_sock_tcp);	// set to non-blocking
	_net_ptr->epoll_control(_sock_tcp, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->fd_bcptr_map_set(_sock_tcp, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(_sock_tcp);
	
	return bitStreamServerPort;
}

int bit_stream_server::handle_pkt_in(int sock)
{
	if (sock != _sock_tcp) {
		return RET_ERROR;
	}

	int new_fd;
	socklen_t sock_len = sizeof(_cin);

	new_fd = accept(sock, (struct sockaddr *)&_cin, &sock_len);
	if (new_fd < 0) {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] Error occured in accept");
		debug_printf("Error occured in accept \n");
		return RET_ERROR;
	}
	
	struct sockaddr_in addr;
	int addrLen = sizeof(struct sockaddr_in);
	int aa;
	aa = getpeername(new_fd, (struct sockaddr *)&addr, &addrLen);
	_log_ptr->write_log_format("s(u) d s d \n", __FUNCTION__, __LINE__, aa, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	
	if (MODE == MODE_BitStream) {
		_bit_stream_out_ptr = new bit_stream_out(_stream_id, _net_ptr, _log_ptr, this, _pk_mgr_ptr, fd_list_ptr);
		if (!_bit_stream_out_ptr) {
			debug_printf("[ERROR] new bit_stream_out error \n");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] new bit_stream_out error \n");
			_net_ptr->close(new_fd);
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] new bit_stream_out error");
			PAUSE
			return RET_ERROR;
		}
		
		//debug_printf("new bit_stream_out successfully \n");
		//_pk_mgr_ptr->add_stream(new_fd, (stream*)_bit_stream_out_ptr, STRM_TYPE_MEDIA);

		_bit_stream_out_ptr->set_client_sockaddr(&_cin);
		_net_ptr->set_nonblocking(new_fd);
		_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN);
		_net_ptr->fd_bcptr_map_set(new_fd, dynamic_cast<basic_class *>(_bit_stream_out_ptr));
		fd_list_ptr->push_back(new_fd);

		return RET_OK;
	}
	else if (MODE == MODE_HTTP) {
		_bit_stream_httpout_ptr = new bit_stream_httpout(_stream_id, _net_ptr, _log_ptr, this, _pk_mgr_ptr, fd_list_ptr, new_fd);
		if (!_bit_stream_httpout_ptr) {
			debug_printf("[ERROR] new bit_stream_httpout error \n");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] new bit_stream_httpout error \n");
			_net_ptr->close(new_fd);
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] new bit_stream_httpout error");
			PAUSE
			return RET_ERROR;
		}
					
		//debug_printf("new bit_stream_httpout successfully socket = %d \n", new_fd);
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "new bit_stream_httpout successfully");
		
		_pk_mgr_ptr->add_stream(new_fd, (stream*)_bit_stream_httpout_ptr, STRM_TYPE_MEDIA);

		_bit_stream_httpout_ptr->set_client_sockaddr(&_cin);
		_net_ptr->set_nonblocking(new_fd);
		_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN);
		_net_ptr->fd_bcptr_map_set(new_fd, dynamic_cast<basic_class *>(_bit_stream_httpout_ptr));
		fd_list_ptr->push_back(new_fd);
		
		return RET_OK;
	}
	else if (MODE == MODE_RTMP) {
		// other mode  RTSP RTMP FILE TE_SG
		return RET_OK;
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
//	if(mode == mode_BitStream){
//		delete dynamic_cast<bit_stream_out *> (bcptr);
//	}else 	if(mode == mode_HTTP){
//		delete dynamic_cast<bit_stream_httpout *> (bcptr);
//	}else 	if(mode == mode_RTMP){
////		delete dynamic_cast<bit_stream_out *> (bcptr);
//	}
//	_net_ptr->fd_bcptr_map_delete(sock);
//	del_seed(sock);
	data_close(sock, "player obj closed!!");
	_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__," bitstream server obj closed!!");
//	_pk_mgr_ptr->del_stream(sock,(stream *)bcptr, STRM_TYPE_MEDIA);
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

// add_seed  del_seed noused ~
/*
void bit_stream_server:: add_seed(int sock, queue<struct chunk_t *> *queue_out_data_ptr)
{
	_map_seed_out_data[sock] = queue_out_data_ptr;
}
void bit_stream_server:: del_seed(int sock)
{
	_map_seed_out_data.erase(sock);
	cout << "del_seed success" << endl;

}

*/



