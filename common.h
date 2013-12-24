//#ifndef _FIRE_BREATH_MOD_
//#define _FIRE_BREATH_MOD_ 
//#endif

#ifndef __COMMON_H__
#define __COMMON_H__


#ifndef DEBUG
#define DEBUG
#endif

/*
#ifndef DEBUG2
#define DEBUG2
#endif
*/
/*
#ifndef WRITE_LOG
#define WRITE_LOG		// write log.txt
#endif
*/
// Role of function caller
#define RESCUE_PEER		0		// the function caller is child
#define CANDIDATE_PEER	1		// the function caller is parent



#define FD_SETSIZE		2048

#define PK_PID			999999
#define STUNT_PID		999998
#define BIG_CHUNK	8192

////resuce PARAMETER////
#define PARAMETER_X		4				// chunk packets per (1000/PARAMETER_X) ms
#define MAX_DELAY 		20000			// source delay PARAMETER ms
#define SOURCE_DELAY_CONTINUOUS 0.5		// The maximal permissive times that souce-delay is bigger than MAX_DELAY. SOURCE_DELAY_CONTINUOUS * x(packets/s) / substream
// M 次測量發生N次 or 連續P次發生 則判斷需要Rescue(頻寬檢查)
#define PARAMETER_M		8
#define PARAMETER_N		5
#define PARAMETER_P		3

// LOG SERVER
#define LOGPORT		8754
#define LOGIP		"140.114.90.154"
//ms
#define CONNECT_TIME_OUT	4000
#define NETWORK_TIMEOUT		5000		// Period of check peer's unnormal disconnection
#define BASE_RESYN_TIME		20000



//  必須小於bucket_size  (從接收 - > 送到player中間的buff ) 
// BUFF_SIZE sec
#define BUFF_SIZE		2
//CHUNK_LOSE sec, mean lose about CHUNK_LOSE sec packet
#define CHUNK_LOSE		2	//



// Time interval of calculation Xcount
#define XCOUNT_INTERVAL		10000
//io_accept fd remain period
#define FD_REMAIN_PERIOD	1000

//#define MODE MODE_HTTP // mode_BitStream
#define MODE MODE_HTTP

//nat part
#define MAX_ID_LEN	31					//Max Client ID Length
#define DEF_TIMEOUT 15
#define TEST_TIME
#define LISTEN_TIMEOUT	1000	//MUST less than  CONNECT_TIME_OUT

//log part
#define BUFFER_CONTENT_THRESHOLD 2000
#define CHUNK_BUFFER_SIZE 10000
#define TIME_BW	500
#define LOG_BW	1000
#define LOG_TIMES 2
#define TIME_PERIOD 500
//log state part
/*
1. LOG_REGISTER: send register info to get register back
2. LOG_REG_LIST: from recive reg list to timeout(start testing) 
3. LOG_REG_LIST_TESTING: from timeout(start testing) to setmanifest
4. LOG_REG_LIST_DETECTION_TESTING_SUCCESS: from setmanifest to cut pk stream.
5. LOG_REG_LIST_TESTING_FAIL: from timeout(start testing) to send topology to pk.
6. LOG_REG_CUT_PK: send pk cut.
7. LOG_REG_DATA_COME: data come.
*/
#define LOG_REGISTER	0x01
#define LOG_REG_LIST	0x02
#define LOG_REG_LIST_TESTING	0x03
#define LOG_REG_LIST_DETECTION_TESTING_SUCCESS	0x04
#define LOG_REG_LIST_TESTING_FAIL	0x05
#define LOG_REG_CUT_PK	0x06
#define LOG_REG_DATA_COME	0x07

/*
1. LOG_RESCUE_TRIGGER: from sending rescue request to get rescue list
2. LOG_RESCUE_LIST: from reciving rescue list to timeout(start testing)
3. LOG_RESCUE_TESTING: from start testing to setmanifest
4. LOG_RESCUE_DETECTION_TESTING_SUCCESS: from setmanifest to cut pk stream.
5. LOG_RESCUE_LIST_TESTING_FAIL: from timeout(start testing) to send topology to pk.
6. LOG_RESCUE_CUT_PK: send pk cut.
7. LOG_RESCUE_DATA_COME: data come.
*/
#define LOG_RESCUE_TRIGGER	0x08	
#define LOG_RESCUE_LIST	0x09
#define LOG_RESCUE_TESTING	0x0a
#define LOG_RESCUE_DETECTION_TESTING_SUCCESS	0x0b
#define LOG_RESCUE_LIST_TESTING_FAIL	0x0c
#define LOG_RESCUE_CUT_PK	0x0d
#define LOG_RESCUE_DATA_COME	0x0e

