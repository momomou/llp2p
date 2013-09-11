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

/***********************************************************************
*		 Win32 / Linux Intelx86 C++ Compiler File 		    		   				   
*--------------------------------------------------------------		   
*	[For]				{ XSTUNT Library }				  			   
*	[File name]     	{ Client.h }                          		   	   
*	[Description]
*	This file is the PRIVATE header of main implementation code for XSTUNT library. 
*	About STUNT, please read: http://nutss.gforge.cis.cornell.edu/stunt.php for more references.
*	[History]
*  	2005.10		Saikat/Paul		Paper and experiment proposed
*	2006.03		Saikat/Kuanyu	C/C++ implementation code released 
***********************************************************************/
/************************************************************************************************************************/
/*			Include Files																								*/
/************************************************************************************************************************/


#ifdef _WIN32
#if !defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#include<winsock2.h>
#include<MSWSock.h>
#endif
#include<stdio.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<io.h>
#include<time.h>
#include<sys/timeb.h>
#else
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/timeb.h>
#include <error.h>
#include <errno.h>
#endif

#include "stuntdef.h"

typedef struct tcp_punch_info {
	FILE *_g_pDbgFile_v2;

	CHAR _g_chClientID[MAX_ID_LEN + 1];
	INT32 _g_nClientIP;	
	INT32 _g_nServerVersion;
	CHAR _g_szServerIP11[MAX_IP_STR_LEN + 1];
	CHAR _g_szServerIP22[MAX_IP_STR_LEN + 1];

	struct __Fingerprint _g_Fingerprint;
	struct __ServerInfo _g_ServerInfo;			//A buffer to exchange server and client information.

} _tcp_punch_info;


class tcp_punch {
public:
	
	FILE *g_pDbgFile_v2;

	CHAR g_chClientID[MAX_ID_LEN + 1];
	INT32 g_nClientIP;	
	INT32 g_nServerVersion;
	CHAR g_szServerIP1[MAX_IP_STR_LEN + 1];
	CHAR g_szServerIP2[MAX_IP_STR_LEN + 1];

	struct __Fingerprint g_Fingerprint;
	struct __ServerInfo g_ServerInfo;			//A buffer to exchange server and client information.
	
	_tcp_punch_info *tcp_punch_Info;

	tcp_punch();
	~tcp_punch();
	
	
	INT32 XInit(const char* pchIP1, const char* pchIP2, SOCKET* psServerLog, CHAR* pchID, INT32 *pnErrCode);
	INT32 XListen(SOCKET sServerLog, SOCKET* psListen, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode);
	INT32 XConnect(SOCKET sServerLog, CHAR *pchPeerID, SOCKET *psConnect, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode);
	INT32 XDeRegister(SOCKET sServerLog, CHAR* pchID, INT32* pnErrCode);

	INT32	XInitSockAddr(struct sockaddr_in *SockAddr, INT16 wFamily, const CHAR * pchAddr, UINT16 uwPort, UINT32 unConvertedAddr, UINT16 uwConvertedPort);
	INT32	XAsymServer(SOCKET sServerLog, SOCKET sCtrl, SOCKET *psListen, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode); 
	INT32	XAsymClient(SOCKET sServerLog, SOCKET sCtrl, SOCKET *psConnect, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode); 
	INT32	XPredict(SOCKET sServerLog, SOCKET *psAux, struct sockaddr_in *pAddrGlobal, struct sockaddr_in *pAddrLocal);
	INT32	XTryConnect(SOCKET sServerLog, SOCKET sAuxServer, struct sockaddr *pAddrPeer, INT32 nAddrPeerLen, INT32 nMiliSec);
	INT32	XProbe(SOCKET sServerLog);
	INT32	XProbeNAT(SOCKET sServerLog);
	INT32	XCheckConeTCP(SOCKET sServerLog);
	void		XCheckConeTCPProbe(SOCKET sServerLog, SOCKET sEcho, INT32 nSeq, UINT32 *punResAddr, UINT16 *puwResPort);
	void		XAnalyzeNature(UINT32 *punAddrGlobal, UINT16 *puwPortGlobal, UINT16 *puwPortLocal, INT32 nTimes, E_Protocol nProtocol);
	void		XGenFingerprint(CHAR *pchPrint, INT32 nSize);
	INT32	XWriteFingerprint(SOCKET sServerLog);
	INT32	XReadFingerprint(void);
	void		XSleep(INT32 nSec, INT32 nUSec);
	INT32	XGetErrno(void);


private:

	
};




