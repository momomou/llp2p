#include "peer_mgr.h"
#include "peer.h"
#include "network.h"
#include "logger.h"
#include "pk_mgr.h"

using namespace std;


peer_mgr::peer_mgr(list<int> *fd_list)
{
	peer_ptr = NULL;
	fd_list_ptr = fd_list;
	_peer_list_member = 0;
    self_public_ip = 0;
}

peer_mgr::~peer_mgr() 
{

	if(peer_ptr)
		delete peer_ptr;

}


void peer_mgr::peer_mgr_set(network *net_ptr , logger *log_ptr , configuration *prep, pk_mgr * pk_mgr_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
	_pk_mgr_ptr = pk_mgr_ptr;
	peer_ptr = new peer(fd_list_ptr);
	peer_ptr->peer_set(_net_ptr, _log_ptr, _prep, _pk_mgr_ptr, this);		
}


void peer_mgr::connect_peer(struct chunk_level_msg_t *level_msg_ptr, unsigned long pid)
{
    set_up_public_ip(level_msg_ptr);

	for(unsigned long i = 0; i < _pk_mgr_ptr->lane_member; i++) {
		if((level_msg_ptr->level_info[i])->pid != level_msg_ptr->pid) {
			if (build_connection(level_msg_ptr->level_info[i], pid)) {
				cout << "peer_mgr build_connection() success" << endl;
			} else {
				cout << "peer_mgr build_connection() fail" << endl;
				PAUSE
				exit(0);
			}

		}
	}

}

int peer_mgr::connect_other_lane_peer(struct chunk_rescue_list_reply_t *rescue_list_reply_ptr, unsigned long peer_list_member, unsigned long pid, unsigned long outside_lane_rescue_num)
{
	int i;
	list<unsigned long>::iterator rescue_pid_iter;
	
	_peer_list_member = peer_list_member;
	
	srand((long)time(NULL));
	i = rand()%peer_list_member;

	for(rescue_pid_iter = rescue_pid_list.begin(); rescue_pid_iter != rescue_pid_list.end(); rescue_pid_iter++) {
		if(*rescue_pid_iter == rescue_list_reply_ptr->level_info[i]->pid) {
			return i;
		}
	}

	if (build_connection(rescue_list_reply_ptr->level_info[i], pid)) {
		cout << "peer_mgr build_connection() success(rescue to other lane)" << endl;
		map_fd_pid[_sock] = rescue_list_reply_ptr->level_info[i]->pid;
		rescue_pid_list.push_back(rescue_list_reply_ptr->level_info[i]->pid);
		map_rescue_fd_count[_sock] = WIN_COUNTER;
		cout << "rescue_pid_list.size() = " << rescue_pid_list.size()  <<endl;
		cout << "map_rescue_fd_count _size = " << map_rescue_fd_count.size() << endl;
		return i;
	} else {
		cout << "peer_mgr build_connection() fail(rescue to other lane)" << endl;
		return -1;
		//PAUSE
		//exit(0);
	}

}

int peer_mgr::build_connection(struct level_info_t *level_info_ptr, unsigned long pid)
{
	struct sockaddr_in peer_saddr;
	int ret;
	struct in_addr ip;
	//struct in_addr peer;

	if((_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		cout << "init create socket failure" << endl;
#ifdef _WIN32
		::WSACleanup();
#endif
		return 0;
	}

	memset((struct sockaddr_in*)&peer_saddr, 0x0, sizeof(struct sockaddr_in));
	//memset((struct in_addr*)&peer, 0x0, sizeof(in_addr));
	

    if(self_public_ip == level_info_ptr->public_ip)
	    peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
    else
        peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
	peer_saddr.sin_port = htons(level_info_ptr->tcp_port);
	peer_saddr.sin_family = AF_INET;

	//ip.s_addr = level_info_ptr->private_ip;
	//printf("connect to %s\n", inet_ntoa(ip));	
	ip.s_addr = level_info_ptr->public_ip;
	printf("connect to %s\n", inet_ntoa(ip));	
	
	if(connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr)) < 0) {
		cout << "build_connection failure" << endl;

		PAUSE
#ifdef _WIN32
		::closesocket(_sock);
		::WSACleanup();
#else
		::close(_sock);
#endif
		return 0;
	} else {
#ifdef _WIN32
		u_long iMode = 0;
		ioctlsocket(_sock, FIONBIO, &iMode);
#endif
		cout << "connect sock = " << _sock << endl;
		ret = peer_ptr->handle_connect_request(_sock, level_info_ptr, pid);
		if(ret < 0) {
			cout << "handle_connect_request error!!!" << endl;
			return 0;
		} else {
			cout << "_sock = " << _sock << endl;
			_net_ptr->set_nonblocking(_sock);
			_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);	
			_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *>(peer_ptr));
			fd_list_ptr->push_back(_sock);
			return 1;
		}
	}
	

}