/*
the peer's condition
*/
#define LOG_START_DELAY	0x0f
#define LOG_PERIOD_SOURCE_DELAY	0x10
#define LOG_RESCUE_SUB_STREAM	0x11
#define LOG_PEER_LEAVE	0x12	
#define LOG_WRITE_STRING	0x13
#define LOG_BEGINE 0x14

/*
UPDTAE
*/
#define LOG_RESCUE_TRIGGER_BACK 0x15
#define LOG_LIST_EMPTY 0x16
#define LOG_TEST_DELAY_FAIL 0x17		// All connections building of list-peers fail  
#define LOG_TEST_DETECTION_FAIL 0x18	// Test delay fail
#define LOG_DATA_COME_PK 0x19
#define LOG_DELAY_RESCUE_TRIGGER 0x1a
#define LOG_CLIENT_BW 0x1b
#define LOG_MERGE_TRIGGER 0x1c
#define LOG_TIME_OUT 0x1d
#define LOG_PKT_LOSE 0x1e

/*
log parameter
*/
#define LOG_DELAY_SEND_PERIOD 5000
#define LOG_BW_SEND_PERIOD 6000

#include "configuration.h"

#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <iostream>
#include <errno.h>
#include <assert.h>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <bitset>
#include <stdlib.h>
#include <signal.h>
//#include <getopt.h> 
#include <fstream>


#ifdef _WIN32
//#include <Dbghelp.h>
#include <winsock2.h>
#include <sys/timeb.h> 
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>			// for getipv4 mask
#include <Iphlpapi.h>
#include <conio.h>
#include <process.h>
#include <Mmsystem.h>
#include <time.h>
#include <TCHAR.h>
#pragma comment(lib,"ws2_32.lib")				// for visual c++ compatible
#pragma comment(lib,"iphlpapi.lib")				// for visual c++ compatible
#pragma comment (lib, "wsock32.lib")
#pragma comment (lib, "Iphlpapi.lib")
#pragma comment (lib, "Winmm.lib")
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/cdefs.h>
#include <sys/wait.h>
#include <sys/timeb.h> //--!!0208
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <net/if.h>
#include <pthread.h>
#endif

#include <map>
#include <queue>
#include <set>
#include <list>
#include <string>
#include <cstring>

#include "basic_class.h"

#ifndef __GNUG__	// I'm not g++
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#ifdef _WIN32
typedef int socklen_t; 		 // windows 沒有 socklen_t 類型 
#endif


using namespace std;
using std::bitset;

#pragma pack(1)

// Defined Macro
#define PAUSE for (;;) { cout << "PAUSE , Press any key to continue..." << __FUNCTION__ << ":" << __LINE__; fgetc(stdin); }
#ifdef DEBUG
    #define debug_printf(str, ...) do { printf("(%d)\t", __LINE__); printf(str, __VA_ARGS__); } while (0)
#else
    #define debug_printf(str, ...)
#endif
#ifdef DEBUG2
    #define debug_printf2(str, ...) do { printf("(%d)\t", __LINE__); printf(str, __VA_ARGS__); } while (0)
#else
    #define debug_printf2(str, ...)
#endif 

/****************************************************/
/*		Type Definition								*/
/****************************************************/
#ifdef _WIN32
#else
	typedef int8_t      INT8;
	typedef int16_t     INT16;
	typedef int32_t     INT32;
	typedef int64_t     INT64;
	typedef uint8_t     UINT8;
	typedef uint16_t    UINT16;
	typedef uint32_t    UINT32;
	typedef uint64_t    UINT64;
#endif


#define LOGFILE			"log.txt"
#define LOGBINARYFILE	"logbinary.txt"

#define SIG_FREQ		30000
#define SYS_FREQ		1

//#define RTMP_PKT_BUF_MAX	1536	// This value defines the max rtmp packet size
#define RTMP_PKT_BUF_MAX	30000	// This value defines the max rtmp packet size
#define RTMP_PKT_BUF_PAY_SIZE	(RTMP_PKT_BUF_MAX - sizeof(struct chunk_header_t))	// This value defines the max rtp packet size

