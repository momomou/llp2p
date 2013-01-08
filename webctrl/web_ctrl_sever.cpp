#include "web_ctrl_sever.h"
#include "web_ctrl_unit.h"
#include "../network.h"
#include "../logger.h"

web_ctrl_sever::web_ctrl_sever(network *net_ptr, logger *log_ptr, list<int> *fd_list, map<string, unsigned char> *map_stream_name_id)
{
    _net_ptr = net_ptr;
	_log_ptr = log_ptr;
	fd_list_ptr = fd_list;

    clntSock = 0;

    _map_stream_name_id_ptr = map_stream_name_id;
}

web_ctrl_sever::~web_ctrl_sever()
{

}

int web_ctrl_sever::init()
{
    echoServPort = BIND_PORT;
    char enable = 1;
    int iMode = 1;

    /* Create socket for incoming connections */ 
    if ((servSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket() failed\n"); 
        return RET_ERROR;
    }
       
    /* Construct local address structure */ 
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */ 
    echoServAddr.sin_family = AF_INET;                /* Internet address family */ 
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */ 
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */ 
 
    /* Bind to the local address */ 
    if (::bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(struct sockaddr_in)) < 0) {
        printf("bind() failed\n"); 
        return RET_ERROR;
    }
    cout << "http server bind at TCP port: " << echoServPort << endl;
 
    /* Mark the socket so it will listen for incoming connections */ 
    if (::listen(servSock, MAX_POLL_EVENT) < 0) {
        printf("listen() failed\n"); 
        return RET_ERROR;
    }

#ifdef _WIN32
	if (::ioctlsocket(servSock,FIONBIO,(u_long FAR*)&iMode) == SOCKET_ERROR){
		throw "socket error: _sock_tcp";
	}
#endif

    setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    _net_ptr->set_nonblocking(servSock);	// set to non-blocking
	_net_ptr->epoll_control(servSock, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->fd_bcptr_map_set(servSock, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(servSock);

    _web_ctrl_unit_ptr = new web_ctrl_unit(_net_ptr, _log_ptr, _map_stream_name_id_ptr);
	if (!_web_ctrl_unit_ptr) {
		cout << "can not allocate web_ctrl_unit!!!" << endl;
		//::close(new_fd);
		return RET_ERROR;
	}
	printf("new web_ctrl_unit successfully\n");

    return RET_OK;
}
	
int web_ctrl_sever::handle_pkt_in(int sock)
{
    int new_fd;
	socklen_t sock_len;

	if (sock != servSock)
		return RET_ERROR;

	sock_len = sizeof(echoClntAddr);
	new_fd = _net_ptr->accept(sock, (struct sockaddr *)&echoClntAddr, &sock_len);
    clntSock = new_fd;
	if(new_fd < 0) {
		printf("Error occured in accept\n");
		return RET_ERROR;
	}
	printf("accept web client successfully\n");

	_web_ctrl_unit_ptr->set_client_sockaddr(&echoClntAddr);
	_net_ptr->set_nonblocking(new_fd);
	_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN);
	printf("web_server epoll new_fd=>%d\n", new_fd);
	_net_ptr->fd_bcptr_map_set(new_fd, dynamic_cast<basic_class *> (_web_ctrl_unit_ptr));
	_net_ptr->fd_del_hdl_map_set(new_fd, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(new_fd);

    return RET_OK;
}

int web_ctrl_sever::handle_pkt_out(int sock)
{
    return RET_OK;
}

void web_ctrl_sever::handle_pkt_error(int sock)
{
}

void web_ctrl_sever::handle_sock_error(int sock, basic_class *bcptr)
{
	delete dynamic_cast<web_ctrl_unit *> (bcptr);
	_net_ptr->fd_del_hdl_map_delete(sock);
	data_close(sock, "client closed!!");
}

void web_ctrl_sever::handle_job_realtime()
{
}

void web_ctrl_sever::handle_job_timer()
{
}

void web_ctrl_sever::handle_control_stream(string user_name, unsigned char stream_id, int control_type)
{
    if(clntSock != 0)
        _web_ctrl_unit_ptr->handle_control_stream(user_name, stream_id, clntSock, control_type);
}

void web_ctrl_sever::data_close(int cfd, const char *reason) 
{
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "controller", reason);
	cout << "controller " << cfd << " exit by " << reason << ".." << endl;
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	//_net_ptr->close(cfd);
	
}