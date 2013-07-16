
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
//#include "librtmp/rtmp_server.h"
//#include "librtmp/rtmp.h"
//#include "librtmp/rtmp_supplement.h"
#include "bit_stream_server.h"
#include "peer_communication.h"
#include "io_connect.h"
#include "io_accept.h"
#include "logger_client.h"
using namespace std;

#ifdef _FIRE_BREATH_MOD_
#include "JSObject.h"
#include "variant_list.h"
#include "DOM/Document.h"
#include "global/config.h"
#include "llp2pAPI.h"
#endif



const char version[] = "1.0.0";

//struct globalVar{
static volatile sig_atomic_t handle_sig_alarm = 0;
static volatile sig_atomic_t srv_shutdown = 0;
static int errorRestartFlag = 0;
//static logger_client *logger_client_ptr = NULL;
static volatile unsigned short streamingPort = 0;
list<int> fd_list;

#ifdef _FIRE_BREATH_MOD_
static  pk_mgr *pk_mgr_ptr_copy = NULL;
static volatile sig_atomic_t is_Pk_mgr_ptr_copy_delete = TRUE;
static volatile sig_atomic_t http_srv_ready = 0;
static volatile sig_atomic_t threadCtl = 0;    // 0 can open thread  ,1 cant open 
#endif
//};

map<int , struct globeVar *> map_streamPort_globeVar ;
map<int , struct globeVar *> ::iterator map_streamPort_globeVar_iter ;

#ifdef _FIRE_BREATH_MOD_
#else

void signal_handler(int sig)
{
	fprintf(stdout, "\n\nrecv Signal %d\n\n", sig);

	//if(logger_client_ptr == NULL){
	//	logger_client_ptr = new logger_client();
	//	logger_client_ptr->log_init();
	//}

	switch (sig)
	{
		case SIGTERM: 
			srv_shutdown = 1; 
//			logger_client_ptr->log_exit();
			break;
		case SIGINT:
			srv_shutdown = 1;
//			logger_client_ptr->log_exit();
			break;
#ifndef _WIN32
		case SIGALRM: 
			handle_sig_alarm = 1; 
//			logger_client_ptr->log_exit();
			break;
#endif
		case SIGSEGV:
			srv_shutdown = 1;
//			logger_client_ptr->log_exit();
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
//			logger_client_ptr->log_exit();
#ifdef _WIN32
//			MessageBox(NULL, _T("SIGABRT"), _T("EXIT"), MB_OK | MB_ICONSTOP);
#else
			printf("SIGABRT\n");
			PAUSE
			exit(0);		// it must exit on linux
#endif			
			break;	
	}
}

#endif


#ifdef _FIRE_BREATH_MOD_
///////////////////////////////////////////////////////////////////////////////
/// @fn FB::variant llp2pAPI::echo(const FB::variant& msg)
///
/// @brief  Echos whatever is passed from Javascript.
///         Go ahead and change it. See what happens!
///////////////////////////////////////////////////////////////////////////////
FB::variant llp2pAPI::echo(const FB::variant& msg)
{
    static int n(0);
    fire_echo("So far, you clicked this many times: ", n++);

    // return "foobar";
    return msg;
}


///////////////////////////////////////////////////////////////////////////////
/// @fn llp2pPtr llp2pAPI::getPlugin()
///
/// @brief  Gets a reference to the plugin that was passed in when the object
///         was created.  If the plugin has already been released then this
///         will throw a FB::script_error that will be translated into a
///         javascript exception in the page.
///////////////////////////////////////////////////////////////////////////////
llp2pPtr llp2pAPI::getPlugin()
{
    llp2pPtr plugin(m_plugin.lock());
    if (!plugin) {
        throw FB::script_error("The plugin is invalid");
    }
    return plugin;
}

// Read/Write property testString
std::string llp2pAPI::get_testString()
{
    return m_testString;
}

void llp2pAPI::set_testString(const std::string& val)
{
    m_testString = val;
}

// Read-only property version
std::string llp2pAPI::get_version()
{
    return FBSTRING_PLUGIN_VERSION;
}

void llp2pAPI::testEvent()
{
    fire_test();
}

int llp2pAPI::start(int chid)
{
	//thread is running
	if(threadCtl){
		return FALSE;
	//threadCtl == 0
	}else{
		threadCtl = TRUE;  //lock
		http_srv_ready = 0;
		srv_shutdown = 0;
		_beginthread(launchThread, 0, (void*)chid);
		return TRUE;
	}

//	return;
}

void llp2pAPI::stop()
{
    srv_shutdown = 1;
//	return;
}

int llp2pAPI::isReady()
{
	while(!http_srv_ready)
		Sleep(10);
//	Sleep(500);
    return http_srv_ready;
}

