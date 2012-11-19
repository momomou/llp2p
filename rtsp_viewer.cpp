#include "rtsp_viewer.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "stream_udp.h"
#include "stream_handler_udp.h"
#include <sstream>

rtsp_viewer::rtsp_viewer(network *net_ptr, logger *log_ptr, pk_mgr *pk_mgr_ptr, list<int> *fd_list)
{
	
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	fd_list_ptr = fd_list;

}


rtsp_viewer::~rtsp_viewer()
{

	if (_strm_hand_audio)
		delete _strm_hand_audio;
			
	if (_strm_hand_video)
		delete _strm_hand_video;
	
	if (_strm_audio)
		delete _strm_audio;
	
	if (_strm_video)
		delete _strm_video;

}

void rtsp_viewer::init()
{
	struct sockaddr_in sin_audio, sin_video;
	int optval = 1;

	memset((struct sockaddr_in*)&sin_audio, 0x0, sizeof(struct sockaddr_in));
	memset((struct sockaddr_in*)&sin_video, 0x0, sizeof(struct sockaddr_in));
	
	_cln_state = STATE_RTSP_INIT;
	_html_size = HTML_SIZE;
	
	_srv_port_udp_audio = RTSP_PORT_UDP_AUDIO;
	_sock_udp_audio= socket(AF_INET, SOCK_DGRAM, 0);
	if (_sock_udp_audio < 0)
		throw "Can't create socket: _srv_port_udp_audio";	
	sin_audio.sin_family = AF_INET;
	sin_audio.sin_addr.s_addr = INADDR_ANY;
	sin_audio.sin_port = htons(_srv_port_udp_audio);
	if (bind(_sock_udp_audio, (struct sockaddr *)&sin_audio, sizeof(struct sockaddr_in)) == -1) {
		throw "Can't bind socket: _sock_udp_audio";
	} else {
		printf("Server bind at UDP AUDIO port: %d\n", _srv_port_udp_audio);
	}

	if (setsockopt(_sock_udp_audio, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval , sizeof(optval)) == -1) 
		throw "Can't setsockopt socket: _sock_udp_audio";
	
	
	_srv_port_udp_video = RTSP_PORT_UDP_VIDEO;
	_sock_udp_video = socket(AF_INET, SOCK_DGRAM, 0);
	if (_sock_udp_video < 0)
		throw "Can't create socket: _srv_port_udp_video";
	sin_video.sin_family = AF_INET;
	sin_video.sin_addr.s_addr = INADDR_ANY;
	sin_video.sin_port = htons(_srv_port_udp_video);
	if (bind(_sock_udp_video, (struct sockaddr *)&sin_video, sizeof(struct sockaddr_in)) == -1) {
		throw "Can't bind socket: _sock_udp_video";
	} else {
		printf("Server bind at UDP VIDEO port: %d\n", _srv_port_udp_video);
	}

	if (setsockopt(_sock_udp_video, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval , sizeof(optval)) == -1) 
		throw "Can't setsockopt socket: _sock_udp_video";
		

	_strm_hand_audio= new stream_handler_udp();
	if (!_strm_hand_audio)
		throw "Can't new stream_handler_udp";
	_strm_hand_video= new stream_handler_udp();
	if (!_strm_hand_video)
		throw "Can't new stream_handler_udp";
	_strm_audio = new stream_udp(_net_ptr, _log_ptr);
	if (!_strm_audio)
		throw "Can't new stream_handler_udp";
	_strm_video = new stream_udp(_net_ptr, _log_ptr);
	if (!_strm_video)
		throw "Can't new stream_handler_udp";

	_net_ptr->set_fd_bcptr_map(_sock_udp_audio, dynamic_cast<basic_class *> (_strm_hand_audio));
	_net_ptr->set_fd_bcptr_map(_sock_udp_video, dynamic_cast<basic_class *> (_strm_hand_video));

}

