#ifndef __RTMP_H__
#define __RTMP_H__
/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *      Copyright (C) 2008-2009 Andrej Stepanchuk
 *      Copyright (C) 2009-2010 Howard Chu
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with flvstreamer; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "../common.h"
#include "amf.h"
#include <errno.h>
#ifdef _WIN32
	#include "stdint.h"
#else
	#include <stdint.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#define GetSockError()	WSAGetLastError()
#define setsockopt(a,b,c,d,e)	(setsockopt)(a,b,c,(const char *)d,(int)e)
#define EWOULDBLOCK	WSAETIMEDOUT	/* we don't use nonblocking, but we do use timeouts */
#define sleep(n)	Sleep(n*1000)
#define msleep(n)	Sleep(n)
#define socklen_t	int
#define SET_RCVTIMEO(tv,s)	int tv = s*1000
#else
/*#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>*/
#include <netdb.h>
#include <sys/times.h>
#include <netinet/tcp.h>
#define GetSockError()	errno
#define closesocket(s)	close(s)
#define msleep(n)	usleep(n*1000)
#define SET_RCVTIMEO(tv,s)	struct timeval tv = {s,0}
#endif

#define RTMP_PROTOCOL_UNDEFINED	-1
#define RTMP_PROTOCOL_RTMP      0
#define RTMP_PROTOCOL_RTMPT     1	// not yet supported
#define RTMP_PROTOCOL_RTMPS     2	// not yet supported
#define RTMP_PROTOCOL_RTMPE     3
#define RTMP_PROTOCOL_RTMPTE    4	// not yet supported
#define RTMP_PROTOCOL_RTMFP     5	// not yet supported

#define RTMP_DEFAULT_CHUNKSIZE	128

#define RTMP_BUFFER_CACHE_SIZE (16*1024) // needs to fit largest number of bytes recv() may return

#define	RTMP_CHANNELS	65600

extern const char RTMPProtocolStringsLower[][7];
extern bool RTMP_ctrlC;

#define RTMP_PACKET_TYPE_AUDIO 0x08
#define RTMP_PACKET_TYPE_VIDEO 0x09
#define RTMP_PACKET_TYPE_INFO  0x12

#define RTMP_MAX_HEADER_SIZE 18

#define SAVC(x) static const AVal av_##x = AVC(#x)
SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(_result);
SAVC(createStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(fmsVer);
SAVC(mode);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);
SAVC(secureTokenResponse);

SAVC(details);
SAVC(clientid);
SAVC(onStatus);
SAVC(width);
SAVC(height);
SAVC(videodatarate);
SAVC(framerate);
SAVC(videocodecid);
SAVC(audiosamplerate);
SAVC(audiosamplesize);
SAVC(stereo);
SAVC(audiocodecid);
SAVC(filesize);
SAVC(duration);
SAVC(onMetaData);

SAVC(onBWDone);
SAVC(onFCSubscribe);
SAVC(onFCUnsubscribe);
SAVC(_onbwcheck);
SAVC(_onbwdone);
SAVC(_error);
SAVC(close);
SAVC(_checkbw);
SAVC(FCSubscribe);
SAVC(pause);
SAVC(deleteStream);

static const AVal av_NetStream_Failed = AVC("NetStream.Failed");
static const AVal av_NetStream_Play_Failed = AVC("NetStream.Play.Failed");
static const AVal av_NetStream_Play_StreamNotFound = AVC("NetStream.Play.StreamNotFound");
static const AVal av_NetConnection_Connect_InvalidApp = AVC("NetConnection.Connect.InvalidApp");
static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_NetStream_Play_Complete = AVC("NetStream.Play.Complete");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");

typedef unsigned char BYTE;

typedef struct RTMPChunk
{
  int c_headerSize;
  int c_chunkSize;
  char *c_chunk;
  char c_header[RTMP_MAX_HEADER_SIZE];
} RTMPChunk;

typedef struct RTMPPacket
{
  BYTE m_headerType;
  BYTE m_packetType;
  BYTE m_hasAbsTimestamp;	// timestamp absolute or relative?
  int m_nChannel;
  uint32_t m_nInfoField1;	// 3 first bytes
  int32_t m_nInfoField2;	// last 4 bytes in a long header, absolute timestamp for long headers, relative timestamp for short headers
  uint32_t m_nTimeStamp;	// absolute timestamp
  uint32_t m_nBodySize;
  uint32_t m_nBytesRead;
  RTMPChunk *m_chunk;
  char *m_body;
} RTMPPacket;

