#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "common.h"
//#include "logger.h"
#ifdef _WIN32
	#include "EpollFake.h"
#endif
#include "basic_class.h"

class peer;

class network {
	
public:

	unsigned long long int send_byte;
	unsigned long long int recv_byte;
	
	void timer();		// timer
	void setall_fd_epollout();
	void garbage_collection();
	unsigned long getLocalIpv4(); 
	void set_fd_bcptr_map(int sock, basic_class *bcptr);
	void fd_bcptr_map_set(int sock, basic_class *bcptr);
	void fd_bcptr_map_delete(int sock);
	void fd_del_hdl_map_set(int sock, basic_class *bcptr);
	void fd_del_hdl_map_delete(int sock);

	// epoll trigger function
	void epoll_creater(void);
#ifdef _WIN32
	void epoll_waiter(int timeout, list<int> *fd_list); 
#else
	void epoll_waiter(int timeout);
#endif
	void epoll_dispatcher(void);
	void epoll_control(int sock, int op, unsigned int event);

	void handle_rtmp_error(int sock);

	void set_nonblocking(int sock);	// I/O function
	void set_blocking(int sock);

	// socket function
	int socket(int domain, int type, int protocol);
	int connect(int sock, const struct sockaddr *serv_addr, socklen_t addrlen);
	int connect_timeout(int sock, struct sockaddr *addr, size_t size_addr, int timeout);
	int bind(int sock, const struct sockaddr *my_addr, socklen_t addrlen);
	int listen(int sock, int backlog);
	int accept(int sock, struct sockaddr *addr, socklen_t *addrlen);
	int send(int s, const char *buf, size_t len, int flags);
	int sendto(int s, const char *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
	int recv(int s, char *buf, size_t len, int flags);	
	int recvfrom(int s, char *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
	void peer_set(peer *peer_ptr);
	int close(int sock);
	int nonblock_recv(int sock, Recv_nonblocking_ctl* send_info);
	int nonblock_send(int sock, Network_nonblocking_ctl* send_info);
	
	network();
	~network();
	network(const network&);
	network& operator=(const network&);

private:
	
	// class variable
	peer *_peer_ptr;
	int epfd, nfds;
	int _channel_id;
	struct epoll_event events[EVENTSIZE];

	queue<int> *_error_cfd;

	std::map<int, basic_class *> _map_fd_bc_tbl;
	std::map<int, basic_class *> _map_fd_del_hdl_tbl;
	std::map<int, basic_class *>::iterator _map_fd_bc_tbl_iter;
	
	
};

#endif
