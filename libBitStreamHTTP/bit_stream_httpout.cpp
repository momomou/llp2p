#include "bit_stream_httpout.h"
#include "../bit_stream_server.h"
#include "../network.h"
#include "../logger.h"
#include "../pk_mgr.h"
#include <sstream>


//const	char httpHeader[]=	"HTTP/1.1·200·OK\r\n" \
							"Server:·Apache-Coyote/1.1"\
							"Server: Apache/2.2.8 (Win32) PHP/5.2.6\r\n"\
							"Last-Modified:·Thu,·23·May·2013·06:58:16·GMT\r\n"\
							"Content-Type:·video/x-flv\r\n"\
							"Date:·Thu,·23·May·2013·07:23:03·GMT\r\n"\
							"Connection:·close\r\n"\
							"\r\n";


//const	char httpHeader[]=	"HTTP/1.1·200·OK\r\n" \
							"Server:·Apache-Coyote/1.1"\
							"Content-Type:·video/x-flv\r\n"\
							"Pragma:·no-cache"\
							"Cache-Control:·no-cache"\
							"Connection:·close\r\n"\
							"\r\n";

const	char httpHeader[]=	"HTTP/1.1·200·OK\r\n" \
							"Server:·Apache-Coyote/1.1\r\n"\
							"Content-Type:·video/x-flv\r\n"\
							"Connection:·close\r\n"\
							"\r\n" ;


const char crossdomain[]=	 "<?xml version=\"1.0\"?>\r\n"\
							"<!DOCTYPE cross-domain-policy SYSTEM \"http://www.adobe.com/xml/dtds/cross-domain-policy.dtd\">\r\n"\
							"<cross-domain-policy>\r\n"\
							"<allow-access-from domain=\"*\"  to-ports=\"*\" secure=\"false\" />\r\n"\
							"</cross-domain-policy>\r\n" ;



const char flvHeader[] =			{ 'F', 'L', 'V', 0x01,
	0x00,	 /* 0x04 == audio, 0x01 == video */
	0x00, 0x00, 0x00, 0x09,
	0x00, 0x00, 0x00, 0x00
};

//const unsigned char flvHeader[] = { 0x46,0x4c,0x56,0x01,0x05,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x00 };
/*
,0x12,0x00,0x00 \
,0xb6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x0a,0x6f,0x6e,0x4d,0x65,0x74 \
,0x61,0x44,0x61,0x74,0x61,0x08,0x00,0x00,0x00,0x07,0x00,0x08,0x64,0x75,0x72,0x61 \
,0x74,0x69,0x6f,0x6e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x77 \
,0x69,0x64,0x74,0x68,0x00,0x40,0x84,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x68 \
,0x65,0x69,0x67,0x68,0x74,0x00,0x40,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09 \
,0x66,0x72,0x61,0x6d,0x65,0x72,0x61,0x74,0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00 \
,0x00,0x00,0x00,0x0c,0x76,0x69,0x64,0x65,0x6f,0x63,0x6f,0x64,0x65,0x63,0x69,0x64 \
,0x00,0x40,0x1c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0x76,0x69,0x64,0x65,0x6f \
,0x64,0x61,0x74,0x61,0x72,0x61,0x74,0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 \
,0x00,0x00,0x07,0x65,0x6e,0x63,0x6f,0x64,0x65,0x72,0x02,0x00,0x0b,0x4c,0x61,0x76 \
,0x66,0x35,0x32,0x2e,0x38,0x37,0x2e,0x31,0x00,0x08,0x66,0x69,0x6c,0x65,0x73,0x69 \
,0x7a,0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x00,0x00 \
,0x00,0xc1,0x09,0x00,0x00,0x43,0x00,0x00,0x01,0x00,0x00							 \
,0x00,0x00,0x17,0x00,0x00,0x00,0x00,0x01,0x42,0x00,0x1f,0x03,0x01,0x00,0x2f,0x67 \
,0x42,0x80,0x1f,0x96,0x52,0x02,0x83,0xf6,0x02,0xa1,0x00,0x00,0x03,0x00,0x01,0x00 \
,0x00,0x03,0x00,0x1e,0xe0,0x60,0x03,0x0d,0x40,0x00,0x46,0x30,0xff,0x18,0xe3,0x03 \
,0x00,0x18,0x6a,0x00,0x02,0x31,0x87,0xf8,0xc7,0x0e,0xd0,0xa1,0x52,0x40,0x01,0x00 \
,0x04,0x68,0xcb,0x8d,0x48,0x00,0x00,0x00,0x4e									 \
};
*/


