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

/**
## Supported boot modes

0: Start the Lua interpreter communicating over the serial port.

1: Start the kluadebugger on the serial port. This is low-level debugging mode
generally only used when porting to a new board.

2: Display a textual boot menu on the serial port.

3,4: An unfinished password manager that is not in any way useful.

5: Tetris!

6: Show graphical boot menu.
*/

static int displayBootMenu() {
	printk("\
Boot menu:\n\
 Enter, 0: Start interpreter\n\
        1: Start klua debugger\n\
        3: Password manager (console UI)\n\
        4: Password manager (GUI)\n\
        5: Tetris\n\
        6: Graphical Boot Menu\n\
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
