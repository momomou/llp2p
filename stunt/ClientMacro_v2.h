/* Copyright (C) 2006 	Saikat Guha  and					
 *						Kuanyu Chou (xDreaming Tech. Co.,Ltd.)
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef _CLIENT_MACRO_H_
#define _CLIENT_MACRO_H_
/***********************************************************************
*		 Win32 / Linux Intelx86 C++ Compiler File 		    		   				   
*--------------------------------------------------------------		   
*	[For]				{ XSTUNT Library }				  			   
*	[File name]     	{ ClientMacro.h }                          		   	   
*	[Description]
*	This file is the MACRO of main implementation code for XSTUNT library. 
*	About STUNT, please read: http://nutss.gforge.cis.cornell.edu/stunt.php for more references.
*	[History]
*  	2005.10		Saikat/Paul		Paper and experiment proposed
*	2006.03		Saikat/Kuanyu	C/C++ implementation code released 
***********************************************************************/
/************************************************************************************************************************/
/*			Macro Definition: Work for Client.cpp																							*/
/************************************************************************************************************************/
#ifdef _WIN32
#define act_on_error(x, s, a)                                                               \
    errno = 0;                                                                              \
    if ((x) == -1) {                                                                        \
		printf("act_on_error  -1 \n");														\
		INT32 __act_errno;																	\
	    char *__act_error;																	\
		printf("-2\n");	\
		__act_errno = errno == 0 ? WSAGetLastError() : errno;								\
		printf("-3\n");	\
		__act_error = strerror(__act_errno);												\
        printf("-4\n");	\
		if (g_pDbgFile_v2 != NULL) fprintf(g_pDbgFile_v2, "%s:%d: %s ... ERROR(%d)\n", "client.c", __LINE__, (s), __act_errno);	            \
        printf("-5\n");	\
		if (__act_errno != 0) {                                                             \
            if (g_pDbgFile_v2 != NULL) fprintf(g_pDbgFile_v2, "%s:%d: ERROR(%d): %s\n", "client.c", __LINE__, __act_errno, __act_error);		\
        }                                                                                   \
        printf("-6\n");	\
		a;                                                                                  \
	} 
#else
#define act_on_error(x, s, a)
#endif

#ifdef _WIN32
#define log1(sock_logr, s, a1)                                                              \
	do {                                                                                    \
		struct timeb __log_buft;                                                            \
		char __log_buf[2048];                                                               \
		ftime(&__log_buft);                                                                 \
		_snprintf(__log_buf, 2048, "%lld.%03d:%s:%d:" s "\n", __log_buft.time, __log_buft.millitm, "client.c", __LINE__, (a1));	            \
		write2((sock_logr), __log_buf, (INT32) strlen(__log_buf));			                                    \
	} while(false)
#else
#define log1(sock_logr, s, a1)
#endif

#define readtimeout(s, socks, t, g)     \
    do {                                \
        Timeout.tv_sec = (t);           \
        Timeout.tv_usec = 0;            \
        FD_ZERO(&(socks));              \
        FD_SET((s), &(socks));          \
		int nn;							\
		if ((nn=select(1+(INT32)(s), &(socks), NULL, NULL, &Timeout)) == 0) { printf("readtimeout nn:%d \n",nn); goto g;}    \
		if ((nn=select(1+(INT32)(s), &(socks), NULL, NULL, &Timeout)) < 0)  { printf("readtimeout nn:%d \n",nn); goto g;}    \
    } while(false)

#ifdef _WIN32
#define log_on_error(x, s, g)   act_on_error((x), (s), log1(sock_logr, "%s ... ERROR", s); if (__act_errno != 0) {log1(sock_logr, "ERROR(%d)", __act_errno);} goto g)
#else
#define log_on_error(x, s, g)
#endif
#define return_on_error(x, s)   act_on_error((x), (s), return)
#define die_on_error(x, s)      act_on_error((x), (s), exit(1))
#define cont_on_error(x, s)     act_on_error((x), (s), continue)
#ifdef _WIN32
#define write2(s,b,l)           send((s),((char *)b),l,0)
#define read2(s,b,l)            recv((s),((char *)b),l,0)
#define close2(s)               closesocket((s)); (s) = (SOCKET) -1; do { } while(0)
#else
#define write2(s,b,l)
#define read2(s,b,l)
#define close2(s)
#endif
#define cont_on_write_error(sock, buf, size, s)     act_on_error((size) == write2((sock), (buf), (size)) ? 0 : -1, (s), close2((sock)); continue)
#define cont_on_read_error(sock, buf, size, s)      act_on_error((size) == read2((sock), (buf), (size)) ? 0 : -1, (s), close2((sock)); continue)
#ifdef _WIN32
#define log_on_read_error(sock, buf, size, s, g)    act_on_error((size) == read2((sock), (buf), (size)) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "%s ... ERROR", s); if (__act_errno != 0) {log1(sock_logr, "ERROR(%d)", __act_errno);} goto g)
#else
#define log_on_read_error(sock, buf, size, s, g)
#endif
#ifdef _WIN32
#define log_on_write_error(sock, buf, size, s, g)   act_on_error((size) == write2((sock), (buf), (size)) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "%s ... ERROR", s); if (__act_errno != 0) {log1(sock_logr, "ERROR(%d)", __act_errno);} goto g)
#else
#define log_on_write_error(sock, buf, size, s, g)
#endif
#define log_on_sendto_error(sock, buf, size, addr, addrlen, s, g)    act_on_error((size) == sendto((sock), (buf), (INT32)(size), 0, (addr), (addrlen)) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "%s ... ERROR", s); goto g)