//const unsigned char flvHeader[] = {0x46,0x4c,0x56,0x01,0x05,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x00,0x12,0x00,0x01,0x4c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x0a,0x6f,0x6e,0x4d,0x65,0x74,0x61,0x44,0x61,0x74,0x61,0x08,0x00,0x00,0x00,0x0e,0x00,0x08,0x64,0x75,0x72,0x61,0x74,0x69,0x6f,0x6e,0x00,0x40,0x2c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x66,0x69,0x6c,0x65,0x53,0x69,0x7a,0x65,0x00,0x41,0x21,0xce,0xb4,0x00,0x00,0x00,0x00,0x00,0x05,0x77,0x69,0x64,0x74,0x68,0x00,0x40,0x84,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x68,0x65,0x69,0x67,0x68,0x74,0x00,0x40,0x76,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x76,0x69,0x64,0x65,0x6f,0x63,0x6f,0x64,0x65,0x63,0x69,0x64,0x02,0x00,0x04,0x61,0x76,0x63,0x31,0x00,0x0d,0x76,0x69,0x64,0x65,0x6f,0x64,0x61,0x74,0x61,0x72,0x61,0x74,0x65,0x00,0x40,0x7f,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x66,0x72,0x61,0x6d,0x65,0x72,0x61,0x74,0x65,0x00,0x40,0x3e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x61,0x75,0x64,0x69,0x6f,0x63,0x6f,0x64,0x65,0x63,0x69,0x64,0x02,0x00,0x04,0x6d,0x70,0x34,0x61,0x00,0x0d,0x61,0x75,0x64,0x69,0x6f,0x64,0x61,0x74,0x61,0x72,0x61,0x74,0x65,0x00,0x40,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x61,0x75,0x64,0x69,0x6f,0x73,0x61,0x6d,0x70,0x6c,0x65,0x72,0x61,0x74,0x65,0x00,0x40,0xe5,0x88,0x80,0x00,0x00,0x00,0x00,0x00,0x0f,0x61,0x75,0x64,0x69,0x6f,0x73,0x61,0x6d,0x70,0x6c,0x65,0x73,0x69,0x7a,0x65,0x00,0x40,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0x61,0x75,0x64,0x69,0x6f,0x63,0x68,0x61,0x6e,0x6e,0x65,0x6c,0x73,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x73,0x74,0x65,0x72,0x65,0x6f,0x01,0x01,0x00,0x07,0x65,0x6e,0x63,0x6f,0x64,0x65,0x72,0x02,0x00,0x20,0x4f,0x70,0x65,0x6e,0x20,0x42,0x72,0x6f,0x61,0x64,0x63,0x61,0x73,0x74,0x65,0x72,0x20,0x53,0x6f,0x66,0x74,0x77,0x61,0x72,0x65,0x20,0x76,0x30,0x2e,0x35,0x30,0x62,0x00,0x00,0x09,0x00,0x00,0x01,0x5a};


bit_stream_httpout::bit_stream_httpout(int stream_id,network *net_ptr, logger *log_ptr,bit_stream_server *bit_stream_server_ptr ,pk_mgr *pk_mgr_ptr, list<int> *fd_list,int acceptfd)
{

	a = -1;
	b = -1;

	_reqStreamID = -1;
	firstRecvReqSreamID = 0;

	first_pkt = true;
	first_HTTP_Header = true;

	_stream_id = stream_id;
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	fd_list_ptr = fd_list;
	_bit_stream_server_ptr = bit_stream_server_ptr;
	_queue_out_data_ptr = NULL;
	//fp = fopen("video", "wb");
	//fwrite((char *)flvHeader, 1, sizeof(flvHeader), fp);
	
	_queue_out_data_ptr = new std::queue<struct chunk_t *>;	
	if (!_queue_out_data_ptr) {
		printf("new _queue_out_data_ptr in bit_stream_httpout\n");
		_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__," new _queue_out_data_ptr in bit_stream_httpout");
		PAUSE
	}
	memset(&_send_ctl_info, 0x00, sizeof(_send_ctl_info));

	//for debug
	//file_ptr = fopen("./here.flv" , "wb");
	//file_ptr_test = fopen("./metadata" , "rb");
	//fwrite(FLV_Header,1,sizeof(FLV_Header),file_ptr);
}

