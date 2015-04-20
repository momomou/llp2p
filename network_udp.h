#ifndef __NETWORK_UDP_H__
#define __NETWORK_UDP_H__

#include "common.h"
#include "logger.h"
#ifdef _WIN32
	#include "EpollFake.h"
#endif
#include "basic_class.h"
#include "udt_lib/udt.h"

class peer;
class logger;

class network_udp {
	
public:

	#ifdef _FIRE_BREATH_MOD_
		EpollVars epollVar;
	#endif
	unsigned long long int send_byte;
	unsigned long long int recv_byte;
	int *_errorRestartFlag;
	list<int>  * fd_list_ptr;
	int pk_fd;		// PK socket fd
	int log_fd;		// Log-server socket fd

	void timer();		// timer
	void setall_fd_epollout();
	void garbage_collection();
	unsigned long getLocalIpv4(); 
	void set_fd_bcptr_map(int sock, basic_class *bcptr);
	void delete_fd_bcptr_map(int sock);

	// epoll trigger function
	void epoll_creater(void);
#ifdef _WIN32
	void epoll_waiter(int timeout, list<int> *fd_list); 
#else
	void epoll_waiter(int timeout, list<int> *fd_list); 
#endif
	void epoll_dispatcher(void);
	void epoll_control(int sock, int op, unsigned int event);

	void handle_rtmp_error(int sock);

	int GetSockState(int sock);
	void set_nonblocking(int sock);	// I/O function
	void set_blocking(int sock);
	void set_rendezvous(int sock);

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
	void log_set(logger *log_ptr);
	int close(int sock);
	int nonblock_recv(int sock, Nonblocking_Ctl* send_info);
	int nonblock_send(int sock, Network_nonblocking_ctl* send_info);
	
	network_udp(int * errorRestartFlag,list<int>  * fd_list);
	~network_udp();
	network_udp(const network_udp&);
	network_udp();
	network_udp& operator=(const network_udp&);
	void eraseFdList(int sock) ;

	int epfd;
	std::map<int, basic_class *> _map_fd_bc_tbl;
private:
	
	// class variable
	peer *_peer_ptr;
	logger *_log_ptr;

	int nfds;
	
	int _channel_id;
	struct epoll_event events[EVENTSIZE];

	std::set<UDTSOCKET> readfds;
	std::set<UDTSOCKET> writefds;

	queue<int> *_error_cfd;

	
	std::map<int, basic_class *> _map_fd_del_hdl_tbl;
	std::map<int, basic_class *>::iterator _map_fd_bc_tbl_iter;
	
	
};

#endif
