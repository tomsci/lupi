#include "pio.h"

extern void dummy();

#define SPI0_MISO		(1 << 25) // PA25 perif A
#define SPI0_MOSI		(1 << 26) // PA26 perif A
#define SPI0_SPCK		(1 << 27) // PA27 perif A
#define SPI_PINS		(SPI0_MISO | SPI0_MOSI | SPI0_SPCK)

#define SPI_CR			0x00 // Control register
#define SPI_MR			0x04 // Mode register
#define SPI_RDR			0x08 // Receive data register
#define SPI_TDR			0x0C // Transmit data register
#define SPI_SR			0x10 // Status register

// SPI_CS
#define SPI_CR_SPIEN	(1 << 0) // enable
#define SPI_CR_SPIDIS	(1 << 1) // disable
#define SPI_CR_SWRST	(1 << 7) // reset

// SPI_MR
#define SPI_MR_MSTR		(1 << 0) // Master
#define SPI_MR_PS		(1 << 1) // Peripheral Select, 0=Fixed 1=Variable
#define SPI_MR_MODFDIS	(1 << 4) // Mode fault detection, 0=Enabled 1=Disabled

// SPI_SR
#define SPI_SR_RDRF		(1 << 0) // Receive Data Register Full (ie ready to read)
#define SPI_SR_TDRE		(1 << 1) // Transmit Data Register Empty (ie RTS)
#define SPI_SR_TXEMPTY	(1 << 9) // Transmit register empty

void gpio_set(uint32 pin, bool value) {
	uint32 pio = pin & 0xFFFFFF00;
	uint32 pinmask = 1 << (pin & 0x1F);

	if (value) {
		PUT32(pio + PIO_SODR, pinmask);
	} else {
		PUT32(pio + PIO_CODR, pinmask);
	}
}

void spi_init() {
	// SPI0_MOSI, SPI0_MISO, SPI0_SPCK all PA perif A
	PUT32(PIOA + PIO_PDR, SPI_PINS); // Disable PIO cos under peripheral control
	PUT32(PIOA + PIO_IDR, SPI_PINS); // Disable interrupts
	PUT32(PIOA + PIO_PUDR, SPI_PINS); // Disable pull-up

	// Enable peripheral clock for SPI0
	PUT32(PMC_PCER0, 1 << PERIPHERAL_ID_SPI0);

	// SPI_Configure(spi, id, SPI_MR_MSTR | SPI_MR_PS | SPI_MR_MODFDIS);
	// | pmc_enable_periph_clk( id(=SPI0) ) ;
	// | spi->SPI_CR = SPI_CR_SPIDIS ;
	// |
	// | // Execute a software reset of the SPI twice
	// | spi->SPI_CR = SPI_CR_SWRST ;
	// | spi->SPI_CR = SPI_CR_SWRST ;
	// | spi->SPI_MR = SPI_MR_MSTR | SPI_MR_PS | SPI_MR_MODFDIS ;
	// SPI_Enable(spi)

	PUT32(SPI0 + SPI_CR, SPI_CR_SPIDIS); // Disable SPI
	PUT32(SPI0 + SPI_CR, SPI_CR_SWRST); // Reset it
	PUT32(SPI0 + SPI_CR, SPI_CR_SWRST); // Reset it again (?)
	// We are using fixed peripheral select because it fits better with our
	// begin(), write(), end() model so we don't set SPI_MR_PS which is more
	// for stateless activity
	PUT32(SPI0 + SPI_MR, SPI_MR_MSTR | SPI_MR_MODFDIS);
	PUT32(SPI0 + SPI_CR, SPI_CR_SPIEN); // Now enable SPI
}

void spi_beginTransaction(uint32 chipSelectRegAddr) {
	// SPI peripheral selection see p689

	// We use the location of the relevant SPI_CSRn to uniquely identify both
	// the SPI port and the slave device on that port. Saves defining a new
	// enum.

	uint32 spi_n = chipSelectRegAddr & 0xFFFFFF00;
	ASSERT(spi_n == SPI0, spi_n); // Yeah we don't actually support other than 0
	uint32 id = ((chipSelectRegAddr & 0xFF) - SPI_CSR0) >> 2;
	printk("spi_beginTransaction CS=%d\n", id);
	uint32 pcs = 0xF ^ (1 << id); // Wow I actually get to use XOR operator!
	PUT32(SPI0 + SPI_MR, SPI_MR_MSTR | SPI_MR_MODFDIS | (pcs << 16));
}

#define WaitForBit(b) while ((GET32(SPI0 + SPI_SR) & (b)) == 0) { dummy(); }

void spi_endTransaction() {
	WaitForBit(SPI_SR_TXEMPTY);
	// For now don't bother deselecting chip select
}

void spi_readwrite_poll(uint8* buf, int length, bool writeBack) {
	for (int i = 0; i < length; i++) {
		// Wait for fifo to be ready
		//printk("T");
		WaitForBit(SPI_SR_TDRE);
		PUT32(SPI0 + SPI_TDR, buf[i]);
		// Wait for the corresponding read byte
		//printk("R");
		WaitForBit(SPI_SR_RDRF);
		uint32 ret = GET32(SPI0 + SPI_RDR);
		if (writeBack) {
			buf[i] = (uint8)ret;
		}
	}
	//printk("D");
	//WaitForBit(SPI_SR_TXEMPTY);
}