int rtsp_viewer::handle_pkt_in(int sock)
{	
	int recv_byte;
	unsigned long cseq = 0;
	char *find_ptr = NULL;
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	struct chunk_t *chunk_ptr = NULL;
	char html_buf[8192];
	const char *buf_ptr, *port_ptr;
	stringstream ss_tmp;
	string str_tmp;

	memset( html_buf, 0x0, 8192);
	//DBG_PRINTF("here\n");

	if (_cln_state == STATE_RTSP_INIT) {
		// Switch to blocking mode
		_net_ptr->set_blocking(sock);

		recv_byte = _net_ptr->recv(sock, html_buf, sizeof(html_buf), 0);

		if(recv_byte <= 0) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}

		// OPTIONS
		find_ptr = strstr(html_buf, "OPTIONS");
		if (!find_ptr) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "error occured");
			return RET_SOCK_ERROR;
		}

		find_ptr = strstr(html_buf, "CSeq");
		if (!find_ptr) {
			printf("error occured in parsing Session ID cmd\n");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}
		else{
			sscanf(find_ptr, "CSeq: %lu", &cseq);
			printf("CSeq of OPTIONS is %lu\n", cseq);
		}
 
		ss_tmp << "RTSP/1.0 200 OK\r\n"
				<< "Server: QTSS/6.1.0 (Build/532; Platform/MacOSX; Release/Mac OS X Server; )\r\n"								
				<< "Cseq: " << cseq << "\r\n"
				<< "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, OPTIONS\r\n\r\n";		
		str_tmp = ss_tmp.str();
		buf_ptr = str_tmp.c_str();
		_net_ptr->send(sock, buf_ptr, strlen(buf_ptr), 0);

		memset(html_buf, 0x0, _html_size);
		recv_byte = _net_ptr->recv(sock, html_buf, sizeof(html_buf), 0);
		if(recv_byte <= 0) {
			data_close(sock, "recv html_buf error");
			_log_ptr->exit(0, "recv html_buf error");
			return RET_SOCK_ERROR;
		}

		// DESCRIBE
		find_ptr = strstr(html_buf, "DESCRIBE");
		if (!find_ptr) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "error occured");
			return RET_SOCK_ERROR;
		}
		find_ptr = strstr(html_buf, "CSeq");
		if (!find_ptr) {
			printf("error occured in parsing Session ID cmd\n");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}
		else{
			sscanf(find_ptr, "CSeq: %lu", &cseq);
			printf("CSeq of DESCRIBE is %lu\n", cseq);
		}
		ss_tmp.str("");
		ss_tmp.clear();



		//QTSS VTS_DIVX_MP4

						  ss_tmp << "RTSP/1.0 200 OK\r\n"
								<< "Server: QTSS/6.1.0 (Build/532; Platform/MacOSX; Release/Mac OS X Server; )\r\n"
								<< "Cseq: " << cseq << "\r\n"
								<< "Cache-Control: no-cache\r\n"
								//<< "Content-length: 1383\r\n"
								<< "Content-length: 1399\r\n"
								<< "Content-Type: application/sdp\r\n"
								<< "x-Accept-Retransmit: our-retransmit\r\n"
								<< "x-Accept-Dynamic-Rate: 1\r\n"
								<< "Content-Base: rtsp://10.168.27.1:30000/\r\n\r\n"
								<< "v=0\r\n"
								<< "o=QTSS_Play_List 1842928138 1842946564 IN IP4 127.0.0.1\r\n"
								<< "s=C:\\Program Files\\Darwin Streaming Server\\Playlists\\1test\\1test.sdp\r\n"
								<< "c=IN IP4 0.0.0.0\r\n"
								<< "b=AS:300\r\n"
								<< "t=0 0\r\n"
								<< "a=x-broadcastcontrol:RTSP\r\n"
								<< "a=x-copyright: MP4/3GP File hinted with GPAC 0.4.6-DEV (build 1) (C)2000-2005 - http://gpac.sourceforge.net\r\n"
								//<< "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,AoJ9AE8BASkVAQOCAwACQNxkYXRhOmFwcGxpY2F0aW9uL21wZWc0LW9kLWF1O2Jhc2U2NCxBWUVGQVZNRkh3TlBBTWtnQUFJRU5pRVJBQzV0QUFTbWFBQUIrR0FGSndGQ3dBMy80UUFZWjBMQURacDBGQ1B5d2dBQUF3QUNBQUFEQUQwZUtGVkFBUUFFYU00eXlBWVFBT1FBQUFBUEFBQUFBQ0FBQUFBQUF3RXVBcDhES2dCbElBQUNCQkZBRlFEYnVnQUJFK2dBQVBuNEJRSVNFQVlRQU5RQUFLeEVBQUFBQUNBQUFBQUFBdz09BA8BBQAAiAAAAAAAAAAABQAGEADUAAACWAAAAAAgAAAAAAMDbgABYD5kYXRhOmFwcGxpY2F0aW9uL21wZWc0LWJpZnMtYXU7YmFzZTY0LHdCQVNnVEFxQlhKZ1FvSW9LZEJQQUE9PQACBBYCDQAAEAAAAAAAAAAABQcAAHAKAAeABhAA1AAAAlgAAAAAIAAAAAAD\"\r\n"
								<< "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,AoMBAE8BASkVAQOCBwACQOBkYXRhOmFwcGxpY2F0aW9uL21wZWc0LW9kLWF1O2Jhc2U2NCxBWUVKQVZjRkh3TlRBTWtnQUFJRU9pRVJBRWZLQUFjZytBQURtZmdGS3dGQ3dBMy80UUFjWjBMQURacDBIaWQvNEFFZ0FVSUFBQU1BQkFBQUF3QmxIaWhWUUFFQUJHak9Nc2dHRUFEa0FBQUFHUUFBQUFBZ0FBQUFBQU1CTGdLZkF5b0FaU0FBQWdRUlFCVUEyN29BQVJRUUFBRDUrQVVDRWhBR0VBRFVBQUNzUkFBQUFBQWdBQUFBQUFNPQQPAQUAAIwAAAAAAAAAAAUABhAA1AAAAlgAAAAAIAAAAAADA24AAWA+ZGF0YTphcHBsaWNhdGlvbi9tcGVnNC1iaWZzLWF1O2Jhc2U2NCx3QkFTZ1RBcUJYSmdRb0lvS2RCUEFBPT0AAgQWAg0AABAAAAAAAAAAAAUHAABwDwAJAAYQANQAAAJYAAAAACAAAAAAAw==\"\r\n"
								<< "a=control:*\r\n"
								<< "m=video 0 RTP/AVP 96\r\n"
								<< "b=AS:236\r\n"
								<< "a=rtpmap:96 H264/90000\r\n"
								<< "a=mpeg4-esid:201\r\n"
								<< "a=control:trackID=1\r\n"
								<< "a=fmtp:96 profile-level-id=42C00D; packetization-mode=1; sprop-parameter-sets=Z0LADZp0Hid/4AEgAUIAAAMABAAAAwBlHihVQA==,aM4yyA==\r\n"
								<< "a=framesize:96 240-144\r\n"
								<< "m=audio 0 RTP/AVP 97\r\n"
								<< "b=AS:64\r\n"
								<< "a=rtpmap:97 mpeg4-generic/44100/2\r\n"
								<< "a=mpeg4-esid:101\r\n"
								<< "a=control:trackID=2\r\n"
								<< "a=fmtp:97 profile-level-id=41; config=1210; streamType=5; mode=AAC-hbr; objectType=64; constantDuration=1024; sizeLength=13; indexLength=3; indexDeltaLength=3\r\n";