#define CHNK_CMD_PEER_REG				0x01	// register
//#define CHNK_CMD_RESCUE_LIST			0x02	// recv rescue list
//#define CHNK_CMD_PEER_RSC				0x03	// rescue cmd
//#define CHNK_CMD_PEER_CUT				0x04	// cut cmd
//#define CHNK_CMD_PEER_BWN				0x05	// bandwidth notification cmd
#define CHNK_CMD_PEER_CON				0x06	// connect cmd (connect to peer)
#define CHNK_CMD_PEER_DATA				0x07	// Data cmd (this cmd encapsulate data into transport layer)
#define CHNK_CMD_PEER_REQ_FROM			0x08	// req from (the peer will send this command that which seq itslf want to request from)
//#define CHNK_CMD_PEER_RSC_LIST			0x09	// rescue to pk's cmd
//#define CHNK_CMD_PEER_SWAP				0x0a	// swap cmd
//#define CHNK_CMD_CHN_OPEN				0x0b		// open channel
//#define CHNK_CMD_CHN_STOP				0x0c		// stop channel
//#define CHNK_CMD_CHN_INFO				0x0d		// query channel information
//#define CHNK_CMD_RT_NLM					0x0e    // network latency measurement    //--!!0121
//#define CHNK_CMD_PEER_LAT				0x0f	// latency cmd, can only used by NS2, implementation can only use this cmd until iplement NTP to sync peer's time
//#define CHNK_CMD_PEER_DEP				0x10	// departure
//#define CHNK_CMD_PEER_NOTIFY        	0x11
//#define CHNK_CMD_PEER_LATENCY           0x12
#define CHNK_CMD_CHN_UPDATE_DATA        0x13	// update steam id to peer
#define CHNK_CMD_PEER_RESCUE      		0x14	//rescue from pk
#define CHNK_CMD_PEER_RESCUE_UPDATE      	0x15
#define CHNK_CMD_PEER_RESCUE_CAPACITY      	0x16
#define CHNK_CMD_PEER_RESCUE_LIST      	0x17
#define CHNK_CMD_PEER_TEST_DELAY		0x18	//test delay to select peer
#define CHNK_CMD_PEER_SET_MANIFEST		0x19	//set manifest set to parent
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
#define CHNK_CMD_PEER_SYN				0X1A
//////////////////////////////////////////////////////////////////////////////////
#define CHNK_CMD_PEER_SEED				0X1B
#define CHNK_CMD_PEER_MEASURE_DATA		0X1C
#define CHNK_CMD_PARENT_PEER			0X1D
//#define CHNK_CMD_PEER_START_DELAY_UPDATE			0X1C
//#define CHNK_CMD_PEER_PARENT_CHILDREN	0xF0	//暫時不用
#define CHNK_CMD_TOPO_INFO				0x1E
#define CHNK_CMD_ROLE					0x1F
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
#define CHNK_CMD_LOG	0x20
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

#define CHNK_CMD_PEER_UNKNOWN			0xFF	// 1 B cmd => 0xFF is reserved for unknown cmd


#define OK				0x01
#define REJECT			0x02


#define RESTART 0x01

#define RTP_PKT_BUF_MAX	30000	// This value defines the max rtp packet size
#define RTP_PKT_BUF_PAY_SIZE	(RTP_PKT_BUF_MAX - sizeof(struct chunk_header_t) - sizeof(struct rtp_hdr_t))	// This value defines the max rtp packet size
#define MAXFDS 			2048
#define EVENTSIZE 		2048
#define MAX_POLL_EVENT 	64
#define HTML_SIZE 		8192
//#define BUCKET_SIZE		2048/4
//#define BANDWIDTH_BUCKET	7
//#define MAX_PEER_LIST		30
//#define WIN_COUNTER		50

#define STRM_TYPE_AUDIO	0x00	// 0 = AUDIO STREAM
#define STRM_TYPE_VIDEO	0x01	// 1 = VIDEO STREAM
#define STRM_TYPE_MEDIA	0x02	// 2 = MEDIA STREAM

#define ADD_STREAM          2
#define DELETE_STREAM       3

#define CLASS_ARY_SIZE		1
#define CRLF_LEN			4

#define MODE_RTSP			1
#define MODE_RTMP			2
#define MODE_SG				3
#define MODE_BitStream		4
#define MODE_HTTP			5
#define MODE_File			6

#define MOD_TIME__CLOCK		1
#define MOD_TIME_TICK		2

struct timerStruct{
	volatile unsigned long clockTime;
	LARGE_INTEGER tickTime;
	volatile unsigned initTickFlag;
	volatile unsigned initClockFlag;
};

