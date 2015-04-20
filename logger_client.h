#ifndef __LOGGER_CLIENT_H__
#define __LOGGER_CLIENT_H__

#include "common.h"
#include "network.h"
#include "pk_mgr.h"

class logger_client:public basic_class{	
public:
	/*
	1. constructor will init obj
	2. it will call the timer init.
	*/
	logger_client(logger *log_ptr);
	~logger_client();

	/*
	1. the funciton will connect to log server.
	3. it will init the log buffer.
	2. connect to log server
	*/
	void log_init();

	/*
	1. we will call log_to_server and log_clear_buffer after it
	2. set fd handler
	*/
	void set_self_pid_channel(unsigned long pid,unsigned long channel_id);
	void set_self_ip_port(unsigned long public_ip, unsigned short public_port, unsigned long private_ip, unsigned short private_port);
	void set_net_obj(network *net_ptr);
	void set_pk_mgr_obj(pk_mgr *pk_mgr_ptr);
	void set_prep_obj(configuration *prep);
	void add_nat_total_times();
	void add_nat_success_times();

//	void logger_client_getTickTime(LARGE_INTEGER *tickTime);
//	unsigned int logger_client_diffTime_ms(LARGE_INTEGER startTime,LARGE_INTEGER endTime);

	/*
	init the log timer
	*/
	void log_timer_init();

	/*
	it will count the period of start and now, then stores into variable.
	*/
	void log_time_differ();

	/*
	set start delay
	*/
	void count_start_delay();

	/*
	new sub-streams num size table
	*/
	void source_delay_struct_init(unsigned long sub_stream_num);
	
	/*
	set source delay
	*/
	void set_source_delay(unsigned long substream_id,unsigned long source_delay);

	/*
	send max source delay to PK
	*/
	void send_max_source_delay();

	/*
	bw_init
	*/
	void bw_in_struct_init(unsigned long timestamp,unsigned long pkt_size);

	/*
	set in bw
	*/
	void set_in_bw(unsigned long timestamp,unsigned long pkt_size);
	void set_in_message_bw(UINT32 pkt_header_size, UINT32 pkt_payload_size, INT32 pkt_type);

	/*
	send qualtiy and bandwidth to PK
	*/
	void send_bw();

	/*
	send topology to log server
	*/
	void send_topology(unsigned long* parent_list, int sub_stream_num);

	/*
	bw_init
	*/
	void bw_out_struct_init(unsigned long pkt_size);

	/*
	set out bw
	*/
	void set_out_bw(unsigned long pkt_size);
	void set_out_message_bw(UINT32 pkt_header_size, UINT32 pkt_payload_size, INT32 pkt_type);

	/*
	write the log info to log buffer 
	*/
	void log_to_server(int log_mode, ...);

	/*
	start send all buffer content to log server
	*/
	void log_clear_buffer();
	
	/*
	force sending all buffer content to log server
	*/
	void log_exit();

	void handle_error(int exit_code, const char *msg, const char *func, unsigned int line);

	/*
	this part just implement handle pkt out
	*/
	virtual int handle_pkt_in(int sock);
	/*
	write all buffer's content to log server according to log algorithm :
	para : log_times (times/per sec), time_bw (bits/per times), buffer_content_threshold, log_bw (bits/per sec), time_period(sec)
	1. log_bw = log_times * time_bw
	2. 1 = time_period * log_times

	timer = 0
	loop :
		if timer == times of time_period
			if buffer's size < buffer_content_threshold
				if buffer's size > time_bw
					send time_bw's content to log serer
				else
					send all content to log server.
			else
				send all content to log server.
		timer increase
	*/
	virtual int handle_pkt_out(int sock);
	virtual void handle_pkt_error(int sock);
	virtual void handle_sock_error(int sock, basic_class *bcptr);
	virtual void handle_job_realtime();
	virtual void handle_job_timer();

	network *_net_ptr;
	pk_mgr *_pk_mgr_ptr;
	configuration *_prep;
	logger *_log_ptr;

//	LARGE_INTEGER log_start_time,log_record_time;
	struct timerStruct log_start_time,log_record_time;
	unsigned long log_time_dffer;
	unsigned long previous_time_differ;
	double start_delay;
//	LARGE_INTEGER log_period_source_delay_start;
	struct timerStruct log_period_source_delay_start;
	struct log_source_delay_struct *max_source_delay;
	double *delay_list;
	unsigned long *parent_list;
	unsigned long sub_stream_number;
	int log_source_delay_init_flag;

	//struct log_in_bw_struct start_in_bw_record,end_in_bw_record;
//	LARGE_INTEGER start_out_bw_record,end_out_bw_record;
//	LARGE_INTEGER log_period_bw_start;
	//struct timerStruct start_out_bw_record,end_out_bw_record;
	struct timerStruct log_period_bw_start;			// Timer for periodically send bw messages
	struct message_bw_analysis_struct message_bw_analysis;
	unsigned long accumulated_packet_size_in;		// Accumulate total size of packets in
	unsigned long accumulated_packet_size_out;		// Accumulate total size of packets out
	unsigned long pre_in_pkt_size;
	unsigned long pre_out_pkt_size;
	int log_bw_in_init_flag,log_bw_out_init_flag;
	unsigned long bitrate;

	double nat_total_times;							// 嘗試的所有NAT穿透次數
	double nat_success_times;						// 成功的NAT穿透成功次數

	struct quality_struct *quality_struct_ptr;

	struct chunk_t *chunk_buffer;
	queue<struct log_pkt_format_struct*> log_buffer;
	int buffer_size;
	int buffer_clear_flag;
	unsigned long self_pid;
	unsigned long self_channel_id;
	unsigned long my_public_ip;
	unsigned short my_public_port;
	unsigned long my_private_ip;
	unsigned short my_private_port;
	int log_server_sock;
	unsigned char exit_code;		// Error code (for program exit)
	

	Nonblocking_Buff non_log_recv_struct;
};

#endif