#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define MIN(a, b)   ((a) < (b) ? (a) : (b))

#ifdef _WIN32
#define log_on_Xread_error(sock, buf, size, s, g)                       \
    do {                                                                \
        UCHAR __tag[2];                                                 \
        char __buf[256];                                                \
        INT32 dbg;                                                      \
        dbg = read2((sock), __tag, 2);									\
		printf("dbg recv: %d \n", dbg);									\
		act_on_error((2 == dbg) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact Read: %s ... ERROR", s); goto g);    \
        printf("**\n");	\
		if (__tag[0] != TAG_OK) {                                       \
            act_on_error((__tag[1] == read2((sock), __buf, __tag[1])) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact eat ok: %s ... ERROR", s); goto g);  \
			_snprintf(errbuf, 128, " REMOTE-%s", __buf);                \
			__tag[0] = TAG_ACKABORT;                                    \
            __tag[1] = 0;                                               \
            act_on_error((2 == write2((sock), __tag, 2)) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact write abortack: %s ... ERROR", s); NULL);  \
            log1(sock_logr, "Xact: aborted %s", __buf);                 \
            goto g;                                                     \
        } else if ((size) != __tag[1]) {                                \
            log_on_error(-1, "Protocol error", g);                      \
        }                                                               \
        dbg = read2((sock), (buf), (size));								\
		printf("dbg recv: %d \n", dbg);									\
		act_on_error((size) == dbg ? 0 : -1, (s), close2((sock)); log1(sock_logr, "%s ... ERROR", s); log1(sock_logr, "Read %d bytes", dbg); log1(sock_logr, "Expecting %d bytes.", size); goto g);                         \
	} while(false)
#else
#define log_on_Xread_error(sock, buf, size, s, g)
#endif
#define log_on_Xwrite_error(sock, buf, size, s, g)

#ifdef _WIN32
#define log_on_Xabort_error(sock, s, g)                 \
    do {                                                \
        unsigned char __slen = 1 + (unsigned char)strlen(errbuf); \
        UCHAR __tag[2] = {TAG_ABORT, __slen};           \
        char __buf[256];                                \
        INT32 wait = true;                                \
        act_on_error(2 == write2((sock), __tag, 2) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact write abort: %s ... ERROR", s); wait = false);  \
        act_on_error(__slen == write2((sock), errbuf, __slen) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact write abort: %s ... ERROR", (errbuf)); wait = false);  \
        while(wait) {                                   \
            {                                           \
                fd_set __socks;                         \
                struct timeval Timeout;                 \
                readtimeout((sock), __socks, 45, g);    \
            }                                           \
            act_on_error(2 == read2((sock), __tag, 2) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact read ack: %s ... ERROR", s); break); \
            switch(__tag[0]) {          \
                case TAG_OK:            \
                    act_on_error(__tag[1] == read2((sock), __buf, __tag[1]) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact eat ok: %s ... ERROR", s); wait = false; break);  \
                    break;              \
                case TAG_ABORT:         \
                    act_on_error(__tag[1] == read2((sock), __buf, __tag[1]) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact eat ok: %s ... ERROR", s); wait = false; break);  \
					_snprintf(errbuf + __slen - 1, 128 - __slen + 1, " REMOTE-%s", __buf);   \
					__tag[0] = TAG_ACKABORT;       \
                    __tag[1] = 0;       \
                    act_on_error(2 == write2((sock), __tag, 2) ? 0 : -1, (s), close2((sock)); log1(sock_logr, "Xact write abortack: %s ... ERROR", s); wait = false; break);  \
                    break;              \
                case TAG_ACKABORT:      \
                    wait = false;       \
                    break;              \
            }                           \
        }                               \
    } while(false)
#else
#define log_on_Xabort_error(sock, s, g)
#endif
#define log_on_Xcommit_error(sock, s, g1, g2)                                       \
    {                                                                               \
        char bye[] = (s);                                                           \
        log_on_Xwrite_error((sock),bye,sizeof(bye),"Sending bye", g1);              \
        {                                                                           \
            fd_set __socks;                                                         \
            struct timeval timeout;                                                 \
            readtimeout((sock), __socks, 45, g1);                                   \
        }                                                                           \
        log_on_Xread_error((sock),bye,sizeof(bye),"Receiving bye", g2);             \
    } while(false)


#define IPPORT(ip, port) ((ip) >> 0) & 0xFF, ((ip) >> 8) & 0xFF, ((ip) >> 16) & 0xFF, ((ip) >> 24) & 0xFF, ntohs((port))

#endif

