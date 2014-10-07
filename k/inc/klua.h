#ifndef KLUA_H
#define KLUA_H

#include <std.h>

void klua_runInterpreter();
void klua_runInterpreterModule();
void switchToKluaDebuggerMode(uintptr sp);

#endif
