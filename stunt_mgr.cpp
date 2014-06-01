#include "stunt_mgr.h"
#include "io_connect.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "io_nonblocking.h"
#include "io_accept_nat.h"

// Include STUNT-Server file
#include "stunt/ClientMacro_v2.h"
#include "stunt/tcp_punch.h"

#ifdef _WIN32
//#include "ws2tcpip.h"  // for setsockopt(..., IPPROTO_IP, IP_TTL, ...)
#include "mstcpip.h"
#endif

using namespace std;

stunt_mgr::stunt_mgr(list<int> *fd_list)
{
#ifdef _WIN32	
	printf("11 \n");
	_tcp_punch_ptr = NULL;
	printf("12 \n");
	_tcp_punch_ptr = new tcp_punch();
	printf("13 \n");
	fd_list_ptr = fd_list;
	printf("14 \n");
	ctrl_timeout = CTRL_TIMEOUT;
	printf("15 \n");
	comm_timeout = COMM_TIMEOUT;
	printf("16 \n");
#endif
}

stunt_mgr::~stunt_mgr(){
#ifdef _WIN32	
	printf("==============Delete stunt_mgr Success==========\n");
	
	//g_pDbgFile_v2 = stdout;
#endif
}

int stunt_mgr::init(unsigned long myPID)
{
#ifdef _WIN32	
	int nRet, s, e, nErr;
	_pid = myPID;
	itoa(_pid, _pidChar, 10);
	printf("stunt server IP: %s \n", STUNT_SERVER_IP1);
	nRet = _tcp_punch_ptr->XInit(STUNT_SERVER_IP1, STUNT_SERVER_IP1, &_sock, _pidChar, &nErr);
	if (nRet == ERR_NONE) {
		printf("nRet: %d \n", nRet);
		printf("Register to XSTUNT succeeded \n");
	} else {
		printf("Initialization failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
		//return 0;
	}
	
	_io_accept_nat_ptr = new io_accept_nat(_net_ptr, _log_ptr, _prep_ptr, _peer_mgr_ptr, _peer_ptr, _pk_mgr_ptr, _peer_communication_ptr, _logger_client_ptr, this);
	
	
	DWORD dwBytesRet=0; 
	struct tcp_keepalive   alive;
	alive.onoff = TRUE;   
    alive.keepalivetime = 60000;   
    alive.keepaliveinterval = 60000;   
   
    if (WSAIoctl(_sock, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR) {   
        printf("WSAIotcl(SIO_KEEPALIVE_VALS) failed; %d\n", WSAGetLastError());   
        PAUSE 
    }   
    
	
	_net_ptr->set_nonblocking(_sock);
	_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(_sock);	

	Sleep(500);
	return 0;
#endif
}

// Must guarantee that before receive STUNT's messages(handle_pkt_in is triggered), I have the corresponding peer in the list
int stunt_mgr::handle_pkt_in(int sock)
{	
#ifdef _WIN32	
	cout << "---------------stunt_mgr::handle_pkt_in-----";
	
	// using for record handle_pkt_in time
	int timeStart, timeEnd;
	timeStart = GetTickCount();
	
	Echo echoMsg;
	struct sockaddr_in 	addrCtrl,
						addrLocal,
						addrGlobal,
						addrPeer;	
	struct timeval		Timeout;
	struct ctrlstate ctrlState;
	
	SOCKET	sPeerTemp,
			sPeer,
			sCtrl;
	int needRecvByte,	// Expected number of bytes I will receive
		nOne = 1;
	unsigned long peerID;
	
	list<unsigned long>::iterator list_pid_itr;
	
	
	// Temp variables
	struct sockaddr_in addr;
	int addrLen=sizeof(struct sockaddr_in);
	int n;
	unsigned long ulOne = 0;
	char msg[1024] = {0};
	
	memset(&ctrlState, 0, sizeof(ctrlState));
	
	if (sock == _sock) {	// STUNT_LOG
		printf("----STUNT_LOG \n");
		
		//// First, receive STUNT-LOG message that someone will connect to me ////
		//receive message of STUNT-CTRL Info so that I will connect to in next step
		//n=recv(_sock, (char*)&echoMsg, sizeof(echoMsg), 0));
		
		if ((n=recv(_sock, (char*)&echoMsg, sizeof(echoMsg), 0)) != 49) {
			printf("ERROR:  %s:%04d  receive STUNT-LOG message  %d \n", __FUNCTION__, __LINE__, n);
			if (n <= 0) {
				printf("WSAGetLastError():%d \n", WSAGetLastError());
			}
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","ERROR: receive STUNT-LOG message \n");
			_logger_client_ptr->log_exit();
			PAUSE
		}
		
		printf("recv: %d, sizeof(echoMsg): %d \n", n, sizeof(echoMsg));
		memset(msg, 0, sizeof(msg));
		
		if ((n=recv(_sock, (char*)&msg, 3, 0)) != 3) {	// what are these 3 bytes??
			printf("ERROR:  %s:%04  receive STUNT-LOG message  %d \n", __FUNCTION__, __LINE__, n);
			if (n < 0) {
				printf("WSAGetLastError():%d \n", WSAGetLastError());
			}
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","ERROR: receive STUNT-LOG message \n");
			_logger_client_ptr->log_exit();
			PAUSE
		}	
		printf("recv: %d \n", n);
		/*
		if (strncmp(msg, "momo:", 5) == 0) {
			Msg MsgCon;
			memcpy(&MsgCon, msg+5, sizeof(Msg));
			printf("momo:  chDstID: %s, chSrcID: %s  \n", MsgCon.Data.Connection.chDstID, MsgCon.Data.Connection.chSrcID);
			if (strcmp(MsgCon.Data.Connection.chSrcID, _myPID) != 0) {
				printf("exchanged !! \n");
				handle_pkt_out(MsgCon.Data.Connection.chDstID, MsgCon.Data.Connection.chSrcID);
			}
			return 0;
		}
		*/
		printf("======echoMsg=================== \n");
		printf("nIP: %s \n", inet_ntoa(*(struct in_addr *)&(echoMsg.nIP)));
		printf("wPort: %d \n", htons(echoMsg.wPort));
		printf("wPadding: %d \n", echoMsg.wPadding);
		printf("chData: %s \n", echoMsg.Data.chData);
		printf("nConnID: %d \n", echoMsg.Data.Conn.nConnID);
		printf("chPeerID: %s \n", echoMsg.Data.Conn.chPeerID);
		printf("nPeerID: %d \n", echoMsg.Data.Conn.nPeerIP);
		printf("chRole: %c \n", echoMsg.Data.Conn.chRole);
		printf("=============================== \n");
		_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: %s:%04d  receive STUNT-LIG STUNT-CTRL port %d", htons(echoMsg.wPort));
	
		
		peerID = atoi(echoMsg.Data.Conn.chPeerID);
		
		
		map<unsigned long, struct peer_info_t_nat>::iterator map_pid_peerInfo_itr;
		map_pid_peerInfo_itr = _map_pid_peerInfo.find(peerID);
		
		// If not found this pid in _map_pid_peerInfo, which means STUNT-LOG is faster than PK
		// Then we should check _map_pid_peerInfo_unknown
		// This situation is regarded as unnormal condition
		if (map_pid_peerInfo_itr == _map_pid_peerInfo.end()) {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","NORMAL: cannot find pid %d in _map_pid_peerInfo \n",peerID);
			
			map_pid_peerInfo_itr = _map_pid_peerInfo_unknown.find(peerID);
			if (map_pid_peerInfo_itr != _map_pid_peerInfo_unknown.end()) {
				printf("ERROR:  %s:%04  found pid %d in _map_pid_peerInfo_unknown \n", __FUNCTION__, __LINE__, peerID);
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","NORMAL: found pid %d in _map_pid_peerInfo_unknown \n",peerID);
				_logger_client_ptr->log_exit();
				PAUSE
			} 
			
			//_map_pid_peerInfo_unknown[peerID].pid = peerID;
			_map_pid_peerInfo_unknown[peerID].ctrl_ip = echoMsg.nIP;		// STUNT-CTRL IP
			_map_pid_peerInfo_unknown[peerID].ctrl_port = echoMsg.wPort;	// STUNT-CTRL port
			
			cout << "[NAT]NORMAL: push pid " << peerID << " into _map_pid_peerInfo_unknown \n";
			_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: push pid %d into _map_pid_peerInfo_unknown", peerID);
			
			timeEnd = GetTickCount();
			printf("consuming time: %d ms \n", timeEnd-timeStart);
			cout << "---------------stunt_mgr::handle_pkt_in end \n";

			return RET_OK;
		}
		
		_map_pid_peerInfo[peerID].ctrl_ip = echoMsg.nIP;		// STUNT-CTRL IP
		_map_pid_peerInfo[peerID].ctrl_port = echoMsg.wPort;	// STUNT-CTRL port
		
		ConnectToCTRL(_map_pid_peerInfo[peerID]);
		
	} else {
		printf("-----STUNT_CTRL \n");
		
		map<SOCKET, unsigned long>::iterator _map_ctrlfd_pid_ptr;
		map<unsigned long, SOCKET>::iterator _map_pid_speer_ptr;
		map<SOCKET, struct ctrlstate>::iterator _map_ctrlfd_state_ptr;
		map<unsigned long, struct peer_info_t_nat>::iterator _map_pid_peerInfo_itr;
		
		sCtrl = sock;
		n = getsockname(sCtrl, (struct sockaddr *)&addr, &addrLen);
		printf("n:%2d  sCtrl: %2d , SrcAddr: %s:%d \n", n, sCtrl, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		n = getpeername(sCtrl, (struct sockaddr *)&addr, &addrLen);
		printf("n:%2d  sCtrl: %2d , DstAddr: %s:%d \n", n, sCtrl, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
				
		
		// Take out and examine the certain value in map
		_map_ctrlfd_pid_ptr = _map_ctrlfd_pid.find(sock);
		if(_map_ctrlfd_pid_ptr == _map_ctrlfd_pid.end()) {
			printf("ERROR: %s:%04d  %d ctrl socket not found \n", __FUNCTION__, __LINE__, sock);
			PAUSE
			return 0;
		}
		peerID = _map_ctrlfd_pid_ptr->second;
		
		_map_pid_speer_ptr = _map_pid_speer.find(peerID);
		if(_map_pid_speer_ptr == _map_pid_speer.end()) {
			printf("ERROR: %s:%04d  %lu peerID not found \n", __FUNCTION__, __LINE__, peerID);
			PAUSE
			return 0;
		}
		sPeerTemp = _map_pid_speer_ptr->second;
		
		_map_ctrlfd_state_ptr = _map_ctrlfd_state.find(sock);
		if(_map_ctrlfd_state_ptr == _map_ctrlfd_state.end()) {
			printf("ERROR: %s:%04d  %d ctrl-state not found \n", __FUNCTION__, __LINE__, sock);
			PAUSE
			return 0;
		}
		
		_map_pid_peerInfo_itr = _map_pid_peerInfo.find(peerID);
		if(_map_pid_peerInfo_itr == _map_pid_peerInfo.end()) {
			printf("ERROR: %s:%04d  %lu %ld %d peerInfo not found \n", __FUNCTION__, __LINE__, peerID, peerID, peerID);
			cout << "peerID:" << peerID << "  \n";
			n = getsockname(sPeerTemp, (struct sockaddr *)&addr, &addrLen);
			printf("n:%2d  fd: %2d , SrcAddr: %s:%d \n", n, sPeerTemp, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			n = getpeername(sPeerTemp, (struct sockaddr *)&addr, &addrLen);
			printf("n:%2d  fd: %2d , DstAddr: %s:%d \n", n, sPeerTemp, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			
			unsigned long uuu = 1;
			cout << "uuu: " << uuu << "\n";
			
			cout << "_map_ctrlfd_state_ptr->second->state: " << _map_ctrlfd_state_ptr->second.state << "\n";
			cout << "_map_ctrlfd_state_ptr->second->isPuncher: " << _map_ctrlfd_state_ptr->second.isPuncher << "\n";
			cout << "_map_ctrlfd_state_ptr->second->myIP: " << inet_ntoa(_map_ctrlfd_state_ptr->second.myGlobalAddr.sin_addr);
			cout << ":" << ntohs(_map_ctrlfd_state_ptr->second.myGlobalAddr.sin_port) << "\n";
			cout << "_map_ctrlfd_state_ptr->second->peerIP: " << inet_ntoa(_map_ctrlfd_state_ptr->second.peerAddr.sin_addr);
			cout << ":" << ntohs(_map_ctrlfd_state_ptr->second.peerAddr.sin_port) << "\n";
			cout << "first: " << _map_ctrlfd_pid_ptr->first << ", second: " << _map_ctrlfd_pid_ptr->second << "\n";
			
			
			_map_pid_peerInfo_itr = _map_pid_peerInfo_unknown.find(peerID);
			if (_map_pid_peerInfo_itr == _map_pid_peerInfo_unknown.end()) {
				cout << "ERROR:  peerID:" << peerID << " not found in both _map_pid_peerInfo and _map_pid_peerInfo_unknown \n";
				PAUSE
			} else {
				cout << "ERROR:  peerID:" << peerID << " are found in _map_pid_peerInfo_unknown but not in _map_pid_peerInfo \n";
				PAUSE
			}
			
			
			PAUSE
			return 0;
		}
		
		
		cout << "_map_ctrlfd_state_ptr->second->state: " << _map_ctrlfd_state_ptr->second.state << "\n";
		cout << "_map_ctrlfd_state_ptr->second->isPuncher: " << _map_ctrlfd_state_ptr->second.isPuncher << "\n";
		cout << "_map_ctrlfd_state_ptr->second->myIP: " << inet_ntoa(_map_ctrlfd_state_ptr->second.myGlobalAddr.sin_addr);
		cout << ":" << ntohs(_map_ctrlfd_state_ptr->second.myGlobalAddr.sin_port) << "\n";
		cout << "_map_ctrlfd_state_ptr->second->peerIP: " << inet_ntoa(_map_ctrlfd_state_ptr->second.peerAddr.sin_addr);
		cout << ":" << ntohs(_map_ctrlfd_state_ptr->second.peerAddr.sin_port) << "\n";
		cout << "first: " << _map_ctrlfd_pid_ptr->first << ", second: " << _map_ctrlfd_pid_ptr->second << "\n";
		
		
	
		if (_map_ctrlfd_state_ptr->second.state == CTRL_SYNC) {		
			// Unuse
		} else if (_map_ctrlfd_state_ptr->second.state == CTRL_PRCT) {
			// combine in CTRL_SYNC(stateCnt=2)
		} else if (_map_ctrlfd_state_ptr->second.state == CTRL_RECV_PEER) {
			
			// Receive the other peer's OK message
			needRecvByte = 6;	// Expected number of bytes I will receive(address+port=6)
			do {
				if ((n=recv(sCtrl, msg+6-needRecvByte, needRecvByte, 0)) < 0) {
					int nn = WSAGetLastError();
					switch (nn) {
						case WSAEWOULDBLOCK : 	
							continue;
							break;
						case WSAECONNRESET :
							printf("ERROR %d: %s:%04d  STUNT-CTRL unnormally closed socket \n", nn, __FUNCTION__, __LINE__);
							PAUSE
							break;
						default :
							printf("ERROR %d: %s:%04d  Receive peer IP failed \n", nn, __FUNCTION__, __LINE__);
							PAUSE
							break;
					}
				}
				printf("n: %d \n", n);
				needRecvByte -= n;
			} while(needRecvByte != 0);
			
			_map_ctrlfd_state_ptr->second.state = CTRL_CONN_PEER;
			
			
			addrPeer.sin_family = AF_INET;
			memcpy(&addrPeer.sin_addr.s_addr, msg, 4);
			memcpy(&addrPeer.sin_port, msg+4, 2);
			printf("Received peer's IP: %s:%d \n", inet_ntoa(addrPeer.sin_addr), ntohs(addrPeer.sin_port));
			
			// Store peer's IP in ctrlstate
			memcpy(&_map_ctrlfd_state_ptr->second.peerAddr.sin_addr.s_addr, msg, 4);
			memcpy(&_map_ctrlfd_state_ptr->second.peerAddr.sin_port, msg+4, 2);
			
			// Puncher
			if (_map_ctrlfd_state_ptr->second.isPuncher == true) {
				
				int time1 = GetTickCount();
				// punch the hole, default 50 ms for punching hole
				_tcp_punch_ptr->XTryConnect(_sock, sPeerTemp, (struct sockaddr *)&addrPeer, sizeof(addrPeer), 50);
				closesocket(sPeerTemp);
				printf("time: %d ms \n", GetTickCount()-time1);
				
				// bind the socket on my local address and listen for peer
				sPeerTemp = socket(PF_INET, SOCK_STREAM, 0);
				n = setsockopt(sPeerTemp, SOL_SOCKET, SO_REUSEADDR, (char *)&nOne, sizeof(nOne));
				memcpy(&addrLocal, &(_map_ctrlfd_state_ptr->second.myLobalAddr), sizeof(addrLocal));
				printf("My predicted Local IP: %s:%d \n", inet_ntoa(addrLocal.sin_addr), htons(addrLocal.sin_port));
				if (bind(sPeerTemp, (struct sockaddr *)&addrLocal, sizeof(addrLocal)) != 0) {
					printf("ERROR:%d  %s:%04d  Bind failed \n", WSAGetLastError(), __FUNCTION__, __LINE__);
					PAUSE
				}
				if (listen(sPeerTemp, 1) != 0) {
					printf("ERROR:%d  %s:%04d  Listen failed \n", WSAGetLastError(), __FUNCTION__, __LINE__);
					PAUSE
				}
				
				// Send OK message to the other one to inform that I have created listening socket
				if (send(sCtrl, (char *)&nOne, sizeof(nOne), 0) != sizeof(nOne)) {
					printf("ERROR:%d  %s:%04d  Send failed \n", WSAGetLastError(), __FUNCTION__, __LINE__);
					PAUSE
				}
				
				// Map setting
				_net_ptr->set_nonblocking(sPeerTemp);
				_net_ptr->epoll_control(sPeerTemp, EPOLL_CTL_ADD, EPOLLIN);
				_net_ptr->set_fd_bcptr_map(sPeerTemp, dynamic_cast<basic_class *>(_io_accept_nat_ptr));
				fd_list_ptr->push_back(sPeerTemp);
				
				
				// Delete ctrl-socket in map
				_map_ctrlfd_pid.erase(_map_ctrlfd_pid_ptr);
				_map_pid_speer.erase(_map_pid_speer_ptr);
				_map_ctrlfd_state.erase(_map_ctrlfd_state_ptr);
				_net_ptr->epoll_control(sCtrl, EPOLL_CTL_DEL, 0);
				_net_ptr->fd_bcptr_map_delete(sCtrl);
				_net_ptr->eraseFdList(sCtrl);
				closesocket(sCtrl);
				
				// Delete certain fd in map_pid_peerInfo
				_map_pid_peerInfo.erase(_map_pid_peerInfo_itr);
				printf("[NAT]NORMAL: %s:%04d  delete pid %d out of _map_pid_peerInfo \n", __FUNCTION__, __LINE__, peerID);
				
				// Delete certain pid in list
				list<unsigned long>::iterator list_itr;
				list_itr = find(_list_pid.begin(), _list_pid.end(), peerID);
				if (list_itr != _list_pid.end()) {
					_list_pid.erase(list_itr);
					printf("[NAT]NORMAL: %s:%04d  delete pid %d out of _list_pid \n", __FUNCTION__, __LINE__, peerID);
				}
				else {
					printf("ERROR: %s:%04d  delete pid %s in _list_pid \n", __FUNCTION__, __LINE__, peerID);
					_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"ERROR: %s:%04d  delete pid %s in _list_pid", peerID);
					PAUSE
				}
			}
			
		} else if (_map_ctrlfd_state_ptr->second.state == CTRL_CONN_PEER) {
			//// This condition only happen in isPuncher = false
			
		
			// Copy peer's IP into addrPeer
			addrPeer.sin_family = AF_INET;
			memcpy(&addrPeer.sin_addr.s_addr, &_map_ctrlfd_state_ptr->second.peerAddr.sin_addr.s_addr, 4);
			memcpy(&addrPeer.sin_port, &_map_ctrlfd_state_ptr->second.peerAddr.sin_port, 2);
			
			// Receive peer's OK message
			needRecvByte = 4;	// Expected number of bytes I will receive
			do {
				if ((n=recv(sCtrl, msg+4-needRecvByte, needRecvByte, 0)) < 0) {
					if (WSAGetLastError() == WSAEWOULDBLOCK) {
						continue;
					} else {
						printf("ERROR:%d  %s:%04d  Receive failed \n", WSAGetLastError(), __FUNCTION__, __LINE__);
						PAUSE
					}
				}
				printf("n: %d \n", n);
				needRecvByte -= n;
			} while(needRecvByte != 0);
			
			
			//Set to non-blocking I/O
			_net_ptr->set_nonblocking(sPeerTemp);
			printf("sPeerTemp: %d \n", sPeerTemp);
			
			
			if (connect(sPeerTemp, (struct sockaddr *)&addrPeer, sizeof(addrPeer)) < 0) {	
				n = WSAGetLastError();
				if(n == WSAEWOULDBLOCK) {
					printf("WSAGetLastError() = WSAEWOULDBLOCK \n");
					_net_ptr->epoll_control(sPeerTemp, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
					_net_ptr->set_fd_bcptr_map(sPeerTemp, dynamic_cast<basic_class *> (_peer_communication_ptr->_io_connect_ptr));
					_peer_mgr_ptr->fd_list_ptr->push_back(sPeerTemp);	
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"build connection failure : WSAEWOULDBLOCK");
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n", "Build TCP Hole Punching connection, pid",peerID);
				} else {
					printf("ERROR:%d  %s:%04d  Non-blocking connect to peer failed \n", n, __FUNCTION__, __LINE__);
					#ifdef _WIN32
					::closesocket(_sock);
					::WSACleanup();
					
					_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n", "Build TCP Hole Punching connection failure : ",WSAGetLastError());
					_logger_client_ptr->log_exit();
					#else
					::close(_sock);
					#endif
					PAUSE
				}
			} else {
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","Build TCP Hole Punching connection too fast ");
				_logger_client_ptr->log_exit();
				printf("ERROR:  %s:%04d  Non-blocking connect to peer too fast \n", __FUNCTION__, __LINE__);
			}
			
			n = getsockname(sPeerTemp, (struct sockaddr *)&addr, &addrLen);
			printf("aa:%d  sPeerTemp: %d , peerAddr: %s:%d \n", n, sPeerTemp, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			n = getpeername(sPeerTemp, (struct sockaddr *)&addr, &addrLen);
			printf("aa:%d  sPeerTemp: %d , peerAddr: %s:%d \n", n, sPeerTemp, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			
			_log_ptr->write_log_format("s =>u s d s\n", __FUNCTION__,__LINE__,"[NAT CONNECT ++]", n, inet_ntoa(addr.sin_addr));
			
			
			sPeer = sPeerTemp;	
			
			map<int ,  struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
			map_fd_NonBlockIO_iter = _peer_communication_ptr->map_fd_NonBlockIO.find(sPeer);
			if(map_fd_NonBlockIO_iter != _peer_communication_ptr->map_fd_NonBlockIO.end()){
				printf("map_fd_NonBlockIO_iter=_peer_communication_ptr->map_fd_NonBlockIO.find(sPeer) error\n ");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"map_fd_NonBlockIO_iter=_peer_communication_ptr->map_fd_NonBlockIO.find(sPeer) errorn");
				*(_net_ptr->_errorRestartFlag) =RESTART;
				PAUSE
			}
			_peer_communication_ptr->map_fd_NonBlockIO[sPeer]=new struct ioNonBlocking;
			if(!(_peer_communication_ptr->map_fd_NonBlockIO[sPeer] ) ){
				printf("peer_communication::map_fd_NonBlockIO[sPeer]  new error \n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__," _peer_communication_ptr->map_fd_NonBlockIO[sPeer] new error");
				PAUSE
			}
			memset(_peer_communication_ptr->map_fd_NonBlockIO[sPeer], 0, sizeof(struct ioNonBlocking));
			_peer_communication_ptr->map_fd_NonBlockIO[sPeer] ->io_nonblockBuff.nonBlockingRecv.recv_packet_state= READ_HEADER_READY ;
			
		
			
			// This part stores the info in each table
			//_log_ptr->write_log_format("s =>u s d s d s d s d s d s\n", __FUNCTION__,__LINE__,"non blocking connect (before) fd : ",sPeer," manifest : ",manifest," session_id : ",session_id," role : ",fd_role," pid : ",fd_pid," non_blocking_build_connection (candidate peer)\n");
			map<int, struct fd_information *>::iterator map_fd_info_iter;
			map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sPeer);
			if (map_fd_info_iter != _peer_communication_ptr->map_fd_info.end()) {
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : fd %d already in map_fd_info in non_blocking_build_connection\n",sPeer);
				_logger_client_ptr->log_exit();
			}
			_peer_communication_ptr->map_fd_info[sPeer] = new struct fd_information;
			if(!(_peer_communication_ptr->map_fd_info[sPeer]  ) ){
				printf("peer_communication::map_fd_info[sPeer]   new error \n");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"map_fd_info[sPeer]  new error");
				PAUSE
			}
			map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sPeer);
			if(map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()){
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : %d cannot new non_blocking_build_connection\n",sPeer);
				_logger_client_ptr->log_exit();
			}

			memset(map_fd_info_iter->second,0x00,sizeof(struct fd_information));
			map_fd_info_iter->second->flag = 1;					// 0:I am child, 1:I am parent
			map_fd_info_iter->second->manifest = _map_pid_peerInfo_itr->second.manifest;
			map_fd_info_iter->second->pid = peerID;				// The other peer's PID
			map_fd_info_iter->second->session_id = _map_pid_peerInfo_itr->second.session_id;
			
			//_peer_communication_ptr->session_id_count++;
			
			_log_ptr->write_log_format("s =>u s d s d s d s d s d s\n", __FUNCTION__,__LINE__,"non blocking connect fd : ",map_fd_info_iter->first," manifest : ",map_fd_info_iter->second->manifest," session_id : ",map_fd_info_iter->second->session_id," role : ",map_fd_info_iter->second->flag," pid : ",map_fd_info_iter->second->pid," non_blocking_build_connection (candidate peer)\n");
	
			
			
			// Delete ctrl-socket in map
			_map_ctrlfd_pid.erase(_map_ctrlfd_pid_ptr);
			_map_pid_speer.erase(_map_pid_speer_ptr);
			_map_ctrlfd_state.erase(_map_ctrlfd_state_ptr);
			_net_ptr->epoll_control(sCtrl, EPOLL_CTL_DEL, 0);
			_net_ptr->fd_bcptr_map_delete(sCtrl);
			_net_ptr->eraseFdList(sCtrl);
			closesocket(sCtrl);
			
			// Delete certain fd in map_pid_peerInfo
			_map_pid_peerInfo.erase(_map_pid_peerInfo_itr);
			printf("[NAT]NORMAL: %s:%04d  delete pid %d out of _map_pid_peerInfo \n", __FUNCTION__, __LINE__, peerID);
			
			// Delete certain pid in list
			list<unsigned long>::iterator list_itr;
			list_itr = find(_list_pid.begin(), _list_pid.end(), peerID);
			if (list_itr != _list_pid.end()) {
				_list_pid.erase(list_itr);
				printf("[NAT]NORMAL: %s:%04d  delete pid %d out of _list_pid \n", __FUNCTION__, __LINE__, peerID);
			}
			else {
				printf("ERROR: %s:%04d  delete pid %s in _list_pid \n", __FUNCTION__, __LINE__, peerID);
				_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"ERROR: %s:%04d  delete pid %s in _list_pid", peerID);
				PAUSE
			}
		}
	}
	
	//_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : place in io_connect::handle_pkt_in\n");
	//_logger_client_ptr->log_exit();
	timeEnd = GetTickCount();
	printf("_list_pid.size: %d \n", _list_pid.size());
	printf("consuming time: %d ms \n", timeEnd-timeStart);
	cout << "---------------stunt_mgr::handle_pkt_in end \n";

	return RET_OK;
#endif
}

int stunt_mgr::handle_pkt_out(int sock)
{
#ifdef _WIN32	
	return RET_OK;
#endif
}

void stunt_mgr::handle_pkt_error(int sock)
{
#ifdef _WIN32	
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_connect handle_pkt_error error number : ",WSAGetLastError());
	_logger_client_ptr->log_exit();
#endif
}

void stunt_mgr::handle_job_realtime()
{

}


void stunt_mgr::handle_job_timer()
{

}

void stunt_mgr::handle_sock_error(int sock, basic_class *bcptr){
#ifdef _WIN32		
	_peer_communication_ptr->fd_close(sock);
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_connect handle_sock_error error number : ",WSAGetLastError());
	_logger_client_ptr->log_exit();
#endif
}


void stunt_mgr::data_close(int cfd, const char *reason) 
{
#ifdef _WIN32	
//	list<int>::iterator fd_iter;
//
//	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "pk", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".." << endl;
//	_net_ptr->close(cfd);
//
//	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
//		if(*fd_iter == cfd) {
//			fd_list_ptr->erase(fd_iter);
//			break;
//		}
//	}
////	PAUSE
	_peer_communication_ptr->fd_close(cfd);
#endif
}


int stunt_mgr::tcpPunch_connection(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long peerID, int flag, unsigned long session_id)
{
#ifdef _WIN32	
	printf("------------- tcpPunch_connection \n");
	printf("manifest: %u \n", manifest);
	// using for record handle_pkt_out time
	int timeStart, timeEnd;
	timeStart = GetTickCount();
	
	int sComm = -1;
	int nCommRet = 0;
	char peerIDChar[MAX_ID_LEN]={0};
	struct ctrlstate ctrlState;
	struct sockaddr_in AddrComm;
	Msg MsgCon;
	
	list<unsigned long>::iterator list_itr;
	multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_connect_down_t *>::iterator pid_peerDown_info_iter;

	// set initial value
	memset(&ctrlState, 0, sizeof(ctrlState));
	itoa(peerID, peerIDChar, 10);
	
	// Check whether this peer-ID has in pid-list, if not found then store peer-ID in the pid list
	// Must guarantee that cannot create 2 connections with a peer
	list_itr = find(_list_pid.begin(), _list_pid.end(), peerID);
	printf("_list_pid.size: %d \n", _list_pid.size());
	if (list_itr != _list_pid.end()) {
		printf("NORMAL: %s:%04d  fd already in _list_pid \n", __FUNCTION__, __LINE__);
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"NORMAL: %s:%04d  fd already in _list_pid");
		//PAUSE
		
		map<unsigned long, struct peer_info_t_nat>::iterator _map_pid_peerInfo_itr;
		_map_pid_peerInfo_itr = _map_pid_peerInfo.find(peerID);
		if (_map_pid_peerInfo_itr == _map_pid_peerInfo.end()) {
			printf("NORMAL: %s:%04d  _map_pid_peerInfo is not consistent with _list_pid \n", __FUNCTION__, __LINE__);
			PAUSE
		}
		
		return 1;
	}
	else {
		_list_pid.push_back(peerID);
		
		// store arguments in the map. This is for puncher side
		struct peer_info_t_nat peer_info_nat;
		memcpy(&peer_info_nat, level_info_ptr, sizeof(level_info_t));	// sizeof(peer_info_nat) is bigger than sizeof(level_info_t)
		peer_info_nat.manifest = manifest;
		peer_info_nat.session_id = session_id;
		peer_info_nat.isPuncher = true;
		//memcpy(&_map_pid_peerInfo[peerID], &peer_info_nat, sizeof(peer_info_nat));
		_map_pid_peerInfo[peerID] = peer_info_nat;
		
		// 之前已經建立過連線的 在map_in_pid_fd裡面 則不再建立(保證對同個parent不再建立第二條線)
		// map_in_pid_fd: parent-peer which alreay established connection, including temp parent-peer
		for(map_pid_fd_iter = _peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == level_info_ptr->pid ){
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_in_pid_fd in non_blocking_build_connection (rescue peer)");
				return 1;
			}
		}

		// map_pid_peer_info: temp parent-peer
		pid_peer_info_iter = _pk_mgr_ptr ->map_pid_peer_info.find(level_info_ptr ->pid);
		if(pid_peer_info_iter !=  _pk_mgr_ptr ->map_pid_peer_info.end() ){
			//兩個以上就沿用第一個的連線
			if(_pk_mgr_ptr ->map_pid_peer_info.count(level_info_ptr ->pid) >= 2 ){
				printf("pid =%d already in connect find in map_pid_peer_info  testing",level_info_ptr ->pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peer_info testing");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_peer_info in non_blocking_build_connection (rescue peer)");	
				return 1;
			}
		}

		// 若在map_pid_peerDown_info 則不再次建立連線
		// map_pid_peerDown_info: real parent-peer
		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_peerDown_info.find(level_info_ptr ->pid);
		if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_peerDown_info.end()){
			printf("pid =%d already in connect find in map_pid_peerDown_info",level_info_ptr ->pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",level_info_ptr ->pid,"already in connect find in map_pid_peerDown_info");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_peerdown_info in non_blocking_build_connection (rescue peer)");
			return 1;
		}
		
	}
	
	
	
	//Generate the message which will send to STUNT-COMM
	MsgCon.nType = htonl(MSG_CONNECT);
	strcpy(MsgCon.Data.Connection.chDstID, peerIDChar);
	strcpy(MsgCon.Data.Connection.chSrcID, _pidChar);
	
	
	//// First, connect to STUNT-COMM and inform which peer I will connect	////
	_tcp_punch_ptr->XInitSockAddr(&AddrComm, AF_INET, STUNT_SERVER_IP1, SERVER_COMM_PORT, 0, 0);
	sComm = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(sComm, (struct sockaddr *)&AddrComm, sizeof(AddrComm)) < 0) {
		printf("ERROR:%d  %s:%04d  Build connection to STUNT-COMM failed  \n", WSAGetLastError(), __FUNCTION__, __LINE__);
	}
	if (send(sComm, (char*)&MsgCon, sizeof(MsgCon), 0) != sizeof(MsgCon)) {
		printf("ERROR:%d  %s:%04d  Send message to STUNT-COMM failed  \n", WSAGetLastError(), __FUNCTION__, __LINE__);
	}
	
	
	// Receive ACK from STUNT-COMM
	if (recv(sComm, (char*)&nCommRet, sizeof(nCommRet), 0) != sizeof(nCommRet)) {
		printf("ERROR:%d  %s:%04d  Receive ACK from STUNT-COMM failed  \n", WSAGetLastError(), __FUNCTION__, __LINE__);
	}
	
	// if nCommRet!=0 means error occurred, -1
	if (nCommRet < 0) {
		switch (nCommRet) {
			case -1:	printf("ERROR:%d  %s:%04  ID not found 			\n", nCommRet, __FUNCTION__, __LINE__); break;
			case -2:	printf("ERROR:%d  %s:%04  The same ID			\n", nCommRet, __FUNCTION__, __LINE__); break;
			case -3:	printf("ERROR:%d  %s:%04  Behind the same NAT  	\n", nCommRet, __FUNCTION__, __LINE__); break;
			default:	printf("ERROR:%d  %s:%04  Unknown error		  	\n", nCommRet, __FUNCTION__, __LINE__); break;
		}
		PAUSE
		closesocket(sComm);
		return -1;	
	}
	closesocket(sComm);
	
	
	
	 
	timeEnd = GetTickCount();
	printf("Communicate to STUNT-COMM time: %d ms \n", timeEnd-timeStart);
	
	return 0;
