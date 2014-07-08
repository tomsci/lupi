#ifndef KLUA_H
#define KLUA_H

#include <std.h>

void klua_runIntepreterModule(uintptr heapPtr);
void switchToKluaDebuggerMode(uintptr sp);

#endif
