/***********************************************************************
*		 Win32 / Linux Intelx86 C++ Compiler File 		    		   				   
*--------------------------------------------------------------		   
*	[For]				{ XSTUNT Sample Code: Echo Server and Client}				  			   
*	[File name]     	{ Sample.cpp }
*	[Description]
*	Following sample code allows 2 peers behind different NATs do direct TCP connection, 
*	but fail if behind the same NAT. Users should modify the code to prevent this situation.
*	A client here can send message (be care of the message length) to the server and soon get
*	echo message from that. The messeage "exit" will cause both exit the program.
*	Build the program and run on 2 machines behind different NATs. Of course, the server 
*	should run before the client!
*	[Notice]
*	After connecting, the messeage should be sent in 60 seconds or the connection will be closed.
*	[History]
*	2006.04		Kuanyu	First released 
***********************************************************************/
/*#include "ClientAPI.h"
#ifdef _WIN32
#include <conio.h>
#include<process.h>
#else
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#endif

#define CONNECT_TRY_TIMES 1
#define DEF_TIMEOUT 15
#define MAX_MSG_LEN 80
//MARK the following line if you don't want to measure the connection time.
#define TEST_TIME
#define FD_SETSIZE 10
#define MAX_ID_LEN					31					//Max Client ID Length
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

typedef struct _XconnInfo
{
	SOCKET sServer;
	SOCKET sPeer;
	char myId[MAX_ID_LEN + 1];
	char peerId[MAX_ID_LEN + 1];
	int flag;
} XconnInfo;

typedef struct _XconnInfo2
{
	SOCKET sServer;
	SOCKET *sPeer;
	char myId[MAX_ID_LEN + 1];
	char peerId[MAX_ID_LEN + 1];
	int flag;
	fd_set *allset;
	int *maxfd;
	
} XconnInfo2;

#ifdef _WIN32
SOCKET  __stdcall XListenThread(void* argu) {
#else
void *XListenThread(void* argu) {  
#endif
	
	SOCKET sServer = ((XconnInfo2 *)argu)->sServer;
	SOCKET sPeer = -1;
	int nRet, nErr;


	if ((nRet=XListen(sServer, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
	{
		printf("XListenThread: One client successfully connected...\n");
	}
	else
	{
		printf("XListenThread: XListen failed \n");
		
		char msg[1024]={0};
		Msg MsgCon;
		
		memset(msg, 0, sizeof(msg));
		recv(sServer, msg, sizeof(Msg)+5, 0);
		memcpy(&MsgCon, msg+5, sizeof(Msg));
		
		char peerId[MAX_ID_LEN + 1]={0};
		strcpy(peerId, MsgCon.Data.Connection.chSrcID);
		
		char chStr[MAX_MSG_LEN];
		int nStart = 0, nEnd = 0;
		
		printf("Try direct TCP Connetion...\n");
		//XConnect may fail if the listener is in the same NAT.
		//In this situation, local address of the listener will be returned back through error code.
		//Then try to directly connect to the address. The following sample code does not do this action.
#ifdef TEST_TIME
#ifdef _WIN32
		nStart =GetTickCount();
#endif
#endif 
		if ((nRet = XConnect(sServer, peerId, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
		{
#ifdef TEST_TIME
#ifdef _WIN32
			nEnd = GetTickCount();
			printf("XListenThread: Successfully connected after [%d] ms.\n", nEnd - nStart);
#endif
#endif
		}
		else
		{
			printf("XListenThread: failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
			return -1;
		}
		
	}
	
	fd_set *allset = ((XconnInfo2 *)argu)->allset;
	int *maxfd = ((XconnInfo2 *)argu)->maxfd;
	
	FD_SET(sPeer, allset);  // add new descriptor to set
	if (sPeer > *maxfd) 
		*maxfd = sPeer;  // for select
	*(((XconnInfo2 *)argu)->sPeer) = sPeer;
	return 0;
}


#ifdef _WIN32
SOCKET  __stdcall XconnectThread(void* argu) {  
#else
void *XconnectThread(void* argu) {  
#endif

	printf("In  XconnectThread \n");
	
	char chStr[MAX_MSG_LEN];
	SOCKET sPeer = -1;
	SOCKET sServer = ((XconnInfo*)argu)->sServer;
	char myId[MAX_ID_LEN + 1];
	char peerId[MAX_ID_LEN + 1];
	memcpy(myId, ((XconnInfo*)argu)->myId, sizeof(myId));
	memcpy(peerId, ((XconnInfo*)argu)->peerId, sizeof(peerId));
	
	
	int nStart = 0, nEnd = 0;
	int nRet, nErr;
	//XConnect may fail if the listener is in the same NAT.
	//In this situation, local address of the listener will be returned back through error code.
	//Then try to directly connect to the address. The following sample code does not do this action.
#ifdef TEST_TIME
#ifdef _WIN32
	nStart =GetTickCount();
#endif
#endif 
	if ((nRet = XConnect(sServer, peerId, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
	{
#ifdef TEST_TIME
#ifdef _WIN32
		nEnd = GetTickCount();
		printf("Successfully connected after [%d] ms.\n", nEnd - nStart);
#endif
#endif
		
		((XconnInfo*)argu)->sPeer = sPeer;
		return 0;
	}
	else
	{
		printf("XConnect failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
		char msg[1024]={0};
		Msg MsgCon;
		strcpy(MsgCon.Data.Connection.chDstID, peerId);
		strcpy(MsgCon.Data.Connection.chSrcID, myId);
		strcpy(msg, "momo:");
		memcpy(msg+5, &MsgCon, sizeof(MsgCon));
		
		printf("myId: %s, peerId: %s \n", MsgCon.Data.Connection.chSrcID, MsgCon.Data.Connection.chDstID);
		send(sServer, msg, sizeof(Msg)+5, 0);
		
		memset(msg, 0, sizeof(msg));
		recv(sServer, msg, sizeof(Msg)+5, 0);
		memcpy(&MsgCon, msg+5, sizeof(Msg));
		
		printf("myId: %s, peerId: %s \n", MsgCon.Data.Connection.chSrcID, MsgCon.Data.Connection.chDstID);
		
		
		printf("Strat listening...\n");
		while (true)
		{
			char chStr[MAX_MSG_LEN];
			char chChar;
			int nRcv = 0;
			//SOCKET sListen = (SOCKET) -1;
			fd_set Socks;

			if ((nRet=XListen(sServer, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
			{
				printf("One client successfully connected...\n");
				((XconnInfo*)argu)->sPeer = sPeer;
				return 0;
			}
			else
			{
				//printf("XListen failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
			}
		}
	}
	
	return 0;
	
}  */

