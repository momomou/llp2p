#include "network.h"
//#include "peer.h"


void network::timer() 
{
	send_byte = 0ULL;
	recv_byte = 0ULL;
	epfd = 0;
}

void network::setall_fd_epollout()
{
	if (_map_fd_bc_tbl.size()) {
		_map_fd_bc_tbl_iter = _map_fd_bc_tbl.begin();
		_map_fd_bc_tbl_iter++; // skip first fd(client)
		_map_fd_bc_tbl_iter++; // skip second fd(server)
		for(; _map_fd_bc_tbl_iter != _map_fd_bc_tbl.end() ; _map_fd_bc_tbl_iter++) {
			epoll_control(_map_fd_bc_tbl_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		}
	}
}

void network::garbage_collection()
{
	if (_map_fd_bc_tbl.size()) {
		for (_map_fd_bc_tbl_iter = _map_fd_bc_tbl.begin(); _map_fd_bc_tbl_iter != _map_fd_bc_tbl.end();) {
			if (close(_map_fd_bc_tbl_iter->first) == -1){
				debug_printf("_map_fd_bc_tbl_iter error \n");
				//PAUSE

			}
			_map_fd_bc_tbl_iter = _map_fd_bc_tbl.begin();
			if (_map_fd_bc_tbl_iter == _map_fd_bc_tbl.end()){
				break;
			}
		}
	}

	_map_fd_bc_tbl.clear();

}

unsigned long network::getLocalIpv4() 
{
	unsigned long ip = 0UL;
	
#ifdef _WIN32	// WIN32 我們使用 domain 反查方式來處理 ip, 不使用 WIN32 API 
	char hostname[512] = {0};

	gethostname(hostname, sizeof(hostname));
	debug_printf("hostname(%d): %s \n", strlen(hostname), hostname);
	
	struct addrinfo *AddrInfo, *AI;
	
	if(getaddrinfo(hostname, NULL, NULL, &AddrInfo) != 0) {
		return ip;	// error return 0UL
    }	

	debug_printf("hostname(%d): %s \n", strlen(hostname), hostname);
	
	// Search all network adapters
	for(AI = AddrInfo; AI != NULL; AI = AI->ai_next) {
		switch(AI->ai_family) {
			case AF_INET:	// ipv4
				ip = ((struct sockaddr_in *)AI->ai_addr)->sin_addr.s_addr;
				struct in_addr localIP;
				memcpy(&localIP, &ip, sizeof(struct in_addr));
				debug_printf("network::getLocalIpv4 ip: %s \n", inet_ntoa(localIP));
				return ip; // 找到，提早返回
		}
	}
	
#else				// LINUX 我們使用 ioctl 來詢問 , 注意此 ioctl 的方式不支援 v6 取得
	int fd;			// socket fd
	struct ifreq ifr;
	
	if((fd = ::socket(AF_INET, SOCK_DGRAM, 0)) < 0) {	// 注意這邊要用 call 的 socket scope 是外部的  需要使用 :: scope operator
		return ip;  // error return 0UL
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
	
	ioctl(fd, SIOCGIFADDR, &ifr);
	::close(fd);
	
	ip = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
#endif
	
	return ip;  // 這邊對 linux 而言是返回正確， win32 而言是返回錯誤的資訊 
}


void network::set_fd_bcptr_map(int sock, basic_class *bcptr)
{
	_map_fd_bc_tbl[sock] = bcptr;
}

void network::fd_bcptr_map_set(int sock, basic_class *bcptr)
{
	_map_fd_bc_tbl[sock] = bcptr;
}
void network::fd_bcptr_map_delete(int sock)
{
	_map_fd_bc_tbl.erase(sock);
}
/*
void network::fd_del_hdl_map_set(int sock, basic_class *bcptr)
{
	_map_fd_del_hdl_tbl[sock] = bcptr;
}
void network::fd_del_hdl_map_delete(int sock)
{
	_map_fd_del_hdl_tbl.erase(sock);
}
*/

void network::epoll_creater(void) 
{
#ifdef _FIRE_BREATH_MOD_
	epfd = epoll_create(MAXFDS, &epollVar);
#else
	epfd = epoll_create(MAXFDS);
#endif
}

#ifdef _WIN32
void network::epoll_waiter(int timeout, list<int> *fd_list) 
{
	int cfd;
	basic_class *bc_ptr;

#ifdef _FIRE_BREATH_MOD_
	nfds = epoll_wait(epfd, events, EVENTSIZE, timeout, fd_list, &epollVar);
#else
	nfds = epoll_wait(epfd, events, EVENTSIZE, timeout, fd_list);
#endif

	
	if(nfds == -1){
        //printf("epoll_wait failed\n");
		if(_error_cfd ->size() == 0 ){ return ;}
		cfd = _error_cfd->front();
		_error_cfd->pop();
		if(cfd != 0){
			_map_fd_bc_tbl_iter = _map_fd_bc_tbl.find(cfd);
			if (_map_fd_bc_tbl_iter != _map_fd_bc_tbl.end()) {
                //printf("handle sock error\n");
				bc_ptr = _map_fd_bc_tbl_iter->second;
				_map_fd_bc_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
			}
		}
		
		struct sockaddr_in addr;
		int addrLen = sizeof(struct sockaddr_in);
		int	aa;
		aa = getsockname(pk_fd, (struct sockaddr *)&addr, &addrLen);
		debug_printf("  aa:%2d  cfd: %2d , SrcAddr: %s:%d \n", aa, pk_fd, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		aa = getpeername(pk_fd, (struct sockaddr *)&addr, &addrLen);
		debug_printf("  aa:%2d  cfd: %2d , DstAddr: %s:%d \n", aa, pk_fd, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

		PAUSE
	}
}
#else
void network::epoll_waiter(int timeout) 
{
	nfds = epoll_wait(epfd, events, EVENTSIZE, timeout);
}
#endif

void network::epoll_dispatcher(void) 
{
	int cfd;
	basic_class *bc_ptr;

	for(int i=0; i<nfds; ++i) {
		cfd  = events[i].data.fd; 
		_map_fd_bc_tbl_iter = _map_fd_bc_tbl.find(cfd);
		
		if (_map_fd_bc_tbl_iter != _map_fd_bc_tbl.end()) {
			
			bc_ptr = _map_fd_bc_tbl_iter->second;
			
			//printf("nfds: %d, i: %d ", nfds, i);
			struct sockaddr_in addr;
			int addrLen=sizeof(struct sockaddr_in),
				aa;
			//aa=getsockname(cfd, (struct sockaddr *)&addr, &addrLen);
			//printf("  aa:%2d  cfd: %2d , SrcAddr: %s:%d \n", aa, cfd, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			aa=getpeername(cfd, (struct sockaddr *)&addr, (socklen_t *)&addrLen);
			//printf("  aa:%2d  cfd: %2d , DstAddr: %s:%d  ", aa, cfd, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			

			if (events[i].events & (EPOLLRDHUP | EPOLLERR)) {
				if (cfd == pk_fd) {
					// TODO: handle pk socket
					debug_printf("pk socket error");
					PAUSE
				}
				else if (cfd == log_fd) {
					// TODO: handle log-server socket
					debug_printf("log-server socket error");
					PAUSE
				}
				else {
					// EPOLLRDHUP => This socket is closed by client (client has sent a FIN), we have to close it.
#ifdef _WIN32
					int socketErr = WSAGetLastError();
#else
					int socketErr = errno;
#endif
					if (events[i].events & EPOLLRDHUP) {
						debug_printf("Socket %d is closed by remote host, err = %d \n", cfd, socketErr);
						close(cfd);
					}
					else if (events[i].events & EPOLLERR) {
						debug_printf("something wrong: fd = %d  error: %d \n", cfd, socketErr);
						_map_fd_bc_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
						PAUSE
					}
					//debug_printf("something wrong: fd = %d  error: %d \n", cfd, socketErr);
					//_map_fd_bc_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
					//PAUSE
					continue;
				}
			}
			
			if(events[i].events & EPOLLIN) {
				if (ntohs(addr.sin_port) == 8856) {
					u_long n = -1;
					ioctlsocket(cfd, FIONREAD, &n);
					//if (n > 8000) {
						//debug_printf("fd = %d  %d  %d \n", cfd, n, ++nin);
					//}
				}
				
				if (bc_ptr->handle_pkt_in(cfd) == RET_SOCK_ERROR) {
					if (cfd == pk_fd) {
						// TODO: handle pk socket
						debug_printf("pk socket error");
						*_errorRestartFlag = RESTART;
						return ;	// Once pk socket error, immediately restart
					}
					else if (cfd == log_fd) {
						// TODO: handle log-server socket
						debug_printf("log-server socket error");
						*_errorRestartFlag = RESTART;
						//PAUSE
					}
					else {
						debug_printf("%s %d, handle in sock error\n", __FUNCTION__, cfd);
						if(_map_fd_bc_tbl.find(cfd) != _map_fd_bc_tbl.end()){
							_map_fd_bc_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
						}
						close(cfd);
					}
                }
			}
			if(events[i].events & EPOLLOUT) {
				
				if (bc_ptr->handle_pkt_out(cfd) == RET_SOCK_ERROR) {
					if (cfd == pk_fd) {
						// TODO: handle pk socket 
						debug_printf("pk socket error");
						*_errorRestartFlag = RESTART;
						PAUSE
					}
					else if (cfd == log_fd) {
						// TODO: handle log-server socket
						debug_printf("log-server socket error");
						*_errorRestartFlag = RESTART;
						PAUSE
					}
					else {
						debug_printf("%s %d, handle out sock error\n", __FUNCTION__, cfd);
						if(_map_fd_bc_tbl.find(cfd) != _map_fd_bc_tbl.end()){
							_map_fd_bc_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
						}
						close(cfd);
					}
				}
			}
			if(events[i].events & ~(EPOLLIN | EPOLLOUT)) {
				//printf("%d error\n",cfd);
				bc_ptr->handle_pkt_error(cfd);		// error
				PAUSE
			}
		} else {
			// can't fd in map table?
		}
	}
}

void network::epoll_control(int sock, int op, unsigned int event) 
{
	struct epoll_event ev;
	int ret = 0;
	int ret2 = 0;
	memset(&ev, 0x0, sizeof(struct epoll_event));	
	set_nonblocking(sock);
	ev.data.fd = sock;
	ev.events = event | EPOLLRDHUP | EPOLLERR;			// We forece to monitor EPOLLRDHUP and EPOLLERR event.
	
	//debug_printf("sock %d op = %d event = %d \n", sock, op, event);
	if (sock > 65536) PAUSE

#ifdef _FIRE_BREATH_MOD_
	epoll_ctl(epfd, op, sock, &ev, &epollVar);
	ret = epoll_ctl(epfd, op, sock, &ev, &epollVar);
#else
	ret = epoll_ctl(epfd, op, sock, &ev);
	//ret2 = epoll_ctl(epfd, op, sock, &ev);
#endif

	//debug_printf("sock: %d,  ret = %d, ret2 = %d \n", sock, ret, ret2);
	
	if(ret2 == -1) {
		debug_printf("!!!epoll_ctl failed with sock:%d, errno:%d\n",sock, errno);
		//perror("failed with error");
	} else {
		//printf("!!!epoll_ctl success with sock:%d\n",sock);
	}
}


void network::set_nonblocking(int sock) 
{

#ifdef _WIN32
	u_long iMode = 1;
	ioctlsocket(sock, FIONBIO, &iMode);
#else
	fcntl(sock, F_SETFL, O_NONBLOCK | O_RDWR);
#endif

}

void network::set_blocking(int sock) 
{

#ifdef _WIN32
	u_long iMode = 0;
	ioctlsocket(sock, FIONBIO, &iMode);
#else
	fcntl(sock, F_SETFL, O_RDWR);
#endif

}

int network::socket(int domain, int type, int protocol) 
{
	int sock = ::socket(domain, type, protocol);
	assert(sock < MAXFDS);
	return sock;
}

int network::connect(int sock, const struct sockaddr *serv_addr, socklen_t addrlen) 
{
	return ::connect(sock, serv_addr, addrlen);
}

int network::connect_timeout(int sock, struct sockaddr *addr, size_t size_addr, int timeout) 
{
	return 0;
}

int network::bind(int sock, const struct sockaddr *my_addr, socklen_t addrlen) 
{
	return ::bind(sock, my_addr, addrlen);
}

int network::listen(int sock, int backlog) 
{
	return ::listen(sock, backlog);
}

int network::accept(int sock, struct sockaddr *addr, socklen_t *addrlen) 
{
	int new_fd = ::accept(sock, addr, addrlen);
	//assert(new_fd < MAXFDS);
	return new_fd;
}

int network::send(int s, const char *buf, size_t len, int flags) 
{
	int ret;
	if((ret = ::send(s, buf, len, flags)) > 0) {
		send_byte += (unsigned long long int) ret;
	}
	
	return ret;
}

int network::sendto(int s, const char *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	int ret;
	if((ret = ::sendto(s, buf, len, flags, to, tolen)) > 0) {
		send_byte += (unsigned long long int)ret;
	}
	return ret;
}

int network::recv(int s, char *buf, size_t len, int flags) 
{
	int ret;
	if((ret = ::recv(s, buf, len, flags)) > 0) {
		recv_byte += (unsigned long long int) ret;
	}
	return ret;
}

int network::recvfrom(int s, char *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	int ret;
	if((ret = ::recvfrom(s, buf, len, flags, from, fromlen)) > 0) {
		recv_byte += (unsigned long long int)ret;
	}
	return ret;
}

void network::peer_set(peer *peer_ptr)
{
	_peer_ptr = peer_ptr;		
}

void network::log_set(logger *log_ptr)
{
	_log_ptr = log_ptr;
}

//close "sock" in _map_fd_bc_tbl and erase the key-value
//delete sock in epoll_control
//shutdown socket and close socket
int network::close(int sock) 
{
	debug_printf("before close socket %d, _map_fd_bc_tbl.size() = %d \n", sock, _map_fd_bc_tbl.size());
	for (std::map<int, basic_class *>::iterator iter = _map_fd_bc_tbl.begin(); iter != _map_fd_bc_tbl.end(); iter++) {
		struct sockaddr_in src_addr;
		struct sockaddr_in dst_addr;
		int addrLen = sizeof(struct sockaddr_in);
		int	a;
		int b;
		char src_IP[16] = {0};
		char dst_IP[16] = {0};
		a = getsockname(iter->first, (struct sockaddr *)&src_addr, (socklen_t *)&addrLen);
		b = getpeername(iter->first, (struct sockaddr *)&dst_addr, (socklen_t *)&addrLen);
		memcpy(src_IP, inet_ntoa(src_addr.sin_addr), strlen(inet_ntoa(src_addr.sin_addr)));
		memcpy(dst_IP, inet_ntoa(dst_addr.sin_addr), strlen(inet_ntoa(dst_addr.sin_addr)));
		debug_printf(" | (%d) fd:%3d, SA: %s:%d | (%d) fd:%3d, DA: %s:%d | \n", a, iter->first, src_IP, ntohs(src_addr.sin_port),
																				b, iter->first, dst_IP, ntohs(dst_addr.sin_port));
	}
	
	_map_fd_bc_tbl_iter = _map_fd_bc_tbl.find(sock);
	
	if (_map_fd_bc_tbl_iter == _map_fd_bc_tbl.end()) {
		return -1;
	} 
	else {
		//delete _map_fd_bc_tbl_iter->second;
		_map_fd_bc_tbl.erase(_map_fd_bc_tbl_iter);
	}
	
	epoll_control(sock, EPOLL_CTL_DEL, 0);
	
	debug_printf("shutdown: %d \n", ::shutdown(sock, SHUT_RDWR));
#ifdef _WIN32
	::closesocket(sock);
	return 0;
#else
	int ret = ::close(sock);
	debug_printf("ret = %d \n", ret);
	return 0;
#endif

}

//這邊的nonblocking recv 沿用rtmp 原本Recv_nonblocking_ctl的定義方式 重新刻的
//狀態必須是READY or RUNNING 才能用這個function
int network::nonblock_recv(int sock, Nonblocking_Ctl* send_info)
{
	int recv_rt_val;

//	LARGE_INTEGER teststart,testend;

	recv_rt_val = recv(sock, send_info->recv_ctl_info.buffer + send_info->recv_ctl_info.offset, send_info->recv_ctl_info.expect_len, 0);
	//debug_printf("Recv %d(expected:%d) bytes to sock %d \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock);

	if (recv_rt_val < 0) {
		
		#ifdef _WIN32 
		int socket_error = WSAGetLastError();
		if (socket_error == WSAEWOULDBLOCK) {
		#else
		if (errno == EINTR || errno == EAGAIN) {
		#endif		
			if(send_info ->recv_packet_state == READ_HEADER_READY){
				send_info ->recv_packet_state = READ_HEADER_RUNNING;
				//printf("READ_HEADER_RUNNING\n");
			}else if(send_info ->recv_packet_state == READ_HEADER_RUNNING){
				//printf("READ_HEADER_RUNNING\n");
				//also RUNNING
			}else if(send_info ->recv_packet_state == READ_HEADER_OK){
				printf("READ_HEADER_OK");
				PAUSE
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_READY){
				send_info ->recv_packet_state = READ_PAYLOAD_RUNNING;
				//printf("READ_PAYLOAD_RUNNING\n");
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_RUNNING){
				//printf("READ_PAYLOAD_RUNNING\n");
				//also RUNNING
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_OK){
				printf("READ_PAYLOAD_OK");
				PAUSE
			}

			//send_info->recv_ctl_info.ctl_state = RUNNING;
			return RET_OK;
		} 
		else {
			debug_printf("recv %d byte from sock: %d, expected len = %d \n", recv_rt_val, sock, send_info->recv_ctl_info.expect_len);
			#ifdef _WIN32 
			debug_printf("WSAGetLastError() = %d \n", socket_error);
			#else
			#endif
			return RET_SOCK_ERROR;
		}
	} 
	// The connection has been gracefully closed
	else if (recv_rt_val == 0) {
		debug_printf("recv 0 byte from sock: %d, expected len = %d \n", sock, send_info->recv_ctl_info.expect_len);
		#ifdef _WIN32 
		int socket_error = WSAGetLastError();
		debug_printf("WSAGetLastError() = %d \n", socket_error);
		#else
		#endif
		if (send_info->recv_ctl_info.expect_len == 0) {

			if(send_info ->recv_packet_state == READ_HEADER_READY){
				printf("READ_HEADER_READY\n");
				PAUSE
			}else if(send_info ->recv_packet_state == READ_HEADER_RUNNING){
				send_info ->recv_packet_state = READ_HEADER_OK ;
				//printf("READ_HEADER_OK\n");

			}else if(send_info ->recv_packet_state == READ_HEADER_OK){
				printf("READ_HEADER_OK");
				PAUSE
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_READY){
				printf("READ_PAYLOAD_READY\n");
				PAUSE
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_RUNNING){
				send_info ->recv_packet_state = READ_PAYLOAD_OK ;
				//printf("READ_PAYLOAD_OK\n");
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_OK){
				printf("READ_PAYLOAD_OK");
				PAUSE
			}

			//send_info->recv_ctl_info.ctl_state = READY;
			return RET_SOCK_CLOSED_GRACEFUL;
		} else {

			if(send_info ->recv_packet_state == READ_HEADER_READY){
				send_info ->recv_packet_state = READ_HEADER_RUNNING;
				//printf("READ_HEADER_RUNNING\n");
			}else if(send_info ->recv_packet_state == READ_HEADER_RUNNING){
				//also RUNNING
				//printf("READ_HEADER_RUNNING\n");
			}else if(send_info ->recv_packet_state == READ_HEADER_OK){
				printf("READ_HEADER_OK");
				PAUSE
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_READY){
				send_info ->recv_packet_state = READ_PAYLOAD_RUNNING;
				//printf("READ_PAYLOAD_RUNNING\n");
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_RUNNING){
				//also RUNNING
				//printf("READ_PAYLOAD_RUNNING\n");
			}else if(send_info ->recv_packet_state ==READ_PAYLOAD_OK){
				printf("READ_PAYLOAD_OK");
				PAUSE
			}

			//send_info->recv_ctl_info.ctl_state = RUNNING;
			//here maybe a sock error
			return RET_SOCK_CLOSED_GRACEFUL;
			//return RET_OK;
		}
	} else if ((UINT32)recv_rt_val == send_info->recv_ctl_info.expect_len) {

		//QueryPerformanceCounter(&teststart);

		if(send_info ->recv_packet_state == READ_HEADER_READY){
			send_info ->recv_packet_state = READ_HEADER_OK;
			//printf("READ_HEADER_OK\n");
		}else if(send_info ->recv_packet_state == READ_HEADER_RUNNING){
			send_info ->recv_packet_state = READ_HEADER_OK;
			//printf("READ_HEADER_OK\n");
		}else if(send_info ->recv_packet_state == READ_HEADER_OK){
			printf("READ_HEADER_OK");
			PAUSE
		}else if(send_info ->recv_packet_state ==READ_PAYLOAD_READY){

			send_info ->recv_packet_state = READ_PAYLOAD_OK;
			//printf("READ_PAYLOAD_OK\n");
		}else if(send_info ->recv_packet_state ==READ_PAYLOAD_RUNNING){
			send_info ->recv_packet_state = READ_PAYLOAD_OK;
			//printf("READ_PAYLOAD_OK\n");
		}else if(send_info ->recv_packet_state ==READ_PAYLOAD_OK){
			printf("READ_PAYLOAD_OK");
			PAUSE
		}
		return recv_rt_val;
	} else {	
		send_info->recv_ctl_info.expect_len -= recv_rt_val;
		send_info->recv_ctl_info.offset += recv_rt_val;

		if(send_info ->recv_packet_state == READ_HEADER_READY){
			send_info ->recv_packet_state = READ_HEADER_RUNNING;
			//printf("READ_HEADER_RUNNING\n");
		}else if(send_info ->recv_packet_state == READ_HEADER_RUNNING){
			send_info ->recv_packet_state = READ_HEADER_RUNNING;
			//printf("READ_HEADER_RUNNING\n");
		}else if(send_info ->recv_packet_state == READ_HEADER_OK){
			printf("READ_HEADER_OK");
			PAUSE
		}else if(send_info ->recv_packet_state ==READ_PAYLOAD_READY){
			send_info ->recv_packet_state = READ_PAYLOAD_RUNNING;
			//printf("READ_PAYLOAD_RUNNING\n");
		}else if(send_info ->recv_packet_state ==READ_PAYLOAD_RUNNING){
			send_info ->recv_packet_state = READ_PAYLOAD_RUNNING;
			//printf("READ_PAYLOAD_RUNNING\n");
		}else if(send_info ->recv_packet_state ==READ_PAYLOAD_OK){
			printf("READ_PAYLOAD_OK");
			PAUSE
		}

		//send_info->recv_ctl_info.ctl_state = RUNNING;
		return recv_rt_val;
	}


}



int network::nonblock_send(int sock, Network_nonblocking_ctl* send_info)
{
	int send_rt_val;
	if (send_info->ctl_state == READY) {
		send_rt_val = send(sock, send_info->buffer + send_info->offset, send_info->expect_len, 0);
		//debug_printf("Send %d(expected:%d) bytes to sock %d \n", send_rt_val, send_info->expect_len, sock);
	
		if (send_rt_val < 0) {
#ifdef _WIN32 
			int socketErr = WSAGetLastError();
			if (socketErr == WSAEWOULDBLOCK) {
#else
			if (errno == EINTR || errno == EAGAIN) {
#endif
				send_info->ctl_state = RUNNING;
				return RET_OK;
			}
			else {
				debug_printf("send info to log server error : %d %d \n", send_rt_val, socketErr);
				return RET_SOCK_ERROR;
			}
		} else if (send_rt_val == 0) {
			if(send_info->expect_len == 0){
				send_info->ctl_state = READY;
				return RET_OK;
			}else{
				send_info->ctl_state = RUNNING;
				return RET_SOCK_ERROR;
			}
		}
		else if ((UINT32)send_rt_val == send_info->expect_len) {
			send_info->ctl_state = READY;
			return send_rt_val;

		} else {	
			send_info->expect_len -= send_rt_val;
			send_info->offset += send_rt_val;
			send_info->ctl_state = RUNNING;
			return send_rt_val;
		}
	} else { //_send_ctl_info._send_ctl_state is RUNNING
		//DBG_PRINTF("In RUNNING state\n");
		if (send_info->serial_num != send_info->chunk_ptr->header.sequence_number) { //check serial num
			printf("%s, recv returned RET_WRONG_SER_NUM\n", __FUNCTION__);
			return RET_WRONG_SER_NUM;
		}

		send_rt_val = send(sock, send_info->buffer + send_info->offset, send_info->expect_len, 0);
		//debug_printf("Send %d(expected:%d) bytes to sock %d \n", send_rt_val, send_info->expect_len, sock);
		
		if (send_rt_val < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if (errno == EINTR || errno == EAGAIN) {
#endif
				send_info->ctl_state = RUNNING;
				return RET_OK;
			} else {
				return RET_SOCK_ERROR;
			}
		} else if (send_rt_val == 0) {
			if(send_info->expect_len == 0){
				send_info->ctl_state = READY;
				return RET_OK;
			}else{
				send_info->ctl_state = RUNNING;
				return RET_SOCK_ERROR;
			}
		}
		else if ((UINT32)send_rt_val == send_info->expect_len) {
			send_info->ctl_state = READY;
			return send_rt_val;
		} else {	
			send_info->expect_len -= send_rt_val;
			send_info->offset += send_rt_val;
			send_info->ctl_state = RUNNING;
			return send_rt_val;
		}
	}
}

network::network(int * errorRestartFlag,list<int>  * fd_list) 
{
	send_byte = 0ULL;	// typeof(_send_byte) == unsigned long long int
	recv_byte = 0ULL;
	_errorRestartFlag = errorRestartFlag;
	_error_cfd = new std::queue<int>;
	if(!_error_cfd){
		printf("_error_cfd error !!!!!!!!!!!!!!\n");
	}
	fd_list_ptr = fd_list;
}

network::~network() 
{
	printf("==============deldet network success==========\n");
}

//================below not important=====================


void network::handle_rtmp_error(int sock) 
{
	_error_cfd->push(sock);
}


void network::eraseFdList(int sock) 
{
	list<int>::iterator fd_iter;
	for(fd_iter = fd_list_ptr ->begin() ;fd_iter != fd_list_ptr->end();fd_iter++){
		if(*fd_iter == sock) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
}