int peer_mgr::handle_pkt_in(int sock)
{
	int recv_byte;	
	int expect_len;
	int offset = 0;
	unsigned long buf_len;
	struct chunk_t *chunk_ptr = NULL;
	struct chunk_header_t *chunk_header_ptr = NULL;
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	
	socklen_t sin_len = sizeof(struct sockaddr_in);

	int new_fd = _net_ptr->accept(sock, (struct sockaddr *)&_cin, &sin_len);

	if(new_fd < 0) {
		return RET_SOCK_ERROR;
	} else {

		_net_ptr->set_nonblocking(new_fd);
		cout << "new_fd = " << new_fd << endl;   
		//PAUSE
	}
	
	chunk_header_ptr = new struct chunk_header_t;
	
	memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));
	
	expect_len = sizeof(struct chunk_header_t) ;
	
	while (1) {
		recv_byte = recv(new_fd, (char *)chunk_header_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
			} else {
				DBG_PRINTF("here\n");
				//continue;
				data_close(new_fd, "recv error in peer_mgr::handle_pkt_in");
				return RET_SOCK_ERROR;
				//PAUSE
				//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
			}
			
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		
		if (!expect_len)
			break;
	}

	expect_len = chunk_header_ptr->length;
	
	buf_len = sizeof(struct chunk_header_t) + expect_len;
	cout << "buf_len = " << buf_len << endl;

	chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];

	if (!chunk_ptr) {
		data_close(new_fd, "memory not enough");
		_log_ptr->exit(0, "memory not enough");
		return RET_SOCK_ERROR;
	}

	memset(chunk_ptr, 0x0, buf_len);
		
	memcpy(chunk_ptr, chunk_header_ptr, sizeof(struct chunk_header_t));

	if(chunk_header_ptr)
		delete chunk_header_ptr;
	
	while (1) {
		recv_byte = recv(new_fd, (char *)chunk_ptr + offset, expect_len, 0);
		if (recv_byte < 0) {
#ifdef _WIN32 
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif
				continue;
			} else {
				data_close(new_fd, "recv error in peer_mgr::handle_pkt_in");
				cout << "haha5" << endl;
				//PAUSE
				return RET_SOCK_ERROR;
				//_log_ptr->exit(0, "recv error in peer_mgr::handle_pkt_in");
			}
		}
		expect_len -= recv_byte;
		offset += recv_byte;
		if (expect_len == 0)
			break;
	}

	//_net_ptr->set_nonblocking(new_fd);
	_net_ptr->epoll_control(new_fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);	
	_net_ptr->set_fd_bcptr_map(new_fd, dynamic_cast<basic_class *>(peer_ptr));
	fd_list_ptr->push_back(new_fd);
	//printf("%s, new_fd=> %d\n", __FUNCTION__, new_fd);
	
	if (chunk_ptr->header.cmd == CHNK_CMD_PEER_CON) {
		cout << "CHNK_CMD_PEER_CON" << endl;
		//PAUSE
		peer_ptr->handle_connect(new_fd, chunk_ptr,_cin);
	} 

	if(chunk_ptr)
		delete chunk_ptr;

	return RET_OK;
}


int peer_mgr::handle_pkt_out(int sock)
{
	return RET_OK;
}

void peer_mgr::handle_pkt_error(int sock)
{

}

void peer_mgr::handle_sock_error(int sock, basic_class *bcptr)
{

}

void peer_mgr::handle_job_realtime()
{

}


void peer_mgr::handle_job_timer()
{

}

