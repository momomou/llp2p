#include "bit_stream_httpout.h"
#include "../bit_stream_server.h"
#include "../network.h"
#include "../logger.h"
#include "../pk_mgr.h"
#include <sstream>


const	char httpHeader[]=	"HTTP/1.1 200 OK\r\n" 
							"Date: Mon, 22 Oct 2012 18:46:42 GMT"
							"Server: Apache/2.2.8 (Win32) PHP/5.2.6"
							"Connection: close\r\n"
							"Content-Type: application/octet-stream\r\n"
							"\r\n";


const  char FLV_Header[] = { 0x46,0x4c,0x56,0x01,0x05,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x00,0x12,0x00,0x00
							,0xb6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x0a,0x6f,0x6e,0x4d,0x65,0x74
							,0x61,0x44,0x61,0x74,0x61,0x08,0x00,0x00,0x00,0x07,0x00,0x08,0x64,0x75,0x72,0x61
							,0x74,0x69,0x6f,0x6e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x77
							,0x69,0x64,0x74,0x68,0x00,0x40,0x84,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x68
							,0x65,0x69,0x67,0x68,0x74,0x00,0x40,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09
							,0x66,0x72,0x61,0x6d,0x65,0x72,0x61,0x74,0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00
							,0x00,0x00,0x00,0x0c,0x76,0x69,0x64,0x65,0x6f,0x63,0x6f,0x64,0x65,0x63,0x69,0x64
							,0x00,0x40,0x1c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0x76,0x69,0x64,0x65,0x6f
							,0x64,0x61,0x74,0x61,0x72,0x61,0x74,0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
							,0x00,0x00,0x07,0x65,0x6e,0x63,0x6f,0x64,0x65,0x72,0x02,0x00,0x0b,0x4c,0x61,0x76
							,0x66,0x35,0x32,0x2e,0x38,0x37,0x2e,0x31,0x00,0x08,0x66,0x69,0x6c,0x65,0x73,0x69
							,0x7a,0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x00,0x00
							,0x00,0xc1};


bit_stream_httpout::bit_stream_httpout(int stream_id,network *net_ptr, logger *log_ptr,bit_stream_server *bit_stream_server_ptr ,pk_mgr *pk_mgr_ptr, list<int> *fd_list,int acceptfd){

		_reqStreamID=-1;
		first_pkt=true;
		first_HTTP_Header=true;
		
		_stream_id = stream_id;
		_net_ptr = net_ptr;
		_log_ptr = log_ptr;
		_pk_mgr_ptr = pk_mgr_ptr;
		fd_list_ptr = fd_list;
		_bit_stream_server_ptr = bit_stream_server_ptr;

		_queue_out_data_ptr = new std::queue<struct chunk_t *>;	
		memset(&_send_ctl_info, 0x00, sizeof(_send_ctl_info));

//for debug
//		file_ptr = fopen("./here.flv" , "wb");
//		file_ptr_test = fopen("./metadata" , "rb");
//		fwrite(FLV_Header,1,sizeof(FLV_Header),file_ptr);


	}
bit_stream_httpout::~bit_stream_httpout(){
	printf("==============deldet bit_stream_httpout success==========\n");
	if(_queue_out_data_ptr)
	delete _queue_out_data_ptr;
	}

void bit_stream_httpout::init(){
	}
	


//只有一開始第一次接收player的http request 接收後即關閉監聽
int bit_stream_httpout::handle_pkt_in(int sock){
		
		int recv_byte;
		char HTTPrequestBuffer[512]={0x0};
		_net_ptr->set_nonblocking(sock);

		recv_byte = _net_ptr ->recv(sock, HTTPrequestBuffer,  sizeof(HTTPrequestBuffer), 0);

		_reqStreamID= getStreamID_FromHTTP_Request(HTTPrequestBuffer,sizeof(HTTPrequestBuffer));
		printf("\n HTTP  _reqStreamID =%d\n",_reqStreamID);

		if(isStreamID_inChannel(_reqStreamID)){						//得到streamID 並關掉監聽
			_net_ptr->epoll_control(sock, EPOLL_CTL_DEL,EPOLLIN);	
		}else{														//stream ID 錯誤 或player 多次連線
			printf("delete map and bit_stream_httpout_ptr\n");
			_net_ptr->epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN | EPOLLOUT);	
			_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
			_pk_mgr_ptr ->data_close(sock," bit_stream_httpout ");
			delete this;
			}

		return RET_OK;
	}
	

