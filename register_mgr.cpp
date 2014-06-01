#include "register_mgr.h"
#include "common.h"
#include "logger.h"

register_mgr::register_mgr()
{
	pk_ip = "";
	pk_port = "";
	got_pk = false;
}

register_mgr::~register_mgr()
{
	
}

// Connect to register server. Send channel ID request and get pk address response. Then, disconnect with register server.
void register_mgr::build_connect(int channel_id)
{
	int sock;
	int retVal;
	struct sockaddr_in pk_saddr;
	char buff[50] = {0};
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", sock, socketErr);
		return ;
	}

	memset(&pk_saddr, 0, sizeof(struct sockaddr_in));
	pk_saddr.sin_addr.s_addr = inet_addr("140.114.236.72");
	//pk_saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	pk_saddr.sin_port = htons(8840);
	pk_saddr.sin_family = AF_INET;

	if ((retVal = connect(sock, (struct sockaddr*)&pk_saddr, sizeof(pk_saddr))) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("Connect to register server fail %d \n", retVal, socketErr);
		return ;
	}
	
	sprintf(buff, "%d", channel_id);
	buff[strlen(buff)] = 13;
	
	// Send channel ID
	if ((retVal = send(sock, buff, strlen(buff), 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("Send message to register server fail %d \n", retVal, socketErr);
		return ;
	}
	
	memset(buff, 0, sizeof(buff));
	// Receive PK address
	if ((retVal = recv(sock, buff, sizeof(buff), 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("Send message to register server fail %d \n", retVal, socketErr);
		return ;
	}
	debug_printf("Receive message from register: %s \n", buff);
	
	got_pk = true;
	
	string ssss;
	ssss.assign(buff);
	
	int n = ssss.find(":");
	pk_ip = ssss.substr(0, n);
	pk_port = ssss.substr(n+1, ssss.length());
#ifdef _WIN32
	closesocket(sock);
#else
	close(sock);
#endif
}

int register_mgr::handle_pkt_in(int sock)
{
	return RET_OK;
}

int register_mgr::handle_pkt_out(int sock)
{
	return RET_OK;
}

void register_mgr::handle_pkt_error(int sock)
{
	
}

void register_mgr::handle_sock_error(int sock, basic_class *bcptr)
{
	
}

void register_mgr::handle_job_realtime()
{

}

void register_mgr::handle_job_timer()
{
	
}