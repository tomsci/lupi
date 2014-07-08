#include <k.h>
#include <arm.h>

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
//		int i = val % 10;
//		*(--result) = '0' + i;
//		val = val / 10;
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

static char* uintToHex(ulong aVal, char* buf, int buflen, bool caps) {
	// caps also means fill buf with zeros. Why? Because I say so.
	ulong val = aVal;
	char* result = buf + buflen - 1;
	*result = 0;
	char a = caps ? 'A' : 'a';
	do {
		int digit = val & 0xF;
		if (digit >= 10) {
			*(--result) = a + digit - 10;
		} else {
			*(--result) = '0' + digit;
		}
		val = val >> 4;
	} while ((val || caps) && result != buf);
	return result;
}

#ifdef __LP64__
#define BUFLEN 21
#define HEXDIGITS 16
#else
#define BUFLEN 12
#define HEXDIGITS 8
#endif

#define VA_GETLONG(_args, _isLong) ((_isLong) ? va_arg(_args, long) : (long)va_arg(_args, int))
#define VA_GETULONG(_args, _isLong) ((_isLong) ? va_arg(_args, ulong) : (ulong)va_arg(_args, uint))

void printk(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char buf[BUFLEN];
	char ch;
	while ((ch = *fmt++)) {
		switch (ch) {
		case '%': {
			char formatChar = *fmt++;
			int isLong = 0;
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
				putstr(uintToHex(VA_GETULONG(args, isLong), buf, sizeof(buf), false));
				break;
			case 'p': {
				putstr("0x");
				putstr(uintToHex((uintptr)va_arg(args, void*), buf, HEXDIGITS+1, true));
				break;
			}
			case 'X':
				putstr(uintToHex(VA_GETULONG(args, isLong), buf, HEXDIGITS+1, true));
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
	va_end(args);
}

void hexdump(const char* addr, int len) {
	int nlines = len / 16;
	char buf[9];
	for (int i = 0; i < nlines; i++) {
		const char* lineStart = addr + (i * 16);
		printk("%p: ", lineStart);
		for (int j = 0; j < 16; j++) {
			char ch = lineStart[j];
			putstr(uintToHex(ch, buf, 3, true));
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
			putstr(uintToHex(((const uint32*)lineStart)[j], buf, sizeof(buf), true));
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

#ifdef ARM

static uintptr stackBaseForMode(uint32 mode) {
	switch (mode) {
		case KPsrModeUsr: return userStackForThread(TheSuperPage->currentThread);
		case KPsrModeSvc:
			// TODO will probably have to revisit this once again...
			if (TheSuperPage->currentThread) {
				return svcStackBase(TheSuperPage->currentThread->index);
			} else {
				return KKernelStackBase;
			}
		case KPsrModeAbort: return KAbortStackBase;
		case KPsrModeUnd: return KAbortStackBase;
		case KPsrModeIrq: return KIrqStackBase;
		case KPsrModeSystem:
			// We only ever use this mode when kernel debugging where we're using the top of the
			// debugger heap section
			return KLuaDebuggerStackBase;
		default:
			return 0;
	}
}

void dumpRegisters(uint32* regs, uint32 pc, uint32 dataAbortFar) {
	uint32 spsr, r13, r14;
	asm("MRS %0, spsr" : "=r" (spsr));
	const uint32 crashMode = spsr & KPsrModeMask;
	if (crashMode == KPsrModeUsr) {
		uint32 bnked[2] = {0x13131313,0x14141414};
		uint32* bankedStart = bnked;
		// The compiler will 'optimise' out the STM into a single "str %0, [sp]" unless
		// I include the volatile. The fact there's the small matter of the '^' which it is
		// IGNORING when making that decision... aaargh!
		ASM_JFDI("STM %0, {r13, r14}^" : : "r" (bankedStart));
		r13 = bnked[0];
		r14 = bnked[1];
	} else {
		// Mode is something priviledged - switch back to it briefly to get r13 and 14
		uint32 currentMode;
		asm("MRS %0, cpsr" : "=r" (currentMode));
		int zero = 0;
		uint32 tempMode = crashMode | KPsrIrqDisable | KPsrFiqDisable; // Keep interrupts off
		asm("MSR cpsr_c, %0" : : "r" (tempMode)); // ModeSwitch(tempMode)
		DSB_inline(zero);
		asm("MOV %0, r13" : "=r" (r13));
		asm("MOV %0, r14" : "=r" (r14));
		asm("MSR cpsr_c, %0" : : "r" (currentMode)); // ModeSwitch(currentMode)
	}

	printk("r0:  %X r1:  %X r2:  %X r3:  %X\n", regs[0],  regs[1],  regs[2],  regs[3]);
	printk("r4:  %X r5:  %X r6:  %X r7:  %X\n", regs[4],  regs[5],  regs[6],  regs[7]);
	printk("r8:  %X r9:  %X r10: %X r11: %X\n", regs[8],  regs[9],  regs[10], regs[11]);
	printk("r12: %X r13: %X r14: %X r15: %X\n", regs[12], r13,      r14,      pc);
	printk("CPSR was %X\n", spsr);
	uintptr stackBase = stackBaseForMode(crashMode);
	if (r13 < stackBase && dataAbortFar && dataAbortFar < stackBase) {
		printk("BLOWN STACK!\n");
	}
	if (!TheSuperPage->marvin) {
		// First time we hit this, populate crashRegisters
		uint32* cr = TheSuperPage->crashRegisters;
		memcpy(cr, regs, 12*sizeof(uint32));
		cr[13] = r13;
		cr[14] = r14;
		cr[15] = pc;
		cr[16] = spsr;
	}
}

#endif
