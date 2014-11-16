#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>
#include <klua.h>

byte getch();
void test_atomics();
void test_mem();

enum BootMode {
	BootModeUluaInterpreter = 0,
	BootModeKlua = 1,
	BootModeMenu = 2,
	BootModeAtomicTests = 'a',
	BootModeMemTests = 'm',
	BootModeTestInitLua = 't',
};

static int displayBootMenu() {
	printk("\
Boot menu:\n\
 Enter, 0: Start interpreter\n\
        1: Start klua debugger\n\
        3: Password manager (console UI)\n\
        4: Password manager (GUI)\n\
        5: Tetris\n\
Test func:\n\
        a: Run atomics unit tests\n\
        b: Run bitmap tests\n\
        m: Run memory usage tests\n\
    ^X, r: Reboot\n\
        t: Run test/init.lua tests\n\
        y: Run yield scheduling tests\n\
");
	for (;;) {
		int ch = getch();
		switch (ch) {
			case '\r': return 0;
			case '0' ... '9': return ch - '0';

			case 'r': // Drop through
			case 24: // Ctrl-X
				return 'r';

			case 'a':
			case 'b':
			case 'm':
			case 't':
			case 'y':
				return ch;
			default:
				break;
		}
	}
}

int checkBootMode(int bootMode) {
	if (bootMode == BootModeMenu) {
		bootMode = displayBootMenu();
	}
	if (bootMode == BootModeKlua) {
#ifdef KLUA_DEBUGGER
#ifdef HAVE_MMU
		mmu_mapSectionContiguous(Al, KLuaDebuggerSection, KPageKluaHeap);
		mmu_finishedUpdatingPageTables();
#endif
		TheSuperPage->marvin = true; // Required for debugger functionality
		switchToKluaDebuggerMode();
		klua_runInterpreterModule();
#else
		printk("Error: KLUA_DEBUGGER not defined\n");
		kabort();
#endif
	} else if (bootMode == BootModeAtomicTests) {
		test_atomics();
	} else if (bootMode == 'r') {
		reboot();
	}
	return bootMode;
}