#define TE_RTSP				1
#define TE_RTMP				2	
#define TE_SG				3
#define STRATEGY_DFS		1
#define STRATEGY_BFS		2

#define REQUEST				0
#define REPLY				1

#define FREE				0
#define LOCK				1
#define MAIN_LOCKER			2
#define TIMEOUT_LOCKER		3

#define CLOSE_PARENT			0
#define CLOSE_CHILD				1
#define DONT_CARE				2

#define CAPACITY_THRESHOLD 0.75
#define CAPACITY_BASE 5
#define MAX_PEER_LIST		5

//  state 0 normal detection  -> rescue detect  and rescue (go to state 1) -> recv a List (go to state 2) 
#define STATE_DETECTION		0
#define STATE_RESCUE		1
#define STATE_LIST			2
#define STATE_TESTING		3

#define DBG_MODE

#ifdef DBG_MODE

#define DBG_PRINTF( ...) do {	\
									printf("[%s](Line:%d):", __FILE__, __LINE__);\
									printf(__VA_ARGS__);\
								} while (0);
#else
#define DBG_PRINTF 
#endif

enum RET_VALUE {RET_WRONG_SER_NUM = -3, RET_SOCK_ERROR = -2, RET_ERROR = -1, RET_OK = 0};

// This structure is for NAT Hole Punching
struct peer_info_t_nat {
	unsigned long pid;
	unsigned long level;
	unsigned long public_ip;
	unsigned long private_ip;
	unsigned short tcp_port;
	unsigned short udp_port;
//////////NAT////////////
	unsigned short public_port;
	unsigned short private_port;
	unsigned long upnp_acess;	//yes1 no0 
	unsigned long NAT_type;	//from 1 to 4 (4 cannot punch)
//////////NAT////////////
	// My information
	unsigned long manifest;
	unsigned long session_id;
	bool isPuncher;				// If I am puncher, set true; otherwise, false
	// STUNT-CTRL information
	unsigned long	ctrl_ip;	// STUNT-CTRL IP
	unsigned short	ctrl_port;	// STUNT-CTRL port
//	int rescueStatsArry[PARAMETER_M];
};


//down stream
struct peer_info_t {
	unsigned long pid;
	unsigned long level;
	unsigned long public_ip;
	unsigned long private_ip;
	unsigned short tcp_port;
	unsigned short udp_port;
//////////NAT////////////
	unsigned short public_port;
	unsigned short private_port;
	unsigned long upnp_acess;	//yes1 no0 
	unsigned long NAT_type;	//from 1 to 4 (4 cannot punch)
//////////NAT////////////
	unsigned long manifest;
//	int rescueStatsArry[PARAMETER_M];
};



struct level_info_t {
	unsigned long pid;
	unsigned long level;
	unsigned long public_ip;
	unsigned long private_ip;
	unsigned short tcp_port;
	unsigned short udp_port;
	//////////NAT////////////
	unsigned short public_port;
	unsigned short private_port;
	unsigned long upnp_acess;	//yes1 no0 
	unsigned long NAT_type;	//from 1 to 4 (4 cannot punch)
	//////////NAT////////////
};

struct request_info_t {
	unsigned long pid;
	unsigned long channel_id;
	unsigned long private_ip;
	unsigned short tcp_port;
	unsigned short udp_port;
};

struct rtsp_int_hdr_t {
	unsigned char magic;
	unsigned char channel;
	unsigned short length; 
};

struct ts_block_t {		//--!!0124
	unsigned long pid;
	unsigned long time_stamp;
	unsigned long
				rsv:31,
				isDST:1;
};

///P2P  main  header
struct chunk_header_t {
	unsigned char cmd;
	unsigned char 
		stream:3,
		payload_type:5;
	unsigned char 
		rsv_1:1,
		mf:1,
		part_seq:6;
	unsigned char stream_id;
	unsigned int sequence_number;
	unsigned int timestamp;
//	unsigned short rsv_3;
	unsigned long length;
};


//detection Info for each substream
struct detectionInfo{
	//timer
	struct timerStruct	lastAlarm;			// 每一次處理rescue_detecion()結束的時刻
	struct timerStruct	firstAlarm;			// count_X = 1 的時刻 
	struct timerStruct	previousAlarm;		// count_X = Xcount-1 的時刻 

	unsigned int	last_timestamp;			// 每一次處理rescue_detecion()結束的timestamp
	unsigned int	first_timestamp;		// count_X = 1 時刻的 timestamp
	unsigned long	last_seq;				// 每一次處理rescue_detecion()結束的seq