bit_stream_httpout::~bit_stream_httpout(){
	printf("==============deldet bit_stream_httpout success==========\n");
	if(_queue_out_data_ptr)
		delete _queue_out_data_ptr;
}

void bit_stream_httpout::init()
{

}

//只有一開始第一次接收player的http request 接收後即關閉監聽
// Player會發送兩次request, 第一次是"GET /crossdomain.xml HTTP/1.1", 第二次是"GET /0.flv HTTP/1.1"
int bit_stream_httpout::handle_pkt_in(int sock)
{
	int recv_byte;
	char HTTPrequestBuffer[512] = {0};
	_net_ptr->set_nonblocking(sock);

	recv_byte = _net_ptr->recv(sock, HTTPrequestBuffer, sizeof(HTTPrequestBuffer), 0);

	// player拿streamID的時候可能會發起多次連線，通常第一個socket的連線會成功，其餘的socket會被player關閉，因此會收到關閉訊息:recv_byte=0, WSAGetLastError()=0
	if (recv_byte <= 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__, "[ERROR] HTTP Stream in channel WSAGetLastError() =", socketErr, recv_byte);

		struct sockaddr_in addr;
		int addrLen=sizeof(struct sockaddr_in);
		int	aa;
		aa = getpeername(sock, (struct sockaddr *)&addr, (socklen_t *)&addrLen);
		_log_ptr->write_log_format("s(u) d s d \n", __FUNCTION__, __LINE__, aa, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		
		_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
		data_close(sock," bit_stream_httpout::handle_pkt_in HTTPrequestBuffer ");
		delete this;
		return RET_SOCK_ERROR;
	}
	_log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "recv_byte =", recv_byte, "HTTP request:", HTTPrequestBuffer);
	
	struct sockaddr_in addr;
	int addrLen=sizeof(struct sockaddr_in);
	int	aa;
	aa = getpeername(sock, (struct sockaddr *)&addr, (socklen_t *)&addrLen);
	_log_ptr->write_log_format("s(u) d s d \n", __FUNCTION__, __LINE__, aa, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	
	
	//		Sleep(1000);
	//printf("==============REQUEST==============\n");
	//for(int i =0 ; i <512 ;i++)
	//	printf("%c",HTTPrequestBuffer[i]);
	//printf("\n");
	//printf("==============REQUEST==============\n");
	
	if (_reqStreamID == -1) {
		_reqStreamID = getStreamID_FromHTTP_Request(sock, HTTPrequestBuffer, sizeof(HTTPrequestBuffer));
	}
	else {
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLOUT);	
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__,__LINE__," HTTP Stream in channel _reqStreamID", _reqStreamID);
		return RET_OK;
	}

	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "HTTP _reqStreamID =", _reqStreamID);
	
	//得到streamID 並關掉監聽
	if (isStreamID_inChannel(_reqStreamID)) {			
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLOUT);	
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, " HTTP Stream in channel _reqStreamID", _reqStreamID);
		
		send_header_to_player(sock);	// Send HTTP header and FLV header to player
	}
	//stream ID 錯誤 或player 多次連線
	else {														
		//_net_ptr->epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN | EPOLLOUT);	
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "delete map and bit_stream_httpout_ptr _reqStreamID", _reqStreamID);
		_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
		data_close(sock, "bit_stream_httpout");
		delete this;
	}

	return RET_OK;
}