int llp2pAPI::isStreamInChannel(int streamID){

	for(int i=0 ; i<=30;i++){
		if(is_Pk_mgr_ptr_copy_delete ==FALSE){
			map<int, struct update_stream_header *>::iterator  map_streamID_header_iter;
			map_streamID_header_iter = pk_mgr_ptr_copy ->map_streamID_header.find(streamID);
			//		return TRUE;
			if(map_streamID_header_iter != pk_mgr_ptr_copy ->map_streamID_header.end()){
				return TRUE;
			}else{
				return FALSE ;
			}
		}else{
			//	return FALSE;
		}
		Sleep(100);
	}

	return TRUE;
}

unsigned short llp2pAPI::streamingPortIs()
{
	if(is_Pk_mgr_ptr_copy_delete){
		return 0;
	}else{
		return streamingPort;
	}
}


/*
int llp2pAPI::valid(){

	return TRUE;
}
*/


void launchThread(void * arg)
{
	int a = (int)arg;
	threadCtl = mainFunction(a);
//	return;
}

#endif


#ifdef _FIRE_BREATH_MOD_
int mainFunction(int chid){
#else
int main(int argc, char **argv){
#endif

//	while((!srv_shutdown)){

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
		logger_client *logger_client_ptr=NULL;

//		rtmp_server *rtmp_svr = NULL;
//		rtmp *rtmp_ptr = NULL;
//		amf *amf_ptr = NULL;
//		rtmp_supplement *rtmp_supplement_ptr = NULL;

		bit_stream_server *bit_stream_server_ptr =NULL;
		peer_communication *peer_communication_ptr = NULL;

		config_file = "config.ini";

		prep = new configuration(config_file);
		net_ptr = new network(&errorRestartFlag,&fd_list);
		log_ptr = new logger();
		logger_client_ptr = new logger_client();
		log_ptr->logger_set(net_ptr);
		peer_mgr_ptr = new peer_mgr(&fd_list);
		if( (!prep) || (!net_ptr) || (!log_ptr) || (!logger_client_ptr) || (!peer_mgr_ptr) ){
			printf("new (!prep) || (!net_ptr) || (!log_ptr) || (!logger_client_ptr) || (!peer_mgr_ptr) error \n");
		}
		
#ifdef _FIRE_BREATH_MOD_
		char s[64];
		sprintf(s,"%d",chid);
		prep->add_key("channel_id", s);
#endif
		
		prep->read_key("html_size", html_size);
		prep->read_key("svc_tcp_port", svc_tcp_port);
		prep->read_key("svc_udp_port", svc_udp_port);
		prep->read_key("stream_local_port", stream_local_port);

		cout << "html_size=" << html_size << endl;
		cout << "svc_tcp_port=" << svc_tcp_port << endl;
		cout << "svc_udp_port=" << svc_udp_port << endl;
		cout << "stream_local_port=" << stream_local_port << endl;

		pk_mgr_ptr = new pk_mgr(html_size, &fd_list, net_ptr , log_ptr , prep , logger_client_ptr);
		if(!pk_mgr_ptr){
			printf("pk_mgr_ptr error !!!!!!!!!!!!!!!\n");
		}

		net_ptr->epoll_creater();
		log_ptr->start_log_record(SYS_FREQ);

		peer_mgr_ptr->peer_mgr_set(net_ptr, log_ptr, prep, pk_mgr_ptr, logger_client_ptr);
		//peer_mgr_ptr->pk_mgr_set(pk_mgr_ptr);
		pk_mgr_ptr->peer_mgr_set(peer_mgr_ptr);

		peer_communication_ptr = new peer_communication(net_ptr,log_ptr,prep,peer_mgr_ptr,peer_mgr_ptr->get_peer_object(),pk_mgr_ptr,logger_client_ptr);
		if(!peer_communication_ptr){
			printf("peer_commuication_ptr error!!!!!!!!!!\n");
		}
		peer_mgr_ptr->peer_communication_set(peer_communication_ptr);

		logger_client_ptr->set_net_obj(net_ptr);
		logger_client_ptr->set_pk_mgr_obj(pk_mgr_ptr);


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

#ifdef _FIRE_BREATH_MOD_
#else
		signal(SIGTERM, signal_handler);
		signal(SIGINT,  signal_handler);
		signal(SIGSEGV, signal_handler);
		signal(SIGABRT, signal_handler);
#endif

		if(mode==mode_RTMP){
/*
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
*/

			//mode BitStream
		}else if(mode==mode_BitStream  || mode == mode_HTTP){
			printf("mode_BitStream\n");


			bit_stream_server_ptr = new bit_stream_server ( net_ptr,log_ptr,pk_mgr_ptr ,&fd_list);
			if(!bit_stream_server_ptr){
				printf("bit_stream_server_ptr error !!!!!!!!!!!!\n");
			}
			stringstream ss_tmp;
			unsigned short port_tcp;
			ss_tmp << stream_local_port;
			ss_tmp >> port_tcp;
			streamingPort = bit_stream_server_ptr ->init(0,port_tcp);
			log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"new bit_stream_server ok at port ",streamingPort);

		}else if(mode == mode_RTSP){
			printf("mode_RTSP\n");
		}



		svc_fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
		if(svc_fd_tcp < 0) {
			log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"create tcp srv socket fail");
		}
		svc_fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
		if(svc_fd_udp< 0){
			log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"create udp srv socket fail");
		}

		memset(&sin, 0x0, sizeof(struct sockaddr_in));

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
//		sin.sin_port = htons((unsigned short)atoi(svc_tcp_port.c_str()));
		unsigned short ptop_port = (unsigned short)atoi(svc_tcp_port.c_str());

		while(1){
			sin.sin_port = htons(ptop_port);

			if (net_ptr->bind(svc_fd_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == 0) {
				log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"Server bind at TCP port: ",ptop_port);
				cout << "Server bind at TCP port: " << ptop_port << endl;
				setsockopt(svc_fd_tcp, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval , sizeof(optval));
			}else{
				printf("Server bind at TCP port:  REEOR  !!!!!\n",ptop_port);
				log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"Server bind at TCP port: FIRST REEOR  !!!!! ",ptop_port);
				ptop_port++;
				continue;
			}


			if (listen(svc_fd_tcp, MAX_POLL_EVENT) == 0) {
				cout << "Server LISTRN SUCCESS at TCP port: " << ptop_port << endl;
				log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"Server LISTRN SUCCESS at TCP port:",ptop_port);		
				break;
			}else{
				log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"Server LISTRN  ERROR! at TCP port:",ptop_port);		
				cout << "TCP PORT :" << svc_tcp_port << " listen error! " << endl;
				ptop_port++;
				continue;
			}
		}
		
		net_ptr->set_nonblocking(svc_fd_tcp);

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
			log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"setitimer ERROR ");		
			return -1;
		}

		getitimer(ITIMER_REAL, &interval);
