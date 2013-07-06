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
#include "librtmp/rtmp_server.h"
#include "librtmp/rtmp.h"
#include "librtmp/rtmp_supplement.h"
#include "bit_stream_server.h"
#include "peer_communication.h"
#include "io_connect.h"
#include "io_accept.h"
#include "logger_client.h"
using namespace std;

//int mode=mode_BitStream;


const char version[] = "1.0.0";

static volatile sig_atomic_t handle_sig_alarm = 0;
static volatile sig_atomic_t srv_shutdown = 0;
static volatile int errorRestartFlag = 0;
static logger_client *logger_client_ptr = NULL;
list<int> fd_list;


void signal_handler(int sig)
{
	fprintf(stdout, "\n\nrecv Signal %d\n\n", sig);

	if(logger_client_ptr == NULL){
		logger_client_ptr = new logger_client();
		logger_client_ptr->log_init();
	}

	switch (sig)
	{
		case SIGTERM: 
			srv_shutdown = 1; 
			logger_client_ptr->log_exit();
			break;
		case SIGINT:
			srv_shutdown = 1;
			logger_client_ptr->log_exit();
			break;
#ifndef _WIN32
		case SIGALRM: 
			handle_sig_alarm = 1; 
			logger_client_ptr->log_exit();
			break;
#endif
		case SIGSEGV:
			srv_shutdown = 1;
			logger_client_ptr->log_exit();
#ifdef _WIN32
			MessageBox(NULL, _T("SIGSEGV"), _T("EXIT"), MB_OK | MB_ICONSTOP);
#else
			printf("SIGSEGV\n");
			PAUSE
			exit(0);		// it must exit on linux			
#endif
			break;
		case SIGABRT:
			srv_shutdown = 1;
			logger_client_ptr->log_exit();
#ifdef _WIN32
			MessageBox(NULL, _T("SIGABRT"), _T("EXIT"), MB_OK | MB_ICONSTOP);
#else
			printf("SIGABRT\n");
			PAUSE
			exit(0);		// it must exit on linux
#endif			
			break;	
	}
}

