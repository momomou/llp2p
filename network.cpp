#include "network.h"
#include "peer.h"


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
	
	//if (_map_fd_bc_tbl.size()) {
		for(_map_fd_bc_tbl_iter = _map_fd_bc_tbl.begin() ; _map_fd_bc_tbl_iter != _map_fd_bc_tbl.end() ; _map_fd_bc_tbl_iter++) {
			close(_map_fd_bc_tbl_iter->first);
		}
	//}

#ifdef _WIN32
	::closesocket(epfd);
	::WSACleanup();
#else
	::close(epfd);
#endif

}


unsigned long network::getLocalIpv4() 
{
	
	unsigned long ip = 0UL;
	
#ifdef _WIN32	// WIN32 我們使用 domain 反查方式來處理 ip, 不使用 WIN32 API 
	char hostname[512];

	memset(hostname, 0x0, 512);
	gethostname(hostname, sizeof(hostname));
	
	struct addrinfo *AddrInfo, *AI;
	
	if(getaddrinfo(hostname, NULL, NULL, &AddrInfo) != 0) {
		return ip;	// error return 0UL
    }	

	for(AI = AddrInfo; AI != NULL; AI = AI->ai_next) {
		switch(AI->ai_family) {
			case AF_INET:
				ip = ((struct sockaddr_in *)AI->ai_addr)->sin_addr.s_addr;
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
void network::fd_del_hdl_map_set(int sock, basic_class *bcptr)
{
	_map_fd_del_hdl_tbl[sock] = bcptr;
}
void network::fd_del_hdl_map_delete(int sock)
{
	_map_fd_del_hdl_tbl.erase(sock);
}

void network::epoll_creater(void) 
{
	epfd = epoll_create(MAXFDS);
}

#ifdef _WIN32
void network::epoll_waiter(int timeout, list<int> *fd_list) 
{
	int cfd;
	basic_class *bc_ptr;

	nfds = epoll_wait(epfd, events, EVENTSIZE, timeout, fd_list);
	if(nfds == -1){
        //printf("epoll_wait failed\n");
		cfd = _error_cfd->front();
		_error_cfd->pop();
		if(cfd != 0){
			_map_fd_bc_tbl_iter = _map_fd_bc_tbl.find(cfd);
			if (_map_fd_bc_tbl_iter != _map_fd_bc_tbl.end()) {
                //printf("handle sock error\n");
				bc_ptr = _map_fd_bc_tbl_iter->second;
				_map_fd_del_hdl_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
			}
		}
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
			//DBG_PRINTF("here\n");
			bc_ptr = _map_fd_bc_tbl_iter->second;

			if (events[i].events & (EPOLLRDHUP | EPOLLERR)) {
				// EPOLLRDHUP => This socket is closed by client (client has sent a FIN), we have to close it.
				cout << "something wrong: fd = " << cfd << endl;
				//PAUSE
				_map_fd_del_hdl_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
				_peer_ptr->data_close(cfd, "This socket is closed by client (client has sent a FIN)");
				continue;
			}
			
			if(events[i].events & EPOLLIN)
				if (bc_ptr->handle_pkt_in(cfd) == RET_SOCK_ERROR) {		// readable
                    printf("%s,handle in sock error\n",__FUNCTION__);
					if(_map_fd_del_hdl_tbl.find(cfd) != _map_fd_del_hdl_tbl.end()){
						_map_fd_del_hdl_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
					}
					close(cfd);
                }

			if(events[i].events & EPOLLOUT)
				if (bc_ptr->handle_pkt_out(cfd) == RET_SOCK_ERROR) {		// writable
					printf("%s,handle out sock error\n",__FUNCTION__);
					if(_map_fd_del_hdl_tbl.find(cfd) != _map_fd_del_hdl_tbl.end()){
						_map_fd_del_hdl_tbl[cfd]->handle_sock_error(cfd, bc_ptr);
					}
					close(cfd);
				}

			if(events[i].events & ~(EPOLLIN | EPOLLOUT))
				bc_ptr->handle_pkt_error(cfd);		// error

		} else {
			// can't fd in map table?
		}
	}
}

void network::epoll_control(int sock, int op, unsigned int event) 
{
	struct epoll_event ev;
	int ret;
	memset(&ev, 0x0, sizeof(struct epoll_event));	
	set_nonblocking(sock);
	ev.data.fd = sock;
	ev.events = event | EPOLLRDHUP | EPOLLERR;			// We forece to monitor EPOLLRDHUP and EPOLLERR event.
	epoll_ctl(epfd, op, sock, &ev);
	ret = epoll_ctl(epfd, op, sock, &ev);
	if(ret == -1) {
		printf("!!!epoll_ctl failed with sock:%d, errno:%d\n",sock, errno);
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

//close "sock" in _map_fd_bc_tbl and erase the key-value
//delete sock in epoll_control
//shutdown socket and close socket
int network::close(int sock) 
{
	DBG_PRINTF("here\n");

	_map_fd_bc_tbl_iter = _map_fd_bc_tbl.find(sock);
	
	if (_map_fd_bc_tbl_iter == _map_fd_bc_tbl.end()) {
		return -1;
	} else {
		_map_fd_bc_tbl.erase(_map_fd_bc_tbl_iter);
		//cout << "erase fd" << endl;
		//PAUSE
	}
	
	epoll_control(sock, EPOLL_CTL_DEL, 0);
	cout << "fd_size = " << _map_fd_bc_tbl.size() << endl;
	
	::shutdown(sock, SHUT_RDWR);
#ifdef _WIN32
	return ::closesocket(sock);
#else
	return ::close(sock);
#endif

}

int network::nonblock_recv(int sock, Recv_nonblocking_ctl* send_info)
{
	int recv_rt_val;
	if (send_info->recv_ctl_info.ctl_state == READY) {
		recv_rt_val = recv(sock, send_info->recv_ctl_info.buffer + send_info->recv_ctl_info.offset, send_info->recv_ctl_info.expect_len, 0);
		//DBG_PRINTF("%s: send_rt = %d, expect_len = %d", __FUNCTION__, send_rt_val, send_info->expect_len);
	
		if (recv_rt_val < 0) {
			if (errno == EINTR | errno == EAGAIN) {		//這邊寫得怪怪的
				send_info->recv_ctl_info.ctl_state = RUNNING;
				return RET_OK;
			} else {
				return RET_SOCK_ERROR;
			}
		} else if (recv_rt_val == 0) {
			cout << "recv 0 byte from sock: " << sock << ", expect len = " << send_info->recv_ctl_info.expect_len << endl;
			if (send_info->recv_ctl_info.expect_len == 0) {
				send_info->recv_ctl_info.ctl_state = READY;
				return RET_OK;
			} else {
				send_info->recv_ctl_info.ctl_state = RUNNING;
				return RET_OK;
			}
		}
		else if (recv_rt_val == send_info->recv_ctl_info.expect_len) {
			return recv_rt_val;
		} else {	
			send_info->recv_ctl_info.expect_len -= recv_rt_val;
			send_info->recv_ctl_info.offset += recv_rt_val;
			send_info->recv_ctl_info.ctl_state = RUNNING;
			return recv_rt_val;
		}
	} else { //_send_ctl_info._send_ctl_state is RUNNING
		//DBG_PRINTF("In RUNNING state\n");
		recv_rt_val = recv(sock, send_info->recv_ctl_info.buffer + send_info->recv_ctl_info.offset, send_info->recv_ctl_info.expect_len, 0);
		
		//Log(LOGDEBUG, "%s: offset = %d, send_rt_val = %d, expect_len = %d", __FUNCTION__, _send_ctl_info.offset, send_rt_val, _send_ctl_info.expect_len);
		if (recv_rt_val < 0) {
			if ( errno == EINTR | errno == EAGAIN) {
				send_info->recv_ctl_info.ctl_state = RUNNING;
				return RET_OK;
			} else {
				return RET_SOCK_ERROR;
			}
		} else if (recv_rt_val == 0) {
			cout << "recv 0 byte from sock: " << sock << ", expect len = " << send_info->recv_ctl_info.expect_len << endl;
			if (send_info->recv_ctl_info.expect_len == 0) {
				send_info->recv_ctl_info.ctl_state = READY;
				return RET_OK;
			} else {
				send_info->recv_ctl_info.ctl_state = RUNNING;
				return RET_OK;
			}
		}
		else if (recv_rt_val == send_info->recv_ctl_info.expect_len) {
			send_info->recv_ctl_info.ctl_state = READY;
			return recv_rt_val;
		} else {	
			send_info->recv_ctl_info.expect_len -= recv_rt_val;
			send_info->recv_ctl_info.offset += recv_rt_val;
			send_info->recv_ctl_info.ctl_state = RUNNING;
			return recv_rt_val;
		}
	}
}

int network::nonblock_send(int sock, Network_nonblocking_ctl* send_info)
{
	int send_rt_val;
	if (send_info->ctl_state == READY) {
		send_rt_val = send(sock, send_info->buffer + send_info->offset, send_info->expect_len, 0);
		//DBG_PRINTF("%s: send_rt = %d, expect_len = %d", __FUNCTION__, send_rt_val, send_info->expect_len);
	
		if (send_rt_val < 0) {
			if (errno == EINTR | errno == EAGAIN) {
				send_info->ctl_state = RUNNING;
				return RET_OK;
			} else {
				return RET_SOCK_ERROR;
			}
		} else if (send_rt_val == 0) {
			send_info->ctl_state = RUNNING;
			return RET_OK;
		}
		else if (send_rt_val == send_info->expect_len) {
			return send_rt_val;
		} else {	
			send_info->expect_len -= send_rt_val;
			send_info->offset += send_rt_val;
			send_info->ctl_state = RUNNING;
			return send_rt_val;
		}
	} else { //_send_ctl_info._send_ctl_state is RUNNING
		//DBG_PRINTF("In RUNNING state\n");
		if (send_info->serial_num != send_info->rtmp_chunk->header.sequence_number) { //check serial num
			printf("%s, recv returned RET_WRONG_SER_NUM\n", __FUNCTION__);
			return RET_WRONG_SER_NUM;
		}
		send_rt_val = send(sock, send_info->buffer + send_info->offset, send_info->expect_len, 0);
		
		//Log(LOGDEBUG, "%s: offset = %d, send_rt_val = %d, expect_len = %d", __FUNCTION__, _send_ctl_info.offset, send_rt_val, _send_ctl_info.expect_len);
		if (send_rt_val < 0) {
			if (errno == EINTR | errno == EAGAIN) {
				send_info->ctl_state = RUNNING;
				return RET_OK;
			} else {
				return RET_SOCK_ERROR;
			}
		} else if (send_rt_val == 0) {
			send_info->ctl_state = RUNNING;
			return RET_OK;
		}
		else if (send_rt_val == send_info->expect_len) {
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

network::network() 
{
	send_byte = 0ULL;	// typeof(_send_byte) == unsigned long long int
	recv_byte = 0ULL;

	_error_cfd = new std::queue<int>;
}

network::~network() 
{

}

//================below not important=====================


void network::handle_rtmp_error(int sock) 
{
	_error_cfd->push(sock);
}