#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <k.h>

void goDoLuaStuff();
void interactiveLuaPrompt();

void* kernelMemory;

void putbyte(byte b) {
	fprintf(stderr, "%c", b);
}

byte getch() {
	byte result;
	fread(&result, sizeof(byte), 1, stdin);
	if (result == 0x7f) {
		// OS X appears to send delete not backspace. Can't be bothered to look up how to
		// change this in termios
		result = 8;
	} else if (result == 4) {
		// Ctrl-d
		exit(0);
	}
	return result;
}

void hang() {
	abort();
}

void runUserTests();

int main(int argc, char* argv[]) {
	kernelMemory = malloc(KPhysicalRamSize);

	// Can't believe I have to fiddle with termios just to get reading from stdin to work unbuffered...
	struct termios t;
	tcgetattr(fileno(stdin), &t);
	t.c_lflag &= ~(ECHO|ECHONL|ECHOPRT|ECHOCTL|ICANON);
	t.c_iflag &= ~(ICRNL);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(fileno(stdin), TCSANOW, &t);

	printk("\n\n" LUPI_VERSION_STRING "\n");

	runUserTests();
	//printk("Ok.\n");

	//goDoLuaStuff();
	interactiveLuaPrompt();

	return 0;
}

