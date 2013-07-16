#include "logger.h"
#include "common.h"

static const char *levels[] = {
  "CRIT", "ERROR", "WARNING", "INFO",
  "DEBUG", "DEBUG2"
};

void logger::timer() 
{
	timerMod =MOD_TIME__CLOCK ;
	if(--_systime == 0) {
		write_log_format("s => [U/U]\n", 
		"Rate[KB]", _net_ptr->send_byte/SIG_FREQ/1024, _net_ptr->recv_byte/SIG_FREQ/1024);
		_systime = SYS_FREQ;
	}
}

void logger::time_init()
{
	last_alarm = time(NULL);
}

bool logger::handleAlarm() {
	time_t now = time(NULL);

	if(now - last_alarm) {
		last_alarm = now;
		//this->timer();			// call periodical task
		return true;
	}
	
	return false;
}
 

char *logger::get_now_time() 
{
	time_t T;
	time(&T);
	char *ct = ctime(&T);
	ct[strlen(ct)-1] = 0x0;
	return ct;
}

void logger::start_log_record(int time) 
{
	char*   szPath[MAX_PATH]= {'\0'};
#ifdef _FIRE_BREATH_MOD_
	GetEnvironmentVariableA ("APPDATA" ,(LPSTR)szPath,   MAX_PATH);  
	strcat((char*)szPath,"\\LLP2P\\");
	CreateDirectoryA((char*)szPath,NULL);
#endif
	strcat((char*)szPath,"P2PLog.txt");
	printf("PATH = %s  \n",szPath);
	_systime = time;
	_fp = fopen((char*)szPath, "w");
	if(!_fp) {
		cout << "Cannot write log file" << endl;
//		::exit(0);
	}

//	_binary_fp = fopen(LOGBINARYFILE, "wb");
//	if(_binary_fp==NULL) {
//		cout << "Cannot write binary log file" << endl;
//		::exit(0);
//	}
}

void logger::stop_log_record() 
{

	if(_fp== NULL)
		return;
//	if(_binary_fp== NULL)
//		return;

	fprintf(_fp, "===================================================================================================\n");
	fflush(_fp);
	fclose(_fp);
//	fflush(_binary_fp);
//	fclose(_binary_fp);
}

void logger::write_log_format(const char* fmt, ...) 
{
	va_list ap;
	int d;
	unsigned int u;
	char *s;
	unsigned long long int llu;
	struct in_addr pip;

	if(_fp== NULL)
		return;
//	if(_binary_fp== NULL)
//		return;


	fprintf(_fp,"[%s] ", get_now_time());

	for(va_start(ap, fmt); *fmt; fmt++) {
		switch(*fmt) {
			case 's':           /* string */
				s = va_arg(ap, char *);
				fprintf(_fp, "%s", s);
				break;
			case 'd':           /* int */
				d = va_arg(ap, int);
				fprintf(_fp, "%d", d);
				break;
			case 'u':           /* unsigned int */
				u = va_arg(ap, unsigned int);
				fprintf(_fp, "%u", u);
				break;
			case 'x':           /* int */
				d = va_arg(ap, int);
				fprintf(_fp, "%02x", d);
				break;
			case 'U':	    /* unsigned long long int */
				llu = va_arg(ap, unsigned long long int);
				fprintf(_fp, "%llu", llu);
				break;
			case 'V': 	    /* ip address */
				pip = va_arg(ap, struct in_addr);			
				fprintf(_fp, "%s", inet_ntoa(pip));
				break;
			default:
				fprintf(_fp, "%c", *fmt);
		}
	}

	fflush(_fp);
	va_end(ap);
}


void logger::write_binary(unsigned int sequence_number)
{
	time_t now;
	tm *ptm;
	time(&now);
	ptm = gmtime(&now);
	
//	fwrite(ptm ,sizeof(tm) , 1, _binary_fp);
//	fwrite(&sequence_number ,sizeof(unsigned int) , 1, _binary_fp);
}


void logger::exit(int status, const char *action) 
{
	write_log_format("s => s (s)\n", "log Terminate", "exit()", action);
	stop_log_record();
	_net_ptr->garbage_collection();
	printf("logger error\n");
	*(_net_ptr->_errorRestartFlag) =RESTART;
//	PAUSE
	//::exit(status);
}


