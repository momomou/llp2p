#ifndef __IO_NAT_PUNCH_H__
#define __IO_NAT_PUNCH_H__

#include "common.h"
#include "basic_class.h"
#include <iostream>
#include <map>

class network;
class logger;
class peer_mgr;
class peer;
class pk_mgr;
class peer_communication;
class nat_interface;

class io_nat_punch:public basic_class{
public:
	io_nat_punch(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr,nat_interface *nat_interface_ptr);
	~io_nat_punch();

	int start_nat_punch(int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id);
	void register_to_nat_server(unsigned long self_pid);

	virtual int handle_pkt_in(int sock);
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	char chServerIP1[20];
	char chServerIP2[20];
	char self_id[MAX_ID_LEN + 1];
	unsigned long self_id_integer;
	int nat_server_fd;

	//int client_fd[FD_SETSIZE];
	//int client_array_count;

	unsigned long nat_punch_counter;
	map<unsigned long,struct nat_con_thread_struct *> map_counter_thread;
	map<unsigned long,struct nat_con_thread_struct *>::iterator map_counter_thread_iter;
	map<unsigned long,unsigned long> map_counter_session_id;
	map<unsigned long,unsigned long>::iterator map_counter_session_id_iter;

	network *_net_ptr;
	logger *_log_ptr;
	configuration *_prep;
	peer_mgr * _peer_mgr_ptr;
	peer *_peer_ptr;
	pk_mgr * _pk_mgr_ptr;
	nat_interface * _nat_interface_ptr;
	peer_communication *_peer_communication_ptr;
};

#endif