	unsigned int	measure_N;				//第N次測量
	unsigned int	count_X;				//X個封包量一次

	unsigned int	total_buffer_delay;		// 兩個連續封包之間的最大 source-delay
	double			last_sourceBitrate;		// 從 count_X = 1 累積到 count_X = Xcount 的 sourceBitrate
	double			last_localBitrate;		// 從 count_X = 1 累積到 count_X = Xcount 的 localBitrate
	unsigned int	total_byte;
	int				isTesting;
	unsigned int	testing_count;			//用來測試rescue 的計數器
	unsigned		previousParentPID;
};


struct rtp_hdr_t {
	unsigned char
		csrc_cnt:4, // The CSRC count contains the number of CSRC identifiers that follow the fixed header
		extension:1, // set to indicate that fixed header MUST be followed by exactly one header extension
		padding:1,	// set to indicate that there exist padding
		ver:2;
	unsigned char 
		pay_type:7, // payload type
		marker:1;
	unsigned short seq_num;
	unsigned long timestamp;
	unsigned long ssrc;	// identifies the synchronization source		
};


struct chunk_t{
	struct chunk_header_t header;
	unsigned char buf[0];
};






struct chunk_rtp_t{
	struct chunk_header_t header;
//	struct rtp_hdr_t hdr;
	unsigned char payload[RTP_PKT_BUF_PAY_SIZE];
};


struct chunk_rtmp_t{
	struct chunk_header_t header;
	//struct rtmp_hdr_t hdr;
	//unsigned char hdr_buf[13];
	unsigned char buf[RTMP_PKT_BUF_PAY_SIZE];
};

/*
struct chunk_bitstream_t{
	struct chunk_header_t header;
	unsigned char buf[RTMP_PKT_BUF_PAY_SIZE];
};
*/

/*
struct chunk_bitstream_t{
	struct chunk_header_t header;
	unsigned char buf[0];
};
*/

struct chunk_request_msg_t{
	struct chunk_header_t header;
	struct request_info_t info;
	unsigned char buf[0];
};

struct chunk_request_pkt_t{
	struct chunk_header_t header;
	unsigned long pid;
	unsigned int request_from_sequence_number;
	unsigned int request_to_sequence_number;
};

/////////////////////////////////////////////////////

struct peer_connect_down_t {
	struct peer_info_t peerInfo;
	int rescueStatsArry[PARAMETER_M];			// 
	volatile unsigned int timeOutLastSeq;
	volatile unsigned int timeOutNewSeq;		// Sequence number received from this parent so far
	volatile unsigned int lastTriggerCount;
	volatile unsigned int outBuffCount;			// Counter for lost packet whose seq is out of chunk buffer
	
};



struct chunk_delay_test_t{
	struct chunk_header_t header;
	unsigned char buf[0];
};


struct chunk_manifest_set_t{
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;

};

struct chunk_rescue_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;
};




///sent to pk , rescue from pk
struct rescue_pkt_from_server{
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;
	unsigned int rescue_seq_start;
};


//////////////////////////////////////////////////
/****************************************************/
/*		Structures of each P2P header command		*/
/****************************************************/
struct chunk_register_reply_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long level;
	unsigned long bit_rate;
	unsigned long sub_stream_num;
	unsigned long public_ip;
	unsigned long inside_lane_rescue_num;
	struct level_info_t *level_info[0];	
};

/*
struct chunk_bandwidth_t {
	struct chunk_header_t header;
	unsigned long bandwidth;
};
*/

struct chunk_rescue_reply_t {
	struct chunk_header_t header;
	unsigned char reply;
	unsigned long manifest;
};


struct chunk_rescue_list_t {
	struct chunk_header_t header;
	unsigned long pid;
};


struct chunk_rescue_list_reply_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long level;
	struct level_info_t *level_info[0];	
};

enum network_nonblocking_ctl_state {
	READY = 0,
	RUNNING = 1
};

enum pkg_nonblocking_ctl_state {
	READ_HEADER_TYPE = 0,
	READ_HEADER_CHANNEL_0 = 1,
	READ_HEADER_CHANNEL_1 = 2,
	READ_HEADER_LAST = 3,
	READ_HEADER_EXTEND_TIME = 4,
	READ_BODY = 5,
	READ_CHUNK_FINISH = 6,

