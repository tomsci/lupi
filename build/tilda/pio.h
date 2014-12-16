#ifndef LUPI_TILDA_PIO_H
#define LUPI_TILDA_PIO_H

#include <k.h>

#define SPI0			0x40008000
#define SPI1			0x4000C000

#define SPI_CSR0		0x30 // Chip select n config
#define SPI_CSR1		0x34
#define SPI_CSR2		0x38
#define SPI_CSR3		0x3C

// SPI_CSRn bits
#define SPI_MODE0		0x2 // CPOL=0 NCPHA=1 (ie CPHA=0)
#define SPI_MODE3		0x1 // CPOL=1 NCPHA=0 (ie CPHA=1)
#define SPI_CSR_CSAAT	(1 << 3)

void gpio_set(uint32 pin, bool value);
void spi_init();

void spi_beginTransaction(uint32 chipSelectRegAddr);
// void spi_endTransaction();

#define KSpiFlagWriteback 1
#define KSpiFlagLastXfer 2

void spi_readwrite_poll(uint8* buf, int length, uint32 flags);
#define spi_write_poll(buf, length) spi_readwrite_poll((uint8*)(buf), (length), 0)
#define spi_write_last(buf, length) spi_readwrite_poll((uint8*)(buf), (length), KSpiFlagLastXfer)

#endif