void peer_mgr::send_bandwidth(unsigned long pid, unsigned long avg_bit_rate)
{
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	struct chunk_bandwidth_t *bandwidth_ptr = NULL;
	

	bandwidth_ptr = new struct chunk_bandwidth_t;
	
	memset(bandwidth_ptr, 0x0, sizeof(struct chunk_bandwidth_t));

	bandwidth_ptr->header.cmd = CHNK_CMD_PEER_BWN;
	bandwidth_ptr->header.length = sizeof(unsigned long) ;	//pkt_buf paylod length
	bandwidth_ptr->header.rsv_1 = REQUEST;
	bandwidth_ptr->bandwidth = avg_bit_rate;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);

	if(pid_fd_iter == peer_ptr->map_pid_fd.end()) {
		return;
	} else {
	
		fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(pid_fd_iter->second);
		if(fd_queue_iter == peer_ptr->map_fd_out_ctrl.end()) {
			return;
		} else {
			//cout << "find queue sock = " << fd_queue_iter->first << endl;
			queue_out_ctrl_ptr = fd_queue_iter->second;
			queue_out_ctrl_ptr->push((struct chunk_t *)bandwidth_ptr);
			//cout << "queue_out_data_ptr->size() = " << queue_out_data_ptr->size() << endl; 
		}
	}

	if(queue_out_ctrl_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 

}


void peer_mgr::send_rescue(unsigned long pid, unsigned long self_pid, unsigned long manifest)
{

	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	struct chunk_rescue_t *rescue_ptr = NULL;

	rescue_ptr = new struct chunk_rescue_t;
	
	memset(rescue_ptr, 0x0, sizeof(struct chunk_rescue_t));
	
	rescue_ptr->header.cmd = CHNK_CMD_PEER_RSC;
	rescue_ptr->header.length = 2 * sizeof(unsigned long) ;	//pkt_buf paylod length
	rescue_ptr->header.rsv_1 = REQUEST;
	rescue_ptr->pid = self_pid;
	rescue_ptr->manifest = manifest;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);

	if(pid_fd_iter == peer_ptr->map_pid_fd.end()) {
		return;
	} else {

		fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(pid_fd_iter->second);
		if(fd_queue_iter == peer_ptr->map_fd_out_ctrl.end()) {
			return;
		} else {
			//map_rescue_fd_count_iter = map_rescue_fd_count.find(pid_fd_iter->second);
			//if(map_rescue_fd_count_iter == map_rescue_fd_count.end()) {
				//map_rescue_fd_count[pid_fd_iter->second] = WIN_COUNTER;
				//cout << "map_rescue_fd_count _size = " << map_rescue_fd_count.size() << endl;
			//}
			//cout << "find queue sock = " << fd_queue_iter->first << endl;
			queue_out_ctrl_ptr = fd_queue_iter->second;
			queue_out_ctrl_ptr->push((struct chunk_t *)rescue_ptr);
			//cout << "queue_out_data_ptr->size() = " << queue_out_ctrl_ptr->size() << endl; 
		}
	}

	if(queue_out_ctrl_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 

}


void peer_mgr::handle_cut_peer(unsigned long pid, int sock)
{
	map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, int>::iterator map_pid_fd_iter;
	list<unsigned long>::iterator rescue_pid_iter;

	fd_pid_iter = map_fd_pid.find(sock);
	
	if(fd_pid_iter != map_fd_pid.end()) {
		DBG_PRINTF("here\n");
		cout << "the pid we want to cut = " << fd_pid_iter->second << endl;
		send_cut_peer(pid, sock);
		for(rescue_pid_iter = rescue_pid_list.begin(); rescue_pid_iter != rescue_pid_list.end(); rescue_pid_iter++) {
			if(*rescue_pid_iter == fd_pid_iter->second) {
				rescue_pid_list.erase(rescue_pid_iter);
				cout << "rescue_pid_list_size = " << rescue_pid_list.size() << endl;
				DBG_PRINTF("here\n");
				break;
			}
		}
		
		map_pid_fd_iter = peer_ptr->map_pid_fd.find(fd_pid_iter->second);

		if(map_pid_fd_iter != peer_ptr->map_pid_fd.end()) 
			peer_ptr->map_pid_fd.erase(map_pid_fd_iter);
	
	} else {
		fd_pid_iter = peer_ptr->map_fd_pid.find(sock);
		if(fd_pid_iter != peer_ptr->map_fd_pid.end()) {
			cout << "the pid we want to cut = " << fd_pid_iter->second << endl;
			DBG_PRINTF("here\n");
			send_cut_peer(pid, sock);
			for(rescue_pid_iter = rescue_pid_list.begin(); rescue_pid_iter != rescue_pid_list.end(); rescue_pid_iter++) {
				if(*rescue_pid_iter == fd_pid_iter->second) {
					rescue_pid_list.erase(rescue_pid_iter);
					cout << "rescue_pid_list_size = " << rescue_pid_list.size() << endl;
					DBG_PRINTF("here\n");
					break;
				}
			}
			
			map_pid_fd_iter = peer_ptr->map_pid_fd.find(fd_pid_iter->second);

			if(map_pid_fd_iter != peer_ptr->map_pid_fd.end()) 
				peer_ptr->map_pid_fd.erase(map_pid_fd_iter);
			
		} else {
			DBG_PRINTF("here\n");
			return;
		}
	}

}