	READ_HEADER_READY = 7,
	READ_HEADER_RUNNING =8,
	READ_HEADER_OK=9,
	READ_PAYLOAD_READY =10,
	READ_PAYLOAD_RUNNING =11,
	READ_PAYLOAD_OK=12

};


/*
#define READ_HEADER_TYPE		 0
#define	READ_HEADER_CHANNEL_0	 1
#define	READ_HEADER_CHANNEL_1	 2
#define	READ_HEADER_LAST		 3
#define	READ_HEADER_EXTEND_TIME  4
#define	READ_BODY				 5
#define	READ_CHUNK_FINISH	 	6

#define	READ_HEADER_READY  7
#define	READ_HEADER_RUNNING 8
#define	READ_HEADER_OK 9
#define	READ_PAYLOAD_READY 10
#define	READ_PAYLOAD_RUNNING 11
#define	READ_PAYLOAD_OK 12
*/

typedef struct {
	char *buffer;
	struct chunk_t* chunk_ptr;
	unsigned int offset;
	unsigned int total_len;
	unsigned int expect_len;
	unsigned int serial_num; //never use
	network_nonblocking_ctl_state ctl_state;
} Network_nonblocking_ctl;

typedef struct nonblocking_ctrl{
//	unsigned char recv_packet_state;
	pkg_nonblocking_ctl_state recv_packet_state;
	Network_nonblocking_ctl recv_ctl_info;
} Nonblocking_Ctl;

// The lowest level header; each fd has one this buff 0807
typedef struct nonblocking_buff{
	struct nonblocking_ctrl nonBlockingRecv;		// all received messages are store in here 0807
	struct nonblocking_ctrl nonBlockingSendData;
	struct nonblocking_ctrl nonBlockingSendCtrl;
} Nonblocking_Buff;

struct chunk_level_msg_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;
	struct level_info_t *level_info[0];		// each peer's Info
};

struct chunk_cut_peer_info_t {
	struct chunk_header_t header;
	unsigned long pid;
};

struct chunk_rt_latency_t {   //--!!0124
	struct chunk_header_t header;
	unsigned short dts_offset;
	unsigned short dts_length; //followed by struct ts_block_t[]
	struct ts_block_t buf[0];
};

struct web_control_info {
    int type;
    int stream_id;
	char user_name[64];
};

struct channel_stream_map_info_t {
	unsigned char stream_id;
	unsigned char rsv_1;
	char user_name[64];
};

struct channel_chunk_size_info_t {
	unsigned char stream_id;
	unsigned char rsv_1;
	int chunk_size;
};

struct channel_stream_notify {
	struct chunk_header_t header;
	unsigned long total_num;
	struct channel_stream_map_info_t *channel_stream_map_info[0];	
};

struct peer_timestamp_info_t {
	unsigned long pid;
	unsigned long peer_sec;
	unsigned long peer_usec;
};

struct peer_latency_measure {
	struct chunk_header_t header;
	unsigned long pk_sec;
	unsigned long pk_usec;
	unsigned long total_num;
	struct peer_timestamp_info_t peer_timestamp_info[0];
};

//////////////////////////////////////////////////////////////////////////////////measure start delay
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL

struct syn_struct{
	int init_flag; // 0:not init, 1:send, 2:init complete
	UINT32 client_abs_start_time;	// Synchronized time relative to PK
	unsigned long start_seq;

	//timer
	struct timerStruct start_clock;			// The last synchronization time
//	struct timerStruct end_clock;
};
//////////////////////////////////////////////////////////////////////////////////SYN PROTOCOL
struct source_delay {

	unsigned int source_delay_time;
	//timer
	struct timerStruct client_end_time;
	unsigned long end_seq_num;	// The first received packet sequence of a substream
	UINT32 end_seq_abs_time;
	bool first_pkt_recv;
	int rescue_state;			//0 normal 1 rescue trigger 2 testing
	int delay_beyond_count;		// Counter++ if delay more than MAX_DELAY
//	int delay time

};
/****************************************************/
/*		Structures of synchronization				*/
/****************************************************/
struct syn_token_send {
	struct chunk_header_t header;
	unsigned long reserve;
};

struct syn_token_receive {
	struct chunk_header_t header;
	unsigned long seq_now;
	unsigned long pk_RecvTime;
	unsigned long pk_SendTime;
};
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////send capacity
struct rescue_peer_capacity_measurement{
	struct chunk_header_t header;
	unsigned int rescue_num;
	//int rescue_condition;
//	char NAT_status;
	char content_integrity;
	unsigned long *source_delay_measur[0];
};
//////////////////////////////////////////////////////////////////////////////////
struct seed_notify{
	struct chunk_header_t header;
	unsigned int manifest;
};

