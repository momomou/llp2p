#ifndef __STUNTDEF_H__
#define __STUNTDEF_H__


#ifndef _WIN32
#define SOCKET						INT32
#endif

#define BUILD						"BUILD $VER:10$"
#define BUILD_VER					10						//This build version must match the server's or will cause version mismatch error during initialization.

//#define TEST_TIME
#define TEST_DEBUG					1
#define true						1
#define false						0

#define STUNT_SERVER_IP1			"140.114.90.154"
#define STUNT_SERVER_IP2			"127.0.0.1"
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
#define CTRL_TIMEOUT					5					//receive controll message

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

typedef enum
{
	CTRL_SYNC = 0,				// Now can receive Sync message 
	CTRL_PRCT,					// Now can send my predicted IP
	CTRL_RECV_PEER,				// Now can receive peer's IP
	CTRL_CONN_PEER,				// Now can wait peer's OK message, then connect to it
	CTRL_DONE					// Now can close ctrl-socket
} Ctrl_State;

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
struct __ServerInfo
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
};

typedef struct _Echo 
{
	INT32 nIP;
    INT16 wPort;
	INT16 wPadding;
    union 
	{
        CHAR chData[24];
        struct //5
		{
            INT32 nSeq;
            CHAR chResponse;
        } Tcp;
        struct //41
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

struct __Fingerprint {
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

};

struct ctrlstate
{
	int state;								// 0:can sync, 1:can send predicted IP, 2: can receive peer's Info
	int stateCnt;
	int isPuncher;							// If I am puncher, set true; otherwise, false
	struct sockaddr_in myLobalAddr;			// Store my predicted local IP
	struct sockaddr_in myGlobalAddr;		// Store my predicted global IP
	struct sockaddr_in peerAddr;			// Store peer's hole punching IP
};

/************************************************************************************************************************/
/*			Variable Reference																					*/
/************************************************************************************************************************/
/*
extern FILE *g_pDbgFile;

extern CHAR g_chClientID[MAX_ID_LEN + 1];
extern INT32 g_nClientIP;	
extern INT32 g_nServerVersion;
extern CHAR g_szServerIP1[MAX_IP_STR_LEN + 1];
extern CHAR g_szServerIP2[MAX_IP_STR_LEN + 1];

extern Fingerprint g_Fingerprint;
extern ServerInfo g_ServerInfo;			//A buffer to exchange server and client information.
*/

#endif