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
#ifndef _CLIENT_H_
#define _CLIENT_H_
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

/************************************************************************************************************************/
/*			Constant Definition																							*/
/************************************************************************************************************************/
#ifndef _WIN32
#define SOCKET						INT32
#endif

#define BUILD						"BUILD $VER:10$"
#define BUILD_VER					10						//This build version must match the server's or will cause version mismatch error during initialization.

//#define TEST_TIME
#define TEST_DEBUG					0
#define true						1
#define false						0

#define DEF_SERVER_IP1				"59.124.31.21"
#define DEF_SERVER_IP2				"59.124.31.19"
#define SERVER_LOG_PORT				8123
#define SERVER_COMM_PORT			8132				//server communication port: for connection message exchange

#define MAX_IP_STR_LEN				20
#define MAX_ID_LEN					31					//Max Client ID Length
#define MAX_FINGERPRINT_LEN			1024				//Max buffer length for fingerprint

//Define timeout for procedures: Followings are statistical experimental minimal acceptable values.
#define COMM_TIMEOUT					2					//receive communication message:		1~2
#define RCV_PEER_DATA_TIMEOUT			2					//receive peer info from ctrl			0~1
#define RCV_ECHO_TIMEOUT				7					//sync between peers by stunt server	6~7
#define ASYM_TIMEOUT					3					//directly connect to the peer			1~2

#define FINGERPRINT_FILE			"fingerprint.dat"

//Probe related definitions
#define PROBE_RETRY_TIMES				3					//NAT probe max retry times
#define PROBE_FAIL_SLEEP_SEC			10					//(Second) Sleeping time if failed in probing TCP cone type. 
#define RANDOM_INCREMENT				0x10000000			//Random increment of symmetric NAT

//Tag used in sync
typedef enum
{
	TAG_OK = 1,
	TAG_ABORT,
	TAG_ACKABORT,
}E_TagType;

//Client state
typedef enum
{
	CSTATE_PROBE = 0,
	CSTATE_IDLE,
	CSTATE_BUSY,
	CSTATE_DEAD,
	CSTATE_TYPE_AMT
}E_StateType;

//Protocol type
typedef enum 
{
	PRO_UDP = 0, 
	PRO_TCP,
	PRO_AMT
} E_Protocol;

//Message type of communication
typedef enum
{
	MSG_CONNECT = 10001,
	MSG_DEREGISTER,
	MSG_QUERY,
	MSG_AMT
} E_Message;

//Error used in Stunt client
typedef enum
{
	ERR_NONE = 0,
	ERR_FAIL = (-1),
	ERR_CREATE_SOCKET = (-4000),
	ERR_CONNECT,
	ERR_VERSION,
	ERR_DUPLICATE_ID,
	ERR_PROBE,
	ERR_TIMEOUT,
	ERR_COMM_TIMEOUT,
	ERR_ECHO_TIMEOUT,
	ERR_SELECT,
	ERR_RECEIVE,
	ERR_SYN_RECEIVE,			//(-3990)
	ERR_SYN_SEND,				
	ERR_ADDRPORT_RECEIVE,
	ERR_ADDRPORT_SEND,
	ERR_ASYMSERVER,
	ERR_ASYMCLIENT,
	ERR_PREDICT,
	ERR_SEND,
	ERR_TRYCONNECT,
	ERR_SETSOCKOPT,
	ERR_BIND,					//(-3980)
	ERR_LISTEN,					
	ERR_ASYM_TIMEOUT,
	ERR_ACCEPT,
	ERR_MATCH,
	ERR_SAME_NAT,
	ERR_HAIRPIN
} E_ErrorStat;

/************************************************************************************************************************/
/*			Macro Definition																							*/
/************************************************************************************************************************/


/************************************************************************************************************************/
/*			Type Definition																								*/
/************************************************************************************************************************/
typedef char					CHAR;
typedef unsigned char			UCHAR;
typedef signed char				INT8;
typedef unsigned char			UINT8;
typedef signed short			INT16;
typedef unsigned short			UINT16;
typedef signed int				INT32;
typedef unsigned int			UINT32;