#endif

		net_ptr->set_nonblocking(svc_fd_tcp);

		net_ptr->epoll_control(svc_fd_tcp, EPOLL_CTL_ADD, EPOLLIN);

		net_ptr->set_fd_bcptr_map(svc_fd_tcp, dynamic_cast<basic_class *>(peer_communication_ptr->get_io_accept_handler()));


		fd_list.push_back(svc_fd_tcp);
		pk_mgr_ptr->init(ptop_port);
		logger_client_ptr->log_init();


		int testServerfd =0;
		int errorcount =0;
		while(1){
			testServerfd = pk_mgr_ptr ->build_connection("127.0.0.1",stream_local_port);
			if(testServerfd){

				printf("connect ok \n");
				shutdown(testServerfd, SHUT_RDWR);
				closesocket(testServerfd);
				log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"connect ok");
				break ;
			}else{
				Sleep(100);
				errorcount++;
				if(errorcount++ >=50)
					break;
				printf("connectfail  errorcount=%d\n",errorcount);
				log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"connectfail  errorcount",errorcount);
			}
		}

#ifdef _FIRE_BREATH_MOD_
		pk_mgr_ptr_copy =pk_mgr_ptr ;
		http_srv_ready = 1;
		is_Pk_mgr_ptr_copy_delete = FALSE;
#endif

		while((!srv_shutdown)  /*&&  (!errorRestartFlag)*/) {

#ifdef _WIN32
			net_ptr->epoll_waiter(1000, &fd_list);
#else
			net_ptr->epoll_waiter(1000);
#endif
			net_ptr->epoll_dispatcher();

			pk_mgr_ptr->time_handle();
		}

		log_ptr->write_log_format("s =>u s d s d \n", __FUNCTION__,__LINE__,"srv_shutdown",srv_shutdown,"errorRestartFlag",errorRestartFlag);


		net_ptr->garbage_collection();
		log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "PF", "graceful exit!!");
		log_ptr->stop_log_record();

		if(prep) 
			delete prep;
		prep =NULL;

		if(bit_stream_server_ptr)
			delete bit_stream_server_ptr;
		bit_stream_server_ptr =NULL;

		if(peer_communication_ptr)
			delete peer_communication_ptr;
		peer_communication_ptr =NULL;

		if(net_ptr)
			delete net_ptr;
		net_ptr =NULL;	

#ifdef _FIRE_BREATH_MOD_
		is_Pk_mgr_ptr_copy_delete = TRUE;
		pk_mgr_ptr_copy =NULL;
		http_srv_ready = 0;
#endif
		if(pk_mgr_ptr)
			delete pk_mgr_ptr;
		pk_mgr_ptr =NULL;

//		if(rtmp_svr)
//			delete rtmp_svr;
//		rtmp_svr =NULL;

		if(peer_mgr_ptr) 
			delete peer_mgr_ptr;
		peer_mgr_ptr=NULL;

		if(log_ptr)
			delete log_ptr;
		log_ptr =NULL;

		if(logger_client_ptr)
			delete logger_client_ptr;
		logger_client_ptr =NULL;

//		PAUSE
		for(int i= 0 ; i< 10;i++){
			printf("Some Thing Error Wait %d Sec To Restart ............\n",10-i );
//			Sleep(1000);
		}
		errorRestartFlag =0;


//	}

	streamingPort = 0;
	handle_sig_alarm = 0;
	srv_shutdown = 0;
	errorRestartFlag = 0;

	return EXIT_SUCCESS;

}


