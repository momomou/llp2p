#ifndef __PEER_COMMUNICATION_H__
#define __PEER_COMMUNICATION_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network;
class logger;
class peer_mgr;
class peer;
class pk_mgr;
class io_accept;
class io_connect;
class logger_client;
class io_nonblocking;

class peer_communication:public basic_class{
public:
	peer_communication(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, logger_client * logger_client_ptr);
	~peer_communication();

	void set_self_info(unsigned long public_ip);
	int set_candidates_handler(unsigned long rescue_manifest,struct chunk_level_msg_t *testing_info,unsigned int candidates_num, int flag);	//parameter candidates_num may be zero 
	void stop_attempt_connect(unsigned long stop_session_id);
	void clear_fd_in_peer_com(int sock);
	int non_blocking_build_connection(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, int flag, unsigned long session_id);
	io_accept * get_io_accept_handler();
	void accept_check(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id);
	void fd_close(int sock);

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	unsigned long total_manifest;
	unsigned long session_id_count;
	struct level_info_t *self_info;
	map<unsigned long, struct peer_com_info *> session_id_candidates_set;
	map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;

	map<int, struct fd_information *> map_fd_info;
	map<int, struct fd_information *>::iterator map_fd_info_iter;

	map<int , struct ioNonBlocking*> map_fd_NonBlockIO;
	map<int ,  struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
	/*map<int, int> map_fd_flag;	//flag 0 rescue peer, flag 1 candidates, and delete in stop
	map<int, unsigned long> map_fd_session_id;	//must be store before io_connect, and delete in stop
	map<int, unsigned long> map_peer_com_fd_pid;	//must be store before io_connect, and delete in stop
	map<int, unsigned long> map_fd_manifest;	//must be store before io_connect, and delete in stop*/
	//map<int, int>::iterator map_fd_flag_iter;

	//FILE *peer_com_log;
	logger_client * _logger_client_ptr;
	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	pk_mgr * _pk_mgr_ptr;
	io_accept *_io_accept_ptr;
	io_connect *_io_connect_ptr;
	io_nonblocking *_io_nonblocking_ptr;
	list<int> *fd_list_ptr;

};

#endif