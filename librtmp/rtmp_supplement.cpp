#include "rtmp_supplement.h"

//based on libRTMP function
rtmp_supplement::rtmp_supplement(logger *log_ptr, rtmp *rtmp_ptr, amf *amf_ptr)
{
	_rtmp_ptr = rtmp_ptr;
	_log_ptr = log_ptr;
	_amf_ptr = amf_ptr;
}

rtmp_supplement::~rtmp_supplement()
{

}

bool rtmp_supplement::SendBytesReceived(RTMP * r)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x02;	// control channel (invoke)
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = MSG_BYTES_READ;	// bytes in
	packet.m_nInfoField1 = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	packet.m_nBodySize = 4;

	_amf_ptr->AMF_EncodeInt32(packet.m_body, pend, r->m_nBytesIn);	// hard coded for now
	r->m_nBytesInSent = r->m_nBytesIn;

	//Log(LOGDEBUG, "Send bytes report. 0x%x (%d bytes)", (unsigned int)m_nBytesIn, m_nBytesIn);
	return _rtmp_ptr->RTMP_SendPacket(r, &packet, false);
}

//based on libRTMP function
int rtmp_supplement::ReadN(RTMP * r, char *buffer, int n)
{
	int nOriginalSize = n;
	char *ptr;

	r->m_bTimedout = false;

#ifdef _DEBUG
	memset(buffer, 0, n);
#endif

	ptr = buffer;
	while (n > 0) {
		int nBytes = 0, nRead;
		if (r->m_nBufferSize == 0)
		if (_rtmp_ptr->RTMPSockBuf_Fill(&r->m_sb)<1)
		{
			if (!r->m_bTimedout)
			_rtmp_ptr->RTMP_Close(r);
			return 0;
		}
		nRead = ((n < r->m_nBufferSize) ? n : r->m_nBufferSize);
		if (nRead > 0) {
			memcpy(ptr, r->m_pBufferStart, nRead);
			r->m_pBufferStart += nRead;
			r->m_nBufferSize -= nRead;
			nBytes = nRead;
			r->m_nBytesIn += nRead;
			if (r->m_bSendCounter && r->m_nBytesIn > r->m_nBytesInSent + r->m_nClientBW / 2)
			SendBytesReceived(r);
		}

		//Log(LOGDEBUG, "%s: %d bytes\n", __FUNCTION__, nBytes);
#ifdef _DEBUG
		//fwrite(ptr, 1, nBytes, _netstackdump_read);
#endif

		if (nBytes == 0) {
			_log_ptr->Log(LOGDEBUG, "%s, RTMP socket closed by peer", __FUNCTION__);
			//goto again;
			_rtmp_ptr->RTMP_Close(r);
			break;
		}

		n -= nBytes;
		ptr += nBytes;
	}

	return nOriginalSize - n;
}

//based on libRTMP function
bool rtmp_supplement::WriteN(RTMP * r, const char *buffer, int n)
{
	const char *ptr = buffer;

	while (n > 0) {
#ifdef _DEBUG
		//fwrite(ptr, 1, n, _netstackdump);
#endif

		int nBytes = send(r->m_socket, ptr, n, 0);
		//Log(LOGDEBUG, "%s: %d\n", __FUNCTION__, nBytes);

		if (nBytes < 0) {
			int sockerr = GetSockError();
			_log_ptr->Log(LOGERROR, "%s, RTMP send error %d (%d bytes)", __FUNCTION__,
			sockerr, n);

			if (sockerr == EINTR && !RTMP_ctrlC)
			continue;

			_rtmp_ptr->RTMP_Close(r);
			n = 1;
			break;
		}

		if (nBytes == 0)
		break;

		n -= nBytes;
		ptr += nBytes;
	}

	return n == 0;
}