//Server information: for server and client: A message buffer
typedef struct _ServerInfo 
{
    CHAR chID[MAX_ID_LEN + 1];			//ID of the client which is communicating to the server.

	INT32 nIP1;
    INT32 nIP2;
    INT16 wPort1;
    INT16 wPort2;
    INT16 wPort3;			//not used in this Lib
    INT16 wPort4;			//not used in this Lib
    INT16 wPort5;			//not used in this Lib
    INT16 wPort6;			//not used in this Lib
    INT16 wPort7;			//not used in this Lib
    INT16 wPort8;			//not used in this Lib
} ServerInfo;

typedef struct _Echo 
{
	INT32 nIP;
    INT16 wPort;
	INT16 wPadding;
    union 
	{
        CHAR chData[24];
        struct 
		{
            INT32 nSeq;
            CHAR chResponse;
        } Tcp;
        struct 
		{
            INT32 nConnID;
			CHAR chPeerID[MAX_ID_LEN + 1];
            INT32 nPeerIP;
            CHAR chRole;
        } Conn;
    } Data;
} Echo;

//Message which is sent between client and server's communication thread
typedef struct _Msg
{
	INT32 nType;
	union
	{
		struct 
		{
			CHAR chSrcID[MAX_ID_LEN + 1];		//ID of the source client
			CHAR chDstID[MAX_ID_LEN + 1];		//ID of the destination client 
		} Connection;
		CHAR chID[MAX_ID_LEN + 1];		//ID for deregierster
	}Data;
} Msg;

typedef struct _Fingerprint 
{
    INT32		nClientVer;
    INT32		nServerVer;
    CHAR		chID[MAX_ID_LEN + 1];
	INT32		nDone;
	INT32		nGAddr;

	struct 
	{
        INT32			nIncrement;			// 0 : Cone, 0x10000000: Random
        INT8			bPortPreserving;
    } TCP;

} Fingerprint;

/************************************************************************************************************************/
/*			Internal Variable Reference																					*/
/************************************************************************************************************************/
FILE *g_pDbgFile = stdout;

CHAR g_chClientID[MAX_ID_LEN + 1];
INT32 g_nClientIP = 0;	
INT32 g_nServerVersion = 0;
CHAR g_szServerIP1[MAX_IP_STR_LEN + 1] = DEF_SERVER_IP1;
CHAR g_szServerIP2[MAX_IP_STR_LEN + 1] = DEF_SERVER_IP2;

Fingerprint g_Fingerprint;
ServerInfo g_ServerInfo;			//A buffer to exchange server and client information.

/************************************************************************************************************************/
/*			Internal Function Reference																					*/
/************************************************************************************************************************/
static INT32	XInitSockAddr(struct sockaddr_in *SockAddr, INT16 wFamily, const CHAR * pchAddr, UINT16 uwPort, UINT32 unConvertedAddr, UINT16 uwConvertedPort);
static INT32	XAsymServer(SOCKET sServerLog, SOCKET sCtrl, SOCKET *psListen, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode); 
static INT32	XAsymClient(SOCKET sServerLog, SOCKET sCtrl, SOCKET *psConnect, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode); 
static INT32	XPredict(SOCKET sServerLog, SOCKET *psAux, struct sockaddr_in *pAddrGlobal, struct sockaddr_in *pAddrLocal);
static INT32	XTryConnect(SOCKET sServerLog, SOCKET sAuxServer, struct sockaddr *pAddrPeer, INT32 nAddrPeerLen, INT32 nMiliSec);
static INT32	XProbe(SOCKET sServerLog);
static INT32	XProbeNAT(SOCKET sServerLog);
static INT32	XCheckConeTCP(SOCKET sServerLog);
static void		XCheckConeTCPProbe(SOCKET sServerLog, SOCKET sEcho, INT32 nSeq, UINT32 *punResAddr, UINT16 *puwResPort);
static void		XAnalyzeNature(UINT32 *punAddrGlobal, UINT16 *puwPortGlobal, UINT16 *puwPortLocal, INT32 nTimes, E_Protocol nProtocol);
static void		XGenFingerprint(CHAR *pchPrint, INT32 nSize);
static INT32	XWriteFingerprint(SOCKET sServerLog);
static INT32	XReadFingerprint(void);
static INT32	XInitSockAddr(struct sockaddr_in *pSockAddr, INT16 wFamily, const CHAR * pchAddr, UINT16 uwPort, UINT32 unConvertedAddr, UINT16 uwConvertedPort);
static void		XSleep(INT32 nSec, INT32 nUSec);
static INT32	XGetErrno(void);

#endif //_CLIENT_H_

