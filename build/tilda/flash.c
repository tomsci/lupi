#include "pio.h"
#include <exec.h>

// This is for the S25FL216K 16Mbit Spansion NOR flash chip, connected over SPI

#define FLASH_CHIPSELECT (SPI0 + SPI_CSR0)

#define SPI0_NPCS0		(1 << 28) // PA28 peripheral A, chip pin 111

#define FLASH_CS_PIN	SPI0_NPCS0
#define FLASH_HOLD		(1 << 14) // PC14 chip pin 96

#define READ_DATA		0x03
#define READ_STATUS		0x05
#define READ_ID			0x9F

#define WRITE_STATUS	0x01
#define WRITE_ENABLE	0x06
#define PAGE_PROGRAM	0x02

#define SECTOR_ERASE	0x20
#define BLOCK_ERASE		0xD8
#define CHIP_ERASE		0xC7

#define POWER_DOWN		0xB9
#define POWER_UP		0xAB

#define STATUS_WIP		(1 << 0)

static DRIVER_FN(flash_handleSvc);

void flash_init() {

	PUT32(PIOC + PIO_ENABLE, FLASH_HOLD); // PIO enabled
	PUT32(PIOC + PIO_OER, FLASH_HOLD); // Output enabled
	PUT32(PIOC + PIO_SODR, FLASH_HOLD); // Set to 1

	// Configure the chip select pin
	PUT32(PIOA + PIO_IDR, FLASH_CS_PIN);
	PUT32(PIOA + PIO_PUDR, FLASH_CS_PIN);
	PUT32(PIOA + PIO_ODR, FLASH_CS_PIN); // Output disable
	PUT32(PIOA + PERIPHERAL_ENABLE, FLASH_CS_PIN);

	uint32 csr =
		(32 << 24) |	// DLYBCT = 32*32 cycles delay between writes
		(2 << 8) |		// SCBR = 2 with our 84MHz clock means 42MHz SPI clock
		SPI_CSR_CSAAT |
		SPI_MODE0;		// We are a nice boring mode0 device (CPOL=0 CPHA=0)
	PUT32(FLASH_CHIPSELECT, csr);

	spi_beginTransaction(FLASH_CHIPSELECT);
	uint8 id[] = { READ_ID, 0, 0, 0, 0 };
	spi_readwrite_poll(id, sizeof(id), KSpiFlagWriteback | KSpiFlagLastXfer);
	// printk("Flash mfr=%d id=0x%x\n", id[1], ((uint32)id[2] << 8) | (id[3]));

	uint8 status[] = { READ_STATUS, 0 };
	spi_readwrite_poll(status, 2, KSpiFlagWriteback | KSpiFlagLastXfer);
	// printk("Status = %x\n", status[1]);

	uint8 id2[] = { READ_ID, 0, 0, 0, 0 };
	spi_readwrite_poll(id2, sizeof(id2), KSpiFlagWriteback | KSpiFlagLastXfer);
	// printk("Flash mfr=%d id2=0x%x\n", id2[1], ((uint32)id2[2] << 8) | (id2[3]));

	kern_registerDriver(FOURCC("FLSH"), flash_handleSvc);
}

static void setWriteEnable() {
	uint8 cmd = WRITE_ENABLE;
	spi_write_last(&cmd, 1);

	// uint8 status[] = { READ_STATUS, 0 };
	// spi_readwrite_poll(status, 2, KSpiFlagWriteback | KSpiFlagLastXfer);
	// printk("Status after write enable = %x\n", status[1]);
}

static void chipErase() {
	spi_beginTransaction(FLASH_CHIPSELECT);
	setWriteEnable();

	uint8 cmd = CHIP_ERASE;
	spi_write_last(&cmd, 1);

	// uint8 status[] = { READ_STATUS, 0 };
	// spi_readwrite_poll(status, 2, KSpiFlagWriteback | KSpiFlagLastXfer);
	// printk("Status after chip erase = %x\n", status[1]);
}

static int getStatus() {
	spi_beginTransaction(FLASH_CHIPSELECT);
	uint8 status[] = { READ_STATUS, 0 };
	spi_readwrite_poll(status, 2, KSpiFlagWriteback | KSpiFlagLastXfer);
	return status[1];
}

#define ADDR2BYTES(address) ((address) & 0xFF0000) >> 16, ((address) & 0xFF00) >> 8, ((address) & 0xFF)

void flash_readData(uint8* buf, uintptr address, int len) {
	// printk("Reading %d from %p to %p\n", len, (void*)address, buf);
	spi_beginTransaction(FLASH_CHIPSELECT);
	// Using a 42MHz clock means we can just squeeze in without needing to use
	// fast read. (Cutoff is 44MHz according to the datasheet)
	uint8 cmd[] = { READ_DATA, ADDR2BYTES(address) };
	spi_readwrite_poll(cmd, sizeof(cmd), 0);
	spi_readwrite_poll(buf, len, KSpiFlagWriteback | KSpiFlagLastXfer);
}

static void writeData(uint8* buf, uintptr address, int len) {
	// printk("Writing %d from %p to %p\n", len, buf, (void*)address);
	ASSERT_USER_PTR8(buf);
	ASSERT_USER_PTR8(buf + len - 1);
	spi_beginTransaction(FLASH_CHIPSELECT);

	setWriteEnable();

	uint8 cmd[] = { PAGE_PROGRAM, ADDR2BYTES(address) };
	spi_readwrite_poll(cmd, sizeof(cmd), 0);
	spi_readwrite_poll(buf, len, KSpiFlagLastXfer);
}

static DRIVER_FN(flash_handleSvc) {
	switch(arg1) {
		case KExecDriverFlashErase:
			chipErase();
			return 0;
		case KExecDriverFlashStatus:
			return getStatus();
		case KExecDriverFlashRead: {
			uintptr* args = (uintptr*)arg2;
			uint8* buf = (uint8*)args[0];
			int len = (int)args[2];
			ASSERT_USER_WPTR8(buf);
			ASSERT_USER_WPTR8(buf + len - 1);
			flash_readData(buf, args[1], len);
			return 0;
		}
		case KExecDriverFlashWrite: {
			uintptr* args = (uintptr*)arg2;
			writeData((uint8*)args[0], args[1], (int)args[2]);
			return 0;
		}
		default:
			ASSERT(false, arg1);
	}
}
