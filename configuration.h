// Copyright (c) 2010, NetXtream Inc.
// All rights reserved.
// 
// Author: Yeh-Sheng-Lin

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include "common.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>

using namespace std;

class configuration {

public:

	configuration();								// read-only preference
	configuration(string file);						// read-write preference  
	virtual ~configuration();

    void configuration::add_key(const char *key,std::string val);

	void read_key(const char *key, bool & retval) const;		// find the value of parameter in config.ini  

	void read_key(const char *key, short & retval) const;		// find the value of parameter in config.ini   	
	
	void read_key(const char *key, unsigned short & retval) const;		// find the value of parameter in config.ini  
	
	void read_key(const char *key, int & retval) const;		// find the value of parameter in config.ini   
	
	void read_key(const char *key, unsigned int & retval) const;		// find the value of parameter in config.ini   
	
	void read_key(const char *key, long & retval) const;		// find the value of parameter in config.ini  
	
	void read_key(const char *key, unsigned long& retval) const;		// find the value of parameter in config.ini   

	void read_key(const char *key, float & retval) const;		// find the value of parameter in config.ini   

	void read_key(const char *key, double & retval) const;		// find the value of parameter in config.ini   
		
	void read_key(const char *key, long double& retval) const;		// find the value of parameter in config.ini   
	
	void read_key(const char *key, void* & retval) const;		// find the value of parameter in config.ini   
	
	void read_key(const char *key, std::string & retval) const;		// find the value of parameter in config.ini  

	void read_key2(const char *key, int *retval) const;		// find the value of parameter in config.ini  
	

	template <typename T>	// we only expect the T is a C++ primitive type or object, not pointer!!!
	void set_key(const char *key, T value) {	
		stringstream ss;
		ss << value;
		ss >> map_table[key];
	}

	virtual void dump_map() const;

	map<string, string> map_table;

private:
	
	bool tmp_only;
	string filename;
	

	configuration(const configuration&);                 
	configuration operator=(const configuration&);    

};

#endif