//第一次傳送http header 和flv headerd,然後等到第一個keyframe後才開始丟給player 
//判斷delay是否發生並適當的做取樣,接著就把queue 的資料傳出去
int bit_stream_httpout::handle_pkt_out(int sock)
{
	int send_rt_val = 0;
	
	
	
	/*	Commented on 20131017	*/
	/*
	//here is http header and flv header
	if (first_HTTP_Header) {
		int sendHeaderBytes = _net_ptr->send(sock, httpHeader, sizeof(httpHeader)-1, 0);  //-1 to subtract '\0'. Ideally send 97 bytes
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__,"Send HTTP header and FLV header to player. _reqStreamID =" ,_reqStreamID);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__,"send httpHeader sendHeaderBytes =", sendHeaderBytes);

		sendHeaderBytes = _net_ptr->send(sock, (char *)flvHeader, sizeof(flvHeader), 0);
		//debug
		//fwrite(flvHeader,1,sizeof(flvHeader),file_ptr);

		map<int, struct update_stream_header *>::iterator  iter;
		struct update_stream_header *protocol_header = NULL;
		
		iter = _pk_mgr_ptr->map_streamID_header.find(_reqStreamID);
		if (iter != _pk_mgr_ptr->map_streamID_header.end()) {
			protocol_header = iter->second ;
		}
		else {
			debug_printf("[ERROR] not found _reqStreamID in map_streamID_header \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] not found _reqStreamID in map_streamID_header");
			*(_net_ptr->_errorRestartFlag)=RESTART;
			PAUSE
		}
		
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "protocol_header->len =", protocol_header->len);
		
		if (protocol_header->len > 0) {
			sendHeaderBytes = _net_ptr->send(sock, (char *)protocol_header->header, protocol_header->len, 0);	// Ideally send 441 bytes
			//debug
			//		fwrite((char *)protocol_header->header,1,protocol_header ->len,file_ptr);
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "sendFLVHeaderBytes", sendHeaderBytes);
		}
		first_HTTP_Header = false;
	}
	*/
	//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "_queue_out_data_ptr->size() =", _queue_out_data_ptr->size());
	//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "_send_ctl_info.ctl_state =", _send_ctl_info.ctl_state);
	
	if (_send_ctl_info.ctl_state == READY) {
		size_t send_size;

		struct chunk_t *chunk_ptr;

		//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "2");
		
		if (!_queue_out_data_ptr->size()) {
			//_log_ptr->write_log_format("s => s \n", __FUNCTION__, "_queue_out_data_ptr->size =0");
			//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);	
			return RET_OK;
		}

		chunk_ptr = (chunk_t *)_queue_out_data_ptr->front();
		
		if (isKeyFrame(chunk_ptr)) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Is keyframe", chunk_ptr->header.sequence_number);
		}
		
		
		//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "3");
		//first_pkt = false;
		
		//pop until get the first keyframe
		if (first_pkt) {
			//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "4");
			if (_queue_out_data_ptr->size() < 10) {
				return RET_OK;
			}
			//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "5");
			for (int i = 0; i < 5; i++) {
				//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "6");
				if (!isKeyFrame(chunk_ptr)) {
					//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "No Key Frame");
					_queue_out_data_ptr->pop();
					chunk_ptr = _queue_out_data_ptr->front();
				}
				else {
					_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "First Key Frame is ", chunk_ptr->header.sequence_number);
					first_pkt = false;
					break;
				}
			}
			return RET_OK;
		}
		
		//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "7");
	
		
		//here is to down-sampling (這邊可能有些bug 可能會去pop一個空的_queue_out_data_ptr!?)
		/*

		int baseCount= (_pk_mgr_ptr ->Xcount*PARAMETER_X*MAX_DELAY/1000) ;
		int sentSequenceNumber = chunk_ptr ->header.sequence_number ;
		int differenceValue = (_pk_mgr_ptr ->_current_send_sequence_number -sentSequenceNumber);
		if (differenceValue > (baseCount+1)){ //if (recv -sent)diff >100 pkt pop until  queue <30 and continuance pop until last key frame
		while(_queue_out_data_ptr ->size() >=(baseCount *0.3)+2){
		_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "POP queue ", _queue_out_data_ptr ->size());
		_queue_out_data_ptr->pop();
		}
		chunk_ptr = (chunk_t *)_queue_out_data_ptr->front();
		while(! isKeyFrame(chunk_ptr )){
		if( _queue_out_data_ptr ->size()<=2)
		return RET_OK;
		_queue_out_data_ptr->pop();
		_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "POP queue ", _queue_out_data_ptr ->size());
		chunk_ptr = (chunk_t *)_queue_out_data_ptr->front();
		if(_queue_out_data_ptr ->size() <=(baseCount *0.1)+2)
		break;
		}
		}

		else if(differenceValue <=((baseCount)+1) && differenceValue >((baseCount *0.4)+2)){  
		if(! isKeyFrame(chunk_ptr) &&  (sentSequenceNumber % 3 == 0) ){   //not key frame &&  sampling by 1/3
		_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "pkt discard 1/3", chunk_ptr ->header.sequence_number);
		if( _queue_out_data_ptr ->size()<=2)
		return RET_OK;
		_queue_out_data_ptr->pop();
		chunk_ptr = (chunk_t *)_queue_out_data_ptr->front();			 //ignore and not send
		}
		}else if(differenceValue <=((baseCount *0.4)+1) && differenceValue >((baseCount *0.2)+2)){
		if(! isKeyFrame(chunk_ptr) &&  (sentSequenceNumber % 5 == 0) ){   //not key frame &&  sampling by 1/5
		if( _queue_out_data_ptr ->size()<=2)
		return RET_OK;
		_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "pkt discard 1/5", chunk_ptr ->header.sequence_number);
		_queue_out_data_ptr->pop();
		chunk_ptr = (chunk_t *)_queue_out_data_ptr->front();			 //ignore and not send
		}
		}else if (differenceValue <=((baseCount *0.2)+2) && differenceValue >((baseCount *0.05)+2)){
		if(! isKeyFrame(chunk_ptr) &&  (sentSequenceNumber % 7 == 0) ){   //not key frame &&  sampling by 1/7
		if( _queue_out_data_ptr ->size()<=2)
		return RET_OK;
		_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "pkt discard 1/7", chunk_ptr ->header.sequence_number);
		_queue_out_data_ptr->pop();
		chunk_ptr = (chunk_t *)_queue_out_data_ptr->front();			 //ignore and not send
		}	
		}
		*/

		
		
		
		

		//for debug
		//			fwrite(chunk_ptr->buf,1,chunk_ptr->header.length,file_ptr);
		//			_log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "write pkt", chunk_ptr->header.length, _stream_id);


		//to set preTag len=0
		//			memset( chunk_ptr->buf + (chunk_ptr->header.length) -4 ,0x0,4);

		//		unsigned int stamp=getFlvTimeStamp(chunk_ptr);
		if (_queue_out_data_ptr->size() == 0) {
			return RET_OK;
		}
		
	
		/*
		// Debug, keyframe time interval:10s, 1000seq/13s, keyframe seq interval:768
		if (_queue_out_data_ptr->size() < 200) {
			return RET_OK;
		}
		a = chunk_ptr->header.sequence_number;
		if (b == -1) {
			b = a;
		}
		if (a - b > 1000) {
			//first_pkt = true;
			_queue_out_data_ptr->pop();
			printf("\n\n\n\ a - b > 1000 %d %d %d\n\n\n", a, b, _queue_out_data_ptr->size());
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "a - b > 1000", a, b);
			if (a - b > 1100) {
				b = a;
			}
			//add_chunk(chunk_ptr);
			
			//if (a - b > 1900) {
			//	b = a;
			//}
			
			return RET_OK;
			
		} 
		*/
		
		
		
		_queue_out_data_ptr->pop();
		
		
		
		send_size = chunk_ptr->header.length;

		_send_ctl_info.offset = 0;
		_send_ctl_info.total_len = send_size;
		_send_ctl_info.expect_len = send_size;
		_send_ctl_info.buffer = (char *)chunk_ptr->buf;
		_send_ctl_info.chunk_ptr  = (chunk_t *)chunk_ptr;
		_send_ctl_info.serial_num = chunk_ptr->header.sequence_number;

		send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "Send to player", _send_ctl_info.serial_num, send_size);
		
		//int nn = fwrite((char *)chunk_ptr->buf, 1, send_size, fp);
		//_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Write file", nn);
		
		//for debug sequence_number
		/*
		static unsigned long test=0;
		if(test==0){
		test = chunk_ptr->header.sequence_number;

		}
		printf("sequence_number = %d \n",chunk_ptr->header.sequence_number);

		if(test ==chunk_ptr->header.sequence_number){
		test++;
		}else{
		printf("test=%d  sequence_number=%d \n",test,chunk_ptr->header.sequence_number);
		PAUSE
		}
		*/

		//_log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "sent pkt sequence_number", chunk_ptr ->header.sequence_number, send_rt_val);


		if (send_rt_val < 0) {
#ifdef _WIN32
			int socketErr = WSAGetLastError();
#else
			int socketErr = errno;
#endif
			_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__, "socket error number =", socketErr, send_rt_val);