struct chunk_rescue_list {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;
	struct rescue_peer_info *rescue_peer_info[MAX_PEER_LIST];	
};

// header | pid | pid |
struct chunk_ParentChildren_token {
	struct chunk_header_t header;
	unsigned int manifest;
};

struct start_delay_update_info{
	unsigned long substream_id;
	int start_delay_update;
};

struct update_start_delay{
	struct chunk_header_t header;
	struct start_delay_update_info *update_info[0];
};

struct rescue_update_from_server{
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;
};

// header | manifest | parent_num | parentPID | parentPID |....
struct update_topology_info{
	struct chunk_header_t header;
	unsigned int manifest;
	unsigned long parent_num;
	unsigned long parent_pid[0];
};

struct update_stream_header{
	int len;
	unsigned char header[0];
};

struct role_struct{
	struct chunk_header_t header;
	int flag;	//flag 0 another is rescue peer, flag 1 another is candidate
	unsigned long manifest;
	unsigned long send_pid;
	unsigned long recv_pid;
};

struct peer_com_info{
	int peer_num;
	int role;	//caller's role : 0 rescue peer; 1 candidate
	unsigned long manifest;	//caller's manifest
	struct chunk_level_msg_t *list_info;
};

struct manifest_timmer_flag{
	unsigned long	pid;
	unsigned long	firstReplyFlag;
	unsigned long	networkTimeOutFlag;
	unsigned long	connectTimeOutFlag;
	unsigned long	rescue_manifest;	//may be rescue peer or candidates
	int peer_role;	//0 rescue peer 1 candidate
	//timer
	struct	 timerStruct	networkTimeOut;
	struct	 timerStruct	connectTimeOut;
};

struct chunk_child_info {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;	//rescue peer
	struct level_info_t child_level_info;	
};

struct fd_information {
	int flag;	//flag 0 rescue peer, flag 1 candidates, and delete in stop
	unsigned long manifest;	//must be store before io_connect, and delete in stop
	unsigned long pid;	//must be store before io_connect, and delete in stop
	unsigned long session_id;	//must be store before io_connect, and delete in stop
};

typedef struct _XconnInfo
{
	SOCKET sServer;
	SOCKET sPeer;
	char myId[MAX_ID_LEN + 1];
	char peerId[MAX_ID_LEN + 1];
	int flag;
} XconnInfo;

struct nat_con_thread_struct {
	HANDLE hThread;
	unsigned threadID;
	unsigned long manifest;
	unsigned long pid;
	int role;
	void *net_object;
	void *nat_interface_object;
	void *peer_mgr_object;
	XconnInfo Xconn;
};



/*
1. LOG_REGISTER: send register info to get register back
2. LOG_REG_LIST: from recive reg list to timeout(start testing) 
3. LOG_REG_LIST_TESTING: from timeout(start testing) to setmanifest
4. LOG_REG_LIST_DETECTION_TESTING: from setmanifest to cut pk stream.
5. LOG_REG_LIST_TESTING_FAIL: from timeout(start testing) to send topology to pk.
6. LOG_REG_CUT_PK: send pk cut.
7. LOG_REG_DATA_COME: data come.
#define LOG_REGISTER	0x01
#define LOG_REG_LIST	0x02
#define LOG_REG_LIST_TESTING	0x03	
#define LOG_REG_LIST_DETECTION_TESTING	0x04
#define LOG_REG_LIST_TESTING_FAIL	0x05
#define LOG_REG_CUT_PK	0x06
#define LOG_REG_DATA_COME	0x07

1. LOG_RESCUE_TRIGGER: from sending rescue request to get rescue list
2. LOG_RESCUE_LIST: from reciving rescue list to timeout(start testing)
3. LOG_RESCUE_TESTING: from start testing to setmanifest
4. LOG_RESCUE_DETECTION_TESTING: from setmanifest to cut pk stream.
5. LOG_RESCUE_LIST_TESTING_FAIL: from timeout(start testing) to send topology to pk.
6. LOG_RESCUE_CUT_PK: send pk cut.
7. LOG_RESCUE_DATA_COME: data come.
#define LOG_RESCUE_TRIGGER	0x08	
#define LOG_RESCUE_LIST	0x09
#define LOG_RESCUE_TESTING	0x0a
#define LOG_RESCUE_DETECTION_TESTING	0x0b
#define LOG_RESCUE_LIST_TESTING_FAIL	0x0c
#define LOG_RESCUE_CUT_PK	0x0d
#define LOG_RESCUE_DATA_COME	0x0e

the peer's condition
#define LOG_START_DELAY	0x0f
#define LOG_PERIOD_SOURCE_DELAY	0x10
#define LOG_RESCUE_SUB_STREAM	0x11
#define LOG_PEER_LEAVE	0x12	
#define LOG_WRITE_STRING	0x13
#define LOG_BEGINE 0x14

#define LOG_RESCUE_TRIGGER_BACK 0x15
#define LOG_LIST_EMPTY 0x16
#define LOG_TEST_DELAY_FAIL 0x17
#define LOG_TEST_DETECTION_FAIL 0x18
#define LOG_DATA_COME_PK 0x19
*/
struct log_header_t{
	unsigned char cmd;
	unsigned long pid;
	unsigned long log_time;
	unsigned long manifest;
	unsigned long channel_id;
	unsigned long length;
};

