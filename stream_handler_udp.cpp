#include "stream_handler_udp.h"
#include "stream.h"


stream_handler_udp::stream_handler_udp()
{

}

stream_handler_udp::~stream_handler_udp() 
{
	
}

int stream_handler_udp::handle_pkt_in(int sock)
{
	return RET_OK;
}

int stream_handler_udp::handle_pkt_out(int sock)
{
	stream *strm;
	//DBG_PRINTF("here\n");
	//PAUSE
	for (_map_stream_iter = _map_stream.begin(); _map_stream_iter != _map_stream.end(); _map_stream_iter++) {
		//DBG_PRINTF("here\n");
		//PAUSE
		strm = _map_stream_iter->second;
		strm->handle_pkt_out(sock);
	}

	return RET_OK;
}

void stream_handler_udp::handle_pkt_error(int sock)
{

}

void stream_handler_udp::handle_job_realtime()
{

}


void stream_handler_udp::handle_job_timer()
{

}

void stream_handler_udp::add_stream(unsigned long strm_addr, stream *strm)
{
	_map_stream[strm_addr] = strm;
}

void stream_handler_udp::del_stream(unsigned long strm_addr, stream *strm)
{
	if (strm) {
		delete strm;
		_map_stream.erase(strm_addr);
	}
}