#ifdef _WIN32
			// Buffer is full
			if (socketErr == WSAEWOULDBLOCK) {		
				//it not a error
				//_send_ctl_info.ctl_state = RUNNING;
				//					_queue_out_data_ptr->pop();
				_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__," HTTP Stream RUNNINGD  WSAGetLastError", socketErr);

				//					continue;
			}
			else {
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "delete map and bit_stream_httpout_ptr WSAGetLastError", socketErr);

				//						_net_ptr->epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN | EPOLLOUT);	
				_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
				data_close(sock," bit_stream_httpout ");
				delete this;
				return RET_SOCK_ERROR;
			}
#else
#endif
		}

		/*
		switch (send_rt_val) {
		case RET_SOCK_ERROR:
		printf("%s, socket error\n", __FUNCTION__);
		//				return RET_OK;
		return RET_SOCK_ERROR;
		default:
		return RET_OK;
		}
		*/		
	}
	//_send_ctl_info._send_ctl_state is RUNNING
	else { 

		send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);
		_log_ptr->write_log_format("s(u) s d (d) (d)\n", __FUNCTION__, __LINE__, "[DEBUG] _send_ctl_info.ctl_state =", _send_ctl_info.ctl_state, send_rt_val, _stream_id);
		
		if (send_rt_val < 0) {
#ifdef _WIN32
			int socketErr = WSAGetLastError();
#else
			int socketErr = errno;
#endif
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "socket error number =", socketErr);
#ifdef _WIN32
			if (socketErr == WSAEWOULDBLOCK) {		//buff full
				//it not a error
				//				_send_ctl_info.ctl_state = RUNNING;
				//					_queue_out_data_ptr->pop();
				_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__,__LINE__," HTTP Stream RUNNINGD  WSAGetLastError",WSAGetLastError());

				//					continue;
			}
			else {
				_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"delete map and bit_stream_httpout_ptr WSAGetLastError",WSAGetLastError());

				//						_net_ptr->epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN | EPOLLOUT);	
				_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
				data_close(sock," bit_stream_httpout ");
				delete this;
				return RET_SOCK_ERROR;
			}
