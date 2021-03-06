#include "bit_stream_out.h"
#include "../bit_stream_server.h"
#include "../network.h"
#include "../logger.h"
#include "../pk_mgr.h"
#include <sstream>

//this is a implement sample 
//this class should implement add chunk and handle_pkt_out
//if socket error shoule use _bit_stream_server_ptr del map , close sock and del this_ptr 


bit_stream_out::bit_stream_out(int stream_id,network *net_ptr, logger *log_ptr,bit_stream_server *bit_stream_server_ptr ,pk_mgr *pk_mgr_ptr, list<int> *fd_list){
		_stream_id = stream_id;
		_net_ptr = net_ptr;
		_log_ptr = log_ptr;
		_pk_mgr_ptr = pk_mgr_ptr;
		fd_list_ptr = fd_list;
		_bit_stream_server_ptr = bit_stream_server_ptr;
		_reqStreamID =0 ;
		_queue_out_data_ptr = new std::queue<struct chunk_t *>;
		if(!_queue_out_data_ptr){
			printf("!_queue_out_data_ptr in bit_stream_outerror\n");
			_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"!_queue_out_data_ptr in bit_stream_outerror");
			PAUSE
		}
		memset(&_send_ctl_info, 0x00, sizeof(_send_ctl_info));
		first_Header=true;

		file_ptr = fopen("./FILEOUT" , "wb");




	}
bit_stream_out::~bit_stream_out(){
	if(_queue_out_data_ptr)
		delete _queue_out_data_ptr;
	_queue_out_data_ptr=NULL;
	}

void bit_stream_out::init(){
	}
	
int bit_stream_out::handle_pkt_in(int sock){
		return -1;
	}
	
int bit_stream_out::handle_pkt_out(int sock){

	int send_rt_val; //send return value
	send_rt_val=0;

	if(first_Header){
	cout << "============= Acceppt New 127.0.0.1 ============"<<endl;
//	int sendHeaderBytes = _net_ptr->send(sock,httpHeader,sizeof(httpHeader) -1 ,0);  //-1 to subtract '\0'
//		cout << "send_HttpHeaderBytes=" << sendHeaderBytes<<endl;
//		sendHeaderBytes = _net_ptr->send(sock,(char *)FLV_Header,sizeof(FLV_Header),0);
//	sendHeaderBytes = _net_ptr->send(sock,(char *)flvHeader,sizeof(flvHeader),0);

	map<int, struct update_stream_header *>::iterator  map_streamID_header_iter;
	struct update_stream_header *protocol_header =NULL;
	map_streamID_header_iter =_pk_mgr_ptr ->map_streamID_header.find(_reqStreamID);

	if(map_streamID_header_iter !=_pk_mgr_ptr ->map_streamID_header.end()){
		protocol_header = map_streamID_header_iter ->second ;
	}else{
		_log_ptr->write_log_format("s => s \n", __FUNCTION__, "protocol_header = map_streamID_header_iter ->second ");
		*(_net_ptr->_errorRestartFlag)=RESTART;
		PAUSE
	}



	printf("protocol_header ->len %d \n",protocol_header ->len);
	if(protocol_header ->len >0){

	int sendHeaderBytes = _net_ptr ->send(sock,(char *)protocol_header->header,protocol_header ->len,0);
	cout << "send_FLVHeaderBytes=" << sendHeaderBytes<<endl;
	}

	first_Header =false;


	}







//	while(true){

	if (_send_ctl_info.ctl_state == READY) {
		size_t send_size;

		struct chunk_t *chunk_ptr;

		
		if (!_queue_out_data_ptr->size()) {
//			_log_ptr->write_log_format("s => s \n", __FUNCTION__, "_queue_out_data_ptr->size =0");
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);	
			return RET_OK;
		}

		chunk_ptr = (struct chunk_t *)_queue_out_data_ptr->front();

		fwrite(chunk_ptr->buf,1,chunk_ptr->header.length,file_ptr);

		_queue_out_data_ptr->pop();
		send_size = chunk_ptr->header.length;
		
		_send_ctl_info.offset = 0;
		_send_ctl_info.total_len = send_size;
		_send_ctl_info.expect_len = send_size;
		_send_ctl_info.buffer = (char *)chunk_ptr->buf;
		_send_ctl_info.chunk_ptr = (chunk_t *)chunk_ptr;
		_send_ctl_info.serial_num = chunk_ptr->header.sequence_number;


		send_rt_val = _net_ptr->nonblock_send(sock, &_send_ctl_info);



//_log_ptr->write_log_format("s => s d ( d )\n", __FUNCTION__, "sent pkt sequence_number", chunk_ptr ->header.sequence_number, send_rt_val);
		if(send_rt_val<0){
			printf("socket error number=%d\n",WSAGetLastError());
			if((WSAGetLastError()==WSAEWOULDBLOCK )){								//buff full
				//it not a error
				_send_ctl_info.ctl_state = RUNNING;
//				_queue_out_data_ptr->pop();
				}else{
						printf("delete map and bit_stream_out_ptr");
						_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
						data_close(sock,"here sock error");
						_bit_stream_server_ptr -> delBitStreamOut(this);

					}

			}
		//Log(LOGDEBUG, "%s: send_rt = %d, expect_len = %d", __FUNCTION__, send_rt_val,_send_ctl_info.expect_len);
		switch (send_rt_val) {
			case RET_SOCK_ERROR:
				printf("%s, socket error\n", __FUNCTION__);
//				return RET_OK;
				return RET_SOCK_ERROR;
			default:
				return RET_OK;
		}
		
	} else { //_send_ctl_info._send_ctl_state is RUNNING

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
//	}//end while (1)
	}
void bit_stream_out::handle_pkt_error(int sock){
	}
void bit_stream_out::handle_job_realtime(){
	}
void bit_stream_out::handle_job_timer(){
	}

void bit_stream_out::set_client_sockaddr(struct sockaddr_in *cin)
{
	if (cin)
	memcpy(&_cin_tcp, cin, sizeof(struct sockaddr_in));
}

//call by pk_mgr
void bit_stream_out::add_chunk(struct chunk_t *chunk)
{
	_queue_out_data_ptr->push(chunk);
	
}

unsigned char bit_stream_out::get_stream_pk_id()
{
    return -1;
}




void bit_stream_out::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;

	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "bit_stream_httpout", reason);
	cout << "bit_stream_httpout Client " << cfd << " exit by " << reason << ".." << endl;
//	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);

	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}

//	PAUSE
}