typedef struct RTMPSockBuf
{
  int sb_socket;
  int sb_size;				/* number of unprocessed bytes in buffer */
  char *sb_start;			/* pointer into sb_pBuffer of next byte to process */
  char sb_buf[RTMP_BUFFER_CACHE_SIZE];	/* data read from socket */
  bool sb_timedout;
} RTMPSockBuf;

#define RTMPPacket_IsReady(a)	((a)->m_nBytesRead == (a)->m_nBodySize)

typedef struct RTMP_LNK
{
  int m_nBufferMS;
  int m_stream_id;
  bool m_bPlaying;
  
  const char *hostname;
  unsigned int port;
  int protocol;

  AVal playpath;
  AVal tcUrl;
  AVal swfUrl;
  AVal pageUrl;
  AVal app;
  AVal auth;
  AVal flashVer;
  AVal subscribepath;
  AVal token;
  bool authflag;
  AMFObject extras;

  double seekTime;
  uint32_t length;
  bool bLiveStream;

  long int timeout;		// number of seconds before connection times out

  const char *sockshost;
  unsigned short socksport;

} RTMP_LNK;

typedef struct RTMP
{
  int m_inChunkSize;
  int m_outChunkSize;
  int m_nBWCheckCounter;
  int m_nBytesIn;
  int m_nBytesInSent;
  //int m_nBufferMS;
  //int m_stream_id;		// returned in _result from invoking createStream
  int m_mediaChannel;
  uint32_t m_mediaStamp;
  uint32_t m_pauseStamp;
  int m_pausing;
  int m_nServerBW;
  int m_nClientBW;
  uint8_t m_nClientBW2;
  //bool m_bPlaying;
  bool m_bSendEncoding;
  bool m_bSendCounter;

  AVal *m_methodCalls;		/* remote method calls queue */
  int m_numCalls;

  vector<RTMP_LNK> Link;
  int stream_counter;     /* to sendplay */
  RTMPPacket *m_vecChannelsIn[RTMP_CHANNELS];
  RTMPPacket *m_vecChannelsOut[RTMP_CHANNELS];
  int m_channelTimestamp[RTMP_CHANNELS];	// abs timestamp of last packet

  double m_fAudioCodecs;	// audioCodecs for the connect packet
  double m_fVideoCodecs;	// videoCodecs for the connect packet
  double m_fEncoding;		/* AMF0 or AMF3 */

  double m_fDuration;		// duration of stream in seconds

  RTMPSockBuf m_sb;
#define m_socket	m_sb.sb_socket
#define m_nBufferSize	m_sb.sb_size
#define m_pBufferStart	m_sb.sb_start
#define m_pBuffer	m_sb.sb_buf
#define m_bTimedout	m_sb.sb_timedout
} RTMP;

//class amf;
class logger;
class network;


class rtmp {
public:
	rtmp(network * net_ptr, amf *amf_ptr, logger * log_ptr);
	~rtmp();
	uint32_t RTMP_GetTime();
	void RTMPPacket_Reset(RTMPPacket *p);
	void RTMPPacket_Dump(RTMPPacket *p);
	bool RTMPPacket_Alloc(RTMPPacket *p, int nSize);
	void RTMPPacket_Free(RTMPPacket *p);


	void RTMP_SetBufferMS(RTMP *r, int size);
	void RTMP_UpdateBufferMS(RTMP *r);

	void RTMP_SetupStream(RTMP *r, int protocol,
			      const char *hostname,
			      unsigned int port,
			      const char *sockshost,
			      AVal *playpath,
			      AVal *tcUrl,
			      AVal *swfUrl,
			      AVal *pageUrl,
			      AVal *app,
			      AVal *auth,
			      AVal *swfSHA256Hash,
			      uint32_t swfSize,
			      AVal *flashVer,
			      AVal *subscribepath,
			      double dTime,
			      uint32_t dLength, bool bLiveStream, long int timeout);
	void RTMP_SetupStream_t(RTMP_LNK* link,
			 int protocol,
			 const char *hostname,
			 unsigned int port,
			 const char *sockshost,
			 AVal * playpath,
			 AVal * tcUrl,
			 AVal * swfUrl,
			 AVal * pageUrl,
			 AVal * app,
			 AVal * auth,
			 AVal * swfSHA256Hash,
			 uint32_t swfSize,
			 AVal * flashVer,
			 AVal * subscribepath,
			 double dTime,
			 uint32_t dLength, bool bLiveStream, long int timeout);