/*
		ss_tmp << "RTSP/1.0 200 OK\r\n"
				<< "Server: QTSS/6.1.0 (Build/532; Platform/MacOSX; Release/Mac OS X Server; )\r\n"
				<< "Cseq: " << cseq << "\r\n"
				<< "Cache-Control: no-cache\r\n"
				//<< "Content-length: 634\r\n"
				<< "Content-length: 660\r\n"
				<< "Content-Type: application/sdp\r\n"
				<< "x-Accept-Retransmit: our-retransmit\r\n"
				<< "x-Accept-Dynamic-Rate: 1\r\n"
				<< "Content-Base: rtsp://10.168.27.1:30000/\r\n\r\n"
				<< "v=0\r\n"
				<< "o=- 0 2034701545 IN IP4 127.0.0.0\r\n"
				<< "s=QuickTime\r\n"
				<< "c=IN IP4 0.0.0.0\r\n"
				<< "t=0 0\r\n"
				<< "a=range:npt=now-\r\n"
				<< "a=isma-compliance:2,2.0,2\r\n"
				<< "a=control:*\r\n"
				<< "m=audio 0 RTP/AVP 96\r\n"
				<< "b=AS:125\r\n"
				<< "a=3GPP-Adaptation-Support:1\r\n"
				<< "a=rtpmap:96 mpeg4-generic/44100/2\r\n"
				<< "a=fmtp:96 profile-level-id=15;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210\r\n"
				<< "a=mpeg4-esid:101\r\n"
				<< "a=control:trackID=1\r\n"
				<< "m=video 0 RTP/AVP 97\r\n"
				<< "b=AS:300\r\n"
				<< "a=3GPP-Adaptation-Support:1\r\n"
				<< "a=rtpmap:97 H264/90000\r\n"
				<< "a=fmtp:97 packetization-mode=1;profile-level-id=4D400A;sprop-parameter-sets=J01ACqkYUI/LgDUGAQa2wrXvfAQ=,KN4JF6A=\r\n"
				<< "a=mpeg4-esid:201\r\n"
				<< "a=cliprect:0,0,120,160\r\n"
				<< "a=framesize:97 160-120\r\n"
				<< "a=control:trackID=2\r\n";
*/

					
		str_tmp = ss_tmp.str();
		buf_ptr = str_tmp.c_str();
		_net_ptr->send(sock, buf_ptr, strlen(buf_ptr), 0);

		memset(html_buf, 0x0, _html_size);
		recv_byte = _net_ptr->recv(sock, html_buf, sizeof(html_buf), 0);

		if(recv_byte <= 0) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}

		// 1st SETUP
		find_ptr = strstr(html_buf, "SETUP");
		if (!find_ptr) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "error occured");
			return RET_SOCK_ERROR;
		}
		find_ptr = strstr(html_buf, "CSeq");
		if (!find_ptr) {
			printf("error occured in parsing Session ID cmd\n");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}
		else{
			sscanf(find_ptr, "CSeq: %lu", &cseq);
			printf("CSeq of SETUP is %lu\n", cseq);
		}
		port_ptr = strstr(html_buf, "client_port=");
		if (!port_ptr) {
			printf("error occured in parsing Session ID cmd\n");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		} else {
			sscanf(port_ptr, "client_port=%hu", &_cln_port_udp_audio);
			printf("SERVER: Player Audio Port Number is %d---------------\n", _cln_port_udp_audio);
		}
		ss_tmp.str("");
		ss_tmp.clear();
		ss_tmp  << "RTSP/1.0 200 OK\r\n"
				<< "Server: QTSS/6.1.0 (Build/532; Platform/MacOSX; Release/Mac OS X Server; )\r\n"
				<< "Cseq: " << cseq << "\r\n"
				//<< "Cseq: 2\r\n"
				//<< "3GPP-Adaptation: url=\"rtsp://10.168.27.1:30000/trackID=1\";size=187451;target-time=9000\r\n"
				<< "Cache-Control: no-cache\r\n"
				<< "Session: 1848055992938369568\r\n"
				//<< "Date: Fri, 02 Apr 2010 06:29:15 GMT\r\n"
				//<< "Transport: RTP/AVP/UDP;unicast;source=10.168.27.1;client_port=7144-7145;server_port=6970-6971\r\n\r\n";
				<< "Transport: RTP/AVP/UDP;unicast;source=10.168.27.1;client_port=7144-7145;server_port=6970-6970\r\n\r\n";		
		str_tmp = ss_tmp.str();
		buf_ptr = str_tmp.c_str();
		_net_ptr->send(sock, buf_ptr, strlen(buf_ptr), 0);

		memset(html_buf, 0x0, _html_size);
		recv_byte = _net_ptr->recv(sock, html_buf, sizeof(html_buf), 0);

		if(recv_byte <= 0) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}

		// 2nd SETUP
		find_ptr = strstr(html_buf, "SETUP");
		if (!find_ptr) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "error occured");
			return RET_SOCK_ERROR;
		}
		find_ptr = strstr(html_buf, "CSeq");
		if (!find_ptr) {
			printf("error occured in parsing Session ID cmd\n");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}
		else{
			sscanf(find_ptr, "CSeq: %lu", &cseq);
			printf("CSeq of SETUP is %lu\n", cseq);
		}

		port_ptr = strstr(html_buf, "client_port=");
		if (!port_ptr) {
			printf("error occured in parsing Session ID cmd\n");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}
		else{
			sscanf(port_ptr, "client_port=%hu", &_cln_port_udp_video);
			printf("SERVER: Player Video Port Number is %d\n", _cln_port_udp_video);
		}
		ss_tmp.str("");
		ss_tmp.clear();
		ss_tmp  << "RTSP/1.0 200 OK\r\n"
				<< "Server: QTSS/6.1.0 (Build/532; Platform/MacOSX; Release/Mac OS X Server; )\r\n"
				<< "Cseq: " << cseq << "\r\n"
				//<< "Cseq: 3\r\n"
				<< "Cache-Control: no-cache\r\n"
				<< "Session: 1848055992938369568\r\n"
				//<< "Date: Fri, 02 Apr 2010 06:29:15 GMT\r\n"
				<< "Transport: RTP/AVP/UDP;unicast;source=10.168.27.1;client_port=7146-7147;server_port=6971-6971\r\n\r\n"	;		
		str_tmp = ss_tmp.str();
		buf_ptr = str_tmp.c_str();
		_net_ptr->send(sock, buf_ptr, strlen(buf_ptr), 0);

		memset(html_buf, 0x0, _html_size);
		recv_byte = _net_ptr->recv(sock, html_buf, sizeof(html_buf), 0);

		if(recv_byte <= 0) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}

		// PLAY
		find_ptr = strstr(html_buf, "PLAY");
		if (!find_ptr) {
			data_close(sock, "recv html_buf error");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "error occured");
			return RET_SOCK_ERROR;
		}
		find_ptr = strstr(html_buf, "CSeq");
		if (!find_ptr) {
			printf("error occured in parsing Session ID cmd\n");
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			return RET_SOCK_ERROR;
		}
		else{
			sscanf(find_ptr, "CSeq: %lu", &cseq);
			printf("CSeq of PLAY is %lu\n", cseq);
		}
		ss_tmp.str("");
		ss_tmp.clear();
		ss_tmp  <<"RTSP/1.0 200 OK\r\n"
				<<"Server: QTSS/6.1.0 (Build/532; Platform/MacOSX; Release/Mac OS X Server; )\r\n"
				<< "Cseq: " << cseq << "\r\n"
				//<<"Cseq: 4\r\n"
				<<"Session: 1848055992938369568\r\n"
				<<"Range: npt=now-\r\n"
				<<"RTP-Info: url=rtsp://10.168.27.1:30000/trackID=1,url=rtsp://10.168.27.1:30000/trackID=2\r\n\r\n";
		str_tmp = ss_tmp.str();
		buf_ptr = str_tmp.c_str();
		_net_ptr->send(sock, buf_ptr, strlen(buf_ptr), 0);

		_cln_state = STATE_RTSP_PLAY;

		// to construct sockaddr_in for udp sendto
		_cin_udp_audio.sin_family = AF_INET;
		_cin_udp_audio.sin_port = htons(_cln_port_udp_audio);
		_cin_udp_audio.sin_addr = _cin_tcp.sin_addr;
	

		_cin_udp_video.sin_family = AF_INET;
		_cin_udp_video.sin_port = htons(_cln_port_udp_video);
		_cin_udp_video.sin_addr = _cin_tcp.sin_addr;
	

		_strm_audio->set_client_sockaddr(&_cin_udp_audio);
		_strm_video->set_client_sockaddr(&_cin_udp_video);

		//_net_ptr->set_nonblocking(_sock_udp_audio);
		_net_ptr->epoll_control(_sock_udp_audio, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
		fd_list_ptr->push_back(_sock_udp_audio);
		//_net_ptr->set_nonblocking(_sock_udp_video);
		_net_ptr->epoll_control(_sock_udp_video, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
		fd_list_ptr->push_back(_sock_udp_video);

		_pk_mgr_ptr->rtsp_viewer_set(this);
		
		_pk_mgr_ptr->add_stream(_sock_udp_audio, (stream *)_strm_audio, STRM_TYPE_AUDIO);
		_pk_mgr_ptr->add_stream(_sock_udp_video, (stream *)_strm_video, STRM_TYPE_VIDEO);
		_strm_hand_audio->add_stream((unsigned long)_strm_audio, (stream *)_strm_audio);
		_strm_hand_video->add_stream((unsigned long)_strm_video, (stream *)_strm_video);
		

		// switch back to nonblocking
		_net_ptr->set_nonblocking(sock);
		
		//DBG_PRINTF("here\n");
	
		//PAUSE
		return RET_OK;
	}


	switch (_cln_state) {
	case STATE_RTSP_INIT:
		
		break;
	case STATE_RTSP_OPTIONS:
		break;
	case STATE_RTSP_DESCRIBE:
		break;
	case STATE_RTSP_SETUP:
		break;
	case STATE_RTSP_PLAY:
		_net_ptr->set_blocking(sock);
		memset(html_buf, 0x0, 8192);
		recv_byte = _net_ptr->recv(sock, html_buf, sizeof(html_buf), 0);

		if(recv_byte < 0) {
			DBG_PRINTF("here\n");
			data_close(sock, "recv html_buf error");
			data_close(_sock_udp_audio, "recv html_buf error");
			data_close(_sock_udp_video, "recv html_buf error");

			_pk_mgr_ptr->del_stream(_sock_udp_audio, (stream *)_strm_audio, STRM_TYPE_AUDIO);
			_pk_mgr_ptr->del_stream(_sock_udp_video, (stream *)_strm_video, STRM_TYPE_VIDEO);
			_strm_hand_audio->del_stream((unsigned long)_strm_audio, (stream *)_strm_audio);
			_strm_hand_video->del_stream((unsigned long)_strm_video, (stream *)_strm_video);
			
				
			if (_strm_hand_audio)
				delete _strm_hand_audio;
			
			if (_strm_hand_video)
				delete _strm_hand_video;
	
			if (_strm_audio)
				delete _strm_audio;
	
			if (_strm_video)
				delete _strm_video;
			
			_log_ptr->write_log_format("s => s\n", (char*)__PRETTY_FUNCTION__, "recv html_buf error");
			//PAUSE
			DBG_PRINTF("here\n");
			return RET_SOCK_ERROR;
		} 

		// get option cmd when playing video
		find_ptr = strstr(html_buf, "OPTIONS");
		if (find_ptr) {
			PAUSE
			ss_tmp  << "RTSP/1.0 200 OK\r\n"
				<< "Server: QTSS/6.1.0 (Build/532; Platform/MacOSX; Release/Mac OS X Server; )\r\n"								
				<< "Cseq: 5\r\n"
				<< "Session: 1848055992938369568\r\n"
				<< "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, OPTIONS\r\n\r\n";		
			str_tmp = ss_tmp.str();
			buf_ptr = str_tmp.c_str();
			_net_ptr->send(sock, buf_ptr, strlen(buf_ptr), 0);
			_net_ptr->set_nonblocking(sock);
			break;
		}

		find_ptr = strstr(html_buf, "TEARDOWN");
		if (find_ptr) {
			data_close(sock, "VLC player teardown");
			data_close(_sock_udp_audio, "VLC player teardown");
			data_close(_sock_udp_video, "VLC player teardown");

			_pk_mgr_ptr->del_stream(_sock_udp_audio, (stream *)_strm_audio, STRM_TYPE_AUDIO);
			_pk_mgr_ptr->del_stream(_sock_udp_video, (stream *)_strm_video, STRM_TYPE_VIDEO);
			_strm_hand_audio->del_stream((unsigned long)_strm_audio, (stream *)_strm_audio);
			_strm_hand_video->del_stream((unsigned long)_strm_video, (stream *)_strm_video);
			
			if (_strm_hand_audio)
				delete _strm_hand_audio;
			
			if (_strm_hand_video)
				delete _strm_hand_video;
	
			if (_strm_audio)
				delete _strm_audio;
	
			if (_strm_video)
				delete _strm_video;
			
			DBG_PRINTF("here\n");
			//PAUSE
			
			break;
		}

		cout << "html_buf = " << html_buf << endl;
		PAUSE
		_net_ptr->set_nonblocking(sock);
		break;
	case STATE_RTSP_TEARDOWN:
		break;
	default:
		break;
	};

	return RET_OK;
}


