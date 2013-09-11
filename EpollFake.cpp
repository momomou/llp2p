#include "EpollFake.h"
#include "common.h"

#ifdef _FIRE_BREATH_MOD_

int find_max_fd(list<int> *fd_list)
{
	list<int>::iterator fd_iter;
	int max_fd = 0;

	for (fd_iter = fd_list->begin(); fd_iter!= fd_list->end(); fd_iter++) {
		if(max_fd < *fd_iter)
			max_fd = *fd_iter;
	}
	
	return max_fd;
}

int epoll_create (int __size, EpollVars* epollVar) {

	assert(__size == FD_SETSIZE);

	FD_ZERO(&(epollVar->read_master));    // clear the master and temp sets
	FD_ZERO(&(epollVar->read_fds));

	FD_ZERO(&(epollVar->write_master));    // clear the master and temp sets
	FD_ZERO(&(epollVar->write_fds));

	FD_ZERO(&(epollVar->error_master));    // clear the master and temp sets
	FD_ZERO(&(epollVar->error_fds));

	//printf("done\n");
	
	return 1;
}

/* Manipulate an epoll instance "epfd". Returns 0 in case of success,
   -1 in case of error ( the "errno" variable will contain the
   specific error code ) The "op" parameter is one of the EPOLL_CTL_*
   constants defined above. The "fd" parameter is the target of the
   operation. The "event" parameter describes which events the caller
   is interested in and any associated user data.  */


int epoll_ctl (int __epfd, int __op, int __fd, struct epoll_event *__events, EpollVars* epollVar) {

	if(__fd < 0) {
		return -1;
	}

	//printf("__fd %d\n", __fd);

	switch(__op) {
		case EPOLL_CTL_ADD:
	//			printf("EPOLL_CTL_ADD\n");
				if(__events->events & EPOLLIN) {
					FD_SET(__fd, &(epollVar->read_master));
				}
				if(__events->events & EPOLLOUT) {
					FD_SET(__fd, &(epollVar->write_master));
				}
				FD_SET(__fd, &(epollVar->error_master));
			break;

		case EPOLL_CTL_DEL:
	//			printf("EPOLL_CTL_DEL\n");
				FD_CLR(__fd, &(epollVar->read_master));
				FD_CLR(__fd, &(epollVar->write_master));
				FD_CLR(__fd, &(epollVar->error_master));
			break;

		case EPOLL_CTL_MOD:
	//			printf("EPOLL_CTL_MOD\n");

				FD_CLR(__fd, &(epollVar->read_master));
				FD_CLR(__fd, &(epollVar->write_master));

				if(__events->events & EPOLLIN) {
	//				printf("EPOLLIN\n");
					FD_SET(__fd, &(epollVar->read_master));
				}
				if(__events->events & EPOLLOUT) {
	//				printf("EPOLLOUT\n");
					FD_SET(__fd, &(epollVar->write_master));
				}
			break;
	}
	return 1;
}

/* Wait for events on an epoll instance "epfd". Returns the number of
   triggered events returned in "events" buffer. Or -1 in case of
   error with the "errno" variable set to the specific error code. The
   "events" parameter is a buffer that will contain triggered
   events. The "maxevents" is the maximum number of events to be
   returned ( usually size of "events" ). The "timeout" parameter
   specifies the maximum wait time in milliseconds (-1 == infinite).

   This function is a cancellation point and therefore not marked with
   __THROW.  */
