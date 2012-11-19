/*
in main should init sock to listen to listen other peer connected


*/
#include "common.h"

#ifdef _WIN32
	#include "EpollFake.h"
#endif
#include "configuration.h"
#include "logger.h"
#include "network.h"
#include "basic_class.h"
#include "pk_mgr.h"
#include "peer_mgr.h"
#include "peer.h"
#include "stream_server.h"
#include "rtmp_server.h"
#include "rtmp.h"
#include "rtmp_supplement.h"
#include "bit_stream_server.h"


using namespace std;

int mode=mode_BitStream;


const char version[] = "1.0.0";

static volatile sig_atomic_t handle_sig_alarm = 0;
static volatile sig_atomic_t srv_shutdown = 0;
list<int> fd_list;


/*
char* const short_options = "c:i:l:p:";  
struct option long_options[] = {  
	{ "config",     1,   NULL,    'c'     },
	{ "ip",  1,   NULL,    'i'     },  
	{ "local",     1,   NULL,    'l'     },
	{ "port",     1,   NULL,    'p'     },	
	{      0,     0,     0,     0},  
}; 

void print_usage (FILE* stream, int exit_code)
{
    fprintf (stream, "Usage: ray options [ inputfile ... ]\n");
    fprintf (stream, " -c --config file.\n"
                     " -i --pk ip.\n"
                     " -l --local port.\n"
                     " -p --pk port.\n");
    exit (exit_code);
}

*/

void signal_handler(int sig)
{
	fprintf(stdout, "\n\nrecv Signal %d\n\n", sig);
	switch (sig)
	{
		case SIGTERM: 
			srv_shutdown = 1; 
			break;
		case SIGINT:
			srv_shutdown = 1;
			break;
#ifndef _WIN32
		case SIGALRM: 
			handle_sig_alarm = 1; 
			break;
#endif
		case SIGSEGV:
			srv_shutdown = 1;
#ifdef _WIN32
			MessageBox(NULL, _T("SIGSEGV"), _T("EXIT"), MB_OK | MB_ICONSTOP);
#else
			printf("SIGSEGV\n");
			exit(0);		// it must exit on linux			
#endif
			break;
		case SIGABRT:
			srv_shutdown = 1;
#ifdef _WIN32
			MessageBox(NULL, _T("SIGABRT"), _T("EXIT"), MB_OK | MB_ICONSTOP);
#else
			printf("SIGABRT\n");
			exit(0);		// it must exit on linux
#endif			
			break;	
	}
}

