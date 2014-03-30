#include <k.h>
#include <arm.h>
#include <mmu.h>

void uart_init();
void mmu_init();
void mmu_enable(uintptr returnAddr);
void goDoLuaStuff();
void interactiveLuaPrompt();
void putbyte(byte);
