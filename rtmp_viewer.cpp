#include "rtmp_viewer.h"
#include "rtmp_server.h"
#include "network.h"
#include "logger.h"
#include "pk_mgr.h"
#include <sstream>

#include "rtmp_supplement.h"

//based on libRTMP function
// Returns 0 for OK/Failed/error, 1 for 'Stop or Complete'
int rtmp_viewer::ServeInvoke(STREAMING_SERVER *server, RTMP * r, RTMPPacket *packet, unsigned int offset)
{
	const char *body;
	unsigned int nBodySize;
	int ret = 0, nRes;
    int i = 0;
    map<unsigned char, int>::const_iterator iter;

	body = packet->m_body + offset;
	nBodySize = packet->m_nBodySize - offset;

	if (body[0] != 0x02) {		// make sure it is a string method name we start with
		_log_ptr->Log(LOGWARNING, "%s, Sanity failed. no string method in invoke packet",
		__FUNCTION__);
		return 0;
	}

	AMFObject obj;
	nRes = _amf_ptr->AMF_Decode(&obj, body, nBodySize, false);
	if (nRes < 0) {
		_log_ptr->Log(LOGERROR, "%s, error decoding invoke packet", __FUNCTION__);
		return 0;
	}

	_amf_ptr->AMF_Dump(&obj);
	AVal method;
	_amf_ptr->AMFProp_GetString(_amf_ptr->AMF_GetProp(&obj, NULL, 0), &method);
	double txn = _amf_ptr->AMFProp_GetNumber(_amf_ptr->AMF_GetProp(&obj, NULL, 1));
	_log_ptr->Log(LOGDEBUG, "%s, client invoking <%s>", __FUNCTION__, method.av_val);

	if (AVMATCH(&method, &av_connect)) {
		//cout << "av_connect" << endl;
		AMFObject cobj;
		AVal pname, pval;
		int i;

		server->connect = packet->m_body;
		packet->m_body = NULL;

		_amf_ptr->AMFProp_GetObject(_amf_ptr->AMF_GetProp(&obj, NULL, 2), &cobj);
		for (i=0; i<cobj.o_num; i++) {
			pname = cobj.o_props[i].p_name;
			pval.av_val = NULL;
			pval.av_len = 0;
			if (cobj.o_props[i].p_type == AMF_STRING)
			pval = cobj.o_props[i].p_vu.p_aval;

			if (AVMATCH(&pname, &av_app)) {
				r->Link[0].app = pval;
				pval.av_val = NULL;
			} else if (AVMATCH(&pname, &av_flashVer)) {
				r->Link[0].flashVer = pval;
				pval.av_val = NULL;
			} else if (AVMATCH(&pname, &av_swfUrl)) {
				r->Link[0].swfUrl = pval;
				pval.av_val = NULL;
			} else if (AVMATCH(&pname, &av_tcUrl)) {
				r->Link[0].tcUrl = pval;
				pval.av_val = NULL;
			} else if (AVMATCH(&pname, &av_pageUrl)) {
				r->Link[0].pageUrl = pval;
				pval.av_val = NULL;
			} else if (AVMATCH(&pname, &av_audioCodecs)) {
				r->m_fAudioCodecs = cobj.o_props[i].p_vu.p_number;
			} else if (AVMATCH(&pname, &av_videoCodecs)) {
				r->m_fVideoCodecs = cobj.o_props[i].p_vu.p_number;
			} else if (AVMATCH(&pname, &av_objectEncoding)) {
				r->m_fEncoding = cobj.o_props[i].p_vu.p_number;
			}
		}
		/* Still have more parameters? Copy them */
		if (obj.o_num > 3) {
			int i = obj.o_num - 3;
			r->Link[0].extras.o_num = i;
			r->Link[0].extras.o_props = (AMFObjectProperty *)malloc(i*sizeof(AMFObjectProperty));
			memcpy(r->Link[0].extras.o_props, obj.o_props+3, i*sizeof(AMFObjectProperty));
			obj.o_num = 3;
		}
		_rtmp_supplement_ptr->SendConnectResult(r, txn);
		//cout << "SendConnectResult fin!" << endl;
	}
	else if (AVMATCH(&method, &av_createStream)) {
		//cout << "av_createStream" << endl;
		server->streamID = r->Link[0].m_stream_id;
		_rtmp_supplement_ptr->SendResultNumber(r, txn, server->streamID);
	}
	else if (AVMATCH(&method, &av_getStreamLength)) {
		//cout << "av_getStreamLength" << endl;
		_rtmp_supplement_ptr->SendResultNumber(r, txn, 10.0);
	}
	else if (AVMATCH(&method, &av_play)) {
		//cout << "av_play" << endl;
		RTMPPacket pc = {0};
		_amf_ptr->AMFProp_GetString(_amf_ptr->AMF_GetProp(&obj, NULL, 3), &r->Link[0].playpath);
		r->Link[0].seekTime = _amf_ptr->AMFProp_GetNumber(_amf_ptr->AMF_GetProp(&obj, NULL, 4));
		if (obj.o_num > 5)
		r->Link[0].length = _amf_ptr->AMFProp_GetNumber(_amf_ptr->AMF_GetProp(&obj, NULL, 5));
		if (r->Link[0].tcUrl.av_len) {
			printf("\nflvstreamer -r \"%s\"", r->Link[0].tcUrl.av_val);
			if (r->Link[0].app.av_val)
				printf(" -a \"%s\"", r->Link[0].app.av_val);
			if (r->Link[0].flashVer.av_val)
				printf(" -f \"%s\"", r->Link[0].flashVer.av_val);
			if (r->Link[0].swfUrl.av_val){
				printf(" -W \"%s\"", r->Link[0].swfUrl.av_val);
				printf(" -t \"%s\"", r->Link[0].tcUrl.av_val);
			}
			if (r->Link[0].pageUrl.av_val)
				printf(" -p \"%s\"", r->Link[0].pageUrl.av_val);
			if (r->Link[0].auth.av_val)
				printf(" -u \"%s\"", r->Link[0].auth.av_val);
			if (r->Link[0].extras.o_num) {
				_rtmp_supplement_ptr->dumpAMF(&r->Link[0].extras);
				_amf_ptr->AMF_Reset(&r->Link[0].extras);
			}
			printf(" -y \"%.*s\" -o output.flv\n\n",
			r->Link[0].playpath.av_len, r->Link[0].playpath.av_val);
			fflush(stdout);
		}

        _stream_pk_id = 0;
        while(cRtmp.Link[0].playpath.av_val[i] >= 48 && cRtmp.Link[0].playpath.av_val[i] <= 57) {
            _stream_pk_id = _stream_pk_id * 10 + (cRtmp.Link[0].playpath.av_val[i] - 48);
            i ++;
        }

        //cout << "stream id = " << _stream_pk_id << endl;

		pc.m_body = server->connect;
		server->connect = NULL;
		_rtmp_ptr->RTMPPacket_Free(&pc);
		
		//r->m_inChunkSize = 1024;  //manual change

		//_rtmp_client->inchuck_size_get(&(r->m_outChunkSize));
		//r->m_outChunkSize = crtmp.m_inChunkSize;
        iter = _pk_mgr_ptr->map_rtmp_chunk_size.find(get_stream_pk_id());
	    if(iter != _pk_mgr_ptr->map_rtmp_chunk_size.end() && iter->second > 0){   
		    r->m_outChunkSize = iter->second;
	    }
        else{
		    r->m_outChunkSize = 1024; //RTMP_DEFAULT_CHUNKSIZE
        }
        printf("set rtmp chunk size %d\n", r->m_outChunkSize);

		_rtmp_supplement_ptr->SendChangeChunkSize(r);

		_rtmp_ptr->RTMP_SendCtrl(r, 0, r->Link[0].m_stream_id, 0); //Stream Begin
		
		_rtmp_supplement_ptr->SendOnStatus(r);
		//SendOnMetaData(r);

		server->state = STREAMING_IN_PROGRESS;
		_log_ptr->Log(LOGDEBUG, "Server State: INVOKEMETADATA complete");

		//_rtmp_server->add_seed(r->m_socket,_queue_out_data_ptr);
		_pk_mgr_ptr->rtmp_sock_set(r->m_socket);
		_pk_mgr_ptr->add_stream(r->m_socket, (stream *)this, STRM_TYPE_MEDIA);

		_net_ptr->epoll_control(r->m_socket, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		
		_log_ptr->Log(LOGDEBUG, "!!!Server Invoke's epoll fd=>%d", r->m_socket);

		ret = 1;
	}
	_amf_ptr->AMF_Reset(&obj);
	return ret;
}

//based on libRTMP function
int rtmp_viewer::ServePacket(STREAMING_SERVER *server, RTMP *r, RTMPPacket *packet)
{
	int ret = 0;

	_log_ptr->Log(LOGDEBUG, "%s, received packet type %02X, size %lu bytes", __FUNCTION__,
	packet->m_packetType, packet->m_nBodySize);

	switch (packet->m_packetType) {
	case MSG_CHUNK_SIZE:
		// chunk size
		//      HandleChangeChunkSize(r, packet);
		break;

	case MSG_BYTES_READ:
		// bytes read report
		break;

	case MSG_CTRL:
		// ctrl
		_rtmp_ptr->HandleCtrl(r, packet);
		break;

	case MSG_SERVER_BW:
		// server bw
		//      HandleServerBW(r, packet);
		break;

	case MSG_CLIENT_BW:
		// client bw
		//     HandleClientBW(r, packet);
		break;

	case MSG_AUDIO_DATA:
		// audio data
		//Log(LOGDEBUG, "%s, received: audio %lu bytes", __FUNCTION__, packet.m_nBodySize);
		break;

	case MSG_VIDEO_DATA:
		// video data
		//Log(LOGDEBUG, "%s, received: video %lu bytes", __FUNCTION__, packet.m_nBodySize);
		break;

	case MSG_FLEX_STREAM_SEND:			// flex stream send
		break;

	case MSG_FLEX_SHARED_OBJECT:			// flex shared object
		break;

	case MSG_FLEX_MESSAGE:			// flex message
		{
			_log_ptr->Log(LOGDEBUG, "%s, flex message, size %lu bytes, not fully supported",
			__FUNCTION__, packet->m_nBodySize);
			//LogHex(packet.m_body, packet.m_nBodySize);

			// some DEBUG code
			/*RTMP_LIB_AMFObject obj;
			int nRes = obj.Decode(packet.m_body+1, packet.m_nBodySize-1);
			if(nRes < 0) {
			Log(LOGERROR, "%s, error decoding AMF3 packet", __FUNCTION__);
			//return;
			}

			obj.Dump(); */

			ServeInvoke(server, r, packet, 1);
			break;
		}
	case MSG_METADATA:
		// metadata (notify)
		break;

	case MSG_SHARED_OBJECT:
		/* shared object */
		break;

	case MSG_INVOKE:
		// invoke
		_log_ptr->Log(LOGDEBUG, "%s, received: invoke %lu bytes", __FUNCTION__,
		packet->m_nBodySize);
		//LogHex(packet.m_body, packet.m_nBodySize);

		if (ServeInvoke(server, r, packet, 0))
		//RTMP_Close(r);
		break;

	case MSG_FLV:
		/* flv */
		break;
	default:
		_log_ptr->Log(LOGDEBUG, "%s, unknown packet type received: 0x%02x", __FUNCTION__,
		packet->m_packetType);
#ifdef _DEBUG
		_log_ptr->LogHex(LOGDEBUG, packet->m_body, packet->m_nBodySize);
#endif
	}
	return ret;
}


rtmp_viewer::rtmp_viewer(int stream_id, network *net_ptr, logger *log_ptr, rtmp_server *rtmp_server, amf *amf_ptr, rtmp *rtmp_ptr, rtmp_supplement *rtmp_supplement_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_rtmp_server = rtmp_server;
	_amf_ptr = amf_ptr;
	_rtmp_ptr = rtmp_ptr;
	_rtmp_supplement_ptr = rtmp_supplement_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	fd_list_ptr = fd_list;
	control_pkt = 0;
    _stream_pk_id = 0;


	file_ptr = fopen(".//123.flv","wb");
	
	_queue_out_data_ptr = new std::queue<struct chunk_t *>;
	memset(&_send_ctl_info, 0x00, sizeof(_send_ctl_info));
	struct sockaddr_in sin_addr;
	_stream_id = stream_id;
	server = (STREAMING_SERVER *) calloc(1, sizeof(STREAMING_SERVER));
	server->state = HANDSHAKING_1;
		
	_sock_tcp= socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
	if (_sock_tcp < 0)
		throw "Can't create socket: _sock_tcp";	
	sin_addr.sin_family = AF_INET;
	sin_addr.sin_addr.s_addr = INADDR_ANY;

}


rtmp_viewer::~rtmp_viewer()
{
	_rtmp_ptr->RTMP_Close(&cRtmp);
	delete server;
	delete _queue_out_data_ptr;
}

//based on libRTMP function
bool rtmp_viewer::SHandShake1(RTMP * r)
{
	int i;
	uint32_t uptime;
	serversig = serverbuf+1;

	if (_rtmp_ptr->ReadN(r, serverbuf, 1) != 1)	// 0x03 or 0x06
	return false;

	_log_ptr->Log(LOGDEBUG, "%s: Type Request  : %02X", __FUNCTION__, serverbuf[0]);

	if (serverbuf[0] != 3) {
		_log_ptr->Log(LOGERROR, "%s: Type unknown: client sent %02X",
		__FUNCTION__, serverbuf[0]);
		return false;
	}

	uptime = htonl(_rtmp_ptr->RTMP_GetTime());
	memcpy(serversig, &uptime, 4);

	memset(&serversig[4], 0, 4);
#ifdef _DEBUG
	for (i = 8; i < RTMP_SIG_SIZE; i++)
	serversig[i] = 0xff;
#else
	for (i = 8; i < RTMP_SIG_SIZE; i++)
	serversig[i] = (char) (rand() % 256);
#endif

	if (!_rtmp_ptr->WriteN(r, serverbuf, RTMP_SIG_SIZE + 1))
	return false;

	if (_rtmp_ptr->ReadN(r, clientsig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
	return false;

	// decode client response

	memcpy(&uptime, clientsig, 4);
	uptime = ntohl(uptime);

	//printf("%s: Client Uptime : %d\n", __FUNCTION__, uptime);
	//printf("%s: Player Version: %d.%d.%d.%d\n", __FUNCTION__, clientsig[4],clientsig[5], clientsig[6], clientsig[7]);

	_log_ptr->Log(LOGDEBUG, "%s: Client Uptime : %d", __FUNCTION__, uptime);
	_log_ptr->Log(LOGDEBUG, "%s: Player Version: %d.%d.%d.%d", __FUNCTION__, clientsig[4],
	clientsig[5], clientsig[6], clientsig[7]);

	
	return true;
}

//based on libRTMP function
bool rtmp_viewer::SHandShake2(RTMP * r)
{
	// 2nd part of handshake
	if (!_rtmp_ptr->WriteN(r, clientsig, RTMP_SIG_SIZE))
	return false;

	if (_rtmp_ptr->ReadN(r, clientsig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
	return false;

	bool bMatch = (memcmp(serversig, clientsig, RTMP_SIG_SIZE) == 0);
	if (!bMatch) {
		cout << __FUNCTION__ << ", client signature does not match!" << endl;
		_log_ptr->Log(LOGWARNING, "%s, client signature does not match!", __FUNCTION__);
	}
	return true;
}

//based on libRTMP function
int rtmp_viewer::handle_pkt_in(int sock)
{	
	if(server->state == HANDSHAKING_1) {
		//printf("rtmp_viewer:HANDSHAKING_1\n");
		server->socket = sock;
		//cout << "sock = " << sock << endl;
		
		memset(&packet, 0x00, sizeof(packet));
		memset(&cRtmp, 0x00, sizeof(cRtmp));
		
		_rtmp_ptr->RTMP_Init(&cRtmp);
		RTMP_LNK stream;
		memset(&stream, 0x00, sizeof(RTMP_LNK));
		stream.m_stream_id = _stream_id;
		cRtmp.Link.push_back(stream);
		
		cRtmp.m_socket = sock;
	if (!SHandShake1(&cRtmp)) {
			_log_ptr->Log(LOGERROR, "Handshake1 with client failed");
			//cout << "Handshake1 with client failed" << endl;
			//goto cleanup;
			return RET_ERROR;
		}		
		server->state = HANDSHAKING_2;
		_log_ptr->Log(LOGDEBUG, "Server State: HANDSHAKING_1 complete");
		//printf("rtmp_viewer:handle_pkt_in fin!1\n");

		return RET_OK;
	}
	if(server->state == HANDSHAKING_2) {
		//printf("rtmp_viewer:HANDSHAKING_2\n");
		if (!SHandShake2(&cRtmp)) {
			_log_ptr->Log(LOGERROR, "Handshake2 with client failed");
			cout << "Handshake1 with client failed" << endl;
			//goto cleanup;
			return RET_ERROR;
		}		
		server->state = INVOKEMETADATA;
		_log_ptr->Log(LOGDEBUG, "Server State: HANDSHAKING_2 complete");
		handle_pkt_in(sock);
		
		return RET_OK;
	}
	if(server->state == INVOKEMETADATA) {
		//printf("rtmp_viewer:INVOKEMETADATA\n");
		_log_ptr->Log(LOGDEBUG, "INVOKEMETADATA");	
		while (_rtmp_ptr->RTMP_IsConnected(&cRtmp) && _rtmp_ptr->RTMP_ReadPacket(&cRtmp, &packet)) {
			if (!RTMPPacket_IsReady(&packet))
				continue;
			ServePacket(server, &cRtmp, &packet);
			_rtmp_ptr->RTMPPacket_Free(&packet);
		}

		return RET_OK;
	}
	if(server->state == STREAMING_IN_PROGRESS) {
		//printf("rtmp_viewer:STREAMING_IN_PROGRESS\n");
		_log_ptr->Log(LOGDEBUG, "STREAMING_IN_PROGRESS");
		if (!_rtmp_ptr->RTMP_IsConnected(&cRtmp) || !_rtmp_ptr->RTMP_ReadPacket(&cRtmp, &packet)) {
			_log_ptr->Log(LOGERROR, "RTMP not Connected");
            //printf("RTMP not Connected\n");
			return RET_ERROR;
		}
		ServePacket(server, &cRtmp, &packet);
		_rtmp_ptr->RTMPPacket_Free(&packet);
	}

	return RET_OK;
	/*
	cleanup:
	LogPrintf("Closing connection... ");
	RTMP_Close(&cRtmp);
	// Should probably be done by RTMP_Close() ...
	cRtmp.Link.playpath.av_val = NULL;
	cRtmp.Link.tcUrl.av_val = NULL;
	cRtmp.Link.swfUrl.av_val = NULL;
	cRtmp.Link.pageUrl.av_val = NULL;
	cRtmp.Link.app.av_val = NULL;
	cRtmp.Link.auth.av_val = NULL;
	cRtmp.Link.flashVer.av_val = NULL;
	LogPrintf("done!\n\n");
*/
}


//based on libRTMP function
int rtmp_viewer::handle_pkt_out(int sock) {
	int send_rt_val; //send return value
	int channel_num;
	int basic_header_type;
	int ms_type_id = 0;
	int mss_id = 0;
	//printf("%s \n", __FUNCTION__);

	if (_send_ctl_info.ctl_state == READY) {
		size_t send_size;

		struct chunk_rtmp_t *chunk_ptr;
		if (!_queue_out_data_ptr->size()) {
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);	
			return RET_OK;
		}
		//Log(LOGDEBUG, "!!!handle_pkt_out: sending packet sockfd=>%d ", sock);
	
		chunk_ptr = (struct chunk_rtmp_t *)_queue_out_data_ptr->front();

//for debug
//		fwrite(chunk_ptr->buf,1,chunk_ptr->header.length,file_ptr);

		_queue_out_data_ptr->pop();
		send_size = chunk_ptr->header.length;




		basic_header_type = (chunk_ptr->buf[0] & 0xf0) >> 6;
		channel_num = (chunk_ptr->buf[0] & 0x3f);
		if (channel_num == 0) {
			channel_num = (unsigned) chunk_ptr->buf[1] + 64;
		}
		else if (channel_num == 1) {
			channel_num = (((unsigned) chunk_ptr->buf[2]) << 8) + (unsigned) chunk_ptr->buf[1] + 64;
		}		

		//Log(LOGDEBUG, "%s: channel = %d, header type = %d", __FUNCTION__, chunk_ptr->buf[0] & 0x3f, (chunk_ptr->buf[0] & 0xf0) >> 6);
		//LogHexString(LOGDEBUG2, (char *)&chunk_ptr->buf[0], send_size);

		//if (_channel_map.find(channel_num) == _channel_map.end() && !(channel_num == 2 || channel_num == 3)) { //make sure that this channel chunk will be sent from basic header type 0
		if (_channel_map.find(channel_num) == _channel_map.end() && !(channel_num == 2)) { //make sure that this channel chunk will be sent from basic header type 0
			if (basic_header_type != RTMP_HEADER_TYPE_0) {
				//printf("basic_header_type != RTMP_HEADER_TYPE_0\n");
				return RET_OK;
			}
			else {
				_channel_map[channel_num] = true;
			}
		}
		//printf("basic_header_type = %d, channel_num = %d\n", basic_header_type, channel_num);

		_send_ctl_info.offset = 0;
		_send_ctl_info.total_len = send_size;
		_send_ctl_info.expect_len = send_size;
		_send_ctl_info.buffer = (char *)chunk_ptr->buf;
		_send_ctl_info.rtmp_chunk = chunk_ptr;
		_send_ctl_info.serial_num = chunk_ptr->header.sequence_number;

		/*if(control_pkt < 1000){
			control_pkt ++;
			send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);
			//printf("pkt num:%d\n", _send_ctl_info.serial_num);
		}
		else{
			control_pkt = 0;
			printf("block pkt num:%d\n", _send_ctl_info.serial_num);
			send_rt_val = RET_OK;
		}*/

		/*if(basic_header_type == 0){
			ms_type_id =  chunk_ptr->buf[8];
			mss_id = (( chunk_ptr->buf[12]) << 24) + (( chunk_ptr->buf[11]) << 16) + (( chunk_ptr->buf[10]) << 8) + chunk_ptr->buf[9];
			printf("pkt num:%d basic_header_type:%d channel_num:%d ms_type_id:%d mss_id:%d\n", _send_ctl_info.serial_num,basic_header_type,channel_num,ms_type_id,mss_id);
		}
		else if(basic_header_type == 1){
			ms_type_id =  chunk_ptr->buf[8];
			printf("pkt num:%d basic_header_type:%d channel_num:%d ms_type_id:%d\n", _send_ctl_info.serial_num,basic_header_type,channel_num,ms_type_id);
		}
		else{
			//printf("pkt num:%d basic_header_type:%d channel_num:%d\n", _send_ctl_info.serial_num,basic_header_type,channel_num);
		}*/
		
        _log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "sent pkt", _send_ctl_info.serial_num, _stream_id);
		send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);
		//Log(LOGDEBUG, "%s: send_rt = %d, expect_len = %d", __FUNCTION__, send_rt_val,_send_ctl_info.expect_len);
		switch (send_rt_val) {
			case RET_SOCK_ERROR:
				//delete viewer will be done by rtmp_server handle_sock_error
				printf("%s, socket error\n", __FUNCTION__);
				return RET_SOCK_ERROR;
			default:
				return RET_OK;
		}
		
	} else { //_send_ctl_info._send_ctl_state is RUNNING
		//Log(LOGDEBUG, "%s, in RUNNING state", __FUNCTION__);
        _log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "sent pkt", _send_ctl_info.serial_num, _stream_id);
		send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);
		switch (send_rt_val) {
			case RET_WRONG_SER_NUM:
				_log_ptr->Log(LOGDEBUG, "%s, serial number changed, queue rewrite?", __FUNCTION__);
				printf("%s, serial number changed, queue rewrite?\n", __FUNCTION__);
			case RET_SOCK_ERROR:				
				//delete viewer will be done by rtmp_server handle_sock_error
				printf("%s, socket error\n", __FUNCTION__);
				return RET_SOCK_ERROR;
			default:
				return RET_OK;
		}
	}
}

void rtmp_viewer::handle_pkt_error(int sock)
{

}

void rtmp_viewer::handle_sock_error(int sock, basic_class *bcptr)
{

}

void rtmp_viewer::handle_job_realtime()
{

}


void rtmp_viewer::handle_job_timer()
{

}

void rtmp_viewer::data_close(int cfd, const char *reason)
{
	list<int>::iterator fd_iter;

	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "rtmp_viewer", reason);
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);	

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
}

void rtmp_viewer::set_client_sockaddr(struct sockaddr_in *cin)
{
	if (cin)
	memcpy(&_cin_tcp, cin, sizeof(struct sockaddr_in));
}

void rtmp_viewer::add_chunk(struct chunk_t *chunk)
{
	_queue_out_data_ptr->push(chunk);
	
}

unsigned char rtmp_viewer::get_stream_pk_id()
{
    return _stream_pk_id;
}