//第一次傳送http header 和flv headerd,然後等到第一個keyframe後才開始丟給player 
//判斷delay是否發生並適當的做取樣,接著就把queue 的資料傳出去
int bit_stream_httpout::handle_pkt_out(int sock){

	int send_rt_val = 0; //send return value

	//here is http header and flv header
	if(first_HTTP_Header){
		cout << "============= Acceppt New Player ============"<<endl;
		int sendHeaderBytes = _net_ptr->send(sock,httpHeader,sizeof(httpHeader) -1 ,0);  //-1 to subtract '\0'
		cout << "send_HttpHeaderBytes=" << sendHeaderBytes<<endl;
		sendHeaderBytes = _net_ptr->send(sock,FLV_Header,sizeof(FLV_Header),0);
		cout << "send_FLVHeaderBytes=" << sendHeaderBytes<<endl;
		first_HTTP_Header =false;
	}


	while(true){

	if (_send_ctl_info.ctl_state == READY) {
		size_t send_size;

		struct chunk_bitstream_t *chunk_ptr;

		if (!_queue_out_data_ptr->size()) {
			_log_ptr->write_log_format("s => s \n", __FUNCTION__, "_queue_out_data_ptr->size =0");
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);	
			return RET_OK;
		}

		chunk_ptr = (chunk_bitstream_t *)_queue_out_data_ptr->front();


//pop until get the first keyframe
		while(first_pkt){
			if(_queue_out_data_ptr ->size() >=10){
				for(int i=0;i<5;i++){
					if(!isKeyFrame(chunk_ptr )){
					_queue_out_data_ptr->pop();
					chunk_ptr = (chunk_bitstream_t *)_queue_out_data_ptr->front();
					}else{
						_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "First Key Frame is ", chunk_ptr ->header.sequence_number);
						first_pkt=false;
						break;
					}
				return RET_OK;
				}
			}else
				return RET_OK;
		}


//here is to down-sampling
		int sentSequenceNumber = chunk_ptr ->header.sequence_number ;
		int differenceValue = (_pk_mgr_ptr ->_least_sequence_number -sentSequenceNumber);
		if (differenceValue > 100){ //if (recv -sent)diff >100 pkt pop until  queue <=30 and continuance pop until last key frame
			while(_queue_out_data_ptr ->size() <=30){
				_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "POP queue ", _queue_out_data_ptr ->size());
				_queue_out_data_ptr->pop();
			}
			chunk_ptr = (chunk_bitstream_t *)_queue_out_data_ptr->front();
			while(! isKeyFrame(chunk_ptr )){
				_queue_out_data_ptr->pop();
				_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "POP queue ", _queue_out_data_ptr ->size());
				chunk_ptr = (chunk_bitstream_t *)_queue_out_data_ptr->front();
				if(_queue_out_data_ptr ->size() <=10)
					break;
				}
		}
		else if(differenceValue <=100 && differenceValue >40){  
			  	if(! isKeyFrame(chunk_ptr) &&  (sentSequenceNumber % 3 == 0) ){   //not key frame &&  sampling by 1/3
					_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "pkt discard 1/3", chunk_ptr ->header.sequence_number);
					_queue_out_data_ptr->pop();
					chunk_ptr = (chunk_bitstream_t *)_queue_out_data_ptr->front();			 //ignore and not send
					}
		}else if(differenceValue <=40 && differenceValue >20){
				if(! isKeyFrame(chunk_ptr) &&  (sentSequenceNumber % 5 == 0) ){   //not key frame &&  sampling by 1/5
					_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "pkt discard 1/5", chunk_ptr ->header.sequence_number);
					_queue_out_data_ptr->pop();
					chunk_ptr = (chunk_bitstream_t *)_queue_out_data_ptr->front();			 //ignore and not send
					}
		}else if (differenceValue <=20 && differenceValue >5){
				if(! isKeyFrame(chunk_ptr) &&  (sentSequenceNumber % 7 == 0) ){   //not key frame &&  sampling by 1/7
					_log_ptr->write_log_format("s => s d\n", __FUNCTION__, "pkt discard 1/7", chunk_ptr ->header.sequence_number);
					_queue_out_data_ptr->pop();
					chunk_ptr = (chunk_bitstream_t *)_queue_out_data_ptr->front();			 //ignore and not send
					}	
		}



