/*

*/

#include "peer.h"
#include "network.h"
#include "logger.h"
#include "pk_mgr.h"
#include "peer_mgr.h"

using namespace std;

peer::peer(list<int> *fd_list)
{
	queue_out_ctrl_ptr = NULL;
	queue_out_data_ptr = NULL;
	_send_byte = 0;
	_expect_len = 0;
	_offset = 0;
	_recv_byte_count = 0;
    _recv_parent_byte_count = 0;
	_time_start = 0;
	count = 0;
	avg_bandwidth = 0;
    parent_bandwidth = 0;
    parent_manifest = 0;
	fd_list_ptr = fd_list;
	_chunk_ptr = NULL;

/*
	for(unsigned long i = 0; i < BANDWIDTH_BUCKET; i++) {
		bandwidth_bucket[i] = 0;
	}

*/

}

peer::~peer() 
{
	clear_map();

}

void peer::clear_map()
{

	map<int, queue<struct chunk_t *> *>::iterator iter;	
	struct chunk_t *chunk_ptr = NULL;
	
	for (iter = map_fd_out_ctrl.begin(); iter != map_fd_out_ctrl.end(); iter++) {
		queue_out_ctrl_ptr = iter->second;
		while(queue_out_ctrl_ptr->size()) {
			chunk_ptr = queue_out_ctrl_ptr->front();
			queue_out_ctrl_ptr->pop();
			if(chunk_ptr)
				delete chunk_ptr;
		}

		if(queue_out_ctrl_ptr)
			delete queue_out_ctrl_ptr;
	}

	for (iter = map_fd_out_data.begin(); iter != map_fd_out_data.end(); iter++) {
		queue_out_data_ptr = iter->second;
		while(queue_out_data_ptr->size()) {
			queue_out_data_ptr->pop();
			if(chunk_ptr)
				delete chunk_ptr;
		}

		if(queue_out_data_ptr)
			delete queue_out_data_ptr;
	}

}

void peer::peer_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr *pk_mgr_ptr, peer_mgr *peer_mgr_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_net_ptr->peer_set(this);
}

//
void peer::handle_connect(int sock, struct chunk_t *chunk_ptr, struct sockaddr_in cin)
{

	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	struct peer_info_t *rescue_peer = NULL;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;

	chunk_request_ptr = (struct chunk_request_msg_t *)chunk_ptr;

	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	map_pid_fd[chunk_request_ptr->info.pid] = sock;
	map_fd_pid[sock] = chunk_request_ptr->info.pid;
	map_fd_out_ctrl[sock] = queue_out_ctrl_ptr;
	map_fd_out_data[sock] = queue_out_data_ptr;

	pid_peer_info_iter = _pk_mgr_ptr->map_pid_peer_info.find(chunk_request_ptr->info.pid);
	
	if(pid_peer_info_iter == _pk_mgr_ptr->map_pid_peer_info.end()) {
		rescue_peer = new struct peer_info_t;
		memset(rescue_peer, 0x0 , sizeof(struct peer_info_t));
		rescue_peer->pid = chunk_request_ptr->info.pid;
		rescue_peer->public_ip = cin.sin_addr.s_addr;
		rescue_peer->private_ip = chunk_request_ptr->info.private_ip;
		rescue_peer->tcp_port = chunk_request_ptr->info.tcp_port;
		rescue_peer->udp_port = chunk_request_ptr->info.udp_port;
		_pk_mgr_ptr->map_pid_rescue_peer_info[rescue_peer->pid] = rescue_peer;
	}
	
}