int main(int argc, char **argv){

	while((!srv_shutdown)){


		int svc_fd_tcp, svc_fd_udp;
		unsigned long html_size;
		int optval = 1;
		struct sockaddr_in sin;

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
		peer_communication *peer_communication_ptr = NULL;

		config_file = "config.ini";

		prep = new configuration(config_file);
		net_ptr = new network();
		log_ptr = new logger();
		logger_client_ptr = new logger_client();
		peer_mgr_ptr = new peer_mgr(&fd_list);

		prep->read_key("html_size", html_size);
		prep->read_key("svc_tcp_port", svc_tcp_port);
		prep->read_key("svc_udp_port", svc_udp_port);
		prep->read_key("stream_local_port", stream_local_port);

		cout << "html_size=" << html_size << endl;
		cout << "svc_tcp_port=" << svc_tcp_port << endl;
		cout << "svc_udp_port=" << svc_udp_port << endl;
		cout << "stream_local_port=" << stream_local_port << endl;

		pk_mgr_ptr = new pk_mgr(html_size, &fd_list, net_ptr , log_ptr , prep , logger_client_ptr);

		log_ptr->logger_set(net_ptr);
		net_ptr->epoll_creater();
		log_ptr->start_log_record(SYS_FREQ);

		peer_mgr_ptr->peer_mgr_set(net_ptr, log_ptr, prep, pk_mgr_ptr, logger_client_ptr);
		//peer_mgr_ptr->pk_mgr_set(pk_mgr_ptr);
		pk_mgr_ptr->peer_mgr_set(peer_mgr_ptr);

		peer_communication_ptr = new peer_communication(net_ptr,log_ptr,prep,peer_mgr_ptr,peer_mgr_ptr->get_peer_object(),pk_mgr_ptr,logger_client_ptr);
		peer_mgr_ptr->peer_communication_set(peer_communication_ptr);

		logger_client_ptr->set_net_obj(net_ptr);
		logger_client_ptr->set_pk_mgr_obj(pk_mgr_ptr);
		//	_beginthread(pk_mgr::threadTimeout, 0,NULL );
		//	_beginthread(pk_mgr_ptr ->threadTimeout, 0,NULL );

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
		}else if(mode==mode_BitStream  || mode == mode_HTTP){
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
		if(svc_fd_udp< 0){
			throw "create udp srv socket fail";
		}

		memset(&sin, 0x0, sizeof(struct sockaddr_in));

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons((unsigned short)atoi(svc_tcp_port.c_str()));

		if ( setsockopt(svc_fd_tcp, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval , sizeof(optval)) == -1){
			printf("setsockopt \n ");
			return -1;
		}

		if (net_ptr->bind(svc_fd_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == 0) {

			cout << "Server bind at TCP port: " << svc_tcp_port << endl;
		}else{



			cout <<  "ERROR NUM :" << WSAGetLastError() <<"TCP PORT :" << svc_tcp_port << "bind error! " << endl;
			PAUSE
				return -1;
		}

		if (listen(svc_fd_tcp, MAX_POLL_EVENT) == 0) {
			cout << "Server LISTRN SUCCESS at TCP port: " << svc_tcp_port << endl;
		}else{
			cout << "TCP PORT :" << svc_tcp_port << " listen error! " << endl;
			PAUSE
				return -1;
		}


		cout << "tst_speed_svr " << version << " (Compiled Time: "__DATE__ << " "__TIME__")" << endl;


		log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "PF", "ready now");

		if(!log_ptr->check_arch_compatible()) {
			cout << "Hardware Architecture is not support." << endl;
			PAUSE
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

		net_ptr->set_fd_bcptr_map(svc_fd_tcp, dynamic_cast<basic_class *>(peer_communication_ptr->get_io_accept_handler()));


		fd_list.push_back(svc_fd_tcp);
		pk_mgr_ptr->init();
		logger_client_ptr->log_init();
		//	bit_stream_server_ptr->_map_seed_out_data

		while(1){
			int testServerfd =0;
			testServerfd = pk_mgr_ptr ->build_connection("127.0.0.1",stream_local_port);
			if(testServerfd){

				printf("connect ok \n");
				shutdown(testServerfd, SHUT_RDWR);
				//		closesocket(testServerfd);
				closesocket(testServerfd);
				break ;
			}else{

				printf("connectfail \n");
			}
		}





		while((!srv_shutdown)  ||  (!errorRestartFlag)) {

#ifdef _WIN32
			net_ptr->epoll_waiter(1000, &fd_list);
#else
			net_ptr->epoll_waiter(1000);
#endif
			net_ptr->epoll_dispatcher();

			pk_mgr_ptr->time_handle();
		}

		net_ptr->garbage_collection();
		log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "PF", "graceful exit!!");
		log_ptr->stop_log_record();

		if(prep) 
			delete prep;
		prep =NULL;

		if(log_ptr)
			delete log_ptr;
		log_ptr =NULL;

		if(bit_stream_server_ptr)
			delete bit_stream_server_ptr;
		bit_stream_server_ptr =NULL;

		if(peer_communication_ptr)
			delete peer_communication_ptr;
		peer_communication_ptr =NULL;

		if(net_ptr)
			delete net_ptr;
		net_ptr =NULL;	

		if(pk_mgr_ptr)
			delete pk_mgr_ptr;
		pk_mgr_ptr =NULL;

		if(rtmp_svr)
			delete rtmp_svr;
		rtmp_svr =NULL;

		if(peer_mgr_ptr) 
			delete peer_mgr_ptr;
		peer_mgr_ptr=NULL;

		if(logger_client_ptr)
			delete logger_client_ptr;
		logger_client_ptr =NULL;


	}

	return EXIT_SUCCESS;

}


