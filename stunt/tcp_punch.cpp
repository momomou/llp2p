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
#pragma warning (disable: 4127 4057 4100 4389 4996)


//#include "Client.h"
#include "tcp_punch.h"
#include "ClientMacro_v2.h"
#ifdef _WIN32
#include <errno.h>
#else
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif


tcp_punch::tcp_punch()
{
	tcp_punch_Info = new struct tcp_punch_info; 
}

tcp_punch::~tcp_punch()
{

}

/***********************************************************************
 * FUNCTION:		XInit() 
 * DESCRIPTION:  	Given two IPs of the STUNT server and a client ID, the function will probe the NAT type, 
 *					register the specified ID and then return a log socket of STUNT server if the process works 
 *					successfully.
 * PARAMETERS:		-> pchIP1:			The 1st IP of the STUNT server. Pass NULL to use the default STUNT server.
 *					-> pchIP2:			The 2nd IP of the STUNT server. Pass NULL to use the default STUNT server.
 *					<-> *psServerLog:	The STUNT server socket
 *					->  *pchID:			The client ID which will be registered to the STUNT server. The length of the 
 *										ID must shorter than 32 characters long and not an empty string.
 *					<-> *pnErrCode: Error code
 * RETURNED:		
 *					ERR_NONE			Successful.
 *					ERR_CREATE_SOCKET	Fail to create a socket.
 *					ERR_CONNECT			Fail to connect to the STUNT server.
 *					ERR_RECEIVE			Fail to send data.
 *					ERR_SEND			Fail to receive data.
 *					ERR_VERSION			The required client version mismatch.
 *					ERR_PROBE			Fail during probing the NAT type.
 *					ERR_DUPLICATE_ID	The specified ID is already registered in the STUNT server.
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XInit(const char* pchIP1, const char* pchIP2, SOCKET* psServerLog, CHAR* pchID, INT32 *pnErrCode)
{
	printf("haha");
#ifdef _WIN32
	WSADATA Wsadata;
#endif
	struct sockaddr_in AddrLog, AddrLocal;
#ifdef _WIN32
	INT32 nAddrLen = sizeof(struct sockaddr_in);
#else
	socklen_t nAddrLen = sizeof(struct sockaddr_in); 
#endif
	INT32 nClientVersion = 0, nServerVersion = 0;
    INT32 nRetVal = 0; 
	UINT8 ubClientState = 0;
	UINT32	unLocalAddr = 0;

	*pnErrCode = 0;
	*psServerLog = (SOCKET) -1;

	//fprintf(stdout, "%s:%d: %s ... \n", "client.c", __LINE__, "1233321", errno);
	printf("haha");
	
	//Using in macro by Saikat: 
	//CHAR errbuf[128];
	SOCKET sock_logr = *psServerLog;
	/////////////////////////////////////////////////////////////
	printf("haha\n");
	printf("%s  %d\n", pchIP1, strlen(pchIP1));
	printf("%s  %d\n", pchIP2, strlen(pchIP2));
	//printf("%s  %d\n", pchIP1, strlen(g_szServerIP1));
	//printf("%s  %d\n", pchIP2, strlen(g_szServerIP2));
	
	//Set server IP address
	if (pchIP1 != NULL && pchIP2 != NULL) {
		printf("haha\n");
		sprintf(g_szServerIP1,"%s", pchIP1);
		//memset(tcp_punch_Info->g_szServerIP11, 0, sizeof(tcp_punch_Info->g_szServerIP11));
		//memcpy(tcp_punch_Info->g_szServerIP11, pchIP1, strlen(pchIP1));
		printf("haha\n");
		sprintf(g_szServerIP2,"%s", pchIP2);
		printf("haha\n");
	}

	printf("[XInit] 000 \n");
	
	//The WSAStartup function initiates use of WS2_32.DLL by a process
#ifdef _WIN32
	//WSAStartup(MAKEWORD(2,2), &Wsadata);
#endif
	//Assign the socket address/port of of LOG SERVER
	XInitSockAddr(&AddrLog, AF_INET, g_szServerIP1, SERVER_LOG_PORT, 0, 0);
	
	close2(*psServerLog);
	act_on_error(*psServerLog = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "Creating logger socket", goto LAB_ERR_CREATE_SOCKET);
	act_on_error(nRetVal = connect(*psServerLog, (struct sockaddr *)&AddrLog, sizeof(AddrLog)), "Connecting to server", goto LAB_ERR_CONNECT);
	//read finger print from the current data
	XReadFingerprint();
	//Reset the client ID to the new one.
	strcpy(g_chClientID, pchID);
	
	//receive the client build version from server
	log_on_read_error(*psServerLog, (char*)&nClientVersion, sizeof(nClientVersion), "Receiving required version", LAB_ERR_RECEIVE);
	printf("[XInit] nClientVersion: %d  %d \n", nClientVersion, ntohl(nClientVersion));
	if (ntohl(nClientVersion) != BUILD_VER) {
		close2(*psServerLog);
		return ERR_VERSION;
	}
	
	//receive the server version from server
	log_on_read_error(*psServerLog, (char*)&nServerVersion, sizeof(nServerVersion), "Receiving server version", LAB_ERR_RECEIVE);
	g_nServerVersion = ntohl(nServerVersion);
	if ((INT32)ntohl(nServerVersion) != g_Fingerprint.nServerVer) {
		g_Fingerprint.nDone = false;
	}
	printf("[XInit] nServerVersion: %d \n", ntohl(nServerVersion));
	
	//receive the client IP from server
	log_on_read_error(*psServerLog, (char*)&g_nClientIP, sizeof(g_nClientIP), "Receiving client IP", LAB_ERR_RECEIVE);
	if (g_nClientIP != g_Fingerprint.nGAddr) {
		g_Fingerprint.nDone = false;
	}
	printf("[XInit] g_nClientIP: %d  \n", ntohl(g_nClientIP));
	
	//if the finger print is not done, then get the fingerprint
	if (!g_Fingerprint.nDone) {
		printf("[XInit] !g_Fingerprint.nDone \n");
		memset(&g_Fingerprint, 0, sizeof(g_Fingerprint));
		g_Fingerprint.nServerVer = g_nServerVersion;
		g_Fingerprint.nGAddr = g_nClientIP;
		g_Fingerprint.nClientVer = BUILD_VER;
		
		// receive ServerInfo from STUNT-Server
		log_on_write_error(*psServerLog,  (char*)g_chClientID, sizeof(g_chClientID), "sending client ID", LAB_ERR_SEND);
		log_on_read_error(*psServerLog, (char*)&g_ServerInfo, sizeof(g_ServerInfo), "Receiving server config", LAB_ERR_RECEIVE);
		printf("[XInit] g_ServerInfo.nIP1: %s \n", inet_ntoa(*(struct in_addr *)&g_ServerInfo.nIP1));
		printf("[XInit] g_ServerInfo.nIP2: %s \n", inet_ntoa(*(struct in_addr *)&g_ServerInfo.nIP2));
		printf("[XInit] g_ServerInfo.chID: %s \n", g_ServerInfo.chID);
		if (strlen(g_ServerInfo.chID) == 0) {
			close2(*psServerLog);
			return ERR_DUPLICATE_ID;
		}
		ubClientState = CSTATE_PROBE;
		log_on_write_error(*psServerLog,  (char*)&ubClientState, sizeof(UINT8), "sending client state", LAB_ERR_SEND);
		
		// get local IP according to socket, and then send this Info to STUNT-Server
		getsockname(*psServerLog,(struct sockaddr *)&AddrLocal, &nAddrLen);
		unLocalAddr = AddrLocal.sin_addr.s_addr;
		log_on_write_error(*psServerLog,  (char*)&unLocalAddr, sizeof(UINT32), "sending client local address", LAB_ERR_SEND);

		strcpy(g_chClientID, g_ServerInfo.chID);
		strcpy(g_Fingerprint.chID, g_chClientID);
		printf("[XInit()] 2220 \n");
	 	nRetVal = XProbe(*psServerLog);
		printf("[XInit()] 2221 \n");
		close2(*psServerLog);
		if (nRetVal == -1) {
			return ERR_PROBE;
		}
		act_on_error(*psServerLog = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "Creating new logger socket", goto LAB_ERR_CREATE_SOCKET);
		act_on_error(nRetVal = connect(*psServerLog, (struct sockaddr *)&AddrLog, sizeof(AddrLog)), "Connecting to server", goto LAB_ERR_CONNECT);
		log_on_read_error(*psServerLog, (char*)&nClientVersion, sizeof(nClientVersion), "Receiving required version", LAB_ERR_RECEIVE);
		log_on_read_error(*psServerLog, (char*)&nServerVersion, sizeof(nServerVersion), "Receiving server version", LAB_ERR_RECEIVE);
		log_on_read_error(*psServerLog, (char*)&g_nClientIP, sizeof(g_nClientIP), "Receiving client IP", LAB_ERR_RECEIVE);
	}
	
	printf("[XInit] g_ServerInfo.chID: %s \n", g_ServerInfo.chID);
	log_on_write_error(*psServerLog,  (char*)g_chClientID, sizeof(g_chClientID), "sending client ID", LAB_ERR_SEND);
	log_on_read_error(*psServerLog, (char*)&g_ServerInfo, sizeof(g_ServerInfo), "Receiving server config", LAB_ERR_RECEIVE);
	printf("[XInit] g_ServerInfo.chID: %s \n", g_ServerInfo.chID);

	if (strlen(g_ServerInfo.chID) == 0) {
		close2(*psServerLog);
		return ERR_DUPLICATE_ID;
	}
	ubClientState = CSTATE_IDLE;
	log_on_write_error(*psServerLog,  (char*)&ubClientState, sizeof(UINT8), "sending client state", LAB_ERR_SEND);

	getsockname(*psServerLog,(struct sockaddr *)&AddrLocal, &nAddrLen);
	unLocalAddr = AddrLocal.sin_addr.s_addr;
	log_on_write_error(*psServerLog,  (char*)&unLocalAddr, sizeof(UINT32), "sending client local address", LAB_ERR_SEND);

	return ERR_NONE;

LAB_ERR_CREATE_SOCKET:
	close2(*psServerLog);
	*pnErrCode = XGetErrno();
	return ERR_CREATE_SOCKET;
LAB_ERR_CONNECT:
	close2(*psServerLog);
	*pnErrCode = XGetErrno();
	return ERR_CONNECT;
LAB_ERR_RECEIVE:
	close2(*psServerLog);
	*pnErrCode = XGetErrno();
	return ERR_RECEIVE;
LAB_ERR_SEND:
	close2(*psServerLog);
	*pnErrCode = XGetErrno();
	return ERR_SEND;
}

/***********************************************************************
 * FUNCTION:		XListen() 
 * DESCRIPTION:  	Listen on a specified socket through the help of a STUNT server.
 * PARAMETERS:		
 *					->	sServerLog	The STUNT server socket. This socket must be gotten from XInit() and be valid.
 *					<->	psListen	User specified listen socket. User should access this socket to read/ write 
 *									data if the process works successfully.
 *					<->	pAddrPeer	The address information of the connecting peer. Pass NULL if user does not 
 *									need this information.
 *					->	nTimeoutSec	The timeout is used when another peer is attempting to connect to. It's not 
 *									the listen waiting time.  
 *					<->	pnErrCode	Error code.
 * RETURNED:		
 *					ERR_NONE			Successful.
 *					ERR_TIMEOUT			Timeout during waiting the connection request from STUNT server.
 *					ERR_SELECT			SELECT fail during waiting the connection request from STUNT server.
 *					ERR_RECEIVE			Fail during receiving control channel data from STUNT server.
 *					ERR_CREATE_SOCKET	Fail to create a socket.
 *					ERR_CONNECT			Fail to connect to the control channel of STUNT server. 
 *					ERR_ECHO_TIMEOUT	Timeout during reading sync-echo
 *					ERR_SYN_RECEIVE		Fail to receive echo
 *					ERR_SYN_SEND		Fail to send echo.
 *					ERR_ASYMSERVER		Fail during being an asymmetric server.
 *
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XListen(SOCKET sServerLog, SOCKET* psListen, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode)
{
	Echo Peer;
    struct timeval	Timeout;
    fd_set Socks;
    SOCKET sCtrl = (SOCKET) -1;
	struct sockaddr_in AddrCtrl;
	INT32 nOne = 1, nAsymErrCode = 0;   
    INT32 nRetVal = 0;

	//Using in macro by Saikat: 
	CHAR errbuf[128];
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////

	FD_ZERO(&Socks);
	FD_SET(sServerLog, &Socks);
	Timeout.tv_sec = 0;
	Timeout.tv_usec = 1;
	//Waiting client: set timeout to 1 usec for non-blocking 
	nRetVal = select(((INT32)sServerLog) + 1, &Socks, NULL, NULL, &Timeout); 
	if (nRetVal == 0)
	{	
		*pnErrCode = XGetErrno();
		return ERR_TIMEOUT;
	}
	else if (nRetVal == -1)	//SOCKET_ERROR
	{
		*pnErrCode = XGetErrno();
		return ERR_SELECT;
	}

	log_on_read_error(sServerLog, (CHAR*)&Peer, sizeof(Peer), "Receiving peer data", LAB_ERR_RECEIVE);

	//Reset Control socket
	close2(sCtrl);
	//Assign the address family of of CTRL SERVER
	//Assign the socket address/port of of the server (control channel port) which is returned by channel
	XInitSockAddr(&AddrCtrl, AF_INET, NULL, 0, Peer.nIP, Peer.wPort);
	//Create control channel socket
	act_on_error(sCtrl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "Creating control channel socket", goto LAB_ERR_CREATE_SOCKET);
	act_on_error(nRetVal = connect(sCtrl, (struct sockaddr *)&AddrCtrl, sizeof(AddrCtrl)), "Connecting to server", goto LAB_ERR_CONNECT);
	//Sync with peer
	readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_ECHO_TIMEOUT);
	log_on_Xread_error(sCtrl,&nOne,sizeof(nOne),"Receiving echo", LAB_ERR_SYN_RECEIVE);
	log_on_Xwrite_error(sCtrl, &nOne, sizeof(nOne), "Sending echo", LAB_ERR_SYN_SEND);
	readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_ECHO_TIMEOUT);
	log_on_Xread_error(sCtrl,&nOne,sizeof(nOne),"Receiving echo", LAB_ERR_SYN_RECEIVE);

printf("aaa \n");
	nRetVal = XAsymServer(sServerLog, sCtrl, psListen, pAddrPeer, ASYM_TIMEOUT, &nAsymErrCode); 
printf("bbb \n");
printf("nRetVal= %d \n", nRetVal);
	if (nRetVal != ERR_NONE)
	{
		//Todo: The error code should be recorded.
		*pnErrCode = nRetVal;
		close2(sCtrl);		//need to close???
		return ERR_ASYMSERVER;
	}
	else
	{
		close2(sCtrl);
		return ERR_NONE;
	}

LAB_ERR_RECEIVE:
	*pnErrCode = XGetErrno();
	return ERR_RECEIVE;

LAB_ERR_CREATE_SOCKET:
	*pnErrCode = XGetErrno();
	return ERR_CREATE_SOCKET;

LAB_ERR_CONNECT:
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_CONNECT;

LAB_ERR_ECHO_TIMEOUT:
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_ECHO_TIMEOUT;

LAB_ERR_SYN_RECEIVE:
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_SYN_RECEIVE;

LAB_ERR_SYN_SEND:
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_SYN_RECEIVE;
}

/***********************************************************************
 * FUNCTION:		XConnect() 
 * DESCRIPTION:  	Create a STUNT connection to a specified peer through the help of a STUNT server.
 * PARAMETERS:		
 *					->	sServerLog	The STUNT server socket. This socket must be gotten from XInit() and be valid.
 *					<->	pchPeerID	The ID of the destination peer who will be connected to. The length of the 
 *									ID must shorter than 32 characters long.
 *					<->	psConnect	User specified connection socket.
 *					<->	pAddrPeer	The address information of the connecting peer. Pass NULL if user does not need 
 *									this information.
 *					->	nTimeoutSec	The timeout is used when the function is attempting to connect to.  
 *					<->	pnErrCode	Error code.
 * RETURNED:		
 *					ERR_NONE			Successful.
 *					ERR_CREATE_SOCKET	Fail to create the communication/ control channel socket.
 *					ERR_CONNECT			Fail to connect to the communication/ control channel of the STUNT server. 
 *					ERR_MATCH			The matching process is failed. (The reason will be shown on the STUNT server.)
 *					ERR_SAME_NAT		The destination peer is behind the same NAT. The local address of the peer 
 *										will be returned through pnErrCode. Programmers should try the direct 
 *										connection in LAN by using this address.
 *					ERR_TIMEOUT			Timeout during waiting receiving peer data from STUNT server.
 *					ERR_RECEIVE			Fail to receive data.
 *					ERR_CREATE_SOCKET	Fail to create a socket.
 *					ERR_ECHO_TIMEOUT	Timeout during reading sync-echo.
 *					ERR_SYN_RECEIVE		Fail to receive echo.
 *					ERR_SYN_SEND		Fail to send echo.
 *					ERR_COMM_TIMEOUT	Timeout during reading response from communication service.
 *					ERR_ASYMCLIENT		Fail during being a asymmetric client.
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XConnect(SOCKET sServerLog, CHAR *pchPeerID, SOCKET *psConnect, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode)
{
	Msg MsgCon;
	Echo Peer;
    struct timeval      Timeout;
    INT32 nRetVal = 0, nCommRet = 0;
    fd_set Socks;
	SOCKET sComm = (SOCKET) -1;
    SOCKET sCtrl = (SOCKET) -1;
	struct sockaddr_in AddrComm, AddrCtrl;
	INT32 nDelay = 0, nOne = 1, nAsymErrCode = 0;
#ifndef _WIN32
	struct timeb tStart, tCurrent;	
#endif

#ifdef TEST_TIME
#ifdef _WIN32
	INT32 nStart = 0, nEnd = 0;
#endif
#endif

	//Using in macro by Saikat: 
	CHAR errbuf[128];
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////

	XInitSockAddr(&AddrComm, AF_INET, g_szServerIP1, SERVER_COMM_PORT, 0, 0);
	close2(sComm);
	//Create communication socket
	act_on_error(sComm = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "Creating communication socket", goto LAB_ERR_CREATE_SOCKET);
	//Connect to the comm
	act_on_error(nRetVal = connect(sComm, (struct sockaddr *)&AddrComm, sizeof(AddrComm)), "Connecting to communication server", goto LAB_ERR_CONNECT);
	//Write the Message sent to Communication server
	MsgCon.nType = htonl(MSG_CONNECT);
	strcpy(MsgCon.Data.Connection.chDstID, pchPeerID);
	strcpy(MsgCon.Data.Connection.chSrcID, g_chClientID);
	//send the message
	log_on_write_error(sComm,  (char*)&MsgCon, sizeof(MsgCon), "Sending communication message", LAB_ERR_SEND);
	//receive the result
#ifdef TEST_TIME
#ifdef _WIN32
	nStart = GetTickCount();
#endif
#endif
    readtimeout(sComm, Socks, nTimeoutSec, LAB_ERR_COMM_TIMEOUT);	//COMM_TIMEOUT
#ifdef TEST_TIME
#ifdef _WIN32
	nEnd = GetTickCount();
	printf("Receiving communication result [%d] ms.\n", nEnd - nStart);
#endif
#endif
	log_on_read_error(sComm, (char*)&nCommRet, sizeof(nCommRet), "Receiving communication result", LAB_ERR_RECEIVE);

	if (nCommRet == -1)
	{
		close2(*psConnect);
		close2(sComm);
		return ERR_MATCH;		
	}
	else if (nCommRet != 0)
	{
		close2(*psConnect);
		close2(sComm);
		*pnErrCode = nCommRet;
		return ERR_SAME_NAT;		
	}
#ifdef TEST_TIME
#ifdef _WIN32
	nStart = GetTickCount();
#endif
#endif
    readtimeout(sServerLog, Socks, nTimeoutSec, LAB_ERR_TIMEOUT);	
#ifdef TEST_TIME
#ifdef _WIN32
	nEnd = GetTickCount();
	printf("Receiving peer data [%d] ms.\n", nEnd - nStart);
#endif
#endif
	log_on_read_error(sServerLog, (CHAR*)&Peer, sizeof(Peer), "Receiving peer data", LAB_ERR_RECEIVE);
	close2(sCtrl);
	//Assign the socket address/port of of the server (control channel port) which is returned by channel
	XInitSockAddr(&AddrCtrl, AF_INET, NULL, 0, Peer.nIP, Peer.wPort);
	//Create control channel socket
	sCtrl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	act_on_error(sCtrl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "Creating control socket", goto LAB_ERR_CREATE_SOCKET);
	act_on_error(connect(sCtrl, (struct sockaddr *)&AddrCtrl, sizeof(AddrCtrl)), "Connecting to control server",  goto LAB_ERR_CONNECT);
	//Sync with peer
#ifdef _WIN32
	nDelay = GetTickCount();
#else
	ftime(&tStart);
#endif
	log_on_Xwrite_error(sCtrl, &nOne, sizeof(nOne), "Sending echo", LAB_ERR_SYN_SEND);
#ifdef TEST_TIME
#ifdef _WIN32
	nStart = GetTickCount();
#endif
#endif
    readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_ECHO_TIMEOUT);		//here we let programmer to set this value
#ifdef TEST_TIME
#ifdef _WIN32
	nEnd = GetTickCount();
	printf("Reading echo [%d] ms.\n", nEnd - nStart);
#endif
#endif
    log_on_Xread_error(sCtrl, &nOne, sizeof(nOne), "Reading echo", LAB_ERR_SYN_RECEIVE);
#ifdef _WIN32
	nDelay = GetTickCount() - nDelay;
#else
	ftime(&tCurrent);
	nDelay = (int) (1000.0 * (tCurrent.time - tStart.time)
        + (tCurrent.millitm - tStart.millitm));
#endif
	log_on_Xwrite_error(sCtrl, &nOne, sizeof(nOne), "Sending echo", LAB_ERR_SYN_SEND);
#ifdef _WIN32
	Sleep(nDelay / 2);
#else
	XSleep(0, nDelay / 2);
#endif
#ifdef TEST_TIME
#ifdef _WIN32
	nStart = GetTickCount();
#endif
#endif
	nRetVal = XAsymClient(sServerLog, sCtrl, psConnect, pAddrPeer, ASYM_TIMEOUT, &nAsymErrCode);
#ifdef TEST_TIME
#ifdef _WIN32
	nEnd = GetTickCount();
	printf("AsymClient [%d] ms.\n", nEnd - nStart);
#endif
#endif

	if (nRetVal != ERR_NONE)
	{
		*pnErrCode = nRetVal;
		close2(*psConnect);
		close2(sComm);
		close2(sCtrl);
		printf("ERR_ASYMCLIENT \n");
		return ERR_ASYMCLIENT;
	}
	else
	{
		close2(sComm);
		close2(sCtrl);
		return ERR_NONE;
	}

LAB_ERR_CREATE_SOCKET:
	close2(sComm);
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_CREATE_SOCKET;

LAB_ERR_CONNECT:
	close2(sComm);
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_CONNECT;

LAB_ERR_SEND:
	close2(sComm);
	*pnErrCode = XGetErrno();
	return ERR_SEND;

LAB_ERR_COMM_TIMEOUT:
	close2(sComm);
	*pnErrCode = XGetErrno();
	return ERR_COMM_TIMEOUT;

LAB_ERR_RECEIVE:
	close2(sComm);
	*pnErrCode = XGetErrno();
	return ERR_RECEIVE;

LAB_ERR_TIMEOUT:
	close2(sComm);
	*pnErrCode = XGetErrno();
	return ERR_TIMEOUT;

LAB_ERR_ECHO_TIMEOUT:
	close2(sComm);
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_ECHO_TIMEOUT;

LAB_ERR_SYN_RECEIVE:
	close2(sComm);
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_SYN_RECEIVE;

LAB_ERR_SYN_SEND:
	close2(sComm);
	close2(sCtrl);
	*pnErrCode = XGetErrno();
	return ERR_SYN_SEND;
}


/***********************************************************************
 * FUNCTION:		XDeRegister() 
 * DESCRIPTION:  	Deregister the client on and close the socket of the connection to the STUNT server.
 * PARAMETERS:		
 *					->	sServerLog	The STUNT server socket. This socket must be gotten from XInit() and be valid.
 *					->	pchID		The client ID which will be deregistered from the STUNT server.
 *					<->	pnErrCode	Error code.
 * RETURNED:		
 *					ERR_NONE			Successful.
 *					ERR_CREATE_SOCKET	Fail to create a socket to the communication port.
 *					ERR_CONNECT			Fail to connect to the communication port of STUNT server. 
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XDeRegister(SOCKET sServerLog, CHAR* pchID, INT32* pnErrCode)
{
	//send message to the stunt server by communication thread to remove the client by ID
	Msg MsgDereg;
    INT32 nRetVal = 0;
	SOCKET sComm = (SOCKET) -1;
	struct sockaddr_in AddrComm;

	XInitSockAddr(&AddrComm, AF_INET, g_szServerIP1, SERVER_COMM_PORT, 0, 0);
	close2(sComm);
	//Create communication socket
	sComm = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
	if (sComm == INVALID_SOCKET )
#else
	if (sComm == -1 )
#endif
	{
		close2(sComm);
		*pnErrCode = XGetErrno();
		return ERR_CREATE_SOCKET;
	}

	//Connect to the comm
	nRetVal = connect(sComm, (struct sockaddr *)&AddrComm, sizeof(AddrComm));
	if (nRetVal == -1) //SOCKET_ERROR 
	{
		close2(sComm);
		*pnErrCode = XGetErrno();
		return ERR_CONNECT;
	}

	//Write the Message sent to Communication server
	MsgDereg.nType = htonl(MSG_DEREGISTER);
	//ID of the client;
	strcpy(MsgDereg.Data.chID, pchID);
	//send the message
	send(sComm, (char*)&MsgDereg, sizeof(MsgDereg), 0);
	//close the socket
	close2(sServerLog);
	return ERR_NONE;
}

/************************************************************************************************************************/
/*			Private Function Definition																					*/
/************************************************************************************************************************/
/***********************************************************************
 * FUNCTION:		XAsymServer() 
 * DESCRIPTION:  	This function predicts the global port, exchanges the IP/PORT information through 
 *					STUNT server, and finally connects to the Asymmetric Client.
 * PARAMETERS:
 *					->	sServerLog	The STUNT server socket. This socket must be gotten from XInit() and be valid.
 *					->	sCtrl	A valid Control service socket of the STUNT server.
 *					<->	psListen	User specified listen socket. User should access this socket to 
 *									read/ write data if the process works successfully.
 *					<->	pAddrPeer	The address information of the connecting peer. Pass NULL if user 
 *									does not need this information.
 *					->	nTimeoutSec	The timeout will be set in all of the waiting procedures in this 
 *									function.
 *					<->	pnErrCode	Error code.
 * RETURNED:		
 *					ERR_NONE	Successful.
 *					ERR_PREDICT	Fail during port prediction. Check in XPredict().
 *					ERR_SEND	Fail to send public IP/PORT of the client.
 *					ERR_TIMEOUT	Timeout during waiting reading data from STUNT server.
 *					ERR_SELECT	SELECT fail during connecting to another peer.
 *					ERR_RECEIVE	Fail during receiving peer information or sync data from STUNT server.
 *					ERR_CONNECT	Fail to connect to the destination peer. 
 *					ERR_HAIRPIN	The public IP addresses of source and destination are the same.
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XAsymServer(SOCKET sServerLog, SOCKET sCtrl, SOCKET *psListen, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode) 
{
    struct sockaddr_in AddrPeer, AddrClient, AddrGlobal;
    struct timeval Timeout;
    fd_set Socks;
    INT32 nOne = 0, nRetVal = 0;
	unsigned long ulOne = 0;

	//Using in macro by Saikat: 
	CHAR errbuf[256];
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////

	*psListen = (SOCKET) -1;
    
    strncpy(errbuf, "PORTPRED", 128);
    log_on_error(XPredict(sServerLog, psListen, &AddrGlobal, &AddrClient), "Predicting port", LAB_ERR_PREDICT);
    strncpy(errbuf, "CONTROL", 128);
    log_on_Xwrite_error(sCtrl, &AddrGlobal.sin_addr.s_addr, sizeof(AddrGlobal.sin_addr.s_addr), "Sending address", LAB_ERR_SEND);
    log_on_Xwrite_error(sCtrl, &AddrGlobal.sin_port, sizeof(AddrGlobal.sin_port), "Sending predicted port", LAB_ERR_SEND);

    AddrPeer.sin_family = AF_INET;
	 printf("  1 \n");
    readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_TIMEOUT);
    log_on_Xread_error(sCtrl, &AddrPeer.sin_addr.s_addr, sizeof(AddrPeer.sin_addr.s_addr), "Receiving address", LAB_ERR_RECEIVE);
	 printf("  2 \n");
    readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_TIMEOUT);
    log_on_Xread_error(sCtrl, &AddrPeer.sin_port, sizeof(AddrPeer.sin_port), "Receiving port", LAB_ERR_RECEIVE);
	 printf("  3 \n");
    readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_TIMEOUT);
	 printf("  4 \n");
    log_on_Xread_error(sCtrl, &nOne, sizeof(nOne), "Ready", LAB_ERR_RECEIVE);
	 printf("  5 \n");

	//HairPin Problem: do not process
	if (AddrGlobal.sin_addr.s_addr == AddrPeer.sin_addr.s_addr)
	{
		return ERR_HAIRPIN;
	}

	//Set psListen to non-blocking I/O
#ifdef _WIN32
	ulOne = 1;
	ioctlsocket(*psListen, FIONBIO, &ulOne);
#else
	fcntl(*psListen, F_SETFL, O_NONBLOCK);
#endif
	
	nRetVal = connect(*psListen, (struct sockaddr *)&AddrPeer, sizeof(AddrPeer));

	if (nRetVal == -1)
	{	
		*pnErrCode = XGetErrno();
#ifdef _WIN32
		if (*pnErrCode != WSAEWOULDBLOCK)	//would block
#else
		if (*pnErrCode != 115) //would block
#endif
			return ERR_CONNECT;
	}

	FD_ZERO(&Socks);
	FD_SET(*psListen, &Socks);
	Timeout.tv_sec = nTimeoutSec;
	Timeout.tv_usec = 0;
	//waiting user set timeout, this value MUST be the same as setting in XAsmClient.
	nRetVal = ::select(((INT32)*psListen) + 1, NULL, &Socks, NULL, &Timeout); 
	printf("nRetVal in AsymServer: %d \n", nRetVal);
	if (nRetVal == 0 || nRetVal == -1) //Timeout or SOCKET_ERROR
	{
		*pnErrCode = XGetErrno();
		return ERR_SELECT;
	}
	else
	{
		if (pAddrPeer != NULL)
		{
			pAddrPeer->sin_addr.s_addr = AddrPeer.sin_addr.s_addr;
			pAddrPeer->sin_port = AddrPeer.sin_port;
		}
		return ERR_NONE;
	}

LAB_ERR_PREDICT:
	return ERR_PREDICT;
LAB_ERR_SEND:
	*pnErrCode = XGetErrno();
	return ERR_SEND;
LAB_ERR_TIMEOUT:
	*pnErrCode = XGetErrno();
	return ERR_TIMEOUT;
LAB_ERR_RECEIVE:
	*pnErrCode = XGetErrno();
	return ERR_RECEIVE;

}

/***********************************************************************
 * FUNCTION:		XAsymClient() 
 * DESCRIPTION:  	This function predicts the global port, exchanges the IP/PORT information through 
 *					STUNT server, tries to connect to the destination peer, theoretically gets a failure,
 *					 listens to the Asymmetric Server and finally establishes the connection.
 * PARAMETERS:		
 *					->	sServerLog	The STUNT server socket. This socket must be gotten from XInit() and be valid.
 *					->	sCtrl	A valid Control service socket of the STUNT server.
 *					<->	psConnect	User specified connection socket.
 *					<->	pAddrPeer	The address information of the connecting peer. Pass NULL if 
 *									user does not need this information.
 *					->	nTimeoutSec	The timeout will be set in all of the waiting procedures in 
 *									this function.
 *					<->	pnErrCode	Error code.
 * RETURNED:
 *					ERR_NONE	Successful.
 *					ERR_PREDICT	Fail during port prediction. Check in XPredict().
 *					ERR_SEND	Fail to send public IP/PORT of the client.
 *					ERR_TIMEOUT	Timeout during waiting reading data from STUNT server.
 *					ERR_SELECT	SELECT fail during connecting to another peer.
 *					ERR_RECEIVE	Fail during receiving peer information or sync data from STUNT server.
 *					ERR_CONNECT	Fail to connect to the destination peer. 
 *					ERR_HAIRPIN	The public IP addresses of source and destination are the same.
 *					ERR_TRYCONNECT	XTryConnect() returned ERR_NONE, but it represents that the function should return back failure.
 *					ERR_CREATE_SOCKET	Fail to create a new socket.
 *					ERR_SETSOCKOPT	Fail to set socket option.
 *					ERR_BIND	Fail to bind a new socket.
 *					ERR_LISTEN	Fail to listen.
 *					ERR_ASYM_TIMEOUT	Timeout during waiting the connection from AsymServer.
 *					ERR_ACCEPT	Fail to accept.
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XAsymClient(SOCKET sServerLog, SOCKET sCtrl, SOCKET *psConnect, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode) 
{
    struct sockaddr_in AddrGlobal;
    struct sockaddr_in AddrPeer;
    struct sockaddr_in AddrClient;
#ifdef _WIN32
	INT32 nAddrLen = sizeof(AddrPeer);
#else
	socklen_t nAddrLen = sizeof(AddrPeer);
#endif
	struct timeval Timeout;
    fd_set Socks;
    INT32 nOne = 1;
	SOCKET sAuxServer = (SOCKET)-1;

	//Using in macro by Saikat: 
	CHAR errbuf[256];
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////
	//Initialize socket psConnect
	*psConnect = (SOCKET)-1;

    strncpy(errbuf, "PORTPRED", 128);
    log_on_error(XPredict(sServerLog, &sAuxServer, &AddrGlobal, &AddrClient), "Predicting port", LAB_ERR_PREDICT);
    strncpy(errbuf, "CONTROL", 128);
    log_on_Xwrite_error(sCtrl, &AddrGlobal.sin_addr.s_addr, sizeof(AddrGlobal.sin_addr.s_addr), "Sending address", LAB_ERR_SEND);
    log_on_Xwrite_error(sCtrl, &AddrGlobal.sin_port, sizeof(AddrGlobal.sin_port), "Sending predicted port", LAB_ERR_SEND);

    AddrPeer.sin_family = AF_INET;
    readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_TIMEOUT);
    log_on_Xread_error(sCtrl, &AddrPeer.sin_addr.s_addr, sizeof(AddrPeer.sin_addr.s_addr), "Receiving address", LAB_ERR_RECEIVE);
    readtimeout(sCtrl, Socks, nTimeoutSec, LAB_ERR_TIMEOUT);
    log_on_Xread_error(sCtrl, &AddrPeer.sin_port, sizeof(AddrPeer.sin_port), "Receiving port", LAB_ERR_RECEIVE);

    strncpy(errbuf, "HOLE", 128);
	log_on_error(-1 == XTryConnect(sServerLog, sAuxServer, (struct sockaddr *)&AddrPeer, sizeof(AddrPeer), 1000) ? 0 : -1, "Opening hole", LAB_ERR_TRYCONNECT);
    strncpy(errbuf, "OTHER", 128);
    close2(sAuxServer);
	log_on_error(sAuxServer = socket(PF_INET, SOCK_STREAM, 0), "Creating new socket", LAB_ERR_CREATE_SOCKET);
	log_on_error(setsockopt(sAuxServer, SOL_SOCKET, SO_REUSEADDR, (char *)&nOne, sizeof(nOne)), "Setting REUSEADDR", LAB_ERR_SETSOCKOPT);
	log_on_error(bind(sAuxServer, (struct sockaddr *)&AddrClient, sizeof(AddrClient)), "Binding to local address", LAB_ERR_BIND);

    log_on_error(listen(sAuxServer, 1), "Listening on socket", LAB_ERR_LISTEN);
    strncpy(errbuf, "CONTROL", 128);
    log_on_Xwrite_error(sCtrl, &nOne, sizeof(nOne), "Ready", LAB_ERR_SEND);
	

	printf("AddrGlobal.sin_addr.s_addr: %x \n", AddrGlobal.sin_addr.s_addr);
	printf("AddrPeer.sin_addr.s_addr: %x \n", AddrPeer.sin_addr.s_addr);
	//HairPin Problem: do not process
	if (AddrGlobal.sin_addr.s_addr == AddrPeer.sin_addr.s_addr)
	{
		close2(sAuxServer);
		return ERR_HAIRPIN;
	}
printf("111 \n");
	//waiting for connection from XAsmServer: timeout MUST be the same as XAsmServer
    readtimeout(sAuxServer, Socks, nTimeoutSec, LAB_ERR_ASYM_TIMEOUT);	printf("222 \n");
    strncpy(errbuf, "ACCEPT", 128);
    log_on_error(*psConnect = accept(sAuxServer, (struct sockaddr *)&AddrPeer, &nAddrLen), "Accepting client", LAB_ERR_ACCEPT);

	if (pAddrPeer != NULL)
	{
		pAddrPeer->sin_addr.s_addr = AddrPeer.sin_addr.s_addr;
		pAddrPeer->sin_port = AddrPeer.sin_port;
	}
	close2(sAuxServer);
	return ERR_NONE;

LAB_ERR_PREDICT:
	close2(sAuxServer);
	return ERR_PREDICT;
LAB_ERR_SEND:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_SEND;
LAB_ERR_TIMEOUT:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_TIMEOUT;
LAB_ERR_RECEIVE:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_RECEIVE;
LAB_ERR_TRYCONNECT:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_TRYCONNECT;
LAB_ERR_CREATE_SOCKET:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_CREATE_SOCKET;
LAB_ERR_SETSOCKOPT:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_SETSOCKOPT;
LAB_ERR_BIND:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_SETSOCKOPT;
LAB_ERR_LISTEN:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_LISTEN;
LAB_ERR_ASYM_TIMEOUT:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_ASYM_TIMEOUT;
LAB_ERR_ACCEPT:
	close2(sAuxServer);
	*pnErrCode = XGetErrno();
	return ERR_ACCEPT;
}

/***********************************************************************
 * FUNCTION:		XPredict() 
 * DESCRIPTION:  	This function predicts the global port that will be used in the next connection 
 *					and return the socket with that global port number on NAT.
 * PARAMETERS:		
 *					->	sServerLog	The STUNT server socket. This socket must be gotten from XInit() 
 *									and be valid.
 *					<->	psAux	A socket descriptor. It will be set to a valid socket bound on the 
 *								local address: pAddrLocal and with a predicted port: pAddrGlobal on NAT.
 *					<->	pAddrGlobal	The function will write the address with global predicted port for 
 *									next connection.
 *					<->	pAddrLocal	The function will randomly assign an address to this variable 
 *									and bind it on psAux.
 * RETURNED:
 *					ERR_NONE	Successful.
 *					ERR_FAIL	Failed in this function.
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XPredict(SOCKET sServerLog, SOCKET *psAux, struct sockaddr_in *pAddrGlobal, struct sockaddr_in *pAddrLocal) 
{
	printf("[XPredict] *** \n");
    SOCKET sEcho = (SOCKET) -1;
    INT32 nOne = 1;
#ifdef _WIN32    
	INT32 nAddrLen = sizeof(struct sockaddr_in);
#else
	socklen_t nAddrLen = sizeof(struct sockaddr_in);
#endif
	UINT16 usGlobalPort = 0;
    char chBuf[256];
#ifdef _WIN32
	INT32 nStart = GetTickCount();
#else
	struct timeb tStart, tCurrent;	
	INT32 nTimeInterval = 0;
	ftime(&tStart);
#endif

	//Using in macro by Saikat: 
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////

	pAddrLocal->sin_family = AF_INET;
	pAddrLocal->sin_addr.s_addr = INADDR_ANY;
	pAddrLocal->sin_port = 0;

    log_on_error(sEcho = socket(PF_INET, SOCK_STREAM, 0), "Crearing prediction socket", LAB_ERR);
    log_on_error(setsockopt(sEcho,SOL_SOCKET,SO_REUSEADDR,(char *)&nOne,sizeof(nOne)), "Setting SO_REUSEADDR", LAB_ERR);
    log_on_error(bind(sEcho,(struct sockaddr *)pAddrLocal, sizeof(struct sockaddr_in)), "Binding to local address", LAB_ERR);
    log_on_error(getsockname(sEcho,(struct sockaddr *)pAddrLocal, &nAddrLen), "Retreiving local address", LAB_ERR);
    XCheckConeTCPProbe(sServerLog, sEcho, 1, (UINT32 *)&pAddrGlobal->sin_addr.s_addr, &usGlobalPort);
    close2(sEcho);
    log_on_error(sEcho = socket(PF_INET, SOCK_STREAM, 0), "Creating actual socket", LAB_ERR);
    log_on_error(setsockopt(sEcho,SOL_SOCKET,SO_REUSEADDR,(char *)&nOne,sizeof(nOne)), "Setting SO_REUSEADDR", LAB_ERR);
    log_on_error(bind(sEcho,(struct sockaddr *)pAddrLocal, sizeof(struct sockaddr_in)), "Binding to local address", LAB_ERR);

	if (g_Fingerprint.TCP.nIncrement != RANDOM_INCREMENT) 
	{
		usGlobalPort = (UINT16) (usGlobalPort + g_Fingerprint.TCP.nIncrement);
    } else 
	{
        // random. 1 is as good a random number as any
        usGlobalPort = (UINT16) (usGlobalPort + 1);
    }
    pAddrGlobal->sin_port = ntohs(usGlobalPort);
#ifdef _WIN32
    _snprintf(chBuf, 256, "PREDICT: %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d [%dms]",
				IPPORT(pAddrLocal->sin_addr.s_addr, pAddrLocal->sin_port),
				IPPORT(pAddrGlobal->sin_addr.s_addr, pAddrGlobal->sin_port),
				GetTickCount()-nStart
			 );
#else
	ftime(&tCurrent);
	nTimeInterval = (int) (1000.0 * (tCurrent.time - tStart.time)
        + (tCurrent.millitm - tStart.millitm));             
    snprintf(chBuf, 256, "PREDICT: %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d [%dms]",
				IPPORT(pAddrLocal->sin_addr.s_addr, pAddrLocal->sin_port),
				IPPORT(pAddrGlobal->sin_addr.s_addr, pAddrGlobal->sin_port),
				nTimeInterval
			);
#endif
    log1(sServerLog, "%s", chBuf);
    *psAux = sEcho;

    return ERR_NONE;
LAB_ERR:
    close2(sEcho);
    return ERR_FAIL;
}

/***********************************************************************
 * FUNCTION:		XTryConnect() 
 * DESCRIPTION:  	This function tries to make a connection to the destination peer.
 * PARAMETERS:
 *					->	sServerLog	The STUNT server socket. This socket must be gotten from XInit() 
 *									and be valid.
 *					->	sAuxServer	The socket for creating this connection.
 *					->	pAddrPeer	Destination peer address.
 *					->	pAddPeerLen	Length of pAddrPeer
 *					->	pMiliSec	Timeout of this non-blocking connection
 * RETURNED:			
 *					ERR_NONE	Successful.
 *					ERR_FAIL	Failed in this function.
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XTryConnect(SOCKET sServerLog, SOCKET sAuxServer, struct sockaddr *pAddrPeer, INT32 nAddrPeerLen, INT32 nMiliSec) 
{
#ifdef _WIN32
	int time1 = GetTickCount();
	struct sockaddr_in AnyAddr;
    INT32 nRetVal = 0;

    fd_set Socks;
	struct timeval	Timeout;


	//Using in macro by Saikat: 
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////


	AnyAddr.sin_family = AF_INET;
    AnyAddr.sin_addr.s_addr = INADDR_ANY;
    AnyAddr.sin_port = 0;


	bind(sAuxServer, (struct sockaddr *)&AnyAddr, sizeof(struct sockaddr_in));
	// set non-blocking mode
	u_long iMode = 1;
	ioctlsocket(sAuxServer, FIONBIO, &iMode);
	if (-1 == connect(sAuxServer, pAddrPeer, nAddrPeerLen))
	{	
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			goto LAB_ERR;
	}
	FD_ZERO(&Socks);
	FD_SET(sAuxServer, &Socks);
	Timeout.tv_sec = nMiliSec/1000;
	Timeout.tv_usec = (nMiliSec%1000)*1000;
	printf("%d \n", Timeout.tv_usec);
	nRetVal = select(((INT32)sAuxServer) + 1, NULL, &Socks, NULL, &Timeout); 
	
	printf("-time: %d ms \n", GetTickCount()-time1);
	if (nRetVal == 0 || nRetVal == -1) //Timeout or SOCKET_ERROR
		goto LAB_ERR;
	else
	{
		return ERR_NONE;
	}


LAB_ERR:
    return ERR_FAIL;

#endif
}

/***********************************************************************
 * FUNCTION:		XProbe() 
 * DESCRIPTION:  	This function will fetch the OS type, probe the NAT type of the client, 
 *					generate/write the fingerprint, and send the result to the STUNT server.
 * PARAMETERS:		-> sServerLog: The STUNT server socket. This must be a valid socket connecting
 *									with STUNT server.
 * RETURNED:		ERR_NONE: successful
 *					ERR_FAIL: failed
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XProbe(SOCKET sServerLog) 
{
#ifdef _WIN32
	OSVERSIONINFOEX OSVer;
#endif
	char chBuf[128];
    char chPrint[MAX_FINGERPRINT_LEN];
    INT32 nErr = ERR_FAIL;
#ifdef _WIN32
    ZeroMemory(&OSVer, sizeof(OSVERSIONINFOEX));
    OSVer.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
	
    if( !GetVersionEx((OSVERSIONINFO *)&OSVer)) 
	{
        OSVer.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
        if (!GetVersionEx((OSVERSIONINFO *)&OSVer)) 
		{
			//set all zeros
			OSVer.dwPlatformId = 0; 
			OSVer.dwMajorVersion = 0;
			OSVer.dwMinorVersion = 0;
			OSVer.wServicePackMajor = 0;
			OSVer.wServicePackMinor = 0;
        }
    }
	//printf("[XProbe] 111 \n");
    _snprintf(chBuf, 128, "START %s: WIN32 %d.%d.%d.%d.%d, %s", 
        g_chClientID,
		OSVer.dwPlatformId,
        OSVer.dwMajorVersion,
        OSVer.dwMinorVersion,
        OSVer.wServicePackMajor,
        OSVer.wServicePackMinor,
        BUILD);
#else
    snprintf(chBuf, 128, "START %s: NON_WIN32, %s", g_chClientID, BUILD);
#endif
	//printf("[XProbe] 1111 \n");
    
    log1(sServerLog, "%s", chBuf);
	//printf("[XProbe] 222 \n");
    if (XProbeNAT(sServerLog) == ERR_FAIL) 
	{
		printf("[XProbe] XProbeNAT failed \n");
        nErr = ERR_FAIL;
    }
	else
	{
		//printf("[XProbe] 2220 \n");
		g_Fingerprint.nServerVer = g_nServerVersion;
		g_Fingerprint.nClientVer = BUILD_VER;
		strcpy(g_Fingerprint.chID, g_chClientID);
		g_Fingerprint.nDone = true;
		XGenFingerprint(chPrint,MAX_FINGERPRINT_LEN);
		printf("Fingerprint: %s \n", chPrint);
		log1(sServerLog, "FINGERPRINT: %s", chPrint);
		g_Fingerprint.nGAddr = g_nClientIP;		
		if (XWriteFingerprint(sServerLog) == ERR_FAIL) 
			nErr = ERR_FAIL;
		else
			nErr = ERR_NONE;
	}
	//printf("[XProbe] 333 \n");
	if (nErr == ERR_FAIL) 
	{
		g_Fingerprint.nDone = false;
    }
    return nErr;
}

/***********************************************************************
 * FUNCTION:		XProbeNAT() 
 * DESCRIPTION:  	This is a wrap of XCheckConeTCP(). It just controls the retry times and sleeping time.
 * PARAMETERS:		-> sServerLog: The STUNT server socket. This must be a valid socket connecting
 *									with STUNT server.
 * RETURNED:		ERR_NONE: successful
 *					ERR_FAIL: failed after all retries
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XProbeNAT(SOCKET sServerLog) 
 {
     INT32 nRetry = 0;
	//printf("[XProbeNAT] 000 \n");
     if (XCheckConeTCP(sServerLog) == ERR_FAIL) 
	 { 
		 nRetry++;
		//printf("[XProbeNAT] nRetry: %d \n", nRetry);
#ifdef _WIN32
		 Sleep(PROBE_FAIL_SLEEP_SEC * 1000); 
#else
		 XSleep(0, PROBE_FAIL_SLEEP_SEC * 1000);
#endif
		 if (nRetry == PROBE_RETRY_TIMES) 
			return ERR_FAIL;
	 }

     return ERR_NONE;
 }


/***********************************************************************
 * FUNCTION:		XCheckConeTCP() 
 * DESCRIPTION:  	This function check NAT's TCP port mapping characteristics. It's a wrap of
 *					XCheckConeTCPProbe() and XAnalyzeNature(). 
 * PARAMETERS:		-> sServerLog: The STUNT server socket. This must be a valid socket connecting
 *									with STUNT server.
 * RETURNED:		ERR_NONE: successful
 *					ERR_FAIL: failed
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XCheckConeTCP(SOCKET sServerLog) 
{
    SOCKET sEcho = 0;
    struct sockaddr_in AddrLocal;
    INT32 nOne = 1;
    INT32 i = 0;
    INT32 nAddrLocalLen = sizeof(AddrLocal);
    UINT16 uwTryBindPort = 1024;
    UINT32 unAddrGlobal[4];
    UINT16 uwPortLocal[4];
    UINT16 uwPortGlobal[4];
    INT32 nErr = ERR_FAIL;

	//Using in macro by Saikat: 
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////

	AddrLocal.sin_family = AF_INET;
	AddrLocal.sin_addr.s_addr = INADDR_ANY;
	AddrLocal.sin_port = 0;

	//Bind 1 valid port and try to detect NAT characteristics.
    for(i = 0; i < 2; i++)
	{
		printf("[XCheckConeTCP] i: %d \n", i);
        log_on_error(sEcho = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "Creating echo socket", LAB_DONE);
        log_on_error(setsockopt(sEcho,SOL_SOCKET,SO_REUSEADDR,(char *)&nOne,sizeof(nOne)), "Setting SO_REUSEADDR", LAB_DONE);
		printf("[XCheckConeTCP] 111 \n");
LAB_TRY_BIND:
        printf("[XCheckConeTCP] 222 \n");
		if (AddrLocal.sin_port == 0) 
		{
            AddrLocal.sin_port = htons(uwTryBindPort);
            uwTryBindPort++;
        }
        log_on_error(bind(sEcho,(struct sockaddr *)&AddrLocal, nAddrLocalLen), "Binding to local address", LAB_BIND_ERR);
        goto LAB_SOCK_READY;

LAB_BIND_ERR:
		printf("[XCheckConeTCP] 333 \n");
        AddrLocal.sin_port = 0;
        goto LAB_TRY_BIND;

LAB_SOCK_READY:
		printf("[XCheckConeTCP] 444 \n");
        uwPortLocal[i] = ntohs(AddrLocal.sin_port);
		XCheckConeTCPProbe(sServerLog, sEcho, i, &unAddrGlobal[i], &uwPortGlobal[i]);
#ifdef _WIN32
		Sleep(50);
#else
		XSleep(0, 50);
#endif
    }

    XAnalyzeNature(unAddrGlobal, uwPortGlobal, uwPortLocal, 4, PRO_TCP);
    nErr = ERR_NONE;

LAB_DONE:
    return nErr;
}

/***********************************************************************
 * FUNCTION:		XCheckConeTCPProbe() 
 * DESCRIPTION:  	This function try to connect to the echo service with different IP-PORT combinations and 
 *					then get the public IP and port of the client. 
 * PARAMETERS:		-> sServerLog: The STUNT server socket. This must be a valid socket connecting
 *									with STUNT server.
 *					-> sEcho: A echo socket created in XCheckConeTCP(). It will be used to connect to the 
 *								echo service of STUNT server.
 *					-> nSeq: Test sequence number (0 ~ 4).
 *					<-> *punResAddr: The public address of the client returned from the STUNT server.
 *					<-> *puwResPort: The public port of the client returned from the STUNT server.
 * RETURNED:		N/A
 * REVISION HISTORY:
 ***********************************************************************/