void peer_mgr::send_cut_peer(unsigned long pid, int sock)
{
	DBG_PRINTF("here\n");

	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	struct chunk_cut_peer_info_t  *cut_peer_ptr = NULL;

	cut_peer_ptr = new struct chunk_cut_peer_info_t;
	
	memset(cut_peer_ptr, 0x0, sizeof(struct chunk_cut_peer_info_t));
	
	cut_peer_ptr->header.cmd = CHNK_CMD_PEER_CUT;
	cut_peer_ptr->header.length = sizeof(unsigned long) ;	//pkt_buf paylod length
	cut_peer_ptr->header.rsv_1 = REQUEST;
	cut_peer_ptr->pid = pid;
	
	fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(sock);
	if(fd_queue_iter == peer_ptr->map_fd_out_ctrl.end()) {
		DBG_PRINTF("here\n");
		return;
	} else {
		queue_out_ctrl_ptr = fd_queue_iter->second;
		queue_out_ctrl_ptr->push((struct chunk_t *)cut_peer_ptr);
		DBG_PRINTF("here\n");
		//cout << "queue_out_data_ptr->size() = " << queue_out_data_ptr->size() << endl; 
	}
	
	if(queue_out_ctrl_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 

}


void peer_mgr::rescue_reply(unsigned long pid, unsigned long manifest)
{
	
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_ctrl_ptr = NULL;
	struct chunk_rescue_reply_t *rescue_reply_ptr = NULL;
	

	rescue_reply_ptr = new struct chunk_rescue_reply_t;
	
	memset(rescue_reply_ptr, 0x0, sizeof(struct chunk_rescue_reply_t));

	
	rescue_reply_ptr->header.cmd = CHNK_CMD_PEER_RSC;
	rescue_reply_ptr->header.length = sizeof(unsigned char) + sizeof(unsigned long);	//pkt_buf paylod length
	rescue_reply_ptr->header.rsv_1 = REPLY;
	rescue_reply_ptr->reply = OK;
	rescue_reply_ptr->manifest = manifest;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);

	if(pid_fd_iter == peer_ptr->map_pid_fd.end()) {
		return;
	} else {
	
		fd_queue_iter = peer_ptr->map_fd_out_ctrl.find(pid_fd_iter->second);
		if(fd_queue_iter == peer_ptr->map_fd_out_ctrl.end()) {
			return;
		} else {
			//cout << "find queue sock = " << fd_queue_iter->first << endl;
			queue_out_ctrl_ptr = fd_queue_iter->second;
			queue_out_ctrl_ptr->push((struct chunk_t *)rescue_reply_ptr);
			//cout << "queue_out_data_ptr->size() = " << queue_out_data_ptr->size() << endl; 
		}
	}

	if(queue_out_ctrl_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 

}


void peer_mgr::add_downstream(unsigned long pid, struct chunk_t *chunk_ptr)
{
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	queue<struct chunk_t *> *queue_out_data_ptr = NULL;
	struct peer_info_t *downstream_peer = NULL;
	unsigned long ss_id;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);
	pid_peer_info_iter = _pk_mgr_ptr->map_pid_peer_info.find(pid);

	if(pid_peer_info_iter != _pk_mgr_ptr->map_pid_peer_info.end()) {
		downstream_peer = pid_peer_info_iter->second;
		ss_id = (chunk_ptr->header.sequence_number % _pk_mgr_ptr->sub_stream_num);
		if((downstream_peer->manifest & (1 << ss_id))) {
			if(pid_fd_iter == peer_ptr->map_pid_fd.end()) {
				return;
			} else {
	
				fd_queue_iter = peer_ptr->map_fd_out_data.find(pid_fd_iter->second);
				if(fd_queue_iter == peer_ptr->map_fd_out_data.end()) {
					return;
				} else {
					//cout << "find queue sock = " << fd_queue_iter->first << endl;
					queue_out_data_ptr = fd_queue_iter->second;
					queue_out_data_ptr->push(chunk_ptr);
					//cout << "sequence_number = " << chunk_ptr->header.sequence_number << endl;
					//cout << "queue_out_data_ptr->size() = " << queue_out_data_ptr->size() << endl; 
				}
			}
		} else {
            //cout << "cut seq = " << chunk_ptr->header.sequence_number << endl;
			return;
		}
	}

	if(queue_out_data_ptr->size() != 0 ) {
		_net_ptr->epoll_control(fd_queue_iter->first, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	} 

}


void peer_mgr::add_rescue_downstream(unsigned long pid)
{
	int sock;
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter2;
	queue<struct chunk_t *> *queue_out_data_ptr = NULL;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);

	if(pid_fd_iter != peer_ptr->map_pid_fd.end()) {
		sock = pid_fd_iter->second;
		map_fd_pid[sock] = pid;
		fd_queue_iter = peer_ptr->map_fd_out_data.find(sock);
		if(fd_queue_iter == peer_ptr->map_fd_out_data.end()) {
			return;
		} else {
			fd_queue_iter2 = _map_fd_downstream.find(sock);
			if(fd_queue_iter2 == _map_fd_downstream.end()) {
				queue_out_data_ptr = fd_queue_iter->second;
				_map_fd_downstream[sock] = queue_out_data_ptr;
			} else {
				return;
			}
		}
		
	}

}