bool logger::check_arch_compatible() 
{

	if(!is_little_endian()) {
		return false;
	}

	if(sizeof(int) != 4) {
		return false;
	}

	
	return true;
}


int logger::set_resource_limit(int maxfds) 
{	
	return 1;
}

void logger::logger_set(network *net_ptr )
{
	_net_ptr = net_ptr;

}


logger::logger()
{
	_neednl = 0;
	_debuglevel = LOGCRIT;
    is_diff_timmer_set = 0;
}

logger::~logger() 
{
	printf("==============deldet logger success==========\n");
}


int logger::is_little_endian() 
{
	int one = 1;
	return *(char*)&one;
}

void logger::LogPrintf(const char *format, ...)
{
	char str[MAX_PRINT_LEN]="";
        int len;
	va_list args;
	va_start(args, format);
	len = vsnprintf(str, MAX_PRINT_LEN-1, format, args);
	va_end(args);

	if ( _debuglevel==LOGCRIT )
		return;

	if (_neednl) {
		putc('\n', _fp);
		_neednl = 0;
	}

	if (len > MAX_PRINT_LEN-1)
		len = MAX_PRINT_LEN-1;
	fprintf(_fp, "%s", str);
	if (str[len-1] == '\n')
		fflush(_fp);
}

void logger::LogStatus(const char *format, ...)
{
	char str[MAX_PRINT_LEN]="";
	va_list args;
	va_start(args, format);
	vsnprintf(str, MAX_PRINT_LEN-1, format, args);
	va_end(args);

	if ( _debuglevel==LOGCRIT )
		return;

	fprintf(_fp, "%s", str);
	fflush(_fp);
	_neednl = 1;
}

void logger::Log(int level, const char *format, ...)
{
#ifdef _DEBUG

	char str[MAX_PRINT_LEN]="";
	va_list args;
	va_start(args, format);
	vsnprintf(str, MAX_PRINT_LEN-1, format, args);
	va_end(args);

	// Filter out 'no-name'
	if ( _debuglevel<LOGALL && strstr(str, "no-name" ) != NULL )
		return;

	if ( level <= _debuglevel ) {
		if (_neednl) {
			putc('\n', _fp);
			_neednl = 0;
		}
		fprintf(_fp, "%s: %s\n", levels[level], str);

		fflush(_fp);

	}
#endif
}

void logger::LogHex(int level, const char *data, unsigned long len)
{
	unsigned long i;
	if ( level > _debuglevel )
		return;
	for(i=0; i<len; i++) {
		LogPrintf("%02X ", (unsigned char)data[i]);
	}
	LogPrintf("\n");
}

void logger::LogHexString(int level, const char *data, unsigned long len)
{
	static const char hexdig[] = "0123456789abcdef";
#define BP_OFFSET 9
#define BP_GRAPH 60
#define BP_LEN	80
	char	line[BP_LEN];
	unsigned long i;

	if ( !data || level > _debuglevel )
		return;

	/* in case len is zero */
	line[0] = '\n';
	line[1] = '\0';

	for ( i = 0 ; i < len ; i++ ) {
		int n = i % 16;
		unsigned off;

		if( !n ) {
			if( i ) LogPrintf( "%s", line );
			memset( line, ' ', sizeof(line)-2 );
			line[sizeof(line)-2] = '\n';
			line[sizeof(line)-1] = '\0';

			off = i % 0x0ffffU;

			line[2] = hexdig[0x0f & (off >> 12)];
			line[3] = hexdig[0x0f & (off >>  8)];
			line[4] = hexdig[0x0f & (off >>  4)];
			line[5] = hexdig[0x0f & off];
			line[6] = ':';
		}

		off = BP_OFFSET + n*3 + ((n >= 8)?1:0);
		line[off] = hexdig[0x0f & ( data[i] >> 4 )];
		line[off+1] = hexdig[0x0f & data[i]];

		off = BP_GRAPH + n + ((n >= 8)?1:0);

		if ( isprint( (unsigned char) data[i] )) {
			line[BP_GRAPH + n] = data[i];
		} else {
			line[BP_GRAPH + n] = '.';
		}
	}

	LogPrintf( "%s", line );
}

