config = {
	toolchainPrefix = "arm-none-eabi-",
	machine = { "arm", "armv6", "arm1176jzf-s" },

	include = "pi.h",
	userInclude = "piuser.h",

	--# NOTE: -fno-reorder-functions is essential to prevent the linker moving the entry point!
	--# Also note, -Os can't be used if klua and ulua are false, because it insists on converting
	--# division into __aeabi_uidiv and friends (presumably to save space)
	platOpts = "-mcpu=arm1176jzf-s -mabi=aapcs -mno-unaligned-access -nostartfiles -Os -fno-reorder-functions",

	entryPoint = "build/pi/piboot.c",

	sources = {
		"build/pi/uart.c",
		"build/pi/irq.c",
	},

	extraStdInc = "build/pi/stdinc",

	klua = true,
	ulua = true,
}