void tcp_punch::XCheckConeTCPProbe(SOCKET sServerLog, SOCKET sEcho, INT32 nSeq, UINT32 *punResAddr, UINT16 *puwResPort) 
{
	//printf("[XCheckConeTCPProbe] *** \n");
    struct sockaddr_in AddrEcho;
    struct sockaddr_in AddrLocal;
#ifdef _WIN32
	INT32 nAddrLocalLen = sizeof(AddrLocal);
#else
	socklen_t nAddrLocalLen = sizeof(AddrLocal);
#endif
    char chBuf[256];
    Echo Rep;
	//Using in macro by Saikat: 
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////

	//try 4 combinations of the echo service - IP1:Port1, IP1:Port2, IP2:Port1, IP2:Port2
    AddrEcho.sin_family = AF_INET;
    AddrEcho.sin_addr.s_addr = nSeq / 2 == 0 ? g_ServerInfo.nIP1 : g_ServerInfo.nIP2;
    AddrEcho.sin_port = nSeq % 2 == 0 ? g_ServerInfo.wPort1 : g_ServerInfo.wPort2;

    log_on_error(getsockname(sEcho,(struct sockaddr *)&AddrLocal, &nAddrLocalLen), "Resolving local address", LAB_DONE);
#ifdef _WIN32
	_snprintf(chBuf, 256, "Sending TCP probe %d: %d.%d.%d.%d:%d => %d.%d.%d.%d:%d",
				nSeq + 1,
				IPPORT(AddrLocal.sin_addr.s_addr, AddrLocal.sin_port),
				IPPORT(AddrEcho.sin_addr.s_addr, AddrEcho.sin_port)
			 );
	printf("[XCheckConeTCPProbe] Sending TCP probe %d: %d.%d.%d.%d:%d => %d.%d.%d.%d:%d \n",
				nSeq + 1,
				IPPORT(AddrLocal.sin_addr.s_addr, AddrLocal.sin_port),
				IPPORT(AddrEcho.sin_addr.s_addr, AddrEcho.sin_port)
			 );
#else
	snprintf(chBuf, 256, "Sending TCP probe %d: %d.%d.%d.%d:%d => %d.%d.%d.%d:%d",
				nSeq + 1,
				IPPORT(AddrLocal.sin_addr.s_addr, AddrLocal.sin_port),
				IPPORT(AddrEcho.sin_addr.s_addr, AddrEcho.sin_port)
			);
#endif
	printf("[XCheckConeTCPProbe] 111 \n");
	log_on_error(connect(sEcho, (struct sockaddr *)&AddrEcho, sizeof(AddrEcho)), chBuf, LAB_DONE);
    printf("[XCheckConeTCPProbe] 222 \n");
	log_on_read_error(sEcho, &Rep, sizeof(Rep), "Reading server response", LAB_DONE);
	printf("[XCheckConeTCPProbe] 333 \n");
#ifdef _WIN32
	_snprintf(chBuf, 256, "PROBE TCP %d: %d.%d.%d.%d:%d (%d.%d.%d.%d:%d) => %d.%d.%d.%d:%d",
				nSeq,
				IPPORT(AddrLocal.sin_addr.s_addr, AddrLocal.sin_port),
				IPPORT(Rep.nIP, Rep.wPort),
				IPPORT(AddrEcho.sin_addr.s_addr, AddrEcho.sin_port)
			 );
#else    
	snprintf(chBuf, 256, "PROBE TCP %d: %d.%d.%d.%d:%d (%d.%d.%d.%d:%d) => %d.%d.%d.%d:%d",
				nSeq,
				IPPORT(AddrLocal.sin_addr.s_addr, AddrLocal.sin_port),
				IPPORT(Rep.nIP, Rep.wPort),
				IPPORT(AddrEcho.sin_addr.s_addr, AddrEcho.sin_port)
			);
#endif
	//Log the procedure on STUNT server
	log1(sServerLog, "%s", chBuf);

	//Return the result
	*punResAddr = Rep.nIP;
	*puwResPort = ntohs(Rep.wPort);

LAB_DONE:
    close2(sEcho);
    return;
}