int epoll_wait (int __epfd, struct epoll_event *__events, int __maxevents, int __timeout, list<int> *fd_list, EpollVars* epollVar) {

//	printf("trigger\n");

    //printf("epoll 108 assert!\n");
	assert(__maxevents == FD_SETSIZE);

	struct timeval tv;
	list<int>::iterator fd_iter;
	
	tv.tv_sec =  __timeout / 1000;
	tv.tv_usec = (__timeout % 1000) * 1000;
	
	(epollVar->read_fds) = (epollVar->read_master);
	(epollVar->write_fds) = (epollVar->write_master);
	(epollVar->error_fds) = (epollVar->error_master);

	int i;
	int max_fd;
	int num_ready;

	for(i=0; i<FD_SETSIZE; i++) {
		__events[i].events = 0;
		__events[i].data.fd = 0;
	}

	//cout << "bbbbbbbbbbbbbbb" <<endl;

	max_fd = find_max_fd(fd_list);
	num_ready = select(max_fd + 1, &(epollVar->read_fds), &(epollVar->write_fds), &(epollVar->error_fds), &tv);

	if(num_ready < 0) {
		//cout << "max_fd = " << max_fd << endl;
		//cout << "select" << endl;
	
		switch (WSAGetLastError()) {
    			case WSANOTINITIALISED:
        			printf("A successful WSAStartup call must occur before using this function. ");
        			break;
   	 			case WSAENETDOWN:
        			printf("The network subsystem has failed. ");
        			break;
    			case WSAEACCES:
        			printf("The requested address is a broadcast address, but the appropriate flag was not set. Call setsockopt with the SO_BROADCAST parameter to allow the use of the broadcast address. ");
        			break;
    			case WSAEINVAL:
       	 			printf("An unknown flag was specified, or MSG_OOB was specified for a socket with SO_OOBINLINE enabled. ");
        			break;
    			case WSAEINTR:
        			printf("A blocking Windows Sockets 1.1 call was canceled through WSACancelBlockingCall. ");
        			break;
    			case WSAEINPROGRESS:
        			printf("A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function. ");
        			break;
    			case WSAEFAULT:
        			printf("The buf or to parameters are not part of the user address space, or the tolen parameter is too small. ");
        			break;
    			case WSAENETRESET:
        			printf("The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress. ");
        			break;
    			case WSAENOBUFS:
        			printf("No buffer space is available. ");
        			break;
		    	case WSAENOTCONN:
					printf("The socket is not connected (connection-oriented sockets only). ");
					break;
		    	case WSAENOTSOCK:
					printf("The descriptor is not a socket. ");
					//system("PAUSE");
					break;
		    	case WSAEOPNOTSUPP:
					printf("MSG_OOB was specified, but the socket is not stream-style such as type SOCK_STREAM, OOB data is not supported in the communication domain associated with this socket, or the socket is unidirectional and supports only receive operations. ");
					break;
		    	case WSAESHUTDOWN:
					printf("The socket has been shut down; it is not possible to sendto on a socket after shutdown has been invoked with how set to SD_SEND or SD_BOTH. ");
					break;
			    case WSAEWOULDBLOCK:
			        printf("The socket is marked as nonblocking and the requested operation would block. ");
			        break;
			    case WSAEMSGSIZE:
			        printf("The socket is message oriented, and the message is larger than the maximum supported by the underlying transport. ");
			        break;
			    case WSAEHOSTUNREACH:
			        printf("The remote host cannot be reached from this host at this time. ");
			        break;
			    case WSAECONNABORTED:
			        printf("The virtual circuit was terminated due to a time-out or other failure. The application should close the socket as it is no longer usable. ");
			        break;
			    case WSAECONNRESET:
			        printf("The virtual circuit was reset by the remote side executing a hard or abortive close. For UPD sockets, the remote host was unable to deliver a previously sent UDP datagram and responded with a \"Port Unreachable\" ICMP packet. The application should close the socket as it is no longer usable. ");
			        break;
			    case WSAEADDRNOTAVAIL:
			        printf("The remote address is not a valid address, for example, ADDR_ANY. ");
			        break;
			    case WSAEAFNOSUPPORT:
			        printf("Addresses in the specified family cannot be used with this socket. ");
			        break;
			    case WSAEDESTADDRREQ:
			        printf("A destination address is required. ");
			        break;
			    case WSAENETUNREACH:
			        printf("The network cannot be reached from this host at this time. ");
			        break;
			    case WSAETIMEDOUT:
			        printf("The connection has been dropped, because of a network failure or because the system on the other end went down without notice. ");
			        break;
			    default:
			        printf("Unknown socket error. ");
			        break;
			}
//2013/01/28
		return -1;
//		return	0;
		//perror("select");
	} else if(num_ready == 0){
		
	}else{
	//normal
	}

	int ret_count = 0;
	int change;


	for (fd_iter = fd_list->begin(); fd_iter!= fd_list->end(); fd_iter++) {
	
		change = 0;

		if(FD_ISSET(*fd_iter, &(epollVar->read_fds))) {
			change = 1;
//			printf("%d IN\n", *fd_iter);
			__events[ret_count].events |= EPOLLIN;
			__events[ret_count].data.fd = *fd_iter;
		}

		if(FD_ISSET(*fd_iter, &(epollVar->write_fds))) {
			change = 1;
//			printf("%d OUT\n", *fd_iter);
			__events[ret_count].events |= EPOLLOUT;
			__events[ret_count].data.fd = *fd_iter;
		}

		if(FD_ISSET(*fd_iter, &(epollVar->error_fds))) {
			change = 1;
//			printf("%d ERR\n", *fd_iter);
			__events[ret_count].events |= EPOLLERR;
			__events[ret_count].data.fd = *fd_iter;
		}

		if(change) ++ret_count;
	}

//	printf("ret: %d\n", ret_count);
	//cout << "ret_count = " << ret_count << endl;
	return ret_count;

}