int peer::handle_connect_request(int sock, struct level_info_t *level_info_ptr, unsigned long pid)
{
	map<unsigned long, int>::iterator map_pid_fd_iter;
	
	queue_out_ctrl_ptr = new std::queue<struct chunk_t *>;
	queue_out_data_ptr = new std::queue<struct chunk_t *>;

	map_pid_fd_iter = map_pid_fd.find(level_info_ptr->pid);

	if(map_pid_fd_iter == map_pid_fd.end())
		map_pid_fd[level_info_ptr->pid] = sock;
	
	map_fd_pid[sock] = level_info_ptr->pid;
	map_fd_out_ctrl[sock] = queue_out_ctrl_ptr;
	map_fd_out_data[sock] = queue_out_data_ptr;
	
	struct chunk_t *chunk_ptr = NULL;
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	int send_byte;
	unsigned long channel_id;
	string svc_tcp_port("");
	string svc_udp_port("");
	
	_prep->read_key("channel_id", channel_id);
	_prep->read_key("svc_tcp_port", svc_tcp_port);
	_prep->read_key("svc_udp_port", svc_udp_port);
	
	chunk_request_ptr = (struct chunk_request_msg_t *)new unsigned char[sizeof(struct chunk_request_msg_t)];
	
	memset(chunk_request_ptr, 0x0, sizeof(struct chunk_request_msg_t));
		
	chunk_request_ptr->header.cmd = CHNK_CMD_PEER_CON;
	chunk_request_ptr->header.rsv_1 = REQUEST;
	chunk_request_ptr->header.length = sizeof(struct request_info_t);
	chunk_request_ptr->info.pid = pid;
	chunk_request_ptr->info.channel_id = channel_id;
	chunk_request_ptr->info.private_ip = _net_ptr->getLocalIpv4();
	chunk_request_ptr->info.tcp_port = (unsigned short)atoi(svc_tcp_port.c_str());
	chunk_request_ptr->info.udp_port = (unsigned short)atoi(svc_udp_port.c_str());

	send_byte = send(sock, (char *)chunk_request_ptr, sizeof(struct chunk_request_msg_t), 0);
	
	if( send_byte <= 0 ) {
		data_close(sock, "send html_buf error");		
		_log_ptr->exit(0, "send html_buf error");
		return -1;
	} else {
		if(chunk_request_ptr)
			delete chunk_request_ptr;
		return 0;
	}

}

int peer::handle_pkt_in(int sock)
{
	ftime(&interval_time);	//--!!0215
	unsigned long buf_len;
//	unsigned long i;
	int recv_byte;
	int expect_len;
	int offset = 0;
	unsigned long total_bit_rate = 0;
	unsigned long bandwidth = 0;
	int different = 0;
	unsigned long pid;
//	unsigned long manifest;
//	unsigned long n_alpha;
//	unsigned char reply;

	if(!_time_start) {
		_log_ptr->time_init();
		_time_start = 1;
	}

	struct chunk_t *chunk_ptr = NULL;
	struct chunk_header_t *chunk_header_ptr = NULL;
	
	chunk_header_ptr = new struct chunk_header_t;
	memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));
	
	expect_len = sizeof(struct chunk_header_t) ;
	
	while (1) {
		//cout << "in sock = " << sock << endl;
		recv_byte = recv(sock, (char *)chunk_header_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
				
			} else {
				DBG_PRINTF("here\n");
				data_close(sock, "recv error in peer::handle_pkt_in");
				//PAUSE
				return RET_SOCK_ERROR;
			}
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		
		if (!expect_len)
			break;
	}
	
	expect_len = chunk_header_ptr->length;
	//cout << "sequence_number = " << chunk_header_ptr->sequence_number << endl;
	buf_len = sizeof(struct chunk_header_t) + expect_len;
	//cout << "buf_len = " << buf_len << endl;
	chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

	if (!chunk_ptr) {
		data_close(sock, "memory not enough");
		_log_ptr->exit(0, "memory not enough");
		return RET_SOCK_ERROR;
	}

	memset(chunk_ptr, 0x0, buf_len);
		
	memcpy(chunk_ptr, chunk_header_ptr, sizeof(struct chunk_header_t));

	if(chunk_header_ptr)
		delete chunk_header_ptr;
	
	while (1) {
		recv_byte = recv(sock, (char *)chunk_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
			} else {

				data_close(sock, "recv error in peer::handle_pkt_in");

				return RET_SOCK_ERROR;
			}
		}

		expect_len -= recv_byte;
		offset += recv_byte;
		if (expect_len == 0)
			break;
	}

	offset = 0;

