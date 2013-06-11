
#include "io_nat_punch.h"
#include "nat_interface.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "ClientAPI.h"

using namespace std;

io_nat_punch::io_nat_punch(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr,nat_interface *nat_interface_ptr){
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_communication_ptr = peer_communication_ptr;
	_nat_interface_ptr = nat_interface_ptr;

	memset(chServerIP1,0x00,20);
	memset(chServerIP2,0x00,20);
	memset(self_id,0x00,(MAX_ID_LEN + 1));

	cout<<chServerIP1<<"140.114.90.146";
	cout<<chServerIP2<<"140.114.90.1";
	
	nat_punch_counter = 0;
	self_id_integer = 0;
	
}

io_nat_punch::~io_nat_punch(){
}

void io_nat_punch::register_to_nat_server(unsigned long self_pid){
	/*
	the peer will register to nat server at the join phase
	*/
	ultoa(self_pid,self_id,10);
	self_id_integer = self_pid;

	INT32 nRet,nErr;
	nRet = XInit(chServerIP1, chServerIP2, (SOCKET *)&nat_server_fd, (char*)self_id, &nErr);
	if (nRet == ERR_NONE)
	{
		printf("nRet: %d \n", nRet);
		printf("Register to XSTUNT succeeded \n");
	}
	else
	{
		printf("Initialization failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
		exit(1);
	}

	//Delay 2.5 seconds for registering!!!
	#ifdef _WIN32
			Sleep(2500);
	#else
			sleep(3);
	#endif

	_net_ptr->set_nonblocking(nat_server_fd);

	_net_ptr->epoll_control(nat_server_fd, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->set_fd_bcptr_map(nat_server_fd, dynamic_cast<basic_class *> (this));
	_peer_mgr_ptr->fd_list_ptr->push_back(nat_server_fd);
}

/*
if nat punch succeed, we have to bind th fd to nat interface
*/
#ifdef _WIN32
SOCKET  __stdcall XListenThread(void* argu) {
#else
void *XListenThread(void* argu) {  
#endif
	
	SOCKET sServer = (( struct nat_con_thread_struct *)argu)->Xconn.sServer;
	SOCKET sPeer = -1;
	int nRet, nErr;


	if ((nRet=XListen(sServer, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
	{
		printf("XListenThread: One client successfully connected...\n");
		((struct nat_con_thread_struct*)argu)->Xconn.sPeer = sPeer;
		/*
		bind to nat_interface object
		*/
		((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_nonblocking(sPeer);
		((network *)(((struct nat_con_thread_struct*)argu)->net_object))->epoll_control(sPeer, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
		((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_fd_bcptr_map(sPeer, dynamic_cast<basic_class *> ((nat_interface *)(((struct nat_con_thread_struct*)argu)->nat_interface_object)));
		((peer_mgr *)(((struct nat_con_thread_struct*)argu)->peer_mgr_object))->fd_list_ptr->push_back(sPeer);
		return 0;
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
		
		//char chStr[MAX_MSG_LEN];
		int nStart = 0, nEnd = 0;
		
		printf("Try direct TCP Connetion...\n");
		//XConnect may fail if the listener is in the same NAT.
		//In this situation, local address of the listener will be returned back through error code.
		//Then try to directly connect to the address. The following sample code does not do this action.
#ifdef TEST_TIME
#ifdef _WIN32
	nStart =GetTickCount();
#else
	printf("timer cannot use\n");
	exit(1);
#endif

#else
	printf("error : not define test time\n");
	exit(1);
#endif 
		if ((nRet = XConnect(sServer, peerId, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
		{
#ifdef TEST_TIME
#ifdef _WIN32
			nEnd = GetTickCount();
			printf("XListenThread: Successfully connected after [%d] ms.\n", nEnd - nStart);
			((struct nat_con_thread_struct*)argu)->Xconn.sPeer = sPeer;
			/*
			bind to nat_interface object
			*/
			((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_nonblocking(sPeer);
			((network *)(((struct nat_con_thread_struct*)argu)->net_object))->epoll_control(sPeer, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_fd_bcptr_map(sPeer, dynamic_cast<basic_class *> ((nat_interface *)(((struct nat_con_thread_struct*)argu)->nat_interface_object)));
			((peer_mgr *)(((struct nat_con_thread_struct*)argu)->peer_mgr_object))->fd_list_ptr->push_back(sPeer);
			return 0;
#endif
#endif
		}
		else
		{
			printf("XListenThread: failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
			return 0;
		}
		
	}
	
	/*fd_set *allset = ((XconnInfo2 *)argu)->allset;
	int *maxfd = ((XconnInfo2 *)argu)->maxfd;
	
	FD_SET(sPeer, allset);  // add new descriptor to set
	if (sPeer > *maxfd) 
		*maxfd = sPeer;  // for select
	*(((XconnInfo2 *)argu)->sPeer) = sPeer;*/
	return 0;
}

/*
if nat punch succeed, we have to bind th fd to nat interface
*/
#ifdef _WIN32
SOCKET  __stdcall XconnectThread(void* argu) {  
#else
void *XconnectThread(void* argu) {  
#endif

	printf("In  XconnectThread \n");
	
	//char chStr[MAX_MSG_LEN];
	SOCKET sPeer = -1;
	SOCKET sServer = ((struct nat_con_thread_struct*)argu)->Xconn.sServer;
	char myId[MAX_ID_LEN + 1] = {0};
	char peerId[MAX_ID_LEN + 1] = {0};
	memcpy(myId,  ((struct nat_con_thread_struct*)argu)->Xconn.myId, sizeof(myId));
	memcpy(peerId,  ((struct nat_con_thread_struct*)argu)->Xconn.peerId, sizeof(peerId));
	
	
	int nStart = 0, nEnd = 0;
	int nRet, nErr;
	//XConnect may fail if the listener is in the same NAT.
	//In this situation, local address of the listener will be returned back through error code.
	//Then try to directly connect to the address. The following sample code does not do this action.
#ifdef TEST_TIME
#ifdef _WIN32
	nStart =GetTickCount();
#else
	printf("timer cannot use\n");
	exit(1);
#endif

#else
	printf("error : not define test time\n");
	exit(1);
#endif 
	if ((nRet = XConnect(sServer, peerId, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
	{
#ifdef TEST_TIME
#ifdef _WIN32
		nEnd = GetTickCount();
		printf("Successfully connected after [%d] ms.\n", nEnd - nStart);
#endif
#endif
		
		((struct nat_con_thread_struct*)argu)->Xconn.sPeer = sPeer;
		/*
		bind to nat_interface object
		*/
		((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_nonblocking(sPeer);
		((network *)(((struct nat_con_thread_struct*)argu)->net_object))->epoll_control(sPeer, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
		((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_fd_bcptr_map(sPeer, dynamic_cast<basic_class *> ((nat_interface *)(((struct nat_con_thread_struct*)argu)->nat_interface_object)));
		((peer_mgr *)(((struct nat_con_thread_struct*)argu)->peer_mgr_object))->fd_list_ptr->push_back(sPeer);
		return 0;
	}
	else
	{
		nEnd = GetTickCount();
		printf("XConnect failed. ErrType(%d) ErrCode(%d) time [%d] ms.\n", nRet, nErr,nEnd - nStart);
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
		nStart = GetTickCount();
		while (((nEnd - nStart) <= LISTEN_TIMEOUT))
		{
			//char chStr[MAX_MSG_LEN];
			//char chChar;
			//int nRcv = 0;
			//SOCKET sListen = (SOCKET) -1;
			//fd_set Socks;

			if ((nRet=XListen(sServer, &sPeer, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
			{
				printf("One client successfully connected...\n");
				((struct nat_con_thread_struct*)argu)->Xconn.sPeer = sPeer;

				/*
				bind to nat_interface object
				*/
				((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_nonblocking(sPeer);
				((network *)(((struct nat_con_thread_struct*)argu)->net_object))->epoll_control(sPeer, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				((network *)(((struct nat_con_thread_struct*)argu)->net_object))->set_fd_bcptr_map(sPeer, dynamic_cast<basic_class *> ((nat_interface *)(((struct nat_con_thread_struct*)argu)->nat_interface_object)));
				((peer_mgr *)(((struct nat_con_thread_struct*)argu)->peer_mgr_object))->fd_list_ptr->push_back(sPeer);
				return 0;
			}
			else
			{
				printf("XListen failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
			}

			nEnd = GetTickCount();
			if((nEnd - nStart) > LISTEN_TIMEOUT){
				printf("timeout : LISTEN_TIMEOUT\n");
			}
		}
	}
	
	return 0;
	
}  

int io_nat_punch::start_nat_punch(int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id){
	/*
	1. it must be active mode, if we call this func.
	2. this function will try to connect fd_pid.
	3. if the connection is built before, then don't build again.
	*/
	

	//char chStr[MAX_MSG_LEN];
	int nStart = 0, nEnd = 0;
	char oppsite_pid[MAX_ID_LEN + 1] = {0};


	if(fd_role == 0){
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

		//之前已經建立過連線的 在map_in_pid_fd裡面 則不再建立(保證對同個parent不再建立第二條線)
		for(map_pid_fd_iter = _peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == fd_pid ){
				return 1;
			}
		}

		/*
		this may have problem****************************************************************
		*/
		pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(fd_pid);
		if(pid_peer_info_iter !=  _pk_mgr_ptr ->map_pid_peer_info.end() ){
			//兩個以上就沿用第一個的連線
			if(_pk_mgr_ptr ->map_pid_peer_info.count(fd_pid) >= 2 ){
				printf("pid =%d already in connect find in map_pid_peer_info  testing",fd_pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",fd_pid,"already in connect find in map_pid_peer_info testing");
					return 1;
			}
		}

		//若在map_pid_peerDown_info 則不再次建立連線
		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(fd_pid);
		if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
			printf("pid =%d already in connect find in map_pid_peerDown_info",fd_pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",fd_pid,"already in connect find in map_pid_peerDown_info");
			return 1;
		}
	}
	else{
	/*
	this part means that if the child is exist, we don't create it again.
	*/
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_info_t *>::iterator map_pid_rescue_peer_info_iter;

		//之前已經建立過連線的 在map_out_pid_fd裡面 則不再建立(保證對同個child不再建立第二條線)
		for(map_pid_fd_iter = _peer_ptr->map_out_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_out_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == fd_pid ){
				return 1;
			}
		}

		//若在map_pid_rescue_peer_info 則不再次建立連線
		map_pid_rescue_peer_info_iter = _pk_mgr_ptr ->map_pid_rescue_peer_info.find(fd_pid);
		if(map_pid_rescue_peer_info_iter != _pk_mgr_ptr ->map_pid_rescue_peer_info.end()){
			printf("pid =%d already in connect find in map_pid_rescue_peer_info",fd_pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",fd_pid,"already in connect find in map_pid_rescue_peer_info");
			return 1;
		}
	}

	ultoa(fd_pid,oppsite_pid,10);
	//SOCKET sConnect = (SOCKET) -1;
	printf("Try direct TCP Connetion...\n");
	
	map_counter_thread_iter = map_counter_thread.find(nat_punch_counter);
	if(map_counter_thread_iter != map_counter_thread.end()){
		printf("error : nat_punch_counter already exist in io_nat_punch::start_nat_punch\n");
		exit(1);
	}

	map_counter_thread[nat_punch_counter] = new struct nat_con_thread_struct;
	map_counter_thread_iter = map_counter_thread.find(nat_punch_counter);
	if(map_counter_thread_iter == map_counter_thread.end()){
		printf("error : nat_punch_counter doesn't exist in io_nat_punch::start_nat_punch\n");
		exit(1);
	}

	map_counter_session_id_iter = map_counter_session_id.find(nat_punch_counter);
	if(map_counter_session_id_iter != map_counter_session_id.end()){
		printf("error : nat_punch_counter already exist in map_counter_session_id io_nat_punch::start_nat_punch\n");
		exit(1);
	}

	map_counter_session_id[nat_punch_counter] = session_id;

	memset(&(map_counter_thread_iter->second), 0, sizeof(struct nat_con_thread_struct));
	printf("size of struct nat_con_thread_struct : %d\n",sizeof(struct nat_con_thread_struct));

	map_counter_thread_iter->second->manifest = manifest;
	map_counter_thread_iter->second->role = fd_role;
	map_counter_thread_iter->second->pid = fd_pid;
	map_counter_thread_iter->second->Xconn.sServer = nat_server_fd;
	map_counter_thread_iter->second->net_object = _net_ptr;
	map_counter_thread_iter->second->nat_interface_object = _nat_interface_ptr;
	map_counter_thread_iter->second->peer_mgr_object = _peer_mgr_ptr;
	memcpy(&(map_counter_thread_iter->second->Xconn.myId), self_id, sizeof(map_counter_thread_iter->second->Xconn.myId));
	memcpy(&(map_counter_thread_iter->second->Xconn.peerId), oppsite_pid, sizeof(map_counter_thread_iter->second->Xconn.peerId));
	
	map_counter_thread_iter->second->hThread = (HANDLE)_beginthreadex(NULL, 0, &XconnectThread, map_counter_thread_iter->second, 0, &(map_counter_thread_iter->second->threadID));

	nat_punch_counter++;
}

int io_nat_punch::handle_pkt_in(int sock)
{	
	/*
	inside this part means that nat server tells the peer, where someone wants to NAT Punch with itself. (candidates)
	*/
	/*int i, maxi, maxfd, listenfd, connfd, sockfd;
	int nready, client[FD_SETSIZE];
	fd_set rset, allset;
		
	maxfd = sServer;  // initialize
	maxi = -1;  // index into client[] array
	for (i = 0; i < FD_SETSIZE; i++) 
		client[i] = -1;  // -1 indicates available entry
	FD_ZERO(&allset); 
	FD_SET(sServer, &allset);*/
		
	printf("Strat listening... in io_nat_punch::handle_pkt_in\n");
	
	map_counter_thread_iter = map_counter_thread.find(nat_punch_counter);
	if(map_counter_thread_iter != map_counter_thread.end()){
		printf("error : nat_punch_counter already exist in io_nat_punch::start_nat_punch\n");
		exit(1);
	}

	map_counter_thread[nat_punch_counter] = new struct nat_con_thread_struct;
	map_counter_thread_iter = map_counter_thread.find(nat_punch_counter);
	if(map_counter_thread_iter == map_counter_thread.end()){
		printf("error : nat_punch_counter doesn't exist in io_nat_punch::start_nat_punch\n");
		exit(1);
	}

	memset(&(map_counter_thread_iter->second), 0, sizeof(struct nat_con_thread_struct));
	printf("size of struct nat_con_thread_struct : %d\n",sizeof(struct nat_con_thread_struct));

	/*HANDLE hThread;
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
				}*/

	map_counter_thread_iter->second->role = -1;
	map_counter_thread_iter->second->Xconn.sServer = nat_server_fd;
	map_counter_thread_iter->second->net_object = _net_ptr;
	map_counter_thread_iter->second->nat_interface_object = _nat_interface_ptr;
	map_counter_thread_iter->second->peer_mgr_object = _peer_mgr_ptr;
	//memcpy(&(map_counter_thread_iter->second->Xconn.myId), self_id, sizeof(map_counter_thread_iter->second->Xconn.myId));
	//memcpy(&(map_counter_thread_iter->second->Xconn.peerId), oppsite_pid, sizeof(map_counter_thread_iter->second->Xconn.peerId));
	
	map_counter_thread_iter->second->hThread = (HANDLE)_beginthreadex(NULL, 0, &XListenThread, map_counter_thread_iter->second, 0, &(map_counter_thread_iter->second->threadID));

	nat_punch_counter++;
		//while (true)
		//{
			/*char chStr[MAX_MSG_LEN];
			char chChar;
			int nRcv = 0;
			SOCKET sListen = (SOCKET) -1;
			fd_set Socks;
			struct timeval	Timeout;*/
			
			//rset = allset;  // structure assignment
			//FD_SET(sServer, &rset);
			//FD_SET(fileno(stdin), &rset);  it doesn't work!!
			
			/*Timeout.tv_sec = 0;
			Timeout.tv_usec = 500000;
			printf("111 \n");
			nready = select(maxfd+1, &rset, NULL, NULL, &Timeout);
			printf("222 \n");*/
			
			
		/*	if (FD_ISSET(sServer, &rset)) 
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
			*/
			/*printf("nready: %d \n", nready);
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
		}*/

	return RET_OK;
}

int io_nat_punch::handle_pkt_out(int sock)
{
	/*
	cannot inside this scope
	*/
	printf("error : place in io_nat_punch::handle_pkt_in\n");
	exit(1);

	return RET_OK;
}

void io_nat_punch::handle_pkt_error(int sock)
{
	cout << "error in io_nat_punch handle_pkt_error error number : "<<WSAGetLastError()<< endl;
	exit(1);
}

void io_nat_punch::handle_job_realtime()
{

}


void io_nat_punch::handle_job_timer()
{

}

void io_nat_punch::handle_sock_error(int sock, basic_class *bcptr){
	cout << "error in io_nat_punch handle_sock_error error number : "<<WSAGetLastError()<< endl;
	exit(1);
}