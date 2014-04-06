#include <k.h>
#include <exec.h>

int handleSvc(int cmd, uintptr arg1, uintptr arg2) {
	Process* p = TheSuperPage->currentProcess;
	switch (cmd) {
		case KExecSbrk:
			if (arg1) return process_grow_heap(p, (int)arg1);
			else return p->heapLimit;
		case KExecPrintString:
			// TODO sanitise parameters!
			printk("%s", (const char*)arg1);
			break;
		default:
			break;
	}
	return 0;
}
