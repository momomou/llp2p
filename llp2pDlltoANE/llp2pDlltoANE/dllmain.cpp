#include <string>
#include <assert.h>
#include <sstream>
#include <iostream>
#include "flash_sdk/FlashRuntimeExtensions.h"
#include "../../llp2pmain.h"
#include "json_lib/json.h"

#pragma comment(lib,"FlashRuntimeExtensions.lib")
#pragma comment(lib, "../../P2P_Client_vs2013/Release/llp2pDll.lib")
//#pragma comment(lib, "../../P2P_Client_vs2013/Debug/llp2pDll.lib")

using namespace std;


FREObject ASPassAString(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
{
	// What this function does: reads a string passed in from ActionScript,
	// outputs it to the console, then sends it back to ActionScript.

	// This enumerator helps keep track of the parameters
	// we expect from ActionScript and their order. 
	// Not technically necessary, but a good habit:
	// saves you havhavnig to remember which parameter you should access as argv[ 3 ].
	enum
	{
		ARG_STRING_ARGUMENT = 0,

		ARG_COUNT
	};

	// Another good habit, though not a requirement:
	// ARG_COUNT will have the value of the number of arguments you expect.
	// The assertion will fire (in a debug build) to tell you
	// if you mistakenly passed the wrong number of arguments
	// from ActionScritpt.
	assert(ARG_COUNT == argc);

	// Read the ActionScript String object, packed here as a FREObject,
	// into a character array:
	uint32_t strLength = 0;
	const uint8_t * nativeCharArray = NULL;
	FREResult status = FREGetObjectAsUTF8(argv[ARG_STRING_ARGUMENT], &strLength, &nativeCharArray);

	FREObject retObj = NULL;

	if ((FRE_OK == status) && (0 < strLength) && (NULL != nativeCharArray))
	{
		// Read the characters into a c string... 
		std::string nativeString((const char *)nativeCharArray);

		// ...and output it into the console to see what we received:
		std::stringstream  stros;
		stros << "This is the string we received from ActionScript: ";
		stros << nativeString;

		// Now let's put the characters back into a FREObject...
		FRENewObjectFromUTF8(strLength - 2, nativeCharArray, &retObj);
	}

	// ... and send them back to ActionScript:
	return retObj;
}

FREObject ASPassAString2(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
{
	
	// What this function does: reads a string passed in from ActionScript,
	// outputs it to the console, then sends it back to ActionScript.

	// This enumerator helps keep track of the parameters
	// we expect from ActionScript and their order. 
	// Not technically necessary, but a good habit:
	// saves you havhavnig to remember which parameter you should access as argv[ 3 ].
	enum
	{
		ARG_STRING_ARGUMENT = 0,

		ARG_COUNT
	};

	// Another good habit, though not a requirement:
	// ARG_COUNT will have the value of the number of arguments you expect.
	// The assertion will fire (in a debug build) to tell you
	// if you mistakenly passed the wrong number of arguments
	// from ActionScritpt.
	assert(ARG_COUNT == argc);

	// Read the ActionScript String object, packed here as a FREObject,
	// into a character array:
	uint32_t strLength = 0;
	const uint8_t * nativeCharArray = NULL;
	FREResult status = FREGetObjectAsUTF8(argv[ARG_STRING_ARGUMENT], &strLength, &nativeCharArray);

	FREObject retObj = NULL;

	if ((FRE_OK == status) && (0 < strLength) && (NULL != nativeCharArray))
	{
		std::string input((const char *)nativeCharArray);
		Json::Reader reader;
		Json::Value value;

		Json::Value root;
		root["BUCKET_SIZE"] = 8192;
		root["CHANNEL_ID"] = 1;

		cout << root.toStyledString() << endl;

		
		if (reader.parse(input, value)) {
			cout << value["BUCKET_SIZE"].asString() << endl;
			cout << value["CHANNEL_ID"].asString() << endl;
		}
		
		cout << value.toStyledString() << endl;


		// Read the characters into a c string... 
		std::string nativeString((const char *)nativeCharArray);

		// ...and output it into the console to see what we received:
		std::stringstream  stros;
		stros << "This is the string we received from ActionScript: ";
		stros << nativeString;

		// Now let's put the characters back into a FREObject...
		FRENewObjectFromUTF8(strLength - 2, (const uint8_t *)value["123"].asString().c_str(), &retObj);
	}

	// ... and send them back to ActionScript:
	return retObj;
}

FREObject ASPassAString3(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
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

	// ... and send them back to ActionScript:
	FREObject retObj = NULL;
	return retObj;
}

FREObject ASrunP2PClient(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
{
	// What this function does: reads a string passed in from ActionScript,
	// outputs it to the console, then sends it back to ActionScript.

	// This enumerator helps keep track of the parameters
	// we expect from ActionScript and their order. 
	// Not technically necessary, but a good habit:
	// saves you havhavnig to remember which parameter you should access as argv[ 3 ].
	enum
	{
		ARG_STRING_ARGUMENT = 0,

		ARG_COUNT
	};

	// Another good habit, though not a requirement:
	// ARG_COUNT will have the value of the number of arguments you expect.
	// The assertion will fire (in a debug build) to tell you
	// if you mistakenly passed the wrong number of arguments
	// from ActionScritpt.
	assert(ARG_COUNT == argc);

	// Read the ActionScript String object, packed here as a FREObject,
	// into a character array:
	uint32_t strLength = 0;
	const uint8_t * nativeCharArray = NULL;
	FREResult status = FREGetObjectAsUTF8(argv[ARG_STRING_ARGUMENT], &strLength, &nativeCharArray);

	FREObject retObj = NULL;

	if ((FRE_OK == status) && (0 < strLength) && (NULL != nativeCharArray))
	{
		// Read the characters into a c string... 
		std::string nativeString((const char *)nativeCharArray);

		// ...and output it into the console to see what we received:
		std::stringstream  stros;
		stros << "This is the string we received from ActionScript: ";
		stros << nativeString;

		// Now let's put the characters back into a FREObject...
		FRENewObjectFromUTF8(strLength, nativeCharArray, &retObj);
	}

	// ... and send them back to ActionScript:

	//return retObj;

	std::string input((const char *)nativeCharArray);








	Json::Value root;
	root["BUCKET_SIZE"] = 8192;
	root["CHANNEL_ID"] = 1;
	root["HTML_SIZE"] = 1024;
	root["LANE_DEPTH"] = 3;
	root["MAX_LANE"] = 8;
	root["MIN_LANE"] = 1;
	root["PK_SERVER"]["IP"] = "140.114.71.166";
	root["PK_SERVER"]["PORT"] = 8855;
	root["REG_SERVER"]["IP"] = "140.114.71.174";
	root["REG_SERVER"]["PORT"] = 7756;
	root["LOG_SERVER"]["IP"] = "140.114.71.166";
	root["LOG_SERVER"]["PORT"] = 9955;
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

	//if (LLP2P::setConfig(thread_key, root.toStyledString()) < 0) {
	if (LLP2P::setConfig(thread_key, input) < 0) {
		cout << "Set config Error" << endl;
	}

	string aaaa = LLP2P::getConfig(thread_key);

	cout << LLP2P::getConfig(thread_key) << endl;

	cout << LLP2P::start(thread_key) << endl;

	//system("pause");

	//return 0;
	
	return retObj;
	
}

void contextInitializer(
	void                     * extData,
	const uint8_t             * ctxType,
	FREContext                   ctx,
	uint32_t                 * numFunctionsToSet,
	const FRENamedFunction    ** functionsToSet)
{
	// Create mapping between function names and pointers in an array of FRENamedFunction.
	// These are the functions that you will call from ActionScript -
	// effectively the interface of your native library.
	// Each member of the array contains the following information:
	// { function name as it will be called from ActionScript,
	//   any data that should be passed to the function,
	//   a pointer to the implementation of the function in the native library }
	static FRENamedFunction extensionFunctions[] =
	{
		{ (const uint8_t*) "as_passAString", NULL, &ASPassAString },
		{ (const uint8_t*) "as_passAString2", NULL, &ASPassAString2 },
		{ (const uint8_t*) "as_runp2pclient", NULL, &ASrunP2PClient }
	};

	// Tell AIR how many functions there are in the array:
	*numFunctionsToSet = sizeof(extensionFunctions) / sizeof(FRENamedFunction);

	// Set the output parameter to point to the array we filled in:
	*functionsToSet = extensionFunctions;
}

void contextFinalizer(FREContext ctx)
{
	return;
}

extern "C"
{
	__declspec(dllexport) void ExtensionInitializer(void** extData, FREContextInitializer* ctxInitializer, FREContextFinalizer* ctxFinalizer)
	{
		*ctxInitializer = &contextInitializer; // The name of function that will intialize the extension context
		*ctxFinalizer = &contextFinalizer; // The name of function that will finalize the extension context
	}

	__declspec(dllexport) void ExtensionFinalizer(void* extData)
	{
		return;
	}
}