#endif
		}
		
		/*
		switch (send_rt_val) {
		case RET_WRONG_SER_NUM:
		_log_ptr->Log(LOGDEBUG, "%s, serial number changed, queue rewrite?", __FUNCTION__);
		printf("%s, serial number changed, queue rewrite?\n", __FUNCTION__);
		case RET_SOCK_ERROR:				

		printf("%s, socket error\n", __FUNCTION__);
		return RET_SOCK_ERROR;
		default:
		return RET_OK;
		}
		*/
	}

}

// Send HTTP header and FLV header to player
void bit_stream_httpout::send_header_to_player(int sock)
{
	if (first_HTTP_Header) {
		int sendHeaderBytes = _net_ptr->send(sock, httpHeader, sizeof(httpHeader)-1, 0);  //-1 to subtract '\0'. Ideally send 97 bytes
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__,"Send HTTP header and FLV header to player. _reqStreamID =" ,_reqStreamID);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__,"send httpHeader sendHeaderBytes =", sendHeaderBytes);

		sendHeaderBytes = _net_ptr->send(sock, (char *)flvHeader, sizeof(flvHeader), 0);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__,"send flvHeader sendHeaderBytes =", sendHeaderBytes);
		//debug
		//fwrite(flvHeader,1,sizeof(flvHeader),file_ptr);

		map<int, struct update_stream_header *>::iterator  iter;
		struct update_stream_header *protocol_header = NULL;
		
		iter = _pk_mgr_ptr->map_streamID_header.find(_reqStreamID);
		if (iter != _pk_mgr_ptr->map_streamID_header.end()) {
			protocol_header = iter->second ;
		}
		else {
			debug_printf("[ERROR] not found _reqStreamID in map_streamID_header \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] not found _reqStreamID in map_streamID_header");
			*(_net_ptr->_errorRestartFlag) = RESTART;
			PAUSE
		}
		
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "protocol_header->len =", protocol_header->len);
		
		if (protocol_header->len > 0) {
			sendHeaderBytes = _net_ptr->send(sock, (char *)protocol_header->header, protocol_header->len, 0);	// Ideally send 441 bytes
			//fwrite((char *)protocol_header->header, 1, protocol_header->len, fp);
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "send streamHeader sendHeaderBytes", sendHeaderBytes);
		}
		first_HTTP_Header = false;
		
		printf("\n\n\n\n\n-\n");
		for (int i = 0; i < protocol_header->len; i++) {
			printf("%x ", protocol_header->header[i]);
			if (((i+1) % 20) == 0) {
				printf("\n");
			}
		}
		printf("\n-\n\n\n\n\n");
		//PAUSE
		
	}
}

