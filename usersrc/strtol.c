#include <stdlib.h>
#include <ctype.h>

long strtol(const char * restrict str, char** restrict endptr, int base) {
	long result = 0;
	int neg = 0;
	const char* s;
	for (s = str; *s != 0; s++) {
		char ch = *s;
		if (isspace(ch)) continue;
		if (ch == '-') {
			neg = 1;
			continue;
		} else if (ch == '+') {
			continue;
		}
		if ((base == 0 || base == 16) && *str == '0' && s == str+1 && (ch == 'x' || ch == 'X')) {
			base = 16;
			continue;
		}
		if (!isalnum(ch)) break;
		int val = (isdigit(ch) ? (ch - '0') : isupper(ch) ? (ch + 10 - 'A') : (ch + 10 - 'a'));
		if (base == 0 && val > 0 && val < 10) base = 10;
		if (val < 0 || (val && val >= base)) break;
		result = (result * base) + val;
	}
	if (endptr) {
		*endptr = (char*)s;
	}
	return neg ? -result : result;
}