//recv ok  

	
//CHNK_CMD_PEER_DATA
	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_DATA) {


//the main handle
		_pk_mgr_ptr->handle_stream(chunk_ptr, sock);



		_recv_byte_count += chunk_ptr->header.length;

//?????  what's this?

//hidden at 2013/01/16
/*
        if(~parent_manifest & (1 << (chunk_ptr->header.sequence_number % _pk_mgr_ptr->sub_stream_num))){
            _recv_parent_byte_count += chunk_ptr->header.length; 
            //cout << "parent seq " << chunk_ptr->header.sequence_number << endl;
        }
*/

/*
		if(_log_ptr->handleAlarm()) {
			bandwidth_bucket[count % BANDWIDTH_BUCKET] = _recv_byte_count;
			count++;
			if(count >= BANDWIDTH_BUCKET) {
				avg_bandwidth = (unsigned long)((bandwidth_bucket[(count -1) % BANDWIDTH_BUCKET] - bandwidth_bucket[count % BANDWIDTH_BUCKET]) * 8 / 1000 / (BANDWIDTH_BUCKET - 1));
				cout << "avg bandwidth = " << avg_bandwidth << " kbps" <<endl;
				_pk_mgr_ptr->handle_bandwidth(avg_bandwidth);
			}
		}
*/


//hidden   at  2013/01/09
/*
		if(_log_ptr->handleAlarm()) {
			count++;
			if(count >= BANDWIDTH_BUCKET) {
				if(count == BANDWIDTH_BUCKET) {
					avg_bandwidth = _recv_byte_count * 8 / 1000 / BANDWIDTH_BUCKET;
                    parent_bandwidth = _recv_parent_byte_count * 8 / 1000 / BANDWIDTH_BUCKET;
					//cout << "avg bandwidth = " << avg_bandwidth << " kbps" << endl;
                    //cout << "parent bandwidth = " << parent_bandwidth << " kbps" << endl;
					_recv_byte_count = 0;
                    _recv_parent_byte_count = 0;
					_pk_mgr_ptr->handle_bandwidth(avg_bandwidth);
					
				} else {
					avg_bandwidth = avg_bandwidth * (BANDWIDTH_BUCKET - 1) / BANDWIDTH_BUCKET + _recv_byte_count * 8 / 1000 / BANDWIDTH_BUCKET; 
                    parent_bandwidth = parent_bandwidth * (BANDWIDTH_BUCKET - 1) / BANDWIDTH_BUCKET + _recv_parent_byte_count * 8 / 1000 / BANDWIDTH_BUCKET; 
					//cout << "avg bandwidth = " << avg_bandwidth << " kbps" << endl;
                    //cout << "parent bandwidth = " << parent_bandwidth << " kbps" << endl;
					_recv_byte_count = 0;
                    _recv_parent_byte_count = 0;
					_pk_mgr_ptr->handle_bandwidth(avg_bandwidth);
				}
			}
		}
*/
		



//cmd =CHNK_CMD_PEER_BWN
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_BWN) {

//hidden at  2013/01/09
/*
		if(count >= BANDWIDTH_BUCKET) {
			memcpy(&bandwidth, (char *)chunk_ptr + sizeof(struct chunk_t), sizeof(unsigned long));
			different = bandwidth - avg_bandwidth;
			//cout << "different = " << different << " kbps" << endl;
		}

		if(different > (int)(bandwidth / _pk_mgr_ptr->sub_stream_num )) {
            different = (bandwidth - parent_bandwidth) * 0.9;
			//n_alpha = (different /(int)(_pk_mgr_ptr->bit_rate / _pk_mgr_ptr->sub_stream_num)) + 1;
            n_alpha = (different /(int)(bandwidth / _pk_mgr_ptr->sub_stream_num)) + 1;
			cout << "rescue " << n_alpha << " alpha" << endl;
            cout << "need bandwidth = " << (bandwidth / _pk_mgr_ptr->sub_stream_num) <<endl;
            //cout << "bandwidth = " << bandwidth << " avg_bandwidth = " << avg_bandwidth <<endl;
			for(i = 0; i < n_alpha; i++) {
				manifest |= (1 << (_pk_mgr_ptr->sub_stream_num - (1 + i)));
			}
            parent_manifest = manifest;
			_pk_mgr_ptr->send_rescue(manifest);
		}
*/
		


//cmd = CHNK_CMD_PEER_RSC
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_RSC) {
//hidden at 2013/01/13
/*
		if(chunk_ptr->header.rsv_1 == REQUEST) {
			memcpy(&pid, (char *)chunk_ptr + sizeof(struct chunk_t), sizeof(unsigned long));
			memcpy(&manifest, ((char *)chunk_ptr + sizeof(struct chunk_t) + sizeof(unsigned long)), sizeof(unsigned long));
			_pk_mgr_ptr->handle_rescue(pid, manifest);
		} else if(chunk_ptr->header.rsv_1 == REPLY) {
			memcpy(&reply, (char *)chunk_ptr + sizeof(struct chunk_t), sizeof(unsigned char));
			memcpy(&manifest, ((char *)chunk_ptr + sizeof(struct chunk_t) + sizeof(unsigned char)), sizeof(unsigned long));
			if(reply == OK) {
//				_pk_mgr_ptr->send_rescue_to_upstream(manifest);
			} else {
			}
		}
*/




//cmd == CHNK_CMD_PEER_CUT
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_CUT) {
		//cout << "CHNK_CMD_PEER_CUT" << endl;
		memcpy(&pid, (char *)chunk_ptr + sizeof(struct chunk_t), sizeof(unsigned long));
		_peer_mgr_ptr->cut_rescue_downstream(pid);
		


//cmd == CHNK_CMD_PEER_LATENCY
	} else if(chunk_ptr->header.cmd == CHNK_CMD_PEER_LATENCY){
//hidden at 2013/01/16
/*
        unsigned long sec, usec, peer_id;
        unsigned long peer_num;

        memcpy(&sec, (char *)chunk_ptr + sizeof(struct chunk_t), sizeof(unsigned long));
        memcpy(&usec, (char *)chunk_ptr + sizeof(struct chunk_t) + sizeof(unsigned long), sizeof(unsigned long));
        memcpy(&peer_num, (char *)chunk_ptr + sizeof(struct chunk_t) + 2 * sizeof(unsigned long), sizeof(unsigned long));
        //printf("server time: %ld.%06ld\n", sec, usec);
        for(int i = 0;i < peer_num;i ++){
            memcpy(&peer_id, (char *)chunk_ptr + sizeof(struct chunk_t) + 3 * sizeof(unsigned long) + i * sizeof(struct peer_timestamp_info_t), sizeof(unsigned long));
            memcpy(&sec, (char *)chunk_ptr + sizeof(struct chunk_t) + 4 * sizeof(unsigned long) + i * sizeof(struct peer_timestamp_info_t), sizeof(unsigned long));
            memcpy(&usec, (char *)chunk_ptr + sizeof(struct chunk_t) + 5 * sizeof(unsigned long) + i * sizeof(struct peer_timestamp_info_t), sizeof(unsigned long));
            //printf("peer %d time: %ld.%03ld\n", peer_id, sec, usec);
        }

        _pk_mgr_ptr->handle_latency(chunk_ptr, sock);
*/


//cmd == CHNK_CMD_RT_NLM
    } else if(chunk_ptr->header.cmd == CHNK_CMD_RT_NLM) {	//--!! 0128

//hidden at 2013/01/16
/*

		map<unsigned long, int>::iterator pid_fd_iter;
		map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
		queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
		if(chunk_ptr->header.rsv_1 ==  REQUEST) {
			if(_pk_mgr_ptr->current_child_pid == 0) {	//reply
				struct chunk_t *reply_msg_ptr = chunk_ptr;
				reply_msg_ptr->header.rsv_1 = REPLY;
				//send $(*reply_msg_ptr) to his parent
				if (_pk_mgr_ptr->level_msg_ptr->level == 0) {
					//to lightning
					_net_ptr->send(_pk_mgr_ptr->get_sock(), (char *)reply_msg_ptr, sizeof(struct chunk_header_t) + chunk_ptr->header.length, 0);
					if (chunk_ptr)
						delete [] (unsigned char*)chunk_ptr;
					//no check 4 success
				} else {
					pid_fd_iter = map_pid_fd.find(_pk_mgr_ptr->level_msg_ptr->level_info[_pk_mgr_ptr->level_msg_ptr->level-1]->pid);
					fd_queue_iter = map_fd_out_ctrl.find(pid_fd_iter->second);
					if (pid_fd_iter == map_pid_fd.end() || fd_queue_iter == map_fd_out_ctrl.end()) {
						if (chunk_ptr)
							delete [] (unsigned char*)chunk_ptr;
						return RET_OK;
					}
					queue_out_ctrl_ptr = fd_queue_iter->second;
					queue_out_ctrl_ptr->push((struct chunk_t *)reply_msg_ptr);
					_net_ptr->epoll_control(pid_fd_iter->second, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
				}
			} else {	//--!! 0218
				struct timeb tmpt;
				ftime(&tmpt);
				//check if sending 
				pid_fd_iter = map_pid_fd.find(_pk_mgr_ptr->current_child_pid);
				if (pid_fd_iter != map_pid_fd.end()) {
					fd_queue_iter = map_fd_out_ctrl.find(pid_fd_iter->second);
					if (fd_queue_iter != map_fd_out_ctrl.end()) {
						//send $(*req_msg_ptr) to his child !!--!!0128
						int chunk_size = sizeof(struct chunk_header_t) + chunk_ptr->header.length;
						struct chunk_rt_latency_t *req_msg_ptr = (struct chunk_rt_latency_t *) new unsigned char[(chunk_size + sizeof(struct ts_block_t))];
						struct ts_block_t *ts_ptr =(struct ts_block_t*)( (char*)req_msg_ptr + chunk_size);
				
						memset(ts_ptr, 0x0, sizeof(struct ts_block_t));
						memcpy(req_msg_ptr, chunk_ptr, chunk_size);
						//cout << "req_msg_ptr->dts_length = " << req_msg_ptr->dts_length << endl;
						//cout << "req_msg_ptr->dts_offset = " << req_msg_ptr->dts_offset << endl;
						req_msg_ptr->dts_offset += sizeof(struct ts_block_t);
						ts_ptr->pid = _pk_mgr_ptr->level_msg_ptr->pid;
						ts_ptr->time_stamp = tmpt.time * 1000ull + tmpt.millitm;
						req_msg_ptr->header.length += sizeof(struct ts_block_t);
						
						queue_out_ctrl_ptr = fd_queue_iter->second;
						queue_out_ctrl_ptr->push((struct chunk_t *)req_msg_ptr);
						_net_ptr->epoll_control(pid_fd_iter->second, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
					}
				}
				if(chunk_ptr)	//--!! 0218 
					delete [] (unsigned char*)chunk_ptr;
			}
		} else {
				struct chunk_rt_latency_t *reply_msg_ptr = (struct chunk_rt_latency_t*) chunk_ptr;
				struct ts_block_t *ts_ptr = (struct ts_block_t*)( (char*)reply_msg_ptr->buf + reply_msg_ptr->dts_offset - sizeof(struct ts_block_t));
				struct timeb tmpt;
				//if() compare if pid equals
				if (ts_ptr->pid == _pk_mgr_ptr->level_msg_ptr->pid) {
					ftime(&tmpt);
					ts_ptr->time_stamp = tmpt.time * 1000ull + tmpt.millitm - ts_ptr->time_stamp;
					ts_ptr->isDST = REPLY;
					
					reply_msg_ptr->dts_offset = reply_msg_ptr->dts_offset - sizeof(struct ts_block_t);
					reply_msg_ptr->dts_length = reply_msg_ptr->dts_length + sizeof(struct ts_block_t);

					//transmit to his parent
					if (_pk_mgr_ptr->level_msg_ptr->level == 0) {
						//to lightning
						_net_ptr->send(_pk_mgr_ptr->get_sock(), (char *)reply_msg_ptr, sizeof(struct chunk_header_t) + chunk_ptr->header.length, 0);
						if (chunk_ptr)
							delete [] (unsigned char*)chunk_ptr;
						//no check 4 success
					} else {
						pid_fd_iter = map_pid_fd.find(_pk_mgr_ptr->level_msg_ptr->level_info[_pk_mgr_ptr->level_msg_ptr->level-1]->pid);
						fd_queue_iter = map_fd_out_ctrl.find(pid_fd_iter->second);
						if (pid_fd_iter == map_pid_fd.end() || fd_queue_iter == map_fd_out_ctrl.end()) {
							if (chunk_ptr)
								delete [] (unsigned char*)chunk_ptr;
							return RET_OK;
						}
						queue_out_ctrl_ptr = fd_queue_iter->second;
						queue_out_ctrl_ptr->push((struct chunk_t *)reply_msg_ptr);
						_net_ptr->epoll_control(pid_fd_iter->second, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
					}
				} else {
					cout << "should be the same, what's going on?" << endl;
				}
		}
	    unsigned long tmp =  interval_time.time * 1000ull + interval_time.millitm;
	    ftime(&interval_time);
	    tmp = interval_time.time * 1000ull + interval_time.millitm - tmp;
	    cout<<"interval delay = "<<tmp<<endl;
*/

	} else {
		cout << "what's this?" << endl;
	}


	if (chunk_ptr)
		delete [] (unsigned char*)chunk_ptr;



	return RET_OK;
}

 
//°equeue_out_ctrl_ptr ©Mqueue_out_data_ptr¥X¥h
int peer::handle_pkt_out(int sock)
{
	struct chunk_t *chunk_ptr;
	
	map<int, queue<struct chunk_t *> *>::iterator fd_out_ctrl_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_out_data_iter;

	fd_out_ctrl_iter = map_fd_out_ctrl.find(sock);
	
	if(fd_out_ctrl_iter != map_fd_out_ctrl.end()) {
		queue_out_ctrl_ptr = fd_out_ctrl_iter->second;
	}
	
	fd_out_data_iter = map_fd_out_data.find(sock);

	if(fd_out_data_iter != map_fd_out_data.end()) {
		queue_out_data_ptr = fd_out_data_iter->second;
	}
	
	if(queue_out_ctrl_ptr->size() != 0 && _chunk_ptr == NULL) {
		chunk_ptr = queue_out_ctrl_ptr->front();
		_expect_len = chunk_ptr->header.length + sizeof(struct chunk_header_t);
		_expect_len = _expect_len - _offset;
		_send_byte = send(sock, (char *)chunk_ptr+_offset, _expect_len, 0);
		if(_send_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				return RET_SOCK_ERROR;
			} else {
				data_close(sock, "error occured in send queue_out_ctrl");
				return RET_SOCK_ERROR;
			}
		} else {
			_offset += _send_byte;
			_expect_len = _expect_len - _send_byte;
			if(_expect_len == 0) {
				queue_out_ctrl_ptr->pop();
				_offset = 0;
				if(chunk_ptr)
					delete chunk_ptr;
			}
		}
		
	} else if(queue_out_data_ptr->size() != 0) {
        //cout << "data buffer size =" << queue_out_data_ptr->size() <<endl;
		chunk_ptr = queue_out_data_ptr->front();
		_chunk_ptr = chunk_ptr;
		_expect_len = chunk_ptr->header.length + sizeof(struct chunk_header_t);
		_expect_len = _expect_len - _offset;
		_send_byte = send(sock, (char *)chunk_ptr+_offset, _expect_len, 0);
		if(_send_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				return RET_SOCK_ERROR;
			} else {
				data_close(sock, "error occured in send queue_out_data");
				//PAUSE
				return RET_SOCK_ERROR;
			}
		} else {
			_offset += _send_byte;
			_expect_len = _expect_len - _send_byte;
			if(_expect_len == 0) {
				queue_out_data_ptr->pop();
				_offset = 0;
				_chunk_ptr = NULL;
			}
		}
	
	} else {
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
	}

	return RET_OK;
}

