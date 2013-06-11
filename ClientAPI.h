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
#ifndef _CLIENTAPI_H_
#define _CLIENTAPI_H_
/***********************************************************************
*		 Win32 / Linux Intelx86 C++ Compiler File 		    		   				   
*--------------------------------------------------------------		   
*	[For]				{ XSTUNT Library }				  			   
*	[File name]     	{ ClientAPI.h }                          		   	   
*	[Description]
*	This file is the PUBLIC header of main implementation code for XSTUNT library. 
*	About STUNT, please read: http://nutss.gforge.cis.cornell.edu/stunt.php for more references.
*	[History]
*  	2005.10		Saikat/Paul		Paper and experiment proposed
*	2006.03		Saikat/Kuanyu	C/C++ implementation code released 
***********************************************************************/
/************************************************************************************************************************/
/*			Include Files																								*/
/******************************************************************************************************************/
#ifdef _WIN32
#if !defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#include<winsock2.h>
#include<MSWSock.h>
#endif
#endif
#include<stdio.h>
//#include<fcntl.h>
//#include<sys/stat.h>
//#include<io.h>
//#include<pcap.h>
//#include<time.h>
//#include<sys/timeb.h>

/************************************************************************************************************************/
/*			Constant Definition																							*/
/************************************************************************************************************************/
#ifndef _WIN32
#define SOCKET					INT32
#endif

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
/*			Public Variable Reference																					*/
/************************************************************************************************************************/

/************************************************************************************************************************/
/*			Public Function Reference																					*/
/************************************************************************************************************************/
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
extern INT32 XInit(const char* pchIP1, const char* pchIP2, SOCKET* psServerLog, CHAR* pchID, INT32 *pnErrCode);
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
extern INT32 XListen(SOCKET sServerLog, SOCKET* psListen, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode);
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
 *					ERR_ASYMCLIENT		Fail during being a asymmetric client.
 * REVISION HISTORY:
 ***********************************************************************/
extern INT32 XConnect(SOCKET sServerLog, CHAR *pchPeerID, SOCKET *psConnect, struct sockaddr_in *pAddrPeer, INT32 nTimeoutSec, INT32* pnErrCode);
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
extern INT32 XDeRegister(SOCKET sServerLog, CHAR* pchID, INT32* pnErrCode);
#endif

