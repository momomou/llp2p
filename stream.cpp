#include "stream.h"

stream::stream()
{
}

stream::~stream()
{
}

int stream::handle_pkt_in(int sock)
{	
	return RET_OK;
}

int stream::handle_pkt_out(int sock)
{
	return RET_OK;
}

void stream::handle_pkt_error(int sock)
{

}

void stream::handle_job_realtime()
{

}


void stream::handle_job_timer()
{

}

void stream::set_client_sockaddr(struct sockaddr_in *cin)
{

}

void stream::add_chunk(struct chunk_t *chunk)
{

}

unsigned char stream::get_stream_pk_id()
{
    return RET_OK;
}

