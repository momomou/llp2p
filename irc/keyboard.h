#define	MY_UP		0x68
#define MY_DOWN		0x62
#define MY_LEFT		0x64
#define MY_RIGHT	0x66
#define MY_A		0x5a
#define MY_B		0x58
#define MY_ENTER	0x0d
#define MY_DEL		0x08

enum Action {
	UP = 0x68,
	DOWN = 0x62,
	LEFT = 0x64,
	RIGHT = 0x66,
	A = 0x5a,
	B = 0x58,
	ENTER = 0x0d
};


void keyboardPress(int vb);
void printKey(int n);