/***********************************************************************
 * FUNCTION:		XAnalyzeNature() 
 * DESCRIPTION:  	This function analyzes the passed in IP:Port data and write the result on fingerprint  
 * PARAMETERS:		
 *					->	*punAddrGlobal	The pointer which points to a global (public) IP array.
 *					->	*puwPortGlobal	The pointer which points to a global (public) Port array
 *					->	*puwPortLocal	The pointer which points to a local (private) Port array
 *					<->	nTimes	Test times. The value is also the size of the above arrays
 *					<->	nProtocol	Always PRO_TCP in this library.
 * RETURNED:		N/A
 * REVISION HISTORY:
 ***********************************************************************/
void tcp_punch::XAnalyzeNature(UINT32 *punAddrGlobal, UINT16 *puwPortGlobal, UINT16 *puwPortLocal, INT32 nTimes, E_Protocol nProtocol) 
{
    INT32	nTmp = 0, 
			nTmp2[2][4], 
			i = 0, 
			nDelta = 0;

    for (i = 0; i < nTimes; i++) 
	{
		//?? seems impossible to be equal zero
		if (g_Fingerprint.nGAddr == 0) 
			g_Fingerprint.nGAddr = punAddrGlobal[i];
		// an abnormal result
		if ((INT32)punAddrGlobal[i] != g_Fingerprint.nGAddr) 
		{
			g_Fingerprint.nGAddr = 0xFFFFFFFF;
            break;
        }
    }

	//Port Preserving : If Ports on local and NAT are the same in different connections
	nTmp = 0;
    for (i = 0; i < nTimes; i++) 
	{
        if (puwPortLocal[i] == puwPortGlobal[i]) 
			nTmp++;
    }
    if (nTmp > 1) 
	{
        switch(nProtocol) 
		{
        case PRO_TCP:
			g_Fingerprint.TCP.bPortPreserving = 1;
            break;
        }
    }

	//NAT type: Cone or Symmetric is determined by the global port status.
    for (i = 0; i < 4; i++) 
	{
        INT32 j = 0;
        for(j = 0; j < 2; j++) 
		{
            nTmp2[j][i] = 0;
        }
    }

    for (i = 1; i < nTimes; i++) 
	{
        INT32 j = 0;
        nTmp = puwPortGlobal[i] - puwPortGlobal[i-1];
        if (nTmp < 0) 
			nTmp += 65536;
        for(j = 0; j < 4; j++) 
		{
            if (nTmp2[0][j] == nTmp) 
			{
                nTmp2[1][j]++;
                break;
            }
        }
        if (j == 4) 
		{
            for(j = 0; j < 4; j++) 
			{
                if (nTmp2[1][j] == 0) 
				{
                    nTmp2[1][j]++;
                    nTmp2[0][j] = nTmp;
                    break;
                }
            }
        }
    }

    nTmp = 0;
    nDelta = 0xFFFFFFFF;
    for(i = 0; i < 4; i++) 
	{
        if (nTmp2[1][i] >= nTmp) 
		{
            nTmp = nTmp2[1][i];
            nDelta = nTmp2[0][i];
        }
    }

    switch(nProtocol) 
	{
    case PRO_TCP:
        g_Fingerprint.TCP.nIncrement = (nTmp == 1) ? RANDOM_INCREMENT : nDelta;
        break;
    }

}