//沒有時計準確到us的實做 其tv_usec 只準確到msec
#ifdef WIN32
int logger::gettimeofday(struct timeval *tv, void *tzp)
{
    union {
        long long ns100;
        FILETIME ft;
    } now;
     
    GetSystemTimeAsFileTime (&now.ft);
    tv->tv_usec = (long) ((now.ns100 / 10LL) % 1000000LL);
    tv->tv_sec = (long) ((now.ns100 - 116444736000000000LL) / 10000000LL);
    return (0);
}
#endif

#ifdef WIN32
unsigned int logger::gettimeofday_ms(/*struct timeval *tv*/)
{
	unsigned int time_ms;
	struct timeval tv;
	gettimeofday(&tv, NULL) ;
	time_ms= (tv.tv_sec)*1000 + (unsigned int)(tv.tv_usec)/1000 ;
	return time_ms;
}
#endif

#ifdef WIN32
DWORD logger::getTime()
{
	return timeGetTime();
//	return (DWORD)clock();
//	return GetTickCount();

}
#endif


#ifdef WIN32
DWORD logger::diffgetTime_ms(DWORD startTime,DWORD endTime)
{
	int diffValue = endTime-startTime;
    return abs(diffValue);
}
#endif



#ifdef WIN32
void logger::getTickTime(LARGE_INTEGER *tickTime)
{
	bool fail =QueryPerformanceCounter(tickTime);
	if(fail == 0){
		timerMod =MOD_TIME__CLOCK ;
		printf("QueryPerformanceCounter fail GetLastError = %d\n",GetLastError());
//		PAUSE
	}
}
#endif


#ifdef WIN32
LONGLONG logger::diffTime_us(LARGE_INTEGER startTime,LARGE_INTEGER endTime)
{
    LARGE_INTEGER CUPfreq;
    LONGLONG llLastTime;
    QueryPerformanceFrequency(&CUPfreq);
    llLastTime = 1000000 * (endTime.QuadPart - startTime.QuadPart) / CUPfreq.QuadPart;
    return llLastTime;
}
#endif

#ifdef WIN32
unsigned int logger::diffTime_ms(LARGE_INTEGER startTime,LARGE_INTEGER endTime)
{
    LARGE_INTEGER CUPfreq;
    LONGLONG llLastTime;
    QueryPerformanceFrequency(&CUPfreq);
    llLastTime = (unsigned int)  (1000 * (endTime.QuadPart - startTime.QuadPart) / CUPfreq.QuadPart);

    return llLastTime;
}
#endif


unsigned long logger::timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
	unsigned long msec;
	msec = (finishtime->tv_sec-starttime->tv_sec) * 1000000;
	msec += (finishtime->tv_usec-starttime->tv_usec);
	return msec;
}

double logger::set_diff_timmer()
{
    clock_t start_time;

    if(is_diff_timmer_set) {
        start_time = detail_time;
        detail_time = clock();
        return (double)(detail_time - start_time) / CLOCKS_PER_SEC;  //CLOCKS_PER_SEC by define =1000
    } else {
        is_diff_timmer_set = 1;
        detail_time = clock();
        return 0;
    }
}



#ifdef WIN32
void logger::timerGet(struct timerStruct *timmer)
{

	getTickTime(&(timmer ->tickTime));

	timmer ->clockTime = gettimeofday_ms() ;
	timmer->initFlag =INITED ;
}
#endif


#ifdef WIN32
unsigned int logger::diff_TimerGet_ms(struct timerStruct *start,struct timerStruct *end)
{

	if(start ->initFlag != INITED ){
		printf("start timer not INITED \n");
		PAUSE
	}

	if(end ->initFlag != INITED ){
		printf("end timer not INITED \n");
		PAUSE
	}

	if(timerMod == MOD_TIME_TICK)
		return diffTime_ms(start->tickTime ,end->tickTime);
	else
		return diffgetTime_ms(start->clockTime,end->clockTime) ;
}
#endif
