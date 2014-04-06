#include <k.h>
#include <exec.h>

void putbyte(byte b);
byte getch();

int handleSvc(int cmd, uintptr arg1, uintptr arg2) {
	Process* p = TheSuperPage->currentProcess;
	switch (cmd) {
		case KExecSbrk:
			if (arg1) {
				uintptr oldLim = p->heapLimit;
				bool ok = process_grow_heap(p, (int)arg1);
				if (ok) return oldLim;
				else return -1;
			} else {
				return p->heapLimit;
			}
		case KExecPrintString:
			// TODO sanitise parameters!
			printk("%s", (const char*)arg1);
			break;
		case KExecPutch:
			putbyte(arg1);
			break;
		case KExecGetch:
			return getch();
		default:
			break;
	}
	return 0;
}