//based on libRTMP function
void rtmp_supplement::HandleCtrl(RTMP * r, const RTMPPacket * packet)
{
	short nType = -1;
	unsigned int tmp;
	if (packet->m_body && packet->m_nBodySize >= 2)
	nType = _amf_ptr->AMF_DecodeInt16(packet->m_body);
	_log_ptr->Log(LOGDEBUG, "%s, received ctrl. type: %d, len: %d", __FUNCTION__, nType,
	packet->m_nBodySize);
	//LogHex(packet.m_body, packet.m_nBodySize);

	if (packet->m_nBodySize >= 6) {
		switch (nType) {
		case 0:
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Stream Begin %d", __FUNCTION__, tmp);
			break;

		case 1:
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Stream EOF %d", __FUNCTION__, tmp);
			if (r->m_pausing == 1)
			r->m_pausing = 2;
			break;

		case 2:
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Stream Dry %d", __FUNCTION__, tmp);
			break;

		case 4:
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Stream IsRecorded %d", __FUNCTION__, tmp);
			break;

		case 6:		// server ping. reply with pong.
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Ping %d", __FUNCTION__, tmp);
			_rtmp_ptr->RTMP_SendCtrl(r, 0x07, tmp, 0);
			break;

		case 31:
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Stream BufferEmpty %d", __FUNCTION__, tmp);
			if (!r->m_pausing)
			{
				r->m_pauseStamp = r->m_channelTimestamp[r->m_mediaChannel];
				_rtmp_ptr->RTMP_SendPause(r, true, r->m_pauseStamp);
				r->m_pausing = 1;
			}
			else if (r->m_pausing == 2)
			{
				_rtmp_ptr->RTMP_SendPause(r, false, r->m_pauseStamp);
				r->m_pausing = 3;
			}
			break;

		case 32:
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Stream BufferReady %d", __FUNCTION__, tmp);
			break;

		default:
			tmp = _amf_ptr->AMF_DecodeInt32(packet->m_body + 2);
			_log_ptr->Log(LOGDEBUG, "%s, Stream xx %d", __FUNCTION__, tmp);
			break;
		}

	}

	if (nType == 0x1A) {
		_log_ptr->Log(LOGDEBUG, "%s, SWFVerification ping received: ", __FUNCTION__);
		_log_ptr->Log(LOGERROR,
		"%s: Ignoring SWFVerification request, no CRYPTO support!",
		__FUNCTION__);
	}
}

//based on libRTMP function
void rtmp_supplement::dumpAMF(AMFObject *obj)
{
	int i;
	const char opt[] = "NBSO Z";

	for (i=0; i < obj->o_num; i++) {
		AMFObjectProperty *p = &obj->o_props[i];
		printf(" -C ");
		if (p->p_name.av_val)
		printf("N");
		printf("%c:", opt[p->p_type]);
		if (p->p_name.av_val)
		printf("%.*s:", p->p_name.av_len, p->p_name.av_val);
		switch(p->p_type) {
		case AMF_BOOLEAN:
			printf("%d", p->p_vu.p_number != 0);
			break;
		case AMF_STRING:
			printf("%.*s", p->p_vu.p_aval.av_len, p->p_vu.p_aval.av_val);
			break;
		case AMF_NUMBER:
			printf("%f", p->p_vu.p_number);
			break;
		case AMF_OBJECT:
			printf("1");
			dumpAMF(&p->p_vu.p_object);
			printf(" -C O:0");
			break;
		case AMF_NULL:
			break;
		default:
			printf("<type %d>", p->p_type);
		}
	}
}


//based on libRTMP function
bool rtmp_supplement::SendResultNumber(RTMP *r, double txn, double ID)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = MSG_INVOKE;   // INVOKE
	packet.m_nInfoField1 = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = _amf_ptr->AMF_EncodeString(enc, pend, &av__result);
	enc = _amf_ptr->AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;
	enc = _amf_ptr->AMF_EncodeNumber(enc, pend, ID);

	packet.m_nBodySize = enc - packet.m_body;

	return _rtmp_ptr->RTMP_SendPacket(r, &packet, false);
}

//based on libRTMP function
bool rtmp_supplement::SendConnectResult(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = MSG_INVOKE;   // INVOKE
	packet.m_nInfoField1 = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = _amf_ptr->AMF_EncodeString(enc, pend, &av__result);
	enc = _amf_ptr->AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_OBJECT;

	STR2AVAL(av, "FMS/3,5,1,525");
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31.0);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	*enc++ = AMF_OBJECT;

	STR2AVAL(av, "status");
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_level, &av);
	STR2AVAL(av, "NetConnection.Connect.Success");
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_code, &av);
	STR2AVAL(av, "Connection succeeded.");
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_description, &av);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
#if 0
	STR2AVAL(av, "58656322c972d6cdf2d776167575045f8484ea888e31c086f7b5ffbd0baec55ce442c2fb");
	enc = AMF_EncodeNamedString(enc, pend, &av_secureToken, &av);
