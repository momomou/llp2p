#include "basic_class.h"
#include <iostream>
#include <map>

using namespace std;


basic_class::basic_class()
{

}

basic_class::~basic_class()
{

}

int basic_class::handle_pkt_in(int sock)
{
	return 0;
}

int basic_class::handle_pkt_out(int sock)
{
	return 0;
}

void basic_class::handle_pkt_error(int sock)
{

}
void basic_class::handle_sock_error(int sock, basic_class *bcptr)
{
	
}

void basic_class::handle_job_realtime()
{

}


void basic_class::handle_job_timer()
{

}


