//for debug
//		fwrite(chunk_ptr->buf,1,chunk_ptr->header.length,file_ptr);
//		_log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "write pkt", chunk_ptr->header.length, _stream_id);
		

//to set preTag len=0
//		memset( chunk_ptr->buf + (chunk_ptr->header.length) -4 ,0x0,4);

//		unsigned int stamp=getFlvTimeStamp(chunk_ptr);

		_queue_out_data_ptr->pop();
		send_size = chunk_ptr->header.length;
		
		_send_ctl_info.offset = 0;
		_send_ctl_info.total_len = send_size;
		_send_ctl_info.expect_len = send_size;
		_send_ctl_info.buffer = (char *)chunk_ptr->buf;
		_send_ctl_info.rtmp_chunk = (chunk_rtmp_t *)chunk_ptr;
		_send_ctl_info.serial_num = chunk_ptr->header.sequence_number;

		send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);

	
		
_log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "sent pkt sequence_number", chunk_ptr ->header.sequence_number, send_rt_val);


		if(send_rt_val<0){
			printf("socket error number=%d\n",WSAGetLastError());
			if((WSAGetLastError()==WSAEWOULDBLOCK )){								//buff full
				//it not a error
				_send_ctl_info.ctl_state = READY;
				_queue_out_data_ptr->pop();
				continue;
			}else{
					printf("delete map and bit_stream_httpout_ptr\n");
					_net_ptr->epoll_control(sock, EPOLL_CTL_DEL, EPOLLIN | EPOLLOUT);	
					_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
					_pk_mgr_ptr ->data_close(sock," bit_stream_httpout ");
					delete this;
					}

			}

		switch (send_rt_val) {
			case RET_SOCK_ERROR:
				printf("%s, socket error\n", __FUNCTION__);
//				return RET_OK;
				return RET_SOCK_ERROR;
			default:
				return RET_OK;
		}
		
	} else { //_send_ctl_info._send_ctl_state is RUNNING
//        _log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "sent pkt", _send_ctl_info.serial_num, _stream_id);

		send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);
//_log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "sent pkt", send_rt_val, _stream_id);

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
		}
	}//end while (1)
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
	if (cin)
	memcpy(&_cin_tcp, cin, sizeof(struct sockaddr_in));
}

void bit_stream_httpout::add_chunk(struct chunk_t *chunk)
{
	_queue_out_data_ptr->push(chunk);
	
}

unsigned char bit_stream_httpout::get_stream_pk_id()
{
    return -1;
}



bool bit_stream_httpout::isKeyFrame(struct chunk_bitstream_t *chunk_ptr)
{
	char flvBitFlag;
	if(*(char*)(chunk_ptr->buf) == 0x09){		//video
	flvBitFlag = *(char*) ((chunk_ptr->buf) + 11);  //get first byte
		if ( ( (flvBitFlag & 0xf0 ) >> 4 ) == 0x01 ) {
		return true;  //it's key frame
		}else 	
		return false;
	}else{			//audio
	
	return false;
	}

}

//return  StreamID  ,streamID >0 ,bufferSize 暫時保留沒用到(習慣上會將陣列的大小一起傳進function)
//example http://127.0.0.1:3000/8877.flv  streamID= 8877
int  bit_stream_httpout::getStreamID_FromHTTP_Request(char *httpBuffer,unsigned long bufferSize )
{
	char *ptr=NULL;
	int streamID;
	ptr = strstr(httpBuffer ,"GET /");
	if(!ptr)
	return -1;//return undefine ID
	else{
	ptr+=5;
	char temp[5]={'0x0','0x0','0x0','0x0','\0'};
	memcpy(temp,ptr,4);
	streamID = atoi(temp);
	return streamID;
	}
}

// T=1/F=0
bool bit_stream_httpout::isStreamID_inChannel(int streamid)
{
	list<int>::iterator IDiter;
	if(streamid < 0)
	return false;
	for(IDiter = _pk_mgr_ptr ->streamID_list.begin() ; IDiter !=_pk_mgr_ptr ->streamID_list.end() ;IDiter++){
	if(streamid== *IDiter)
	return true;
	}
	return false;

}


unsigned int bit_stream_httpout::getFlvTimeStamp(struct chunk_bitstream_t *chunk_ptr)
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