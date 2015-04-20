// MyExecRefsDll.cpp
// compile with: /EHsc /link MathFuncsDll.lib

#include <iostream>
#include <string>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>


#include "../../llp2pmain.h"
#include "../../json_lib/json.h"

#pragma comment(lib, "../Debug/llp2pDll.lib")

using namespace std;

#define TOTALBYTES    8192
#define BYTEINCREMENT 4096
#define SIZE    8192

int main()
{

	

	HKEY keyCurrentUser;
	/*
	int lResult = RegOpenCurrentUser(KEY_READ, &keyCurrentUser);
	if (lResult)
	{
		TCHAR szMsg[100];
		printf("err %d \n", lResult);
		//MessageBox(NULL, szMsg, TEXT("oops..."), MB_TOPMOST | MB_ICONERROR);
	}
	*/

	HKEY key;
	/*
	if (RegOpenKey(HKEY_LOCAL_MACHINE, TEXT("Software\\MyKey\\"), &key) != ERROR_SUCCESS)
	{
		cout << "Unable to open registry key";
	}
	*/
	
	//int result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\WinRAR\\"), 0, KEY_WRITE | KEY_READ, &keyCurrentUser);
	int result = RegOpenKeyEx(HKEY_CLASSES_ROOT, _T("TestApp\\"), 0, KEY_READ, &keyCurrentUser);
	if (result == 5)
	{
		cout << "Please install as administrator \n";
		system("myApp.reg");
	}
	else if (result == 2) {
		// Write into registry
		cout << "Write into registry \n";
		system("myApp.reg");
	}
	
	/*
	if (RegSetValueEx(key, TEXT("value_name"), 0, REG_SZ, (LPBYTE)"value_data", strlen("value_data")*sizeof(char)) != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		cout << "Unable to set registry value value_name";
	}
	else
	{
		cout << "value_name was set" << endl;
	}
	*/
	
	
	/*
	{
		DWORD BufferSize = TOTALBYTES;
		DWORD cbData;
		DWORD dwRet;

		PPERF_DATA_BLOCK PerfData = (PPERF_DATA_BLOCK)malloc(BufferSize);
		cbData = BufferSize;

		printf("\nRetrieving the data...");

		dwRet = RegQueryValueEx(HKEY_PERFORMANCE_DATA,
			TEXT("Global"),
			NULL,
			NULL,
			(LPBYTE)PerfData,
			&cbData);
		while (dwRet == ERROR_MORE_DATA)
		{
			// Get a buffer that is big enough.

			BufferSize += BYTEINCREMENT;
			PerfData = (PPERF_DATA_BLOCK)realloc(PerfData, BufferSize);
			cbData = BufferSize;

			printf(".");
			dwRet = RegQueryValueEx(HKEY_PERFORMANCE_DATA,
				TEXT("Global"),
				NULL,
				NULL,
				(LPBYTE)PerfData,
				&cbData);
		}
		if (dwRet == ERROR_SUCCESS)
			printf("\n\nFinal buffer size is %d\n", BufferSize);
		else printf("\nRegQueryValueEx failed (%d)\n", dwRet);
	}
	*/

	/*
	string jsonstr = "123";

	cout << jsonstr << endl;

	system("pause");

	Json::Value root;
	root["BUCKET_SIZE"] = 8192;
	root["CHANNEL_ID"] = 1;
	root["HTML_SIZE"] = 1024;
	root["LANE_DEPTH"] = 3;
	root["MAX_LANE"] = 8;
	root["MIN_LANE"] = 1;
	root["PK_SERVER"]["IP"] = "140.114.71.174";
	root["PK_SERVER"]["PORT"] = 8856;
	root["REG_SERVER"]["IP"] = "140.114.71.174";
	root["REG_SERVER"]["PORT"] = 7756;
	root["LOG_SERVER"]["IP"] = "140.114.71.174";
	root["LOG_SERVER"]["PORT"] = 9956;
	root["STUN_SERVER"]["IP"] = "140.114.71.174";
	root["STREAM"]["PORT"] = 3000;
	root["P2P_TCP_PORT"] = 5566;
	root["P2P_UDP_PORT"] = 7788;


	int thread_key = 0;

	thread_key = LLP2P::getThreadKey();
	cout << "thread key: " << thread_key << endl;
	
	if (LLP2P::createObj(thread_key) < 0) {
	cout << "Create Object Error" << endl;
	}

	if (LLP2P::setConfig(thread_key, root.toStyledString()) < 0) {
	cout << "Set config Error" << endl;
	}

	cout << LLP2P::getConfig(thread_key) << endl;

	cout << LLP2P::start(thread_key) << endl;
	

	for (int i = 10; i > 0; i--) {
		Sleep(1);
		cout << i << endl;
	}

	

	{
		Json::Value root;
		root["BUCKET_SIZE"] = 8192;
		root["CHANNEL_ID"] = 1;
		root["HTML_SIZE"] = 1024;
		root["LANE_DEPTH"] = 3;
		root["MAX_LANE"] = 8;
		root["MIN_LANE"] = 1;
		root["PK_SERVER"]["IP"] = "140.114.71.174";
		root["PK_SERVER"]["PORT"] = 8856;
		root["REG_SERVER"]["IP"] = "140.114.71.174";
		root["REG_SERVER"]["PORT"] = 7756;
		root["LOG_SERVER"]["IP"] = "140.114.71.174";
		root["LOG_SERVER"]["PORT"] = 9956;
		root["STUN_SERVER"]["IP"] = "140.114.71.174";
		root["STREAM"]["PORT"] = 3000;
		root["P2P_TCP_PORT"] = 5566;
		root["P2P_UDP_PORT"] = 7788;


		int thread_key = 0;

		thread_key = LLP2P::getThreadKey();
		cout << "thread key: " << thread_key << endl;

		if (LLP2P::createObj(thread_key) < 0) {
			cout << "Create Object Error" << endl;
		}

		if (LLP2P::setConfig(thread_key, root.toStyledString()) < 0) {
			cout << "Set config Error" << endl;
		}

		cout << LLP2P::getConfig(thread_key) << endl;

		cout << LLP2P::start(thread_key) << endl;
	}
	*/
	system("pause");



	return 0;
}