struct log_pkt_format_struct{
	struct log_header_t log_header;
	unsigned char buf[0];
};

struct log_register_struct{
	struct log_header_t log_header;
};

struct log_rescue_trigger_struct{
	struct log_header_t log_header;
};

struct log_list_struct{
	struct log_header_t log_header;
	unsigned long list_num;
	unsigned long connect_num;
	unsigned long *list_peer;
	unsigned long *connect_list;
};

struct log_list_testing_struct{
	struct log_header_t log_header;
	unsigned long select_pid;
};

struct log_list_detection_testing_struct{
	struct log_header_t log_header;
	unsigned long testing_result;
	unsigned long select_pid;
};

struct log_list_testing_fail_struct{
	struct log_header_t log_header;
	unsigned long select_pid;
};

struct log_cut_pk_struct{
	struct log_header_t log_header;
};

struct log_data_come_struct{
	struct log_header_t log_header;
};

struct log_start_delay_struct{
	struct log_header_t log_header;
	double start_delay;
};

struct log_period_source_delay_struct{
	struct log_header_t log_header;
	double max_delay;
	unsigned long sub_num;
	double *av_delay;
};

struct log_rescue_sub_stream_struct{
	struct log_header_t log_header;
	unsigned long rescue_num;
};

struct log_peer_leave_struct{
	struct log_header_t log_header;
};

struct log_write_string_struct{
	struct log_header_t log_header;
	unsigned char buf[0];
};

struct log_begine_struct{
	struct log_header_t log_header;
	unsigned long public_ip;
	unsigned short private_port;
};

struct log_rescue_trigger_back_struct{
	struct log_header_t log_header;
};

struct log_list_empty_struct{
	struct log_header_t log_header;
};

struct log_test_delay_fail_struct{
	struct log_header_t log_header;
};

struct log_test_detection_fail_struct{
	struct log_header_t log_header;
	unsigned long select_pid;
};

struct log_data_come_pk_struct{
	struct log_header_t log_header;
};

struct log_client_bw_struct{
	struct log_header_t log_header;
	double should_in_bw;
	double real_in_bw;
	double real_out_bw;
	double quality;
};

struct log_time_out_struct{
	struct log_header_t log_header;
};

struct log_pkt_lose_struct{
	struct log_header_t log_header;
};

struct log_source_delay_struct{
	unsigned int count;
	double delay_now;
	double average_delay;
	double accumulated_delay;
};

struct log_in_bw_struct{
	unsigned long time_stamp;
//	LARGE_INTEGER client_time; 
	struct timerStruct client_time; 
};

// Structure of delay-quality
struct quality_struct{
	unsigned long lost_pkt;			// Number of lost chunks in one calculation
	double accumulated_quality;		// Accumulated quality values in one calculation
	double average_quality;			// Average quality value in one calculation, this value would be sent to PK
	unsigned long total_chunk;		// Number of chunks in one calculation
};


#define INIT 0
#define IO_FINISH 1


struct ioNonBlocking{

	Nonblocking_Buff io_nonblockBuff;
	queue<struct chunk_t *> outPutQue;
	unsigned long ioFinishFlag;

};


typedef struct{

	fd_set read_master, read_fds;   // master file descriptor list
	fd_set write_master, write_fds; // temp file descriptor list for select()
	fd_set error_master, error_fds; // temp file descriptor list for select()

} EpollVars;

#endif
