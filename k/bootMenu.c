#include <k.h>
#include <mmu.h>
#include <pageAllocator.h>
#include <klua.h>

byte getch();
void test_atomics();

static int displayBootMenu() {
	printk("\
Boot menu:\n\
 Enter, 0: Start interpreter\n\
        1: Start klua debugger\n\
        a: Run atomics unit tests\n\
");
	for (;;) {
		int ch = getch();
		switch (ch) {
			case '\r': return 0;
			case '0' ... '9': return ch - '0';
			case 'a':
				return ch;
		}
	}
}

int checkBootMode(int bootMode) {
	if (bootMode == BootModeMenu) {
		bootMode = displayBootMenu();
	}
	if (bootMode == BootModeKlua) {
#ifdef KLUA_DEBUGGER
		mmu_mapSectionContiguous(Al, KLuaDebuggerSection, KPageKluaHeap);
		mmu_finishedUpdatingPageTables();
		TheSuperPage->marvin = true; // Required for debugger functionality
		switchToKluaDebuggerMode(KLuaDebuggerStackBase + 0x1000);
		klua_runIntepreterModule(KLuaDebuggerHeap);
#else
		printk("Error: KLUA_DEBUGGER not defined\n");
		kabort();
#endif
	} else if (bootMode == 'a') {
		test_atomics();
	}
	return bootMode;
}
