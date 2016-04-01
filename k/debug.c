#include <k.h>

extern void putbyte(byte b);

static void putch(char c) {
	if (c == '\n') {
		// Need a CR first
		putch('\r');
	} else if ((c != '\r' && c != '\t' && c < 0x20) || c >= 127) {
		c = '.';
	}
	putbyte(c);
}

static void putstr(const char* str) {
	char ch;
	while ((ch = *str++)) {
		putch(ch);
	}
}

/**
buflen must be >= 2
*/
static char* uintToStr(ulong val, char* buf, int buflen) {
	char* result = buf + buflen - 1;
	*result = 0;
	do {
		ulong newVal = val / 10;
		int i = val - (newVal * 10);
		*(--result) = '0' + i;
		val = newVal;
	} while (val && result != buf);
	return result;
}

static char* intToStr(long val, char* buf, int buflen) {
	bool neg = (val < 0);
	if (neg) {
		val = -val;
	}
	char* result = uintToStr((ulong)val, buf, buflen);
	if (neg && result != buf) {
		*(--result) = '-';
	}
	return result;
}

static char* uintToHex(ulong val, char* buf, int buflen, int zeroFillLen) {
	// Zero fill also means use capitals. Why? Because I say so.
	char* result = buf + buflen - 1;
	*result = 0;
	const char aminus10 = ((zeroFillLen > 0) ? 'A' : 'a') - 10;
	do {
		int digit = val & 0xF;
		if (digit >= 10) {
			*(--result) = aminus10 + digit;
		} else {
			*(--result) = '0' + digit;
		}
		val = val >> 4;
		--zeroFillLen;
	} while (result != buf && (val || (zeroFillLen > 0)));
	return result;
}

#define BUFLEN 21 // Big enough for a 64-bit int in decimal
#define VA_GETLONG(_args, _isLong) ((_isLong) ? va_arg(_args, long) : (long)va_arg(_args, int))
#define VA_GETULONG(_args, _isLong) ((_isLong) ? va_arg(_args, ulong) : (ulong)va_arg(_args, uint))

void uk_print(const char* fmt, va_list args, void (*putch)(char), void (*putstr)(const char*)) {
	char buf[BUFLEN];
	char ch;
	while ((ch = *fmt++)) {
		switch (ch) {
		case '%': {
			char formatChar = *fmt++;
			int isLong = 0;
			while (formatChar >= '0' && formatChar <= '9') {
				// We ignore format specifiers
				formatChar = *fmt++;
			}
			while (formatChar == 'l') {
				isLong++;
				formatChar = *fmt++;
			}
			switch (formatChar) {
			case 'c':
				putch(va_arg(args, int));
				break;
			case 's':
				putstr(va_arg(args, char*));
				break;
			case 'i':
			case 'd': {
				putstr(intToStr(VA_GETLONG(args, isLong), buf, sizeof(buf)));
				break;
			}
			case 'u': {
				ulong val = VA_GETULONG(args, isLong);
				putstr(uintToStr(val, buf, sizeof(buf)));
				break;
			}
			case 'x':
				putstr(uintToHex(VA_GETULONG(args, isLong), buf, sizeof(buf), 0));
				break;
			case 'p': {
				putstr("0x");
				putstr(uintToHex((uintptr)va_arg(args, void*), buf, sizeof(buf), sizeof(uintptr)));
				break;
			}
			case 'X':
				putstr(uintToHex(VA_GETULONG(args, isLong), buf, sizeof(buf), isLong ? sizeof(long) : sizeof(int)));
				break;
			case '%':
				// Drop through
			default:
				putch(ch);
				putch(formatChar);
			}

			break;
		}
		default:
			putch((byte)ch);
			break;
		}
	}
}

void hexdump(const char* addr, int len) {
	int nlines = len / 16;
	char buf[9];
	for (int i = 0; i < nlines; i++) {
		const char* lineStart = addr + (i * 16);
		printk("%p: ", lineStart);
		for (int j = 0; j < 16; j++) {
			char ch = lineStart[j];
			putstr(uintToHex(ch, buf, 3, 2));
			putch(' ');
		}
		putch(' ');
		for (int j = 0; j < 16; j++) {
			char ch = lineStart[j];
			if (ch < ' ' || ch >= 127) ch = '.';
			putch(ch);
		}
		putch('\n');
	}
}

void worddump(const void* aAddr, int len) {
	const char* addr = (const char*)aAddr;
	const int nwords = len / 4;
	int nlines = (nwords + 3) / 4;
	char buf[9];
	for (int i = 0; i < nlines; i++) {
		const char* lineStart = addr + (i * 16);
		printk("%p: ", lineStart);
		const int n = (i+1 == nlines ? (nwords - i*4) : 4);
		for (int j = 0; j < n; j++) {
			putstr(uintToHex(((const uint32*)lineStart)[j], buf, sizeof(buf), sizeof(uint32)));
			putch(' ');
		}
		putch(' ');
		for (int j = 0; j < n*4; j++) {
			char ch = lineStart[j];
			if (ch < ' ' || ch >= 127) ch = '.';
			putch(ch);
		}
		putch('\n');
	}
}

void early_printk(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	uk_print(fmt, args, putch, putstr);
	va_end(args);
}

void printk(const char* fmt, ...) {
	if (TheSuperPage->quiet) return;
	va_list args;
	va_start(args, fmt);
	uk_print(fmt, args, putch, putstr);
	va_end(args);
}
