#include "stream_handler.h"


stream_handler::stream_handler()
{

}

stream_handler::~stream_handler() 
{
	
}

int stream_handler::handle_pkt_in(int sock)
{
	return RET_OK;
}

int stream_handler::handle_pkt_out(int sock)
{
	return RET_OK;
}

void stream_handler::handle_pkt_error(int sock)
{

}

void stream_handler::handle_job_realtime()
{

}


void stream_handler::handle_job_timer()
{

}