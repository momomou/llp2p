#ifndef __LOG_RECORD_H__
#define __LOG_RECORD_H__

#include "common.h"
#include "network.h"
#ifdef _WIN32
#else
#include<sys/time.h>
#endif

typedef enum
{ LOGCRIT=0, LOGERROR, LOGWARNING, LOGINFO,
  LOGDEBUG, LOGDEBUG2, LOGALL
} AMF_LogLevel;

/*
#define Log	AMF_Log
#define LogHex	AMF_LogHex
#define LogHexString	AMF_LogHexString
#define LogPrintf	AMF_LogPrintf
#define LogSetOutput	AMF_LogSetOutput
#define LogStatus	AMF_LogStatus
#define debuglevel	AMF_debuglevel
*/
#define MAX_PRINT_LEN	2048
#define _DEBUG 1
#define NONINIT 0
#define INITED 1


class logger {
	
public:

	volatile int timerMod;
	void timer();
	char *get_now_time();
	void start_log_record(int time);
	void stop_log_record();
	void write_log_format(const char* fmt, ...);
	void write_binary(unsigned int sequence_number);
	void exit(int status, const char *action);

	void time_init();
	bool check_arch_compatible();
	bool handleAlarm();
	int set_resource_limit(int maxfds);
	void logger_set(network *net_ptr);
	void LogPrintf(const char *format, ...);
	void LogStatus(const char *format, ...);
	void Log(int level, const char *format, ...);
	void LogHex(int level, const char *data, unsigned long len);
	void LogHexString(int level, const char *data, unsigned long len);
#ifdef _WIN32
	int getTickTime(LARGE_INTEGER *tickTime);
	LONGLONG diffTime_us(LARGE_INTEGER startTime,LARGE_INTEGER endTime);
	unsigned int diffTime_ms(LARGE_INTEGER startTime,LARGE_INTEGER endTime);
	
	DWORD getTime();
	DWORD diffgetTime_ms(DWORD startTime,DWORD endTime);
#else
	
	unsigned long diffgetTime_ms(unsigned long startTime,unsigned long endTime);
#endif
	unsigned int diff_TimerGet_ms(struct timerStruct *startTime ,struct timerStruct *endTime);
	void timerGet(struct timerStruct *timmer);

	 int gettimeofday(struct timeval *tp, void *tzp);
	 unsigned int gettimeofday_ms(/*struct timeval *tv*/);
#ifdef WIN32
	
#endif
    unsigned long timevaldiff(struct timeval *starttime, struct timeval *finishtime);
    double set_diff_timmer();

	
	logger( );
	~logger();
	logger(const logger&);
	logger& operator=(const logger&);
	int is_little_endian();
	
private:
	
	FILE *_fp;
//	FILE *_binary_fp;
	int _systime;
	network *_net_ptr;
	time_t last_alarm;
	AMF_LogLevel _debuglevel;
	int _neednl, is_diff_timmer_set;

    clock_t detail_time;

};

#endif