void bit_stream_httpout::handle_pkt_error(int sock)
{

}
void bit_stream_httpout::handle_job_realtime()
{

}
void bit_stream_httpout::handle_job_timer()
{	

}

void bit_stream_httpout::set_client_sockaddr(struct sockaddr_in *cin)
{
	if (cin) {
		memcpy(&_cin_tcp, cin, sizeof(struct sockaddr_in));
	}
}

void bit_stream_httpout::add_chunk(struct chunk_t *chunk)
{
	_queue_out_data_ptr->push(chunk);
}

unsigned char bit_stream_httpout::get_stream_pk_id()
{
	return -1;
}

bool bit_stream_httpout::isKeyFrame(struct chunk_t *chunk_ptr)
{
	char flvBitFlag;
	if (*(char*)(chunk_ptr->buf) == 0x09) {		//video
		flvBitFlag = *(char*)((chunk_ptr->buf) + 11);  //get first byte
		if ((flvBitFlag & 0xf0) >> 4 == 0x01 ) {
			return true;  //it's key frame
		}
		else {	
			return false;
		}
	}
	else {		//audio
		return false;
	}
}

//return  StreamID  ,streamID >0 ,bufferSize 暫時保留沒用到(習慣上會將陣列的大小一起傳進function)
//example http://127.0.0.1:3000/8877.flv  streamID= 8877
int  bit_stream_httpout::getStreamID_FromHTTP_Request(int sock, char *httpBuffer, unsigned long bufferSize )
{
	char *ptr = NULL;
	int streamID;

	ptr = strstr(httpBuffer ,"cros");

	//request a crossdomain	
	if (ptr) {
		printf("a crossdomain request");

		char crossDomainHttpHeader[]=			"HTTP/1.1·200·OK\r\n" \
												"Server:·Apache-Coyote/1.1"\
												"Server: Apache/2.2.8 (Win32) PHP/5.2.6\r\n"\
												"Last-Modified:·Thu,·23·May·2013·06:58:16·GMT\r\n"\
												"Content-Type:·application/xml\r\n"\
												"Date:·Thu,·23·May·2013·07:23:03·GMT\r\n"\
												"Connection:·close\r\n" \
												"Content-Lnegth:·";


		char crlf_crlf[] = "\r\n\r\n" ;

		char crossdomain[]=			"<?xml version=\"1.0\"?>\r\n"\
									"<!DOCTYPE cross-domain-policy SYSTEM \"http://www.adobe.com/xml/dtds/cross-domain-policy.dtd\">\r\n"\
									"<cross-domain-policy>\r\n"\
									"<allow-access-from domain=\"*\"  to-ports=\"*\" secure=\"false\" />\r\n"\
									"</cross-domain-policy>\r\n" ;

		int crossdomainHeaderSize = (sizeof(crossdomain)-1) ;

		char conten_length[20] ;
#ifdef _WIN32
		itoa((sizeof(crossdomain)-1),conten_length,10);
#else
		sprintf(conten_length, "%d", sizeof(crossdomain)-1);
#endif
		int len =0 ;	
		while(crossdomainHeaderSize){

			crossdomainHeaderSize = crossdomainHeaderSize/10;
			len ++ ;
		}

		int sendHeaderBytes = 0;
		sendHeaderBytes = _net_ptr->send(sock,crossDomainHttpHeader, sizeof(crossDomainHttpHeader)-1, 0);
		_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__,"Send crossdomain HTTP Byte =", sendHeaderBytes, sizeof(crossDomainHttpHeader)-1);
		
		sendHeaderBytes = _net_ptr->send(sock, conten_length, len, 0);
		_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__,"Send crossdomain HTTP Byte =", sendHeaderBytes, len);
		
		sendHeaderBytes = _net_ptr->send(sock, crlf_crlf, sizeof(crlf_crlf)-1, 0);
		_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__,"Send crossdomain HTTP Byte =", sendHeaderBytes, sizeof(crlf_crlf)-1);


		_net_ptr->set_blocking(sock);
		sendHeaderBytes = _net_ptr->send(sock, crossdomain, sizeof(crossdomain)-1, 0);
		_log_ptr->write_log_format("s(u) s d (d) \n", __FUNCTION__, __LINE__,"Send crossdomain HTTP Byte =", sendHeaderBytes, sizeof(crossdomain)-1);
		_net_ptr->set_nonblocking(sock);

		return -1 ;
	}


	ptr = strstr(httpBuffer ,"GET /");
	if (!ptr) {
		return -1;	//return undefine ID
	}
	else {
		ptr += 5;
		char temp[5] = {0};
		memcpy(temp, ptr, 4);
		_log_ptr->write_log_format("s(u) s s \n", __FUNCTION__, __LINE__,"temp =", temp);
		streamID = atoi(temp);
		return streamID;
	}
}

