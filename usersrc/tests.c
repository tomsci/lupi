#include <stddef.h>
#include <string.h>
#include <stdlib.h>

void printk(const char* fmt, ...) ATTRIBUTE_PRINTF(1, 2);
void hexdump(const char* addr, int len);
void worddump(const char* addr, int len);

#define assert(x) do { if (!(x)) { printk("Assert failed at line %d: %s\n", __LINE__, #x); abort(); } } while(0)

void runUserTests() {
	char s[] = "This is a %s test!";
	char t[] = "This is nota test!";
	assert(strlen(s) == 18);

	const char* found = strchr(s, '%');
	assert(found && *found == '%');

	assert(strcmp(s, s) == 0);
	assert(strcmp(s, t) != 0);

	found = strpbrk(s, "zy%x");
	assert(*found == '%');

	assert(strtol("1234", NULL, 0) == 1234);
	assert(strtol("-5", NULL, 10) == -5);
	assert(strtol("0xABCd", NULL, 0) == 0xABCD);

	char buf[32];
	sprintf(buf, s, "awesome");
	assert(strcmp(buf, "This is a awesome test!") == 0);
}