	bool RTMP_Connect(RTMP *r, RTMPPacket *cp);
	bool RTMP_Connect0(RTMP *r, struct sockaddr *svc);
	bool RTMP_Connect1(RTMP *r, RTMPPacket *cp);
	bool RTMP_Serve(RTMP *r);

	int non_block_readpkg(network * net_ptr, RTMP * r, RTMPPacket * packet, Recv_nonblocking_ctl * recv_pkg_info);

	bool RTMP_ReadPacket(RTMP * r, RTMPPacket * packet);
	bool RTMP_SendPacket(RTMP * r, RTMPPacket * packet, bool queue);
	bool RTMP_SendChunk(RTMP * r, RTMPChunk *chunk);
	bool RTMP_IsConnected(RTMP *r);
	bool RTMP_IsTimedout(RTMP *r);
	double RTMP_GetDuration(RTMP *r);
	bool RTMP_ToggleStream(RTMP *r);

	bool RTMP_ConnectStream(RTMP *r, double seekTime, uint32_t dLength);

	bool RTMP_ReconnectStream(RTMP *r, int bufferTime, double seekTime, uint32_t dLength);
	void RTMP_DeleteStream(RTMP *r);
	int RTMP_GetNextMediaPacket(RTMP *r, RTMPPacket *packet);
	int RTMP_ClientPacket(RTMP *r, RTMPPacket *packet);

	void RTMP_Init(RTMP *r);
	void RTMP_Close(RTMP *r);

	bool RTMP_SendCtrl(RTMP * r, short nType, unsigned int nObject, unsigned int nTime);
	bool RTMP_SendPause(RTMP *r, bool DoPause, double dTime);
	bool RTMP_FindFirstMatchingProperty(AMFObject *obj, const AVal *name,
					      AMFObjectProperty *p);

	bool RTMPSockBuf_Fill(RTMPSockBuf *sb);
	void check_SendBytesReceived(RTMP * r, int read_byte);
	bool WriteN(RTMP * r, const char *buffer, int n);
	int ReadN(RTMP * r, char *buffer, int n);
	void HandleCtrl(RTMP * r, const RTMPPacket * packet);
private:
	bool SocksNegotiate(RTMP * r);
	bool add_addr_info(struct sockaddr_in *service, const char *hostname, int port);
	bool HandShake(RTMP * r, bool FP9HandShake);
	bool SHandShake(RTMP * r);
	bool SendConnectPacket(RTMP *r, RTMPPacket *cp);	
	void HandleChangeChunkSize(RTMP * r, const RTMPPacket * packet);
	void HandleServerBW(RTMP * r, const RTMPPacket * packet);
	void HandleClientBW(RTMP * r, const RTMPPacket * packet);
	void HandleAudio(RTMP * r, const RTMPPacket * packet);
	void HandleVideo(RTMP * r, const RTMPPacket * packet);
	bool HandleMetadata(RTMP * r, char *body, unsigned int len);
	int HandleInvoke(RTMP * r, const char *body, unsigned int nBodySize);
	bool SendBytesReceived(RTMP * r);
	bool SendCreateStream(RTMP * r, double dStreamId);
	bool SendFCSubscribe(RTMP * r, AVal * subscribepath);
	bool SendDeleteStream(RTMP * r, double dStreamId);
	bool SendServerBW(RTMP * r);
	bool SendCheckBW(RTMP * r);
	bool SendCheckBWResult(RTMP * r, double txn);
	bool SendPlay(RTMP * r, RTMP_LNK *stream);
	bool SendSecureTokenResponse(RTMP *r, AVal *resp);
	void AV_erase(AVal * vals, int *num, int i, bool freeit);
	void AV_queue(AVal ** vals, int *num, AVal * av);
	void AV_clear(AVal * vals, int num);
	void DecodeTEA(AVal *key, AVal *text);
	bool DumpMetaData(AMFObject * obj);
	int DecodeInt32LE(const char *data);
	int EncodeInt32LE(char *output, int nVal);
	
	
	int _tmpBytesIn;
	int _tmpsec;
	int _tmpmin;
	amf * _amf_ptr;
	network * _net_ptr;
	logger * _log_ptr;
	

	FILE *_netstackdump;
	FILE *_netstackdump_read;

	
};

#endif