#else

static fd_set read_master, read_fds;   // master file descriptor list
static fd_set write_master, write_fds; // temp file descriptor list for select()
static fd_set error_master, error_fds; // temp file descriptor list for select()

int find_max_fd(list<int> *fd_list)
{
	list<int>::iterator fd_iter;
	int max_fd = 0;

	for (fd_iter = fd_list->begin(); fd_iter!= fd_list->end(); fd_iter++) {
		if(max_fd < *fd_iter)
			max_fd = *fd_iter;
	}
	
	return max_fd;
}

int epoll_create (int __size) {

	assert(__size == FD_SETSIZE);

	FD_ZERO(&read_master);    // clear the master and temp sets
	FD_ZERO(&read_fds);

	FD_ZERO(&write_master);    // clear the master and temp sets
	FD_ZERO(&write_fds);

	FD_ZERO(&error_master);    // clear the master and temp sets
	FD_ZERO(&error_fds);

	//printf("done\n");
	
	return 1;
}

/* Manipulate an epoll instance "epfd". Returns 0 in case of success,
   -1 in case of error ( the "errno" variable will contain the
   specific error code ) The "op" parameter is one of the EPOLL_CTL_*
   constants defined above. The "fd" parameter is the target of the
   operation. The "event" parameter describes which events the caller
   is interested in and any associated user data.  */


int epoll_ctl (int __epfd, int __op, int __fd, struct epoll_event *__events) {

	if(__fd < 0) {
		return -1;
	}

	//printf("__fd %d\n", __fd);

	switch(__op) {
		case EPOLL_CTL_ADD:
	//			printf("EPOLL_CTL_ADD\n");
				if(__events->events & EPOLLIN) {
					FD_SET(__fd, &read_master);
				}
				if(__events->events & EPOLLOUT) {
					FD_SET(__fd, &write_master);
				}
				FD_SET(__fd, &error_master);
			break;

		case EPOLL_CTL_DEL:
	//			printf("EPOLL_CTL_DEL\n");
				FD_CLR(__fd, &read_master);
				FD_CLR(__fd, &write_master);
				FD_CLR(__fd, &error_master);
			break;

		case EPOLL_CTL_MOD:
	//			printf("EPOLL_CTL_MOD\n");

				FD_CLR(__fd, &read_master);
				FD_CLR(__fd, &write_master);

				if(__events->events & EPOLLIN) {
	//				printf("EPOLLIN\n");
					FD_SET(__fd, &read_master);
				}
				if(__events->events & EPOLLOUT) {
	//				printf("EPOLLOUT\n");
					FD_SET(__fd, &write_master);
				}
			break;
	}
	return 1;
}

/* Wait for events on an epoll instance "epfd". Returns the number of
   triggered events returned in "events" buffer. Or -1 in case of
   error with the "errno" variable set to the specific error code. The
   "events" parameter is a buffer that will contain triggered
   events. The "maxevents" is the maximum number of events to be
   returned ( usually size of "events" ). The "timeout" parameter
   specifies the maximum wait time in milliseconds (-1 == infinite).

   This function is a cancellation point and therefore not marked with
   __THROW.  */