// T=1/F=0
bool bit_stream_httpout::isStreamID_inChannel(int streamid)
{
	/*
	list<int>::iterator IDiter;
	if(streamid < 0)
	return false;
	for(IDiter = _pk_mgr_ptr ->streamID_list.begin() ; IDiter !=_pk_mgr_ptr ->streamID_list.end() ;IDiter++){
	if(streamid== *IDiter)
	return true;
	}
	return false;
	*/

	map_streamID_header_iter = _pk_mgr_ptr->map_streamID_header.find(streamid);
	if (map_streamID_header_iter != _pk_mgr_ptr->map_streamID_header.end()) {
		return true;
	}
	else {
		return false ;
	}
}


unsigned int bit_stream_httpout::getFlvTimeStamp(struct chunk_t *chunk_ptr)
{
	unsigned int timeStampInt=0;
	unsigned int *intPtr;
	unsigned int timeStampIntExtend=0;
	//video
	timeStampInt = *(int*) (chunk_ptr->buf +4); 
	intPtr= &(timeStampInt);
	timeStampInt=(unsigned int)ntohl(timeStampInt);
	timeStampInt = timeStampInt >> 8 ;
	//added TimestampExtend to timeStampInt
	timeStampIntExtend =  *(char*)(chunk_ptr->buf +7);
	timeStampInt=(timeStampIntExtend  << 24) +timeStampInt ;
	printf("%d\n",timeStampInt);
	//	if(*(char*)(chunk_ptr->buf) == 0x09 )  //video
	//	_log_ptr->write_log_format("s => s u \n", __FUNCTION__, "video timeStamp=", timeStampInt);
	//	else //audio
	//	 _log_ptr->write_log_format("s => s d \n", __FUNCTION__, "audio timeStamp=", timeStampInt);
	return timeStampInt;

}

void bit_stream_httpout::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;
	_log_ptr->write_log_format("s(u) s (s) \n", __FUNCTION__, __LINE__, "bit_stream_httpout", reason);

	//	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);

	for (fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if (*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}

	//	PAUSE
}