/*
int main(int argc, char **argv) 
{
	SOCKET sServer = (SOCKET) -1;
	INT32 nErr = 0, nRet = 0;
	char chServerIP1[20] = "140.114.90.146";
	char chServerIP2[20] = "140.114.90.1"; 
	INT32 nTry = 0;

	
	char* flag = argv[1];
	char* myId = argv[2];
	char* peerId = argv[3];
	
	
///////////////////////////////////////////////////////////////////////////
//Register Sample Section//////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
	nRet = XInit(chServerIP1, chServerIP2, &sServer, (char*)argv[2], &nErr);
	if (nRet == ERR_NONE)
	{
		printf("nRet: %d \n", nRet);
		printf("Register to XSTUNT succeeded \n");
	}
	else
	{
		printf("Initialization failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
		return 0;
	}

	//Delay 2.5 seconds for registering!!!
	#ifdef _WIN32
			Sleep(2500);
	#else
			sleep(3);
	#endif
	
	
///////////////////////////////////////////////////////////////////////////
//Listen Sample Section////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
	if (strcmp(flag, "1") == 0)
	{
		int i, maxi, maxfd, listenfd, connfd, sockfd;
		int nready, client[FD_SETSIZE];
		fd_set rset, allset;
		
		maxfd = sServer;  // initialize
		maxi = -1;  // index into client[] array
		for (i = 0; i < FD_SETSIZE; i++) 
			client[i] = -1;  // -1 indicates available entry
		FD_ZERO(&allset); 
		FD_SET(sServer, &allset);
		
		printf("Strat listening...\n");
		
		while (true)
		{
			char chStr[MAX_MSG_LEN];
			char chChar;
			int nRcv = 0;
			SOCKET sListen = (SOCKET) -1;
			fd_set Socks;
			struct timeval	Timeout;
			
			rset = allset;  // structure assignment
			//FD_SET(sServer, &rset);
			//FD_SET(fileno(stdin), &rset);  it doesn't work!!
			
			Timeout.tv_sec = 0;
			Timeout.tv_usec = 500000;
			printf("111 \n");
			nready = select(maxfd+1, &rset, NULL, NULL, &Timeout);
			printf("222 \n");
			
			
			if (FD_ISSET(sServer, &rset)) 
			{
				HANDLE hThread;
				unsigned threadID;  
				for (i = 0; i < FD_SETSIZE; i++)
				{
					if (client[i] < 0) 
					{
						XconnInfo2 Xconn;
						Xconn.sServer = sServer;
						Xconn.sPeer = (SOCKET *)&client[i];
						Xconn.allset = (fd_set *)&allset;
						Xconn.maxfd = (int *)&maxfd;
						hThread = (HANDLE)_beginthreadex(NULL, 0, &XListenThread, &Xconn, 0, &threadID);
						break; 
					}
				}
				if (i == FD_SETSIZE) 
					printf("too many clients"); 
				
			}
			
			printf("nready: %d \n", nready);
			for (i = 0; i < FD_SETSIZE; i++)  // check all clients for data
			{
				if ( (sockfd = client[i]) < 0) 
					continue; 
				if (FD_ISSET(sockfd, &rset)) 
				{
					int n, j=0;
					char msg[1024]={0};
					if ((nRcv = recv(sockfd, msg, 1024, 0)) <= 0)
					{
						// P.S  ctrl+c結束程式: windows會發生error；linux會正常關閉連線
						if (nRcv == 0)
							printf("Connection[%d] closed gracefully \n", i);
						else if (nRcv < 0)
							printf("Connection[%d] closed unnormally \n", i);
						FD_CLR(sockfd, &allset);
						client[i] = -1;
					}
					else
					{
						printf("Msg[%d]>> %s\n", i, msg);
						send(sockfd, msg, strlen(msg), 0);
						send(sServer, msg, strlen(msg), 0);
					}
					//printf("--nready: %d \n", nready-1);
					if (--nready <= 0) 
						break;  // no more readable descriptors
				}
			}
		}
		
	}//end if
///////////////////////////////////////////////////////////////////////////
//Connect Sample Section///////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
	else if (strcmp(flag, "2") == 0)
	{
		char chStr[MAX_MSG_LEN];
		int nStart = 0, nEnd = 0;
		SOCKET sConnect = (SOCKET) -1;
		printf("Try direct TCP Connetion...\n");
		
#ifdef _WIN32
		HANDLE hThread[5];
		unsigned threadID[5];  
		XconnInfo Xconn[5];
		
		Xconn[0].sServer = sServer;
		memset(&(Xconn[0].myId), 0, sizeof(Xconn[0].myId));
		memset(&(Xconn[0].peerId), 0, sizeof(Xconn[0].peerId));
		memcpy(&(Xconn[0].myId), myId, strlen(myId));
		memcpy(&(Xconn[0].peerId), "1", 1);
		Xconn[1].sServer = sServer;
		memset(&(Xconn[1].myId), 0, sizeof(Xconn[1].myId));
		memset(&(Xconn[1].peerId), 0, sizeof(Xconn[1].peerId));
		memcpy(&(Xconn[1].myId), myId, strlen(myId));
		memcpy(&(Xconn[1].peerId), "2", 1);

		int i;
		for (i=0; i<2; i++)
		{
			hThread[i] = (HANDLE)_beginthreadex(NULL, 0, &XconnectThread, &Xconn[i], 0, &threadID[i]);
		}
		
		for (i=0; i<2; i++)
		{
			WaitForSingleObject(hThread[i], INFINITE); 
		}
		
		CloseHandle(hThread[0]);  
		CloseHandle(hThread[1]);  
		
		char aaa[1024]={0};
		send(Xconn[1].sPeer, "haaal", 5, 0);
		recv(Xconn[1].sPeer, aaa, 5, 0);
		printf("aaa: %s \n", aaa);
		
		
		
		char chChar;
		int nRcv = 0;
		SOCKET sListen = -1;
		fd_set Socks;
		sListen = Xconn[0].sPeer;
		printf("Xconn[1].sPeer: %d \n", Xconn[1].sPeer);
		printf("Xconn[0].sPeer: %d \n", Xconn[0].sPeer);
		printf("sListen: %d \n", sListen);
		printf("111 \n");
		
		do
		{
			int j = 0;
			for (int  i = 0; i < MAX_MSG_LEN; i++)
				chStr[i] = '\0';
			do 
			{
				FD_ZERO(&Socks);
				FD_SET(sListen, &Socks);
				//sListen is changed to a non-blocking socket, so we need to block it.
				::select(((INT32)sListen) + 1, &Socks, NULL, NULL, NULL);
				nRcv = recv(sListen, &chChar, 1, 0);
				chStr[j] = chChar;
				j++;
			}while(chChar != '\0');
			if (nRcv > 0)
			{
				printf("Msg>> %s\n", chStr);
				send(sListen, chStr, strlen(chStr), 0);
				send(Xconn[1].sPeer, chStr, strlen(chStr), 0);
			}
			else
				break;
		}while (strcmp(chStr, "exit") != 0);
		
		
#else
#endif
		
		
	}//end else if
///////////////////////////////////////////////////////////////////////////
//Deregister Sample Section////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
	XDeRegister(sServer, argv[2], &nErr);
	printf("leave test.\n");
	//end test
	

#ifdef _WIN32
		Sleep(2000);
#endif
	return 1;
}
*/