#endif
	STR2AVAL(p.p_name, "version");
	STR2AVAL(p.p_vu.p_aval, "3,5,1,525");
	p.p_type = AMF_STRING;
	obj.o_num = 1;
	obj.o_props = &p;
	op.p_type = AMF_OBJECT;
	STR2AVAL(op.p_name, "data");
	op.p_vu.p_object = obj;
	enc = _amf_ptr->AMFProp_Encode(&op, enc, pend);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	packet.m_nBodySize = enc - packet.m_body;

	return _rtmp_ptr->RTMP_SendPacket(r, &packet, false);
}

//based on libRTMP function
bool rtmp_supplement::SendChangeChunkSize(RTMP * r)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x02;	// control channel (invoke)
	packet.m_headerType = 0;
	packet.m_packetType = MSG_CHUNK_SIZE;	//change chunk size
	packet.m_nInfoField1 = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	packet.m_nBodySize = 4;

	_amf_ptr->AMF_EncodeInt32(packet.m_body, pend, r->m_outChunkSize);
	return _rtmp_ptr->RTMP_SendPacket(r, &packet, false);
}

//based on libRTMP function
bool rtmp_supplement::SendOnStatus(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;  

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = MSG_INVOKE;   // INVOKE
	packet.m_nInfoField1 = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = _amf_ptr->AMF_EncodeString(enc, pend, &av_onStatus);
	enc = _amf_ptr->AMF_EncodeNumber(enc, pend, 1.0);
	*enc++ = AMF_NULL;
	*enc++ = AMF_OBJECT;

	STR2AVAL(av, "status");
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_level, &av);
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
	STR2AVAL(av, "Started playing a.");
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_description, &av);
	STR2AVAL(av, "a");
	enc = _amf_ptr->AMF_EncodeNamedString(enc, pend, &av_details, &av);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_clientid, 1.0);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	packet.m_nBodySize = enc - packet.m_body;

	return _rtmp_ptr->RTMP_SendPacket(r, &packet, false);
}


//based on libRTMP function
bool rtmp_supplement::SendOnMetaData(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x03;     // control channel (notify)
	packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = MSG_METADATA;   // METADATA (notify)
	packet.m_nInfoField1 = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = _amf_ptr->AMF_EncodeString(enc, pend, &av_onMetaData);
	*enc++ = AMF_OBJECT;

	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_duration, 210.7);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_width, 320.0);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_height, 240.0);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_videodatarate, 0.0);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_framerate, 23.98);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_videocodecid, 2.0);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_audiosamplerate, 11025.0);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_audiosamplesize, 16.0);
	enc = _amf_ptr->AMF_EncodeNamedBoolean(enc, pend, &av_stereo, false);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_audiocodecid, 2.0);
	enc = _amf_ptr->AMF_EncodeNamedNumber(enc, pend, &av_filesize, 6457675.0);
	
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END; 

	packet.m_nBodySize = enc - packet.m_body;

	return _rtmp_ptr->RTMP_SendPacket(r, &packet, false);
}




//based on libRTMP function
int rtmp_supplement::WriteHeader(char **buf,		// target pointer, maybe preallocated
unsigned int len	// length of buffer if preallocated
)
{
	char flvHeader[] = { 'F', 'L', 'V', 0x01,
		0x05,			// video + audio, we finalize later if the value is different
		0x00, 0x00, 0x00, 0x09,
		0x00, 0x00, 0x00, 0x00	// first prevTagSize=0
	};

	unsigned int size = sizeof(flvHeader);

	if (size > len) {
		*buf = (char *) realloc(*buf, size);
		if (*buf == 0) {
			_log_ptr->Log(LOGERROR, "Couldn't reallocate memory!");
			return -1;		// fatal error
		}
	}
	memcpy(*buf, flvHeader, sizeof(flvHeader));
	return size;
}