/***********************************************************************
 * FUNCTION:		XGenFingerprint() 
 * DESCRIPTION:  	This function interprets the binary fingerprint data to a readable string.  
 * PARAMETERS:		
 *					<->	*pchPrint	A buffer for storing the readable fingerprint string.
 *					->	nSize	Size of the above string
 * RETURNED:		N/A
 * REVISION HISTORY:
 ***********************************************************************/
void tcp_punch::XGenFingerprint(CHAR *pchPrint, INT32 nSize) 
{
    strncpy(pchPrint, "", nSize);
    strncat(pchPrint, "TCP ", nSize);
	strncat(pchPrint, g_Fingerprint.TCP.bPortPreserving?"PORT-PRESERVING ":"NON-PORT-PRESERVING ", nSize);

	if (g_Fingerprint.TCP.nIncrement == 0) 
        strncat(pchPrint, "CONE ",nSize);
	else if (g_Fingerprint.TCP.nIncrement == RANDOM_INCREMENT) 
        strncat(pchPrint, "RANDOM-SYMMETRIC ",nSize);
    else
#ifdef _WIN32
		_snprintf(pchPrint+strlen(pchPrint), nSize-strlen(pchPrint), "DELTA-%d-SYMMETRIC ", g_Fingerprint.TCP.nIncrement);
#else
		snprintf(pchPrint+strlen(pchPrint), nSize-strlen(pchPrint), "DELTA-%d-SYMMETRIC ", g_Fingerprint.TCP.nIncrement);
#endif

}

