// Copyright (c) 2010, NetXtream Inc.
// All rights reserved.
// 
// Author: Yeh-Sheng-Lin

// This is a TEMPLATE CLASS TO ACCEPT MULTIPLE read_key value class

#include "configuration.h"
#include "common.h"

configuration::configuration() : filename(""), tmp_only(true) 
{
	
}

configuration::configuration(string file) : filename(file), tmp_only(false) 
{
	printf("-111 \n");
#ifdef _FIRE_BREATH_MOD_
	map_table["bucket_size"] = "8192";
	map_table["channel_id"] = "0";
	map_table["html_size"] = "8192";
	map_table["lane_depth"] = "3";
	map_table["max_lane"] = "8";
	map_table["min_lane"] = "1";
	map_table["pk_ip"] = "140.114.71.174";
	map_table["pk_port"] = "8856";
	map_table["log_ip"] = "140.114.71.174";
	map_table["log_port"] = "9956";
	map_table["stream_local_port"] = "3000";
	map_table["sub_stream_num"] = "4";
	map_table["svc_tcp_port"] = "5566";
	map_table["svc_udp_port"] = "7788";
#else
	printf("-222 \n");
	fstream fh(filename.c_str(), fstream::in);
	printf("-333 \n");
	if (fh.is_open()) {
		printf("-444 \n");
		string linebuf;
		while(getline(fh, linebuf)) {
			size_t pos;                                     // we don't need initial this value

			if(linebuf.empty()) continue;          // equal empty string
			if((pos = linebuf.find_first_of("=")) == string::npos) continue;
			if(pos+1 == linebuf.size()) continue;           // avoid KEY=[NULL]

			string key = linebuf.substr(0, pos);
			string val = linebuf.substr(pos+1);

			if(key.empty() || val.empty()) continue;

			map_table[key] = val;                           // if duplicate key, it will override the value
		}

		fh.close();
	}
    else{
		printf("can't open 'config.ini'\n");

		map_table["bucket_size"] = "8192";
		map_table["channel_id"] = "0";
		map_table["html_size"] = "8192";
		map_table["lane_depth"] = "3";
		map_table["max_lane"] = "8";
		map_table["min_lane"] = "1";
		map_table["pk_ip"] = "140.114.71.174";
		map_table["pk_port"] = "8856";
		map_table["log_ip"] = "140.114.71.174";
		map_table["log_port"] = "9956";
		map_table["stream_local_port"] = "3000";
		map_table["sub_stream_num"] = "4";
		map_table["svc_tcp_port"] = "5566";
		map_table["svc_udp_port"] = "7788";
	
	}
#endif
	
/*
#ifdef _FIRE_BREATH_MOD_
#else
	if(fh.is_open()) {
		string linebuf;
		while(getline(fh, linebuf)) {
			size_t pos;                                     // we don't need initial this value

			if(linebuf.empty()) continue;          // equal empty string
			if((pos = linebuf.find_first_of("=")) == string::npos) continue;
			if(pos+1 == linebuf.size()) continue;           // avoid KEY=[NULL]

			string key = linebuf.substr(0, pos);
			string val = linebuf.substr(pos+1);

			if(key.empty() || val.empty()) continue;

			map_table[key] = val;                           // if duplicate key, it will override the value
		}

		fh.close();
	}
    else{
		printf("can't open 'config.ini'\n");

#endif
		map_table["bucket_size"] = "8192";
		map_table["channel_id"] = "0";
		map_table["html_size"] = "8192";
		map_table["lane_depth"] = "3";
		map_table["max_lane"] = "8";
		map_table["min_lane"] = "1";
		map_table["pk_ip"] = "140.114.71.174";
		map_table["pk_port"] = "8856";
		map_table["log_ip"] = "140.114.71.174";
		map_table["log_port"] = "9956";
		map_table["stream_local_port"] = "3000";
		map_table["sub_stream_num"] = "4";
		map_table["svc_tcp_port"] = "5566";
		map_table["svc_udp_port"] = "7788";
		
#ifdef _FIRE_BREATH_MOD_
#else
	}
#endif

*/
}


configuration::~configuration() 
{
/*
	if(!tmp_only) {
		fstream fh(filename.c_str(), fstream::out);

		if(fh.is_open()) {

			for(map<string, string>::const_iterator iter=map_table.begin(); iter!=map_table.end(); iter++) {
				fh << iter->first << "=" << iter->second << endl;
			}

			fh.close();
		}
	}
*/	
	printf("==============deldet configuration success==========\n");

}

void configuration::add_key(const char *key,std::string val)
{
    map_table[key] = val; 
}

void configuration::read_key(const char *key, bool & retval) const
{	// find the value of parameter in config.ini
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, short & retval) const 
{	// find the value of parameter in config.ini 
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, unsigned short & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, int & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key2(const char *key, int *retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> *retval;
}

void configuration::read_key(const char *key, unsigned int & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, long & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, unsigned long & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, float & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, double & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, long double & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, void* & retval) const 
{	// find the value of parameter in config.ini 
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::read_key(const char *key, std::string & retval) const 
{	// find the value of parameter in config.ini  
	map<string, string>::const_iterator iter;
	stringstream ss;
	if((iter = map_table.find(key)) == map_table.end()) {	// only find once, and get the iter avoid double travesal tree
		ss << "";                                       // not found the key, we use "" to stringstream
	}else{
		ss << iter->second;
	}
	ss >> retval;
}

void configuration::dump_map() const {
	for(map<string, string>::const_iterator iter=map_table.begin(); iter!=map_table.end(); iter++) {
		cout << iter->first << ":" << iter->second << endl;
	}
}