//uint32_t nIgnoredFlvFrameCounter = 0;
//uint32_t nIgnoredFrameCounter = 0;
//based on libRTMP function
// Returns -3 if Play.Close/Stop, -2 if fatal error, -1 if no more media packets, 0 if ignorable error, >0 if there is a media packet
// int WriteStream(RTMP * rtmp, char **buf,	// target pointer, maybe preallocated
// unsigned int len,	// length of buffer if preallocated
// uint32_t * tsm,	// pointer to timestamp, will contain timestamp of last video packet returned
// bool bResume,	// resuming mode, will not write FLV header and compare metaHeader and first kexframe
// bool bLiveStream,	// live mode, will not report absolute timestamps
// uint32_t nResumeTS,	// resume keyframe timestamp
// char *metaHeader,	// pointer to meta header (if bResume == TRUE)
// uint32_t nMetaHeaderSize,	// length of meta header, if zero meta header check omitted (if bResume == TRUE)
// char *initialFrame,	// pointer to initial keyframe (no FLV header or tagSize, raw data) (if bResume == TRUE)
// uint8_t initialFrameType,	// initial frame type (audio or video)
// uint32_t nInitialFrameSize,	// length of initial frame in bytes, if zero initial frame check omitted (if bResume == TRUE)
// uint8_t * dataType	// whenever we get a video/audio packet we set an appropriate flag here, this will be later written to the FLV header
// )
// {
	// static bool bStopIgnoring = false;
	// static bool bFoundKeyframe = false;
	// static bool bFoundFlvKeyframe = false;


	// uint32_t prevTagSize = 0;
	// int rtnGetNextMediaPacket = 0, ret = -1;
	// RTMPPacket packet;
	// memset(&packet, 0x00, sizeof(packet));



	// rtnGetNextMediaPacket = ::RTMP_GetNextMediaPacket(rtmp, &packet);

	// while (rtnGetNextMediaPacket) {


		// char *packetBody = packet.m_body;
		// unsigned int nPacketLen = packet.m_nBodySize;

		//Return -3 if this was completed nicely with invoke message Play.Stop or Play.Complete
		// if (rtnGetNextMediaPacket == 2) {
			// ::Log(LOGDEBUG,
			// "Got Play.Complete or Play.Stop from server. Assuming stream is complete");
			// ret = -3;
			// break;
		// }

		//skip video info/command packets
		// if (packet.m_packetType == MSG_VIDEO_DATA &&
				// nPacketLen == 2 && ((*packetBody & 0xf0) == 0x50)) {
			// ret = 0;
			// break;
		// }

		// if (packet.m_packetType == MSG_VIDEO_DATA && nPacketLen <= 5) {
			// ::Log(LOGWARNING, "ignoring too small video packet: size: %d",
			// nPacketLen);
			// ret = 0;
			// break;
		// }
		// if (packet.m_packetType == 0x08 && nPacketLen <= 1) {
			// ::Log(LOGWARNING, "ignoring too small audio packet: size: %d",
			// nPacketLen);
			// ret = 0;
			// break;
		// }
// #ifdef _DEBUG
		// ::Log(LOGDEBUG, "type: %02X, size: %d, TS: %d ms, abs TS: %d",
		// packet.m_packetType, nPacketLen, packet.m_nTimeStamp,
		// packet.m_hasAbsTimestamp);
		// if (packet.m_packetType == MSG_VIDEO_DATA)
		// ::Log(LOGDEBUG, "frametype: %02X", (*packetBody & 0xf0));
