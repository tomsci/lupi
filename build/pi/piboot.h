#include <k.h>
#include <arm.h>
#include <mmu.h> // DEBUG

void uart_init();
void mmu_init();
void mmu_enable(uintptr returnAddr);
void goDoLuaStuff();
void interactiveLuaPrompt();
void putbyte(byte);

#ifdef MMU_DISABLED

#define eBL(reg, label) asm("BL " label)
#define eB(reg, label) asm("B " label)

#else

// The linker has set the code address to 0xF8008000 because that's where we'll map it
// once we've setup the MMU. However before that happens, and branch instructions will be
// off by 0xF8000000 because the PI bootloader loads the kernel image at 0x8000.
#define eBL(reg, label) \
asm("LDR " #reg ", =" label); \
asm("SUB " #reg ", " #reg ", #0xF8000000"); \
asm("BLX " #reg)

#define eB(reg, label) \
asm("LDR " #reg ", =" label); \
asm("SUB " #reg ", " #reg ", #0xF8000000"); \
asm("BX " #reg)

#endif

