#ifndef _SYS_EPOLL_FAKE_H
#define _SYS_EPOLL_FAKE_H    1


#include "common.h"


// this is implemented by select based wrapper function (LiloHuang 2008)

enum EPOLL_EVENTS
  {
    EPOLLIN = 0x001,
#define EPOLLIN EPOLLIN
    EPOLLPRI = 0x002,
#define EPOLLPRI EPOLLPRI
    EPOLLOUT = 0x004,
#define EPOLLOUT EPOLLOUT
    EPOLLRDNORM = 0x040,
#define EPOLLRDNORM EPOLLRDNORM
    EPOLLRDBAND = 0x080,
#define EPOLLRDBAND EPOLLRDBAND
    EPOLLWRNORM = 0x100,
#define EPOLLWRNORM EPOLLWRNORM
    EPOLLWRBAND = 0x200,
#define EPOLLWRBAND EPOLLWRBAND
    EPOLLMSG = 0x400,
#define EPOLLMSG EPOLLMSG
    EPOLLERR = 0x008,
#define EPOLLERR EPOLLERR
    EPOLLRDHUP = 0x010,
#define EPOLLRDHUP EPOLLRDHUP
    EPOLLONESHOT = (1 << 30),
#define EPOLLONESHOT EPOLLONESHOT
    EPOLLET = (1 << 31)
#define EPOLLET EPOLLET
  };


/* Valid opcodes ( "op" parameter ) to issue to epoll_ctl().  */
#define EPOLL_CTL_ADD 1 /* Add a file decriptor to the interface.  */
#define EPOLL_CTL_DEL 2 /* Remove a file decriptor from the interface.  */
#define EPOLL_CTL_MOD 3 /* Change file decriptor epoll_event structure.  */

typedef union epoll_data
{
  int fd;
} epoll_data_t;

struct epoll_event
{
  unsigned int events;      /* Epoll events */
  epoll_data_t data;    /* User data variable */
};

int epoll_create (int __size);
int epoll_ctl (int __epfd, int __op, int __fd, struct epoll_event *__events);
int epoll_wait (int __epfd, struct epoll_event *__events, int __maxevents, int __timeout, list<int> *fd_list);

#endif