// #endif

		//check the header if we get one
		// if (bResume && packet.m_nTimeStamp == 0) {
			// if (nMetaHeaderSize > 0 && packet.m_packetType == MSG_METADATA) {

				// AMFObject metaObj;
				// int nRes = ::AMF_Decode(&metaObj, packetBody, nPacketLen, false);
				// if (nRes >= 0) {
					// AVal metastring;
					// ::AMFProp_GetString(::AMF_GetProp(&metaObj, NULL, 0),
					// &metastring);

					// if (AVMATCH(&metastring, &av_onMetaData)) {
						//compare
						// if ((nMetaHeaderSize != nPacketLen) ||
								// (memcmp(metaHeader, packetBody, nMetaHeaderSize) !=
									// 0)) {
							// ret = -2;
						// }
					// }
					// ::AMF_Reset(&metaObj);
					// if (ret == -2)
					// break;
				// }
			// }

			//check first keyframe to make sure we got the right position in the stream!
			//(the first non ignored frame)
			// if (nInitialFrameSize > 0) {

				//video or audio data
				// if (packet.m_packetType == initialFrameType
						// && nInitialFrameSize == nPacketLen) {
					//we don't compare the sizes since the packet can contain several FLV packets, just make
					//sure the first frame is our keyframe (which we are going to rewrite)
					// if (memcmp(initialFrame, packetBody, nInitialFrameSize) ==
							// 0) {
						// ::Log(LOGDEBUG, "Checked keyframe successfully!");
						// bFoundKeyframe = true;
						// ret = 0;	// ignore it! (what about audio data after it? it is handled by ignoring all 0ms frames, see below)
						// break;
					// }
				// }

				//hande FLV streams, even though the server resends the keyframe as an extra video packet
				//it is also included in the first FLV stream chunk and we have to compare it and
				//filter it out !!
				
				// if (packet.m_packetType == MSG_FLV) {
					//basically we have to find the keyframe with the correct TS being nResumeTS
					// unsigned int pos = 0;
					// uint32_t ts = 0;

					// while (pos + 11 < nPacketLen) {
						// uint32_t dataSize = ::AMF_DecodeInt24(packetBody + pos + 1);	// size without header (11) and prevTagSize (4)
						// ts = ::AMF_DecodeInt24(packetBody + pos + 4);
						// ts |= (packetBody[pos + 7] << 24);

// #ifdef _DEBUG
						// ::Log(LOGDEBUG,
						// "keyframe search: FLV Packet: type %02X, dataSize: %d, timeStamp: %d ms",
						// packetBody[pos], dataSize, ts);
// #endif
						//ok, is it a keyframe!!!: well doesn't work for audio!
						// if (packetBody[pos /*6928, test 0 */ ] == initialFrameType ) {	//&& (packetBody[11]&0xf0) == 0x10							
							// if (ts == nResumeTS) {
								// ::Log(LOGDEBUG,
								// "Found keyframe with resume-keyframe timestamp!");
								// if (nInitialFrameSize != dataSize
										// || memcmp(initialFrame,
											// packetBody + pos + 11,
											// nInitialFrameSize) != 0) {
									// ::Log(LOGERROR,
									// "FLV Stream: Keyframe doesn't match!");
									// ret = -2;
									// break;
								// }
								// bFoundFlvKeyframe = true;

								//ok, skip this packet
								//check whether skipable:
								// if (pos + 11 + dataSize + 4 > nPacketLen) {
									// ::Log(LOGWARNING,
									// "Non skipable packet since it doesn't end with chunk, stream corrupt!");
									// ret = -2;
									// break;
								// }
								// packetBody += (pos + 11 + dataSize + 4);
								// nPacketLen -= (pos + 11 + dataSize + 4);

								// goto stopKeyframeSearch;

							// }
							// else if (nResumeTS < ts) {
								// goto stopKeyframeSearch;	// the timestamp ts will only increase with further packets, wait for seek
							// }
						// }
						// pos += (11 + dataSize + 4);
					// }
					// if (ts < nResumeTS) {
						// ::Log(LOGERROR,
						// "First packet does not contain keyframe, all timestamps are smaller than the keyframe timestamp, so probably the resume seek failed?");
					// }
