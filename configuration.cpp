// Copyright (c) 2010, NetXtream Inc.
// All rights reserved.
// 
// Author: Yeh-Sheng-Lin

// This is a TEMPLATE CLASS TO ACCEPT MULTIPLE read_key value class

#include "configuration.h"
#include "common.h"
#include <string>
#include <algorithm> // sort
#include <stdio.h>

using namespace std;


configuration::configuration() : filename(""), tmp_only(true) 
{
	PAUSE	
}

configuration::configuration(string file) : filename(file), tmp_only(false) 
{
	std::string input = readConfigFile("llp2p.conf");
	//Json::Reader reader;
	//Json::Value value;
	if (reader.parse(input, value)) {
		map_table["bucket_size"] = value["BUCKET_SIZE"].asString();
		map_table["channel_id"] = value["CHANNEL_ID"].asString();
		map_table["html_size"] = value["HTML_SIZE"].asString();
		map_table["lane_depth"] = value["LANE_DEPTH"].asString();
		map_table["max_lane"] = value["MAX_LANE"].asString();
		map_table["min_lane"] = value["MIN_LANE"].asString();
		map_table["pk_ip"] = value["PK_SERVER"]["IP"].asString();
		map_table["pk_port"] = value["PK_SERVER"]["PORT"].asString();
		map_table["reg_ip"] = value["REG_SERVER"]["IP"].asString();
		map_table["reg_port"] = value["REG_SERVER"]["PORT"].asString();
		map_table["log_ip"] = value["LOG_SERVER"]["IP"].asString();
		map_table["log_port"] = value["LOG_SERVER"]["PORT"].asString();
		map_table["stun_ip"] = value["STUN_SERVER"]["IP"].asString();
		map_table["stream_local_port"] = value["STREAM"]["PORT"].asString();
		map_table["svc_tcp_port"] = value["P2P_TCP_PORT"].asString();
		map_table["svc_udp_port"] = value["P2P_UDP_PORT"].asString();

		debug_printf("---------Configuration Setting--------- \n");
		for (map<string, string>::iterator iter = map_table.begin(); iter != map_table.end(); iter++) {
			debug_printf("%s : %s \n", iter->first.c_str(), iter->second.c_str());
		}
		debug_printf("--------------------------------------- \n");
	}
	else {
		map_table["bucket_size"] = "8192";
		map_table["channel_id"] = "1";
		map_table["html_size"] = "8192";
		map_table["lane_depth"] = "3";
		map_table["max_lane"] = "8";
		map_table["min_lane"] = "1";
		map_table["pk_ip"] = "140.114.71.174";
		map_table["pk_port"] = "8856";
		map_table["reg_ip"] = "140.114.71.174";
		map_table["reg_port"] = "7756";
		map_table["log_ip"] = "140.114.71.174";
		map_table["log_port"] = "9956";
		map_table["stun_ip"] = "140.114.71.174";
		map_table["stream_local_port"] = "3000";
		//map_table["sub_stream_num"] = "4";
		map_table["svc_tcp_port"] = "5566";
		map_table["svc_udp_port"] = "7788";
		debug_printf("----------------*****------------------ \n");
	}
	
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
	debug_printf("Have deleted configuration \n");
}

std::string configuration::readConfigFile(const char *path) {
	FILE *file = fopen(path, "rb");
	if (!file)
		return std::string("");
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	std::string text;
	char *buffer = new char[size + 1];
	buffer[size] = 0;
	if (fread(buffer, 1, size, file) == (unsigned long)size)
		text = buffer;
	fclose(file);
	delete[] buffer;
	return text;
}

void configuration::add_key(const char *key,std::string val)
{
    map_table[key] = val; 
}

void configuration::add_key(const char *key, unsigned short val)
{
	string s;
	stringstream ss(s);
	ss << (int)val;
	map_table[key] = ss.str();
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



