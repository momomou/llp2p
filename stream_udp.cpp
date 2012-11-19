#include "stream_udp.h"
#include "network.h"
#include "logger.h"

stream_udp::stream_udp(network *net_ptr, logger *log_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
}

stream_udp::~stream_udp()
{
}

int stream_udp::handle_pkt_in(int sock)
{	
	return RET_OK;
}


int stream_udp::handle_pkt_out(int sock)
{
	unsigned int send_size;
	int send_byte;
	struct chunk_t *chunk_ptr = NULL;
	if (!_chunk_q.size()) {
		_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
		return RET_SOCK_ERROR;
	}
	//cout << "_chunk_q.size() = " << _chunk_q.size() <<endl;
	//PAUSE
	chunk_ptr = _chunk_q.front();
	_chunk_q.pop();
	send_size = (unsigned int)chunk_ptr->header.length;
	
	//_net_ptr->sendto(sock, (char *)chunk_ptr->buf, send_size, 0, (struct sockaddr *)&_cin, sizeof(_cin));
	send_byte = sendto(sock, (char *)chunk_ptr->buf, send_size, 0, (struct sockaddr *)&_cin, sizeof(_cin));
	//_log_ptr->write_log_format("s = u\t s = d\n", "sequence_number", chunk_ptr->header.sequence_number, "send_byte", send_byte);
	//cout << "sock = " << sock << endl;
	//cout << "send_size = " << send_size << endl;
	//cout << "send_byte = " << send_byte << endl;
	
	//if(chunk_ptr)
		//delete chunk_ptr;
	return RET_OK;
}

void stream_udp::handle_pkt_error(int sock)
{

}

void stream_udp::handle_job_realtime()
{

}


void stream_udp::handle_job_timer()
{

}

void stream_udp::add_chunk(struct chunk_t *chunk)
{
	_chunk_q.push(chunk);
	
}

void stream_udp::set_client_sockaddr(struct sockaddr_in *cin)
{
	if (cin)
		memcpy(&_cin, cin, sizeof(struct sockaddr_in));
}

