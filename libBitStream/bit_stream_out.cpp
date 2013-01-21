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
		
		_queue_out_data_ptr = new std::queue<struct chunk_t *>;
		memset(&_send_ctl_info, 0x00, sizeof(_send_ctl_info));


	}
bit_stream_out::~bit_stream_out(){
	if(_queue_out_data_ptr)
	delete _queue_out_data_ptr;
	}

void bit_stream_out::init(){
	}
	
int bit_stream_out::handle_pkt_in(int sock){
		return -1;
	}
	
int bit_stream_out::handle_pkt_out(int sock){

	int send_rt_val; //send return value
	send_rt_val=0;

	while(true){

	if (_send_ctl_info.ctl_state == READY) {
		size_t send_size;

		struct chunk_bitstream_t *chunk_ptr;

		
		if (!_queue_out_data_ptr->size()) {
			_log_ptr->write_log_format("s => s \n", __FUNCTION__, "_queue_out_data_ptr->size =0");
			_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);	
			return RET_OK;
		}

		chunk_ptr = (struct chunk_bitstream_t *)_queue_out_data_ptr->front();


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
				}else{
						printf("delete map and bit_stream_out_ptr");
						_pk_mgr_ptr ->del_stream(sock,(stream*)this, STRM_TYPE_MEDIA);
						_pk_mgr_ptr ->data_close(sock,"here sock error");
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
	}//end while (1)
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