/***********************************************************************
 * FUNCTION:		XWriteFingerprint() 
 * DESCRIPTION:  	This function writes the NAT fingerprint to specified file on the local machine.  
 * PARAMETERS:		-> sServerLog: The STUNT server socket. This must be a valid socket connecting
 *									with STUNT server.
 * RETURNED:		ERR_NONE: successful
 *					ERR_FAIL: failed
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XWriteFingerprint(SOCKET sServerLog) 
{
    INT32 nFD = 0;
    unlink(FINGERPRINT_FILE);

	//Using in macro by Saikat: 
	SOCKET sock_logr = sServerLog;
	/////////////////////////////////////////////////////////////

#ifdef _WIN32    
	log_on_error(nFD = open(FINGERPRINT_FILE, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE), "Opening fingerprint file", LAB_DONE);
#else
	log_on_error(nFD = open(FINGERPRINT_FILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR), "Opening fingerprint file", LAB_DONE);
#endif
	printf("321 \n");
	log_on_error(write(nFD, &g_Fingerprint, sizeof(g_Fingerprint)) == sizeof(g_Fingerprint) ? 0 : -1, "Dumpting fingerprint to file", LAB_ERR);

LAB_DONE:
    close(nFD);
    return ERR_NONE;
LAB_ERR:
    close(nFD);
    return ERR_FAIL;
}

/***********************************************************************
 * FUNCTION:		XReadFingerprint() 
 * DESCRIPTION:  	This function read fingerprint file and write the data on the global 
 *					fingerprint variable.  
 * PARAMETERS:		N/A
 * RETURNED:		ERR_NONE: successful
 *					ERR_FAIL: failed
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XReadFingerprint(void) 
{
#ifdef _WIN32
    INT32 nFD = 0;
	INT32 nErr = ERR_NONE;
	g_Fingerprint.chID[0] = '\0';
	extern int errno; 
	
#ifdef _WIN32
    nFD = _open(FINGERPRINT_FILE, O_RDONLY | O_BINARY);
#else
    nFD = open(FINGERPRINT_FILE, O_RDONLY, S_IRUSR | S_IWUSR);
#endif
	if (nFD == -1) {
		nErr = ERR_FAIL;
		
		switch (errno) {
			//case EACEES: 	
			//case EEXIST:
			//case EINVAL:
			//case EMFILE:
			case ENOENT:	// file or path not found
				nFD = _open(FINGERPRINT_FILE, _O_CREAT | O_BINARY, _S_IREAD | _S_IWRITE);
				break;
			default:
				printf("Open %s error \n", FINGERPRINT_FILE);
				nErr = ERR_FAIL;
			}
		
	}

    if (read(nFD, &g_Fingerprint, sizeof(g_Fingerprint)) != sizeof(g_Fingerprint)) 
		nErr = ERR_FAIL;

	//check build version
	if (g_Fingerprint.nClientVer != BUILD_VER) 
		nErr = ERR_FAIL;

	//check client ID
	if (g_Fingerprint.chID[0] == '\0') 
		nErr = ERR_FAIL;

	close(nFD);

	if (nErr == ERR_FAIL)
	{
		unlink(FINGERPRINT_FILE);
		strcpy(g_chClientID, g_Fingerprint.chID);
		memset(&g_Fingerprint, 0, sizeof(g_Fingerprint));
	}

	return nErr;
#endif
}

/////////////////////////////////////////////////////////////////////////
//Utilities
/////////////////////////////////////////////////////////////////////////
/***********************************************************************
 * FUNCTION:		XInitSockAddr() 
 * DESCRIPTION:  	Initialize socket address.
 * PARAMETERS:		<-> *SockAddr:	The socket address object needed to set.
 *					-> wfamily:	Net family
 *					-> pchAddr: Socket address in string. Pass NULL if pass in converted data
 *					-> uwPort: Socket port
 *					-> unConvertedAddr: converted address
 *					-> uwConvertedPort: converted port
 * RETURNED:			always ERR_NONE
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XInitSockAddr(struct sockaddr_in *pSockAddr, INT16 wFamily, const CHAR * pchAddr, UINT16 uwPort, UINT32 unConvertedAddr, UINT16 uwConvertedPort)
{
	if (pchAddr != NULL) {
		pSockAddr->sin_family = wFamily;
		pSockAddr->sin_addr.s_addr = inet_addr(pchAddr);
		pSockAddr->sin_port = htons(uwPort);
	}
	else {
		pSockAddr->sin_family = wFamily;
		pSockAddr->sin_addr.s_addr = unConvertedAddr;
		pSockAddr->sin_port = uwConvertedPort;
	}
	return ERR_NONE;
}

/***********************************************************************
 * FUNCTION:		XSleep() 
 * DESCRIPTION:  	A delay function.
 * PARAMETERS:		-> nSec:	Time value, in seconds.
 *					-> nUsec:	Time value, in microseconds
 * RETURNED:		N/A
 * REVISION HISTORY:
 ***********************************************************************/
void tcp_punch::XSleep(INT32 nSec, INT32 nUSec)
{
	struct timeval tv;
	fd_set s;
	
	FD_ZERO(&s);
	tv.tv_sec = nSec;
	tv.tv_usec = nUSec;

	select(0, NULL, NULL, NULL, &tv);
}

/***********************************************************************
 * FUNCTION:		XGetErrno() 
 * DESCRIPTION:  	Get errno in windows and lunux.
 * PARAMETERS:		N/A
 * RETURNED:		WIN_32: window socket error code
 *					o/w:	errno					
 * REVISION HISTORY:
 ***********************************************************************/
INT32 tcp_punch::XGetErrno(void)
{
#ifdef _WIN32
	return (WSAGetLastError());
#else
	return errno;
#endif
}



