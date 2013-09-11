#ifndef __STUNT_MGR_H__
#define __STUNT_MGR_H__


#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network;
class logger;
class peer_mgr;
class peer;
class pk_mgr;
class peer_communication;
class logger_client;
class tcp_punch;
class io_nonblocking;
class io_accept_nat;

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

class stunt_mgr:public basic_class {
public:

	stunt_mgr(list<int> *fd_list_ptr);
	~stunt_mgr();

	int init(unsigned long myPID);

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();
	void data_close(int cfd, const char *reason);
	int tcpPunch_connection(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, int flag, unsigned long session_id);
	void accept_check_nat(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id);
	void ConnectToCTRL(struct peer_info_t_nat);

	SOCKET _sock;	// STUNT-SERVER fd
	unsigned long _pid;	// my PID (type:unsigned long)
	char _pidChar[10];	// my PID (type:char)
	int ctrl_timeout;	// timeout for receive message from STUNT-SERVER of ctrl socket
	int comm_timeout;	// timeout for receive message from STUNT-SERVER of comm socket
	//SOCKET _sPeer;
	
	//FILE *g_pDbgFile_v2;
	Fingerprint g_Fingerprint;
	ServerInfo g_ServerInfo;			//A buffer to exchange server and client information.
	list<int> *fd_list_ptr;
	list<unsigned long> _list_pid;		// store pid which I will do hole punching, also check this list for avoiding duplicate connection
	map<SOCKET, unsigned long> _map_ctrlfd_pid;		// ctrl socket mapping pid
	map<unsigned long, SOCKET> _map_pid_speer;		// ctrl socket mapping pid
	map<unsigned long, struct peer_info_t_nat> _map_pid_peerInfo;		// Store another peer's info
	map<unsigned long, struct peer_info_t_nat> _map_pid_peerInfo_unknown;	// Handle the condition that if STUNT-LOG'message arrives earlier than PK's 
	map<SOCKET, struct ctrlstate> _map_ctrlfd_state;	// ctrl socket mapping ctrl-state


	logger_client * _logger_client_ptr;
	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep_ptr;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	pk_mgr * _pk_mgr_ptr;
	peer_communication *_peer_communication_ptr;
	tcp_punch *_tcp_punch_ptr;
	io_accept_nat *_io_accept_nat_ptr;
};

#endif
