// stopKeyframeSearch:
					// ;
					// if (!bFoundFlvKeyframe) {
						// ::Log(LOGERROR,
						// "Couldn't find the seeked keyframe in this chunk!");
						// ret = 0;
						// break;
					// }
				// }
			// }
		// }

		// if (bResume && packet.m_nTimeStamp > 0
				// && (bFoundFlvKeyframe || bFoundKeyframe)) {
			//another problem is that the server can actually change from 09/08 video/audio packets to an FLV stream
			//or vice versa and our keyframe check will prevent us from going along with the new stream if we resumed
			
			// in this case set the 'found keyframe' variables to true
			// We assume that if we found one keyframe somewhere and were already beyond TS > 0 we have written
			// data to the output which means we can accept all forthcoming data inclusing the change between 08/09 <-> FLV
			// packets
			// bFoundFlvKeyframe = true;
			// bFoundKeyframe = true;
		// }

		//skip till we find out keyframe (seeking might put us somewhere before it)
		// if (bResume && !bFoundKeyframe && packet.m_packetType != MSG_FLV) {
			// ::Log(LOGWARNING,
			// "Stream does not start with requested frame, ignoring data... ");
			// nIgnoredFrameCounter++;
			// if (nIgnoredFrameCounter > MAX_IGNORED_FRAMES)
			// ret = -2;		// fatal error, couldn't continue stream
			// else
			// ret = 0;
			// break;
		// }
		//ok, do the same for FLV streams
		// if (bResume && !bFoundFlvKeyframe && packet.m_packetType == MSG_FLV) {
			// ::Log(LOGWARNING,
			// "Stream does not start with requested FLV frame, ignoring data... ");
			// nIgnoredFlvFrameCounter++;
			// if (nIgnoredFlvFrameCounter > MAX_IGNORED_FRAMES)
			// ret = -2;
			// else
			// ret = 0;
			// break;
		// }

		// if bResume, we continue a stream, we have to ignore the 0ms frames since these are the first keyframes, we've got these
		// so don't mess around with multiple copies sent by the server to us! (if the keyframe is found at a later position
		// there is only one copy and it will be ignored by the preceding if clause)
		// if (!bStopIgnoring && bResume && packet.m_packetType != MSG_FLV) {			// exclude type 0x16 (FLV) since it can conatin several FLV packets
			// if (packet.m_nTimeStamp == 0) {
				// ret = 0;
				// break;
			// }
			// else{
				// bStopIgnoring = true;	// stop ignoring packets
			// }
		// }

		//calculate packet size and reallocate buffer if necessary
		// unsigned int size = nPacketLen
		// +
		// ((packet.m_packetType == MSG_AUDIO_DATA || packet.m_packetType == MSG_VIDEO_DATA
		// || packet.m_packetType ==
		// MSG_METADATA) ? 11 : 0) + (packet.m_packetType != MSG_FLV ? 4 : 0);

		// if (size + 4 > len) {			// the extra 4 is for the case of an FLV stream without a last prevTagSize (we need extra 4 bytes to append it)
			// *buf = (char *) realloc(*buf, size + 4);
			// if (*buf == 0) {
				// ::Log(LOGERROR, "Couldn't reallocate memory!");
				// ret = -1;		// fatal error
				// break;
			// }
		// }
		// char *ptr = *buf, *pend = ptr+size+4;

		// uint32_t nTimeStamp = 0;	// use to return timestamp of last processed packet

		//audio (0x08), video (0x09) or metadata (0x12) packets :
		//construct 11 byte header then add rtmp packet's data
		// if (packet.m_packetType == MSG_AUDIO_DATA || packet.m_packetType == MSG_VIDEO_DATA
				// || packet.m_packetType == MSG_METADATA) {
			//set data type
			// *dataType |=
			// (((packet.m_packetType == MSG_AUDIO_DATA) << 2) | (packet.m_packetType ==
			// MSG_VIDEO_DATA));

			// nTimeStamp = nResumeTS + packet.m_nTimeStamp;
			// prevTagSize = 11 + nPacketLen;

			// *ptr = packet.m_packetType;
			// ptr++;
			// ptr = ::AMF_EncodeInt24(ptr, pend, nPacketLen);

			// /*if(packet.m_packetType == MSG_VIDEO_DATA) { // video

			//H264 fix:
			// if((packetBody[0] & 0x0f) == 7) { // CodecId = H264
			// uint8_t packetType = *(packetBody+1);

			// uint32_t ts = AMF_DecodeInt24(packetBody+2); // composition time
			// int32_t cts = (ts+0xff800000)^0xff800000;
			// ::Log(LOGDEBUG, "cts  : %d\n", cts);

			// nTimeStamp -= cts;
			//get rid of the composition time
			// CRTMP::EncodeInt24(packetBody+2, 0);
			// }
			// ::Log(LOGDEBUG, "VIDEO: nTimeStamp: 0x%08X (%d)\n", nTimeStamp, nTimeStamp);
			// } */

			// ptr = ::AMF_EncodeInt24(ptr, pend, nTimeStamp);
			// *ptr = (char) ((nTimeStamp & 0xFF000000) >> 24);
			// ptr++;

			//stream id
			// ptr = ::AMF_EncodeInt24(ptr, pend, 0);
		// }

		// memcpy(ptr, packetBody, nPacketLen);
		// unsigned int len = nPacketLen;

		//correct tagSize and obtain timestamp if we have an FLV stream
		// if (packet.m_packetType == MSG_FLV) {
			// unsigned int pos = 0;

			// while (pos + 11 < nPacketLen) {
				// uint32_t dataSize = ::AMF_DecodeInt24(packetBody + pos + 1);	// size without header (11) and without prevTagSize (4)
				// nTimeStamp = ::AMF_DecodeInt24(packetBody + pos + 4);
				// nTimeStamp |= (packetBody[pos + 7] << 24);

				// /*
			// CRTMP::EncodeInt24(ptr+pos+4, nTimeStamp);
			// ptr[pos+7] = (nTimeStamp>>24)&0xff;// */

				//set data type
				// *dataType |=
				// (((*(packetBody + pos) ==
				// MSG_AUDIO_DATA) << 2) | (*(packetBody + pos) == MSG_VIDEO_DATA));

				// if (pos + 11 + dataSize + 4 > nPacketLen) {
					// if (pos + 11 + dataSize > nPacketLen) {
						// ::Log(LOGERROR,
						// "Wrong data size (%lu), stream corrupted, aborting!",
						// dataSize);
						// ret = -2;
						// break;
					// }
					// ::Log(LOGWARNING, "No tagSize found, appending!");

					//we have to append a last tagSize!
					// prevTagSize = dataSize + 11;
					// ::AMF_EncodeInt32(ptr + pos + 11 + dataSize, pend, prevTagSize);
					// size += 4;
					// len += 4;
				// }
				// else{
					// prevTagSize =
					// ::AMF_DecodeInt32(packetBody + pos + 11 + dataSize);