#endif
}

void stunt_mgr::accept_check_nat(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id)
{
#ifdef _WIN32	
	printf("====accept_check_nat==== \n");
	list<unsigned long>::iterator list_itr;
	map<unsigned long, struct peer_info_t_nat>::iterator _map_pid_peerInfo_itr;
	
	
	// Check whether this peer-ID has in pid-list, if not found then store peer-ID in the pid list
	// Must guarantee that cannot create 2 duplicate connections to the same peer
	list_itr = find(_list_pid.begin(), _list_pid.end(), fd_pid);
	printf("_list_pid.size: %d \n", _list_pid.size());
	printf("[NAT]NORMAL: %s:%04d  _list_pid.size: %d \n", __FUNCTION__, __LINE__, _list_pid.size());
	_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: %s:%04d  _list_pid.size:", _list_pid.size());
	
	// If found, we do not connect to that peer again to avoid duplicate connections
	if (list_itr != _list_pid.end()) {
		printf("[NAT]NORMAL: %s:%04d  pid %d already in _list_pid \n", __FUNCTION__, __LINE__, fd_pid);
		_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: %s:%04d  fd already in _list_pid", fd_pid);
		//PAUSE
		
		map<unsigned long, struct peer_info_t_nat>::iterator _map_pid_peerInfo_itr;
		_map_pid_peerInfo_itr = _map_pid_peerInfo.find(fd_pid);
		if ((_map_pid_peerInfo_itr == _map_pid_peerInfo.end()) && (_map_pid_peerInfo_itr == _map_pid_peerInfo_unknown.end())) {
			printf("NORMAL: %s:%04d  _map_pid_peerInfo|_map_pid_peerInfo_unknown is not consistent with _list_pid \n", __FUNCTION__, __LINE__);
			PAUSE
		}
	}
	else {
		_list_pid.push_back(fd_pid);
		_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: %s:%04d  push pid %d into _list_pid", fd_pid);
		
		
		_map_pid_peerInfo_itr = _map_pid_peerInfo_unknown.find(fd_pid);
		
		// If find this pid in _map_pid_peerInfo_unknown, means STUNT-LOG is faster than PK.
		// Then copy the info from _map_pid_peerInfo_unknown to _map_pid_peerInfo.
		// Else, the expected condition is that PK is faster than STUNT-LOG
		if (_map_pid_peerInfo_itr != _map_pid_peerInfo_unknown.end()) {
		
			_map_pid_peerInfo[fd_pid].ctrl_ip = _map_pid_peerInfo_unknown[fd_pid].ctrl_ip;		// STUNT-CTRL IP
			_map_pid_peerInfo[fd_pid].ctrl_port = _map_pid_peerInfo_unknown[fd_pid].ctrl_port;	// STUNT-CTRL port
			_map_pid_peerInfo[fd_pid].manifest = manifest;
			_map_pid_peerInfo[fd_pid].session_id = session_id;
			_map_pid_peerInfo[fd_pid].isPuncher = false;
			_map_pid_peerInfo[fd_pid].pid = fd_pid;
			_map_pid_peerInfo_unknown.erase(_map_pid_peerInfo_itr);
			_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: erase pid %d out of _map_pid_peerInfo_unknown", fd_pid);
			_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: _map_pid_peerInfo_unknown.size: %d", _map_pid_peerInfo_unknown.size());
			cout << "[NAT]NORMAL: erase pid " << fd_pid << " out of _map_pid_peerInfo_unknown \n";
			cout << "[NAT]NORMAL: _map_pid_peerInfo_unknown.size: " << _map_pid_peerInfo_unknown.size() << "\n";
		
			ConnectToCTRL(_map_pid_peerInfo[fd_pid]);
			
		}
		else {
			struct peer_info_t_nat peer_info_nat;
			memcpy(&peer_info_nat, level_info_ptr, sizeof(level_info_t));	// sizeof(peer_info_nat) is bigger than sizeof(level_info_t)
			_map_pid_peerInfo[fd_pid].manifest = manifest;
			_map_pid_peerInfo[fd_pid].session_id = session_id;
			_map_pid_peerInfo[fd_pid].isPuncher = false;
			_map_pid_peerInfo[fd_pid].pid = fd_pid;
			
			_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: push pid %d into _map_pid_peerInfo", fd_pid);
		}
	}
	
	return;
#endif	
}

