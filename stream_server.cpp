#include "stream_server.h"
#include "network.h"
#include "logger.h"

using namespace std;


stream_server::stream_server()
{
	

}

stream_server::~stream_server() 
{

	
}


void stream_server::stream_server_set(network *net_ptr , logger *log_ptr , configuration *prep)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep;
		
}

int stream_server::handle_pkt_in(int sock)
{
	return RET_OK;	
}


int stream_server::handle_pkt_out(int sock)
{
	return RET_OK;
}

void stream_server::handle_pkt_error(int sock)
{

}

void stream_server::handle_job_realtime()
{

}


void stream_server::handle_job_timer()
{

}

void stream_server::data_close(int cfd, const char *reason) 
{
	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "pk", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".." << endl;
	_net_ptr->epoll_control(cfd, EPOLL_CTL_DEL, 0);
	_net_ptr->close(cfd);

}