// #ifdef _DEBUG
					// ::Log(LOGDEBUG,
					// "FLV Packet: type %02X, dataSize: %lu, tagSize: %lu, timeStamp: %lu ms",
					// (unsigned char) packetBody[pos], dataSize, prevTagSize,
					// nTimeStamp);
// #endif

					// if (prevTagSize != (dataSize + 11)) {
// #ifdef _DEBUG
						// ::Log(LOGWARNING,
						// "Tag and data size are not consitent, writing tag size according to dataSize+11: %d",
						// dataSize + 11);
// #endif

						// prevTagSize = dataSize + 11;
						// ::AMF_EncodeInt32(ptr + pos + 11 + dataSize, pend, prevTagSize);
					// }
				// }

				// pos += prevTagSize + 4;	//(11+dataSize+4);
			// }
		// }
		// ptr += len;

		// if (packet.m_packetType != MSG_FLV) {			// FLV tag packets contain their own prevTagSize
			// ::AMF_EncodeInt32(ptr, pend, prevTagSize);
			//ptr += 4;
		// }

		//In non-live this nTimeStamp can contain an absolute TS.
		//Update ext timestamp with this absolute offset in non-live mode otherwise report the relative one
		//RTMP_LogPrintf("\nDEBUG: type: %02X, size: %d, pktTS: %dms, TS: %dms, bLiveStream: %d", packet.m_packetType, nPacketLen, packet.m_nTimeStamp, nTimeStamp, bLiveStream);
		// if (tsm)
		// *tsm = bLiveStream ? packet.m_nTimeStamp : nTimeStamp;


		// ret = size;
		// break;
	// }

	// if (rtnGetNextMediaPacket)
	// ::RTMPPacket_Free(&packet);
	// return ret;			// no more media packets
// }