void peer_mgr::add_rescue_fd(unsigned long pid)
{
	map<int, int>::iterator map_rescue_fd_count_iter;
	map<unsigned long, int>::iterator pid_fd_iter;

    //cout << "find pid = " << pid << endl;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);

    if(pid_fd_iter != peer_ptr->map_pid_fd.end()) {

        //cout << "pid_fd_iter _size = " << pid_fd_iter->second << endl;

	    map_rescue_fd_count_iter = map_rescue_fd_count.find(pid_fd_iter->second);
	    if(map_rescue_fd_count_iter == map_rescue_fd_count.end()) {
		    map_rescue_fd_count[pid_fd_iter->second] = WIN_COUNTER;
		    cout << "map_rescue_fd_count _size = " << map_rescue_fd_count.size() << endl;
	    }
    }

}



void peer_mgr::cut_rescue_peer(int sock)
{
	DBG_PRINTF("here\n");
	unsigned long pid;
	list<unsigned long>::iterator rescue_pid_iter;
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_ptr_iter;

	fd_pid_iter = peer_ptr->map_fd_pid.find(sock);
		
	if(fd_pid_iter == peer_ptr->map_fd_pid.end()) {
		return;
	} else {
		pid = fd_pid_iter->second;
		//peer_ptr->map_fd_pid.erase(fd_pid_iter);
	}
		
	fd_pid_iter = map_fd_pid.find(sock);
		
	if(fd_pid_iter != map_fd_pid.end()) 
		map_fd_pid.erase(fd_pid_iter);

	pid_peer_info_iter = _pk_mgr_ptr->map_pid_peer_info.find(pid);
	if(pid_peer_info_iter == _pk_mgr_ptr->map_pid_peer_info.end()) {
		peer_ptr->data_close(sock, "cut_rescue_peer");
	} else {
		cout << "cut_rescue_peer" << endl;
	}

	for(rescue_pid_iter = rescue_pid_list.begin(); rescue_pid_iter != rescue_pid_list.end(); rescue_pid_iter++) {
		if(*rescue_pid_iter == pid) {
			rescue_pid_list.erase(rescue_pid_iter);
		}
	}
	
}


