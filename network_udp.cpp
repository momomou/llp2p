#include "network_udp.h"
#include "peer.h"
//#include "udt_lib/udt.h"

using namespace UDT;

void network_udp::timer() 
{
	send_byte = 0ULL;
	recv_byte = 0ULL;
	epfd = 0;
}

void network_udp::setall_fd_epollout()
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

void network_udp::garbage_collection() 
{
	for (std::map<int, basic_class *>::iterator iter = _map_fd_bc_tbl.begin(); iter != _map_fd_bc_tbl.end(); iter++) {
		debug_printf("size = %lu, fd = %d \n", _map_fd_bc_tbl.size(), iter->first);
	}
	for (std::map<int, basic_class *>::iterator iter = _map_fd_bc_tbl.begin(); iter != _map_fd_bc_tbl.end(); ) {
		close(iter->first);
		iter = _map_fd_bc_tbl.begin();
	}
	_map_fd_bc_tbl.clear();

	UDT::epoll_release(epfd);
}

unsigned long network_udp::getLocalIpv4() 
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
	
	// Search all network_udp adapters
	for(AI = AddrInfo; AI != NULL; AI = AI->ai_next) {
		switch(AI->ai_family) {
			case AF_INET:	// ipv4
				ip = ((struct sockaddr_in *)AI->ai_addr)->sin_addr.s_addr;
				struct in_addr localIP;
				memcpy(&localIP, &ip, sizeof(struct in_addr));
				debug_printf("network_udp::getLocalIpv4 ip: %s \n", inet_ntoa(localIP));
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


void network_udp::set_fd_bcptr_map(int sock, basic_class *bcptr)
{
	_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[INSERT _map_fd_bc_tbl]", sock, bcptr);
	_map_fd_bc_tbl[sock] = bcptr;
}

void network_udp::delete_fd_bcptr_map(int sock)
{
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERASE _map_fd_bc_tbl]", sock);
	_map_fd_bc_tbl.erase(sock);
}

void network_udp::epoll_creater(void) 
{
#ifdef _FIRE_BREATH_MOD_
	//epfd = epoll_create(MAXFDS, &epollVar);
	epfd = UDT::epoll_create();
#else
	epfd = UDT::epoll_create();
#endif
}

#ifdef _WIN32
void network_udp::epoll_waiter(int timeout, list<int> *fd_list) 
{
	int cfd;
	basic_class *bc_ptr;
	//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "1");
#ifdef _FIRE_BREATH_MOD_
	//nfds = epoll_wait(epfd, events, EVENTSIZE, timeout, fd_list, &epollVar);
	nfds = UDT::epoll_wait(epfd, &readfds, &writefds, timeout, NULL);
#else
	//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "2");
	nfds = UDT::epoll_wait(epfd, &readfds, &writefds, timeout, NULL, NULL);
#endif
	//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "3");
	
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
void network_udp::epoll_waiter(int timeout, list<int> *fd_list)
{
	int cfd;
	basic_class *bc_ptr;
	//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "1");
#ifdef _FIRE_BREATH_MOD_
	//nfds = epoll_wait(epfd, events, EVENTSIZE, timeout, fd_list, &epollVar);
	nfds = UDT::epoll_wait(epfd, &readfds, &writefds, timeout, NULL);
#else
	//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "2");
	nfds = UDT::epoll_wait(epfd, &readfds, &writefds, timeout, NULL, NULL);
#endif
	//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "3");

	if (nfds == -1){
		//printf("epoll_wait failed\n");
		if (_error_cfd->size() == 0){ return; }
		cfd = _error_cfd->front();
		_error_cfd->pop();
		if (cfd != 0){
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

#endif

void network_udp::epoll_dispatcher(void) 
{
	int fd;
	basic_class *bc_ptr;
	
	
	if (readfds.size() > 0 || writefds.size() > 0) {
	//if (readfds.size() > 0) {
		//_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "readfds.size()", readfds.size(), "writefds.size()", writefds.size());
	}

	for(set<UDTSOCKET>::iterator i = writefds.begin(); i != writefds.end(); ++i) {
		fd = *i;
		//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "fd", fd);
		if (_map_fd_bc_tbl.find(fd) != _map_fd_bc_tbl.end()) {
			if (_map_fd_bc_tbl[fd]->handle_pkt_out_udp(fd) == RET_SOCK_ERROR) {
				debug_printf("handle_pkt_out_udp::RET_SOCK_ERROR  sock: %d \n", fd);
				close(fd);
				//PAUSE
			}
		}
		//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "fd", fd);
	}

	for (set<UDTSOCKET>::iterator i = readfds.begin(); i != readfds.end(); ++i) {
		fd = *i;
		/*
		struct sockaddr_in src_addr;
		struct sockaddr_in dst_addr;
		int addrLen = sizeof(struct sockaddr_in);
		int	a;
		int b;
		char src_IP[16] = { 0 };
		char dst_IP[16] = { 0 };
		a = getsockname(fd, (struct sockaddr *)&src_addr, (socklen_t *)&addrLen);
		b = getpeername(fd, (struct sockaddr *)&dst_addr, (socklen_t *)&addrLen);
		memcpy(src_IP, inet_ntoa(src_addr.sin_addr), strlen(inet_ntoa(src_addr.sin_addr)));
		memcpy(dst_IP, inet_ntoa(dst_addr.sin_addr), strlen(inet_ntoa(dst_addr.sin_addr)));
		debug_printf(" | (%d) fd:%3d, ME: %s:%d | (%d) fd:%3d, HIM: %s:%d | \n", a, fd, src_IP, ntohs(src_addr.sin_port),
																			b, fd, dst_IP, ntohs(dst_addr.sin_port));
		*/
		//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "fd", fd);
		_map_fd_bc_tbl_iter = _map_fd_bc_tbl.find(fd);
		if (_map_fd_bc_tbl.find(fd) != _map_fd_bc_tbl.end()) {
			if (_map_fd_bc_tbl[fd]->handle_pkt_in_udp(fd) == RET_SOCK_ERROR) {
				debug_printf("handle_pkt_in_udp::RET_SOCK_ERROR  sock: %d \n", fd);
				close(fd);
				//PAUSE
			}
		}
		//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "fd", fd);
	}

	/*
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
						PAUSE
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
						PAUSE
					}
					else if (cfd == log_fd) {
						// TODO: handle log-server socket
						debug_printf("log-server socket error");
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
	*/
}

void network_udp::epoll_control(int sock, int op, unsigned int events) 
{
	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "op", op, "events", events);
	if (op == EPOLL_CTL_ADD) {
		int eventss = events;
		if (UDT::epoll_add_usock(epfd, sock, &eventss) == UDT::ERROR) {
			// 曾經發生 20140814
			_log_ptr->write_log_format("s(u) s d d s \n", __FUNCTION__, __LINE__, "sock", sock, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
			//PAUSE
		}
	} 
	else if (op == EPOLL_CTL_DEL) {
		int eventss = events;
		if (UDT::epoll_remove_usock(epfd, sock) == UDT::ERROR) {
			_log_ptr->write_log_format("s(u) s d d s \n", __FUNCTION__, __LINE__, "sock", sock, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
			//PAUSE
		}
	}
	/*
	else if (op == EPOLL_CTL_MOD) {
		int eventss = events;
		if (UDT::epoll_remove_usock(epfd, sock) == UDT::ERROR) {
			cout <<  UDT::getlasterror().getErrorMessage() << endl;
			PAUSE
		}
		if (UDT::epoll_add_usock(epfd, sock, &eventss) == UDT::ERROR) {
			cout <<  UDT::getlasterror().getErrorMessage() << endl;
			PAUSE
		}
	}
	*/
	/*
	struct epoll_event ev;
	int ret = 0;
	int ret2 = 0;
	memset(&ev, 0x0, sizeof(struct epoll_event));	
	set_nonblocking(sock);
	ev.data.fd = sock;
	ev.events = event | EPOLLRDHUP | EPOLLERR;			// We forece to monitor EPOLLRDHUP and EPOLLERR event.
	
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
	*/
}

int network_udp::GetSockState(int sock)
{
	return UDT::getsockstate(sock);
}

void network_udp::set_nonblocking(int sock) 
{
	
	bool block = false;
	UDT::setsockopt(sock, 0, UDT_SNDSYN, &block, sizeof(bool));
	UDT::setsockopt(sock, 0, UDT_RCVSYN, &block, sizeof(bool));
	
}

void network_udp::set_blocking(int sock) 
{
	bool block = true;
	UDT::setsockopt(sock, 0, UDT_SNDSYN, &block, sizeof(bool));
	UDT::setsockopt(sock, 0, UDT_RCVSYN, &block, sizeof(bool));
}

void network_udp::set_rendezvous(int sock)
{
	bool rendezvous = true;
	UDT::setsockopt(sock, 0, UDT_RENDEZVOUS, &rendezvous, sizeof(bool));
}


int network_udp::socket(int af, int type, int protocol) 
{
	return UDT::socket(af, type, protocol);
}

int network_udp::connect(int sock, const struct sockaddr *serv_addr, socklen_t addrlen) 
{
	return UDT::connect(sock, serv_addr, addrlen);
}

int network_udp::connect_timeout(int sock, struct sockaddr *addr, size_t size_addr, int timeout) 
{
	return 0;
}

int network_udp::bind(int sock, const struct sockaddr *my_addr, socklen_t addrlen) 
{
	return UDT::bind(sock, my_addr, addrlen);
}

int network_udp::listen(int sock, int backlog) 
{
	return ::listen(sock, backlog);
}

int network_udp::accept(int sock, struct sockaddr *addr, socklen_t *addrlen) 
{
	int new_fd = ::accept(sock, addr, addrlen);
	//assert(new_fd < MAXFDS);
	return new_fd;
}

int network_udp::send(int s, const char *buf, size_t len, int flags) 
{
	return UDT::send(s, buf, len, flags);
}

int network_udp::sendto(int s, const char *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	int ret;
	if((ret = ::sendto(s, buf, len, flags, to, tolen)) > 0) {
		send_byte += (unsigned long long int)ret;
	}
	return ret;
}

int network_udp::recv(int s, char *buf, size_t len, int flags) 
{
	int ret;
	if((ret = UDT::recv(s, buf, len, flags)) > 0) {
		recv_byte += (unsigned long long int) ret;
	}
	return ret;
}

int network_udp::recvfrom(int s, char *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	int ret;
	if((ret = ::recvfrom(s, buf, len, flags, from, fromlen)) > 0) {
		recv_byte += (unsigned long long int)ret;
	}
	return ret;
}

void network_udp::peer_set(peer *peer_ptr)
{
	_peer_ptr = peer_ptr;		
}

void network_udp::log_set(logger *log_ptr)
{
	_log_ptr = log_ptr;
}

//close "sock" in _map_fd_bc_tbl and erase the key-value
//delete sock in epoll_control
//shutdown socket and close socket
// Blocking close may operate for more than 45 seconds
int network_udp::close(int sock) 
{
	int ret;
	int sock_state;
	int32_t pendingpkt_size = 0;
	int len = 0;
	//set_blocking(sock);
	
	// Must remove usock before close
	//if ((ret = UDT::epoll_remove_usock(epfd, sock)) != 0) {
	//	debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
	//	PAUSE
	//}
	epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN|EPOLLOUT);

	//delete_fd_bcptr_map(sock);


	sock_state = UDT::getsockstate(sock);
	len = sizeof(pendingpkt_size);
	UDT::getsockopt(sock, 0, UDT_SNDDATA, &pendingpkt_size, &len);
	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "is closing, state", sock_state, "pending packet", pendingpkt_size);

	if ((ret = UDT::close(sock)) == UDT::ERROR) {
		_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "[DEBUG] ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
	}

	sock_state = UDT::getsockstate(sock);
	len = sizeof(pendingpkt_size);
	UDT::getsockopt(sock, 0, UDT_SNDDATA, &pendingpkt_size, &len);
	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "sock", sock, "is closing, state", sock_state, "pending packet", pendingpkt_size);

	delete_fd_bcptr_map(sock);

	return 0;
}

//這邊的nonblocking recv 沿用rtmp 原本Recv_nonblocking_ctl的定義方式 重新刻的
//狀態必須是READY or RUNNING 才能用這個function
int network_udp::nonblock_recv(int sock, Nonblocking_Ctl* send_info)
{
	int recv_rt_val;

	recv_rt_val = recv(sock, send_info->recv_ctl_info.buffer + send_info->recv_ctl_info.offset, send_info->recv_ctl_info.expect_len, 0);
	//debug_printf("recv %d byte from sock: %d, expected len = %d \n", recv_rt_val, sock, send_info->recv_ctl_info.expect_len);

	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "Recv", recv_rt_val, "bytes from sock", sock, "expected len", send_info->recv_ctl_info.expect_len);
	
	//UDT::TRACEINFO trace;
	//memset(&trace, 0, sizeof(UDT::TRACEINFO));
	//int nnn = UDT::perfmon(sock, &trace, false);
	//_log_ptr->write_log_format("s(u) d s d s s \n", __FUNCTION__, __LINE__, nnn, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
	//_log_ptr->write_log_format("s(u) s d s f s f s f s d \n", __FUNCTION__, __LINE__, "sock", sock, "Rrate", trace.mbpsRecvRate, "Erate", trace.mbpsBandwidth, "RTT", trace.msRTT, "onFlight", trace.pktFlightSize);

	if (recv_rt_val < 0) {
		int sock_state = UDT::getsockstate(sock);
		switch (sock_state) {
			case UDTSTATUS::INIT:
				//debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
				break;
			case UDTSTATUS::OPENED:
				//debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
				break;
			case UDTSTATUS::LISTENING:
				//debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
				break;
			case UDTSTATUS::CONNECTING:
				//debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
				break;
			case UDTSTATUS::CONNECTED:
				//debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				// Same as WSAEWOULDBLOCK
				//debug_printf("state: %d, ErrCode: %d, ErrMsg: %s \n", sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				if (send_info->recv_packet_state == READ_HEADER_READY) {
					send_info->recv_packet_state = READ_HEADER_RUNNING;
				}
				else if (send_info->recv_packet_state == READ_HEADER_RUNNING) {

				}
				else if (send_info->recv_packet_state == READ_HEADER_OK) {
					debug_printf("READ_HEADER_OK");
					PAUSE
				}
				else if (send_info->recv_packet_state == READ_PAYLOAD_READY) {
					send_info->recv_packet_state = READ_PAYLOAD_RUNNING;
				}
				else if (send_info->recv_packet_state == READ_PAYLOAD_RUNNING) {

				}
				else if (send_info->recv_packet_state == READ_PAYLOAD_OK) {
					debug_printf("READ_PAYLOAD_OK");
					PAUSE
				}
				return RET_OK;
				break;
			case UDTSTATUS::BROKEN:
				debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				close(sock);
				return RET_SOCK_ERROR;
				break;
			case UDTSTATUS::CLOSING:
				debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
				break;
			case UDTSTATUS::CLOSED:
				debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				close(sock);
				return RET_SOCK_ERROR;
				break;
			case UDTSTATUS::NONEXIST:
				debug_printf("Recv %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", recv_rt_val, send_info->recv_ctl_info.expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				close(sock);
				return RET_SOCK_ERROR;
				break;
			default:
				PAUSE
				break;
		}

	} 
	// The connection has been gracefully closed
	else if (recv_rt_val == 0) {
		int sock_state = UDT::getsockstate(sock);
		
		
		if (sock_state == UDTSTATUS::CONNECTED) {
			// UDT 有可能發生
			return RET_OK;
		}
		
		debug_printf("recv %d byte from sock: %d, expected len = %d \n", recv_rt_val, sock, send_info->recv_ctl_info.expect_len);
		debug_printf("state: %d, ErrCode: %d, ErrMsg: %s \n", sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());

		PAUSE

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

		//debug_printf("recv_packet_state %d \n", send_info->recv_packet_state);
		if(send_info ->recv_packet_state == READ_HEADER_READY){
			send_info ->recv_packet_state = READ_HEADER_OK;
			struct chunk_header_t* chunk_header_ptr = (chunk_header_t *)(send_info->recv_ctl_info.buffer + send_info->recv_ctl_info.offset);
			//debug_printf("cmd %d  len %d \n", chunk_header_ptr->cmd, chunk_header_ptr->length);
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
		//debug_printf("recv_packet_state %d \n", send_info->recv_packet_state);
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


// Sending buffer 滿的時候無法 writable, 如果是 connection broken 的話則依然可以 writable
int network_udp::nonblock_send(int sock, Network_nonblocking_ctl* send_info)
{
	int send_rt_val;
	//debug_printf("send_info->ctl_state = %d \n", send_info->ctl_state);
	if (send_info->ctl_state == READY) {

		send_rt_val = send(sock, send_info->buffer + send_info->offset, send_info->expect_len, 0);
		
		int udt_sndbuf;
		int len1 = sizeof(udt_sndbuf);
		int udp_sndbuf;
		int len2 = sizeof(udp_sndbuf);
		UDT::getsockopt(sock, 0, UDT_SNDBUF, &udt_sndbuf, &len1);
		UDT::getsockopt(sock, 0, UDP_SNDBUF, &udp_sndbuf, &len2);
		_log_ptr->write_log_format("s(u) s d(u) s u s u s u s s d d \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", UDT::getsockstate(sock), "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage(), udt_sndbuf, udp_sndbuf);
		
		UDT::TRACEINFO trace;
		//memset(&trace, 0, sizeof(UDT::TRACEINFO));
		//int nnn = UDT::perfmon(sock, &trace, false);
		//_log_ptr->write_log_format("s(u) d s d s s \n", __FUNCTION__, __LINE__, nnn, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		//_log_ptr->write_log_format("s(u) s d s f s f s f s d \n", __FUNCTION__, __LINE__, "sock", sock, "Srate", trace.mbpsSendRate, "Erate", trace.mbpsBandwidth, "RTT", trace.msRTT, "onFlight", trace.pktFlightSize);

		if (send_rt_val < 0) {
			int sock_state = UDT::getsockstate(sock);
			switch (sock_state) {
				case UDTSTATUS::INIT:
					debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
					PAUSE
					break;
				case UDTSTATUS::OPENED:
					debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
					PAUSE
					break;
				case UDTSTATUS::LISTENING:
					debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
					PAUSE
					break;
				case UDTSTATUS::CONNECTING:
					debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
					PAUSE
					break;
				case UDTSTATUS::CONNECTED:
					_log_ptr->write_log_format("s(u) s d(u) s u s u s u s s \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", sock_state, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
					if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCRCV) {
						send_info->ctl_state = RUNNING;
						return RET_OK;
					}
					else {
						debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
						PAUSE
					}
					break;
				case UDTSTATUS::BROKEN:
					_log_ptr->write_log_format("s(u) s d(u) s u s u s u s s \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", sock_state, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
					//close(sock);
					//_peer_ptr->CloseSocketUDP(sock, false, "Connection broken");
					return RET_SOCK_ERROR;
				case UDTSTATUS::CLOSING:
					debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
					PAUSE
					break;
				case UDTSTATUS::CLOSED:
					debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
					//PAUSE
					//break;
					return RET_SOCK_ERROR;
				case UDTSTATUS::NONEXIST:
					_log_ptr->write_log_format("s(u) s d(u) s u s u s u s s \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", sock_state, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
					//PAUSE
					return RET_SOCK_ERROR;
					break;
				default:
					PAUSE
					break;
			}
		}
		else if (send_rt_val == 0) {
			int sock_state = UDT::getsockstate(sock);
			_log_ptr->write_log_format("s(u) s u(u) s u s u s u s s \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", sock_state, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
			if (send_info->expect_len == 0){
				send_info->ctl_state = READY;
				return RET_OK;
			}else{
				send_info->ctl_state = RUNNING;
				return RET_SOCK_ERROR;
			}
		}
		else if ((UINT32)send_rt_val == send_info->expect_len) {
			send_info->ctl_state = READY;
			struct chunk_header_t* chunk_header_ptr = (chunk_header_t *)(send_info->buffer + send_info->offset);
			//debug_printf("cmd %d  len %d \n", chunk_header_ptr->cmd, chunk_header_ptr->length);
			return send_rt_val;
		}
		else {
			// Usually is that the sending buffer is not available, close the socket
			_peer_ptr->CloseSocketUDP(sock, true, "Run out of sending buffer");
			int sock_state = UDT::getsockstate(sock);
			debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
			send_info->expect_len -= send_rt_val;
			send_info->offset += send_rt_val;
			send_info->ctl_state = RUNNING;
			//PAUSE

			return send_rt_val;
		}
	} 
	else {
		if (send_info->serial_num != send_info->chunk_ptr->header.sequence_number) { //check serial num
			printf("%s, recv returned RET_WRONG_SER_NUM\n", __FUNCTION__);
			return RET_WRONG_SER_NUM;
		}

		send_rt_val = send(sock, send_info->buffer + send_info->offset, send_info->expect_len, 0);
		int udt_sndbuf;
		int len1 = sizeof(udt_sndbuf);
		int udp_sndbuf;
		//int len2 = sizeof(udp_sndbuf);
		UDT::getsockopt(sock, 0, UDT_SNDBUF, &udt_sndbuf, &len1);
		//UDT::getsockopt(sock, 0, UDP_SNDBUF, &udp_sndbuf, &len2);
		_log_ptr->write_log_format("s(u) s u(u) s u s u s u s u d \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", UDT::getsockstate(sock), "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage(), udt_sndbuf);

		//UDT::TRACEINFO trace;
		//memset(&trace, 0, sizeof(UDT::TRACEINFO));
		//int nnn = UDT::perfmon(sock, &trace, false);
		//_log_ptr->write_log_format("s(u) d s d s s \n", __FUNCTION__, __LINE__, nnn, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		//_log_ptr->write_log_format("s(u) s d s f s f s f s d \n", __FUNCTION__, __LINE__, "sock", sock, "Srate", trace.mbpsSendRate, "Erate", trace.mbpsBandwidth, "RTT", trace.msRTT, "onFlight", trace.pktFlightSize);


		if (send_rt_val < 0) {
			int sock_state = UDT::getsockstate(sock);
			switch (sock_state) {
			case UDTSTATUS::INIT:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
					break;
			case UDTSTATUS::OPENED:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
					break;
			case UDTSTATUS::LISTENING:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
					break;
			case UDTSTATUS::CONNECTING:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
					break;
			case UDTSTATUS::CONNECTED:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCRCV) {
					send_info->ctl_state = RUNNING;
					return RET_OK;
				}
				else {
					PAUSE
				}
				break;
			case UDTSTATUS::BROKEN:
				_log_ptr->write_log_format("s(u) s u(u) s u s u s u s u \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", sock_state, "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
				close(sock);
				return RET_SOCK_ERROR;
				break;
			case UDTSTATUS::CLOSING:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
					break;
			case UDTSTATUS::CLOSED:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				//PAUSE
				break;
			case UDTSTATUS::NONEXIST:
				debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
					break;
			default:
				PAUSE
					break;
			}

		}
		else if (send_rt_val == 0) {
			if (send_info->expect_len == 0){
				send_info->ctl_state = READY;
				return RET_OK;
			}
			else{
				_log_ptr->write_log_format("s(u) s u(u) s u s u s u s u \n", __FUNCTION__, __LINE__, "Send", send_rt_val, send_info->expect_len, "bytes to", sock, "state", UDT::getsockstate(sock), "ErrCode", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
				send_info->ctl_state = RUNNING;
				return RET_SOCK_ERROR;
			}
		}
		else if ((UINT32)send_rt_val == send_info->expect_len) {
			send_info->ctl_state = READY;
			return send_rt_val;

		}
		else {
			int sock_state = UDT::getsockstate(sock);
			debug_printf("Send %d(expected:%d) bytes to sock %d, state: %d, ErrCode: %d, ErrMsg: %s \n", send_rt_val, send_info->expect_len, sock, sock_state, UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
			send_info->expect_len -= send_rt_val;
			send_info->offset += send_rt_val;
			send_info->ctl_state = RUNNING;
			//PAUSE
			return send_rt_val;
		}




		/*
		if (send_rt_val < 0) {
			if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCRCV) {
				send_info->ctl_state = RUNNING;
				return RET_OK;
			}
			else {
				debug_printf("Recv %d(expected:%d) bytes to sock %d \n", send_rt_val, send_info->expect_len, sock);
				debug_printf("Sock state: %d \n", UDT::getsockstate(sock));
				debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
				PAUSE
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
		*/
	}
}

network_udp::network_udp(int * errorRestartFlag,list<int>  * fd_list) 
{
	send_byte = 0ULL;	// typeof(_send_byte) == unsigned long long int
	recv_byte = 0ULL;
	_errorRestartFlag = errorRestartFlag;
	_error_cfd = new std::queue<int>;
	if(!_error_cfd){
		printf("_error_cfd error !!!!!!!!!!!!!!\n");
	}
	fd_list_ptr = fd_list;
	epfd = 0;
	nfds = 0;
}

network_udp::network_udp(void) 
{
	
}

network_udp::~network_udp() 
{
	debug_printf("Have deleted network_udp \n");
}

//================below not important=====================


void network_udp::handle_rtmp_error(int sock) 
{
	_error_cfd->push(sock);
}


void network_udp::eraseFdList(int sock) 
{
	list<int>::iterator fd_iter;
	for(fd_iter = fd_list_ptr ->begin() ;fd_iter != fd_list_ptr->end();fd_iter++){
		if(*fd_iter == sock) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
}
