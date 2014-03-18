#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "keyboard.h"

void keyboardPress(int vb)
{
	Sleep(500);
	keybd_event(vb, 0, 0, 0); //«ö¤U
	Sleep(10);
	keybd_event(vb, 0, KEYEVENTF_KEYUP, 0); //©ñ¶}
}

void printKey(int n)
{
	switch (n) {
	case MY_UP:		printf("UP \n");	break;
	case MY_DOWN:	printf("DOWN \n");	break;
	case MY_LEFT:	printf("LEFT \n");	break;
	case MY_RIGHT:	printf("RIGHT \n"); break;
	case MY_ENTER:	printf("ENTER \n"); break;
	case MY_A:		printf("A \n");		break;
	case MY_B:		printf("B \n");		break;
	}
}

/*
int main()
{
	int action[7] = {MY_UP, MY_DOWN, MY_LEFT, MY_RIGHT, MY_A, MY_B, MY_ENTER};
	//system("start calc.exe");
	
	srand(time(NULL));

	Sleep(3000);
	  
	for ( ; ; ) {
		int val = rand() % 7;
		printKey(action[val]);
		keyboardPress(action[val]);
	}

}
*/