void peer::handle_pkt_error(int sock)
{

}

void peer::handle_sock_error(int sock, basic_class *bcptr)
{

}

void peer::handle_job_realtime()
{

}

void peer::handle_job_timer()
{

}

void peer::data_close(int cfd, const char *reason) 
{
	unsigned long pid = 0;
	unsigned long manifest = 0;
	list<int>::iterator fd_iter;
	int sockfd;
	list<unsigned long>::iterator rescue_pid_iter;
	//list<unsigned long>::iterator rescue_pid_list_iter;
	map<int, queue<struct chunk_t *> *>::iterator map_fd_queue_iter;
	map<int , unsigned long>::iterator map_fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<int, int>::iterator map_rescue_fd_count_iter;
	map<int, int>::iterator map_rescue_fd_count_iter2;
	
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "pk", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".." << endl;
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);
	
	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == cfd) {
			fd_list_ptr->erase(fd_iter);
			break;
		}
	}

	map_fd_pid_iter = map_fd_pid.find(cfd);
	if(map_fd_pid_iter != map_fd_pid.end()) {
		pid = map_fd_pid_iter->second;
		map_fd_pid.erase(map_fd_pid_iter);
	}

	map_pid_fd_iter = map_pid_fd.find(pid);

	if(map_pid_fd_iter != map_pid_fd.end()) 
		map_pid_fd.erase(map_pid_fd_iter);

	map_fd_queue_iter = map_fd_out_ctrl.find(cfd);

	if(map_fd_queue_iter != map_fd_out_ctrl.end()) 
		map_fd_out_ctrl.erase(map_fd_queue_iter);
	
	map_fd_queue_iter = map_fd_out_data.find(cfd);

	if(map_fd_queue_iter != map_fd_out_data.end()) 
		map_fd_out_data.erase(map_fd_queue_iter);

	for(rescue_pid_iter = _peer_mgr_ptr->rescue_pid_list.begin(); rescue_pid_iter != _peer_mgr_ptr->rescue_pid_list.end(); rescue_pid_iter++) {
		if(*rescue_pid_iter == pid) {
			_peer_mgr_ptr->rescue_pid_list.erase(rescue_pid_iter);
			cout << "rescue_pid_list_size = " << _peer_mgr_ptr->rescue_pid_list.size() << endl;
			DBG_PRINTF("here\n");
			break;
		}
	}


	map_rescue_fd_count_iter = _peer_mgr_ptr->map_rescue_fd_count.find(cfd);

	if(map_rescue_fd_count_iter != _peer_mgr_ptr->map_rescue_fd_count.end()) {

		if(_peer_mgr_ptr->map_rescue_fd_count.size() == 1) {

			_peer_mgr_ptr->map_rescue_fd_count.erase(map_rescue_fd_count_iter);
//			_pk_mgr_ptr->send_rescue_to_upstream(manifest);
		} else if(_peer_mgr_ptr->map_rescue_fd_count.size() == 2) {

			for(map_rescue_fd_count_iter2 = _peer_mgr_ptr->map_rescue_fd_count.begin(); map_rescue_fd_count_iter2 != _peer_mgr_ptr->map_rescue_fd_count.end(); map_rescue_fd_count_iter2++) {
				if(map_rescue_fd_count_iter2->first != cfd) {
					sockfd = map_rescue_fd_count_iter2->first;
					_peer_mgr_ptr->map_rescue_fd_count[sockfd] = WIN_COUNTER;

				} else {
					map_rescue_fd_count_iter = map_rescue_fd_count_iter2;
				}
			}
			_peer_mgr_ptr->map_rescue_fd_count.erase(map_rescue_fd_count_iter);
			cout << "map_rescue_fd_count_size = " << _peer_mgr_ptr->map_rescue_fd_count.size() << endl;

		} else {

			_peer_mgr_ptr->map_rescue_fd_count.erase(map_rescue_fd_count_iter);
		}
	} else {

	}
	

	map_fd_queue_iter = _peer_mgr_ptr->_map_fd_downstream.find(cfd);

		
	map_fd_pid_iter = _peer_mgr_ptr->map_fd_pid.find(cfd);

		
	if(map_fd_queue_iter != _peer_mgr_ptr->_map_fd_downstream.end()) {

		_peer_mgr_ptr->_map_fd_downstream.erase(map_fd_queue_iter);
	}

	if(map_fd_pid_iter != _peer_mgr_ptr->map_fd_pid.end()) {
		DBG_PRINTF("here\n");
		//if(map_fd_queue_iter == _peer_mgr_ptr->_map_fd_downstream.end()) {
			//_pk_mgr_ptr->send_rescue_to_upstream(manifest);
		//} else {
			_peer_mgr_ptr->map_fd_pid.erase(map_fd_pid_iter);
		//}
	}
	
}
