int epoll_wait (int __epfd, struct epoll_event *__events, int __maxevents, int __timeout, list<int> *fd_list) {

//	printf("trigger\n");

    //printf("epoll 108 assert!\n");
	assert(__maxevents == FD_SETSIZE);

	struct timeval tv;
	list<int>::iterator fd_iter;
	
	tv.tv_sec =  __timeout / 1000;
	tv.tv_usec = (__timeout % 1000) * 1000;
	
	read_fds = read_master;
	write_fds = write_master;
	error_fds = error_master;

	int i;
	int max_fd;
	int num_ready;

	for(i=0; i<FD_SETSIZE; i++) {
		__events[i].events = 0;
		__events[i].data.fd = 0;
	}

	//cout << "bbbbbbbbbbbbbbb" <<endl;

	max_fd = find_max_fd(fd_list);
	num_ready = select(max_fd + 1, &read_fds, &write_fds, &error_fds, &tv);

	if(num_ready < 0) {
		//cout << "max_fd = " << max_fd << endl;
		//cout << "select" << endl;
	
		switch (WSAGetLastError()) {
    			case WSANOTINITIALISED:
        			printf("A successful WSAStartup call must occur before using this function. ");
        			break;
   	 			case WSAENETDOWN:
        			printf("The network subsystem has failed. ");
        			break;
    			case WSAEACCES:
        			printf("The requested address is a broadcast address, but the appropriate flag was not set. Call setsockopt with the SO_BROADCAST parameter to allow the use of the broadcast address. ");
        			break;
    			case WSAEINVAL:
       	 			printf("An unknown flag was specified, or MSG_OOB was specified for a socket with SO_OOBINLINE enabled. ");
        			break;
    			case WSAEINTR:
        			printf("A blocking Windows Sockets 1.1 call was canceled through WSACancelBlockingCall. ");
        			break;
    			case WSAEINPROGRESS:
        			printf("A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function. ");
        			break;
    			case WSAEFAULT:
        			printf("The buf or to parameters are not part of the user address space, or the tolen parameter is too small. ");
        			break;
    			case WSAENETRESET:
        			printf("The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress. ");
        			break;
    			case WSAENOBUFS:
        			printf("No buffer space is available. ");
        			break;
		    	case WSAENOTCONN:
					printf("The socket is not connected (connection-oriented sockets only). ");
					break;
		    	case WSAENOTSOCK:
					printf("The descriptor is not a socket. ");
					PAUSE
					//system("PAUSE");
					break;
		    	case WSAEOPNOTSUPP:
					printf("MSG_OOB was specified, but the socket is not stream-style such as type SOCK_STREAM, OOB data is not supported in the communication domain associated with this socket, or the socket is unidirectional and supports only receive operations. ");
					break;
		    	case WSAESHUTDOWN:
					printf("The socket has been shut down; it is not possible to sendto on a socket after shutdown has been invoked with how set to SD_SEND or SD_BOTH. ");
					break;
			    case WSAEWOULDBLOCK:
			        printf("The socket is marked as nonblocking and the requested operation would block. ");
			        break;
			    case WSAEMSGSIZE:
			        printf("The socket is message oriented, and the message is larger than the maximum supported by the underlying transport. ");
			        break;
			    case WSAEHOSTUNREACH:
			        printf("The remote host cannot be reached from this host at this time. ");
			        break;
			    case WSAECONNABORTED:
			        printf("The virtual circuit was terminated due to a time-out or other failure. The application should close the socket as it is no longer usable. ");
			        break;
			    case WSAECONNRESET:
			        printf("The virtual circuit was reset by the remote side executing a hard or abortive close. For UPD sockets, the remote host was unable to deliver a previously sent UDP datagram and responded with a \"Port Unreachable\" ICMP packet. The application should close the socket as it is no longer usable. ");
			        break;
			    case WSAEADDRNOTAVAIL:
			        printf("The remote address is not a valid address, for example, ADDR_ANY. ");
			        break;
			    case WSAEAFNOSUPPORT:
			        printf("Addresses in the specified family cannot be used with this socket. ");
			        break;
			    case WSAEDESTADDRREQ:
			        printf("A destination address is required. ");
			        break;
			    case WSAENETUNREACH:
			        printf("The network cannot be reached from this host at this time. ");
			        break;
			    case WSAETIMEDOUT:
			        printf("The connection has been dropped, because of a network failure or because the system on the other end went down without notice. ");
			        break;
			    default:
			        printf("Unknown socket error. ");
			        break;
			}
//2013/01/28
		return -1;
//		return	0;
		//perror("select");
	} else if(num_ready == 0){
		
	}else{
	//normal
	}

	int ret_count = 0;
	int change;


	for (fd_iter = fd_list->begin(); fd_iter!= fd_list->end(); fd_iter++) {
	
		change = 0;

		if(FD_ISSET(*fd_iter, &read_fds)) {
			change = 1;
//			printf("%d IN\n", *fd_iter);
			__events[ret_count].events |= EPOLLIN;
			__events[ret_count].data.fd = *fd_iter;
		}

		if(FD_ISSET(*fd_iter, &write_fds)) {
			change = 1;
//			printf("%d OUT\n", *fd_iter);
			__events[ret_count].events |= EPOLLOUT;
			__events[ret_count].data.fd = *fd_iter;
		}

		if(FD_ISSET(*fd_iter, &error_fds)) {
			change = 1;
//			printf("%d ERR\n", *fd_iter);
			__events[ret_count].events |= EPOLLERR;
			__events[ret_count].data.fd = *fd_iter;
		}

		if(change) ++ret_count;
	}

//	printf("ret: %d\n", ret_count);
	//cout << "ret_count = " << ret_count << endl;
	return ret_count;

}

#endif

