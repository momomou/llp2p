#ifndef __RTMP_SUPPLEMENT_H__
#define __RTMP_SUPPLEMENT_H__

#include "rtmp.h"
#include "logger.h"
#include "parseurl.h"

#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#define fseeko fseeko64
#define ftello ftello64
#include <io.h>
#include <fcntl.h>
#define	SET_BINMODE(f)	setmode(fileno(f), O_BINARY)
#else
#define	SET_BINMODE(f)
#endif

#define MAX_IGNORED_FRAMES	50

#define RTMP_PACKET_SIZE_MEDIUM   1
#define STR2AVAL(av,str)	av.av_val = str; av.av_len = strlen(av.av_val)
#define SAVC(x) static const AVal av_##x = AVC(#x)


#define RD_SUCCESS		0
#define RD_FAILED		1
#define RD_INCOMPLETE		2

#define RTMP_SIG_SIZE 1536

#define RTMP_HEADER_TYPE_0	0
#define RTMP_HEADER_TYPE_1	1
#define RTMP_HEADER_TYPE_2	2
#define RTMP_HEADER_TYPE_3	3

enum{
	MSG_CHUNK_SIZE = 0x01,	
	MSG_BYTES_READ = 0x03,
	MSG_CTRL = 0x04,
	MSG_SERVER_BW = 0x05,
	MSG_CLIENT_BW = 0x06,
	MSG_AUDIO_DATA = 0x08,
	MSG_VIDEO_DATA = 0x09,	
	MSG_FLEX_STREAM_SEND = 0x0F,
	MSG_FLEX_SHARED_OBJECT = 0x10,
	MSG_FLEX_MESSAGE = 0x11,	
	MSG_METADATA = 0x12,
	MSG_SHARED_OBJECT = 0x13,
	MSG_INVOKE = 0x14,	
	MSG_FLV = 0x16
};

class rtmp_supplement {
public:
	//based on libRTMP function
	rtmp_supplement(logger *log_ptr, rtmp *rtmp_ptr, amf *amf_ptr);
	~rtmp_supplement();
	bool SendBytesReceived(RTMP * r);
	int ReadN(RTMP * r, char *buffer, int n);
	bool WriteN(RTMP * r, const char *buffer, int n);
	void HandleCtrl(RTMP * r, const RTMPPacket * packet);
	void dumpAMF(AMFObject *obj);
	bool SendResultNumber(RTMP *r, double txn, double ID);
	bool SendConnectResult(RTMP *r, double txn);
	bool SendChangeChunkSize(RTMP * r);
	bool SendOnStatus(RTMP *r);
	bool SendOnMetaData(RTMP *r);
	int WriteHeader(char **buf, unsigned int len);
	//based on libRTMP function
	int WriteStream(RTMP * rtmp, char **buf,	// target pointer, maybe preallocated
		    unsigned int len,	// length of buffer if preallocated
		    uint32_t * tsm,	// pointer to timestamp, will contain timestamp of last video packet returned
		    bool bResume,	// resuming mode, will not write FLV header and compare metaHeader and first kexframe
		    bool bLiveStream,	// live mode, will not report absolute timestamps
		    uint32_t nResumeTS,	// resume keyframe timestamp
		    char *metaHeader,	// pointer to meta header (if bResume == TRUE)
		    uint32_t nMetaHeaderSize,	// length of meta header, if zero meta header check omitted (if bResume == TRUE)
		    char *initialFrame,	// pointer to initial keyframe (no FLV header or tagSize, raw data) (if bResume == TRUE)
		    uint8_t initialFrameType,	// initial frame type (audio or video)
		    uint32_t nInitialFrameSize,	// length of initial frame in bytes, if zero initial frame check omitted (if bResume == TRUE)
		    uint8_t * dataType	// whenever we get a video/audio packet we set an appropriate flag here, this will be later written to the FLV header
		    );
private:
	logger *_log_ptr;
	rtmp *_rtmp_ptr;
	amf *_amf_ptr;

	FILE *_netstackdump;
	FILE *_netstackdump_read;
	
};

#endif



