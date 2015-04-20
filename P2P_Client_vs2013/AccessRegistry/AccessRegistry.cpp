// MyExecRefsDll.cpp
// compile with: /EHsc /link MathFuncsDll.lib

#include <iostream>
#include <string>
#include<iostream>
#include<fstream>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

using namespace std;

#define TOTALBYTES    8192
#define BYTEINCREMENT 4096
#define SIZE    8192

int main(int argc, char* argv[])
{
	cout << "hahahaahahah" << endl;
	for (int i = 0; i < argc; i++) {
		cout << "argv " << i << " : " << argv[i] << endl;
	}
	/*
	char filename[] = "myApp2.reg";
	fstream fp;
	fp.open(filename, ios::out);//開啟檔案
	if (!fp){//如果開啟檔案失敗，fp為0；成功，fp為非0
		cout << "Fail to open file: " << filename << endl;
	}
	fp << "REGEDIT4" << endl;
	fp << endl;
	fp << "[HKEY_CLASSES_ROOT\\TestApp]" << endl;
	fp << "@=\"URL:TestApp Protocol\"" << endl;
	fp << "\"URL Protocol\"=\"\"" << endl;
	fp << endl;
	fp << "[HKEY_CLASSES_ROOT\\TestApp\\DefaultIcon]" << endl;
	fp << "@=\"\\\"C:\\\\Program Files (x86)\\\\llp2pApp\\\\llp2pApp.exe\\\"\"" << endl;
	fp <<  endl;
	fp << "[HKEY_CLASSES_ROOT\\TestApp\\shell]" << endl;
	fp << endl;
	fp << "[HKEY_CLASSES_ROOT\\TestApp\\shell\\open]" << endl;
	fp <<  endl;
	fp << "[HKEY_CLASSES_ROOT\\TestApp\\shell\\open\\command]" << endl;
	fp << "@=\"\\\"C:\\\\Program Files (x86)\\\\llp2pApp\\\\llp2pApp.exe\\\" \\\"%1\\\"\"" << endl;

	fp.close();//關閉檔案
	*/
	HKEY key;
	
	int result = RegOpenKeyEx(HKEY_CLASSES_ROOT, _T("llp2pApp\\"), 0, KEY_READ, &key);
	if (result == 5)
	{
		cout << "Please install as administrator \n";
		system("myApp.reg");
	}
	else if (result == 2) {
		// Write into registry
		cout << "Write into registry \n";
		int nn = system(argv[1]);
		cout << nn << endl;
	}

	system("pause");

	return 0;
}