// Initialize the socket to STUNT-CTRL, and connect to it
void stunt_mgr::ConnectToCTRL(struct peer_info_t_nat peer_info_nat)
{
#ifdef _WIN32	
	printf("=========stunt_mgr::ConnectToCTRL========= \n");
	// using for record handle_pkt_in time
	int timeStart, timeEnd;
	timeStart = GetTickCount();
	
	struct sockaddr_in 	addrCtrl,
						addrLocal,
						addrGlobal;
	struct timeval		Timeout;
	struct ctrlstate ctrlState;
	
	SOCKET	sPeerTemp,
			sCtrl;
	
	//int needRecvByte,	// Expected number of bytes I will receive
	//	nOne = 1;
	
	
	
	// Temp variables
	struct sockaddr_in addr;
	int addrLen=sizeof(struct sockaddr_in);
	int n;
	unsigned long ulOne = 0;
	char msg[1024] = {0};
	
	memset(&ctrlState, 0, sizeof(ctrlState));
	
	if (peer_info_nat.isPuncher) {
		printf("I am puncher \n");
		cout << peer_info_nat.isPuncher << "\n";
		ctrlState.isPuncher = true;
	}
	else {
		printf("I am not puncher \n");
		cout << peer_info_nat.isPuncher << "\n";
		ctrlState.isPuncher = false;
	}
	
	
	// Establish connection to STUNT-CTRL
	_tcp_punch_ptr->XInitSockAddr(&addrCtrl, AF_INET, NULL, 0, peer_info_nat.ctrl_ip, peer_info_nat.ctrl_port);
	sCtrl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	_net_ptr->set_nonblocking(sCtrl);
	
	if (connect(sCtrl, (struct sockaddr *)&addrCtrl, sizeof(addrCtrl)) < 0) {
		if(WSAGetLastError() == WSAEWOULDBLOCK) {
			_net_ptr->set_nonblocking(sCtrl);
			_net_ptr->epoll_control(sCtrl, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_ptr->set_fd_bcptr_map(sCtrl, dynamic_cast<basic_class *> (this));
			_peer_mgr_ptr->fd_list_ptr->push_back(sCtrl);	
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"connect to STUNT-CTRL failure : WSAEWOULDBLOCK");
		} else {
			#ifdef _WIN32
			::closesocket(sCtrl);
			::WSACleanup();
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "connect to STUNT-CTRL failure : ", WSAGetLastError());
			_logger_client_ptr->log_exit();
			#else
			::close(sCtrl);
			#endif
			printf("ERROR:  %s:%04  Connect to SUNTU-CTRL failed \n", __FUNCTION__, __LINE__);
			PAUSE
		}
	}
	
	n = getsockname(sCtrl, (struct sockaddr *)&addr, &addrLen);
	_log_ptr->write_log_format("s =>u s u\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: %s:%04d  connect to STUNT-CTRL port %d", htons(peer_info_nat.ctrl_port));
	_log_ptr->write_log_format("s =>u s s u\n", __FUNCTION__,__LINE__,"[NAT]NORMAL: %s:%04d  my address %s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	n = getsockname(sCtrl, (struct sockaddr *)&addr, &addrLen);
	printf("n:%2d  sCtrl: %2d , SrcAddr: %s:%d \n", n, sCtrl, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	n = getpeername(sCtrl, (struct sockaddr *)&addr, &addrLen);
	printf("n:%2d  sCtrl: %2d , DstAddr: %s:%d \n", n, sCtrl, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			

	int startPred, endPred;
	startPred = GetTickCount();
	// sPeerTemp is binded on addrLocal. For puncher, it represents listening socket; for the other one, it represents actual peer socket
	if (_tcp_punch_ptr->XPredict(_sock, &sPeerTemp, &addrGlobal, &addrLocal) != ERR_NONE) {
		printf("ERROR:  %s:%04  XPredict failed \n", __FUNCTION__, __LINE__);
		PAUSE
	}
	endPred = GetTickCount();
	printf("STUNT-SERVER XPredict time: %d ms \n", endPred-startPred);
	_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"[NAT]STUNT-SERVER XPredict time: %d ms", endPred-startPred);
	
	
	printf("AddrGlobal address: %s:%d \n", inet_ntoa(addrGlobal.sin_addr), ntohs(addrGlobal.sin_port));
	printf("AddrClient address: %s:%d \n", inet_ntoa(addrLocal.sin_addr), ntohs(addrLocal.sin_port));
	n = getsockname(sPeerTemp, (struct sockaddr *)&addr, &addrLen);
	printf("n:%2d  sPeerTemp: %2d , SrcAddr: %s:%d \n", n, sPeerTemp, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	n = getpeername(sPeerTemp, (struct sockaddr *)&addr, &addrLen);
	printf("n:%2d  sPeerTemp: %2d , DstAddr: %s:%d \n", n, sPeerTemp, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	
	// Send my predicted Global IP and Local IP to STUNT-CTRL
	memcpy(msg, &addrGlobal.sin_addr.s_addr, sizeof(addrGlobal.sin_addr.s_addr));
	memcpy(msg+4, &addrGlobal.sin_port, sizeof(addrGlobal.sin_port));
	send(sCtrl, msg, 6, 0);
	
	// set ctrl state 
	ctrlState.state = CTRL_RECV_PEER;
	memcpy(&ctrlState.myGlobalAddr, &addrGlobal, sizeof(addrGlobal));
	memcpy(&ctrlState.myLobalAddr,  &addrLocal,  sizeof(addrLocal));
	
	// Map setting
	_net_ptr->set_nonblocking(sCtrl);
	_net_ptr->epoll_control(sCtrl, EPOLL_CTL_ADD, EPOLLIN);
	_net_ptr->set_fd_bcptr_map(sCtrl, dynamic_cast<basic_class *> (this));
	fd_list_ptr->push_back(sCtrl);
	//_net_ptr->set_blocking(sCtrl);
	_map_ctrlfd_pid[sCtrl] = peer_info_nat.pid;
	_map_pid_speer[peer_info_nat.pid] = sPeerTemp;
	_map_ctrlfd_state[sCtrl] = ctrlState;
#endif
}