void peer_mgr::del_rescue_downstream()
{
	unsigned long pid;
	int sock;
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_ptr_iter;

	
	for(pid_peer_info_iter = _pk_mgr_ptr->map_pid_peer_info.begin(); pid_peer_info_iter != _pk_mgr_ptr->map_pid_peer_info.end(); pid_peer_info_iter++) {
		pid = pid_peer_info_iter->first;
		cout <<"pid = " << pid << endl;
		
		pid_fd_iter = peer_ptr->map_pid_fd.find(pid);
		
		if(pid_fd_iter == peer_ptr->map_pid_fd.end()) {
			continue;
		} else {
			sock = pid_fd_iter->second;
		}
		
		fd_queue_ptr_iter = _map_fd_downstream.find(sock);
	
		fd_pid_iter = map_fd_pid.find(sock);
	
		if(fd_queue_ptr_iter != _map_fd_downstream.end()) 
			_map_fd_downstream.erase(fd_queue_ptr_iter);
		
		if(fd_pid_iter != map_fd_pid.end()) 
			map_fd_pid.erase(fd_pid_iter);
		
	}
	
	
	//_map_fd_downstream.clear();
	//map_fd_pid.clear();
	
}

void peer_mgr::cut_rescue_downstream(unsigned long pid)
{
	DBG_PRINTF("here\n");
	
	int sock;
	list<int>::iterator fd_iter;
	map<unsigned long, int>::iterator pid_fd_iter;
	map<int, unsigned long>::iterator fd_pid_iter;
	map<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_ptr_iter;

	cout << "cut_rescue_pid = " << pid << endl;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);
		
	if(pid_fd_iter == peer_ptr->map_pid_fd.end()) {
		return;
	} else {
		DBG_PRINTF("here\n");
		sock = pid_fd_iter->second;
	}
		
	fd_queue_ptr_iter = _map_fd_downstream.find(sock);
	
	fd_pid_iter = map_fd_pid.find(sock);
	
	if(fd_queue_ptr_iter != _map_fd_downstream.end()) {
		_map_fd_downstream.erase(fd_queue_ptr_iter);
		DBG_PRINTF("here\n");
	}
		
	if(fd_pid_iter != map_fd_pid.end()) {
		map_fd_pid.erase(fd_pid_iter);
		DBG_PRINTF("here\n");
	}

	pid_peer_info_iter = _pk_mgr_ptr->map_pid_peer_info.find(pid);
	if(pid_peer_info_iter == _pk_mgr_ptr->map_pid_peer_info.end()) {
		DBG_PRINTF("here\n");
		//peer_ptr->data_close(sock, "cut_rescue_peer");
		for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
			if(*fd_iter == sock) {
				fd_list_ptr->erase(fd_iter);
				break;
			}
		}
	} else {
		cout << "cut_rescue_peer" << endl;
	}

	
		
}

void peer_mgr::clear_ouput_buffer(unsigned long pid)
{
    map<unsigned long, int>::iterator pid_fd_iter;
	map<int, queue<struct chunk_t *> *>::iterator fd_queue_iter;
	queue<struct chunk_t *> *queue_out_data_ptr = NULL;

	pid_fd_iter = peer_ptr->map_pid_fd.find(pid);

	if(pid_fd_iter == peer_ptr->map_pid_fd.end()) {
		return;
	} else {
		fd_queue_iter = peer_ptr->map_fd_out_data.find(pid_fd_iter->second);
		if(fd_queue_iter == peer_ptr->map_fd_out_data.end()) {
			return;
		} else {
			queue_out_data_ptr = fd_queue_iter->second;		
		}
	}

	while(queue_out_data_ptr->size() != 0 ) {
		queue_out_data_ptr->pop();
	} 
}

void peer_mgr::set_up_public_ip(struct chunk_level_msg_t *level_msg_ptr)
{
    if(self_public_ip == 0){
        for(unsigned long i = 0; i < _pk_mgr_ptr->lane_member; i++) {
		    if((level_msg_ptr->level_info[i])->pid == level_msg_ptr->pid) {
			    self_public_ip = level_msg_ptr->level_info[i]->public_ip;
                //printf("self public ip set to %d\n", self_public_ip);
		    }
	    }
    }
}

void peer_mgr::data_close(int cfd, const char *reason) 
{
	list<int>::iterator fd_iter;
	
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
}

