int rtsp_viewer::handle_pkt_out(int sock)
{
	struct chunk_t *chunk_ptr;

	if (_queue_output_ctrl.size()) {
		chunk_ptr = _queue_output_ctrl.front();
		_expect_len = chunk_ptr->header.length;
		_offset = sizeof(struct chunk_header_t);
		_send_byte = send(sock, (char *)chunk_ptr+_offset, _expect_len, 0);
		if(_send_byte < 0) {
			data_close(sock, "error occured in send queue_out_ctrl");
			_log_ptr->exit(0, "error occured in send queue_out_ctrl");
			return RET_SOCK_ERROR;
		} else {
			cout << "send_byte =" << _send_byte << endl;
			_offset += _send_byte;
			cout << "offset =" << _offset << endl;
			_expect_len = _expect_len - _send_byte;
			cout << "expect_len =" << _expect_len<< endl;
			cout << "sock =" << sock << endl;
			//PAUSE
			if(_expect_len == 0) {
				_queue_output_ctrl.pop();
				_offset = 0;
				delete chunk_ptr;
			}
		}
		_queue_output_ctrl.pop();
	}

	return RET_OK;
}

void rtsp_viewer::handle_pkt_error(int sock)
{

}

void rtsp_viewer::handle_job_realtime()
{

}


void rtsp_viewer::handle_job_timer()
{

}

void rtsp_viewer::data_close(int cfd, const char *reason)
{
	list<int>::iterator fd_iter;
	
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "rtsp_viewer", reason);
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);	

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}
	
}

void rtsp_viewer::set_client_sockaddr(struct sockaddr_in *cin)
{
	if (cin)
		memcpy(&_cin_tcp, cin, sizeof(struct sockaddr_in));
}