int main(int argc, char **argv)
{
    extern char *optarg;
	int svc_fd_tcp, svc_fd_udp;
	unsigned long html_size;
	int optval = 1;
	struct sockaddr_in sin;
	int opt;
	unsigned short bitStreamServerPort;

	string svc_tcp_port("");
	string svc_udp_port("");
	string stream_local_port("");
    string config_file("");

	configuration *prep = NULL;
	logger *log_ptr = NULL;
	network *net_ptr = NULL;
	pk_mgr *pk_mgr_ptr = NULL;
	peer_mgr *peer_mgr_ptr = NULL;
	rtmp_server *rtmp_svr = NULL;
	rtmp *rtmp_ptr = NULL;
	amf *amf_ptr = NULL;
	rtmp_supplement *rtmp_supplement_ptr = NULL;
	bit_stream_server *bit_stream_server_ptr =NULL;
	

    config_file = "config.ini";
	
	prep = new configuration(config_file);
	net_ptr = new network();
	log_ptr = new logger();
	peer_mgr_ptr = new peer_mgr(&fd_list);


	/*
    while((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1)  { 
        switch (opt)  {  
            case 'c':  
                config_file = optarg;
                break;  
            case 'i':  
                prep->add_key("pk_ip",optarg);
                break;  
            case 'l':  
                prep->add_key("stream_local_port",optarg);
                break;  
            case 'p':  
                prep->add_key("pk_port",optarg);
                break;  
            default: 
                print_usage(stderr, 1);
                break;
         }  
    }  

	*/

	prep->read_key("html_size", html_size);
	prep->read_key("svc_tcp_port", svc_tcp_port);
	prep->read_key("svc_udp_port", svc_udp_port);
	prep->read_key("stream_local_port", stream_local_port);

	cout << "html_size=" << html_size << endl;
	cout << "svc_tcp_port=" << svc_tcp_port << endl;
	cout << "svc_udp_port=" << svc_udp_port << endl;
	cout << "stream_local_port=" << stream_local_port << endl;
	
	pk_mgr_ptr = new pk_mgr(html_size, &fd_list, net_ptr , log_ptr , prep);
	
	log_ptr->logger_set(net_ptr);
	net_ptr->epoll_creater();
	log_ptr->start_log_record(SYS_FREQ);

	
	peer_mgr_ptr->peer_mgr_set(net_ptr, log_ptr, prep, pk_mgr_ptr);
	//peer_mgr_ptr->pk_mgr_set(pk_mgr_ptr);
	pk_mgr_ptr->peer_mgr_set(peer_mgr_ptr);
	

#ifndef _WIN32
	signal(SIGALRM, signal_handler);
	signal(SIGPIPE, SIG_IGN);		// avoid crash on socket pipe 
	signal(SIGUSR1, SIG_IGN);	
#endif

#ifdef _WIN32
	WSADATA wsaData;										// Winsock initial data
	if(WSAStartup(MAKEWORD(1,1),&wsaData)) {
		printf("WSAStartup ERROR\n");
		WSACleanup();
		exit(0);
	}
#endif


	signal(SIGTERM, signal_handler);
	signal(SIGINT,  signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGABRT, signal_handler);


	if(mode==mode_RTMP){
	printf("mode_RTMP\n");
	amf_ptr = new amf(log_ptr);
	if (!amf_ptr) {
		printf("Can't new amf class\n");
		return -1;
	}

	rtmp_ptr = new rtmp(net_ptr, amf_ptr, log_ptr);
	if (!rtmp_ptr) {
		printf("Can't new rtmp class\n");
		return -1;
	}

	rtmp_supplement_ptr = new rtmp_supplement(log_ptr, rtmp_ptr, amf_ptr);
	if (!rtmp_supplement_ptr) {
		printf("Can't new rtmp_supplement class\n");
		return -1;
	}

	rtmp_svr = new rtmp_server(net_ptr, log_ptr, amf_ptr, rtmp_ptr, rtmp_supplement_ptr, pk_mgr_ptr, &fd_list);
	if (!rtmp_svr) {
		printf("Can't new rtsp_server class\n");
		return -1;
	}
	rtmp_svr->init(1,stream_local_port);
	printf("new rtmp_svr successfully\n");


//mode BitStream
	}else if(mode==mode_BitStream){
	printf("mode_BitStream\n");
	

  	bit_stream_server_ptr = new bit_stream_server ( net_ptr,log_ptr,pk_mgr_ptr ,&fd_list);
	stringstream ss_tmp;
	unsigned short port_tcp;
	ss_tmp << stream_local_port;
	ss_tmp >> port_tcp;
	bit_stream_server_ptr ->init(0,port_tcp);

	}else if(mode == mode_RTSP){
	printf("mode_RTSP\n");
	}



	svc_fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if(svc_fd_tcp < 0) {
		throw "create tcp srv socket fail";
	}
	svc_fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
	if(svc_fd_udp< 0)
		throw "create udp srv socket fail";
	
	memset(&sin, 0x0, sizeof(struct sockaddr_in));

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_tcp_port.c_str()));
	
	if (bind(svc_fd_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1) {
		cout << "TCP PORT :" << svc_tcp_port << "bind error! " << endl;
		return -1;
	} else {
		cout << "Server bind at TCP port: " << svc_tcp_port << endl;
	}

	if ( setsockopt(svc_fd_tcp, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval , sizeof(optval)) == -1)
		return -1;

	if (listen(svc_fd_tcp, MAX_POLL_EVENT) == -1) {
		cout << "TCP PORT :" << svc_tcp_port << " listen error! " << endl;
		return -1;
	}

	
 	cout << "tst_speed_svr " << version << " (Compiled Time: "__DATE__ << " "__TIME__")" << endl;

 	
       log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "PF", "ready now");
	  
	if(!log_ptr->check_arch_compatible()) {
		cout << "Hardware Architecture is not support." << endl;
		log_ptr->exit(0, "Hardware Architecture is not support.");
	}

#ifndef _WIN32
	struct itimerval interval;
	interval.it_interval.tv_sec = SIG_FREQ;
	interval.it_interval.tv_usec = 0;
	interval.it_value.tv_sec = SIG_FREQ;
	interval.it_value.tv_usec = 0;

	// setup periodic timer (3 second) 
	if(setitimer(ITIMER_REAL, &interval, NULL)) {
		return -1;
	}

	getitimer(ITIMER_REAL, &interval);
#endif
	
	net_ptr->set_nonblocking(svc_fd_tcp);
	
	net_ptr->epoll_control(svc_fd_tcp, EPOLL_CTL_ADD, EPOLLIN);
	 
	net_ptr->set_fd_bcptr_map(svc_fd_tcp, dynamic_cast<basic_class *>(peer_mgr_ptr));


	fd_list.push_back(svc_fd_tcp);
	pk_mgr_ptr->init();


	while(!srv_shutdown) {
		
#ifdef _WIN32
		net_ptr->epoll_waiter(1000, &fd_list);
#else
		net_ptr->epoll_waiter(1000);
#endif
		net_ptr->epoll_dispatcher();
	
	}

	net_ptr->garbage_collection();
	log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "PF", "graceful exit!!");
	log_ptr->stop_log_record();

	if(prep) 
		delete prep;
	
	if(log_ptr)
		delete log_ptr;
	
	if(net_ptr)
		delete net_ptr;
	
	if(pk_mgr_ptr)
		delete pk_mgr_ptr;

	if(rtmp_svr)
		delete rtmp_svr;

	if(peer_mgr_ptr) 
		delete peer_mgr_ptr;

	return EXIT_SUCCESS;
		
}


