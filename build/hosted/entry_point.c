#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <k.h>

void goDoLuaStuff();
void interactiveLuaPrompt();
void runLuaIntepreterModule(uintptr heapBase);
void runUserTests();

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
	exit(0);
}

typedef struct lua_State lua_State;

/*
#define CHECK_RET(op) { int ret = op; if (ret) { ret = errno; fprintf(stderr, "Error %d in %s\n", ret, #op); return ret; } }
typedef int (*lua_Writer) (lua_State *L, const void* p, size_t sz, void* ud);
typedef const char * (*lua_Reader) (lua_State *L, void *data, size_t *size);

#define BUFLEN 16*1024
void* gBuf;

static const char* readerFn(lua_State* L, void* data, size_t* size) {
	FILE* f = (FILE*)data;
	int len = fread(gBuf, 1, BUFLEN, f);
	if (ferror(f)) return NULL;
	*size = len;
	return gBuf;
}

int dumper(lua_State *L, const void* p, size_t sz, void* ud) {
	int ret = fwrite(p, 1, sz, (FILE*)ud);
	if (ret != sz) return 1;
	else return 0;
}

int klua_dump_reader(const char* name, lua_Reader reader, void* readData, lua_Writer writer, void* writeData);

int readModuleAndDump(const char* module, const char* out) {
	char buf[64];
	strcpy(buf, "modules/");
	strcat(buf, module);
	strcat(buf, ".lua");
	FILE* f = fopen(buf, "r");
	if (!f) {
		fprintf(stderr, "Couldn't open %s\n", buf);
		return 1;
	}
	gBuf = malloc(BUFLEN);

	FILE* outf = fopen(out, "w");
	if (!outf) {
		fprintf(stderr, "Couldn't open %s\n", out);
		return 1;
	}
	CHECK_RET(klua_dump_reader(module, readerFn, f, dumper, outf));
	fclose(outf);
	free(gBuf);

	return 0;
}
*/

int main(int argc, char* argv[]) {
	kernelMemory = malloc(KPhysicalRamSize);

	/*
	if (argc == 3) {
		// Have to do this in C as our Lua env has no filesystem access, sigh
		return readModuleAndDump(argv[1], argv[2]);
	}
	*/

	// Can't believe I have to fiddle with termios just to get reading from stdin to work unbuffered...
	struct termios t;
	tcgetattr(fileno(stdin), &t);
	t.c_lflag &= ~(ECHO|ECHONL|ECHOPRT|ECHOCTL|ICANON);
	t.c_iflag &= ~(ICRNL);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(fileno(stdin), TCSANOW, &t);

	printk(LUPI_VERSION_STRING);
	const char* lupi = " \xF0\x9F\x8C\x99\xF0\x9D\x9B\x91\n";
	while (*lupi) {
		putbyte(*lupi++);
	}

	runUserTests();
	//printk("Ok.\n");

	//goDoLuaStuff();
	//interactiveLuaPrompt();
	runLuaIntepreterModule(KLuaHeapBase);

	return 0;
}

