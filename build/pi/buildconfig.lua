config = {
	toolchainPrefix = "arm-none-eabi-",
	machine = { "arm", "armv6", "armv6-m", "arm1176jzf-s" },
	postLinker = build.postLinkElf32,

	include = "pi.h",
	userInclude = "piuser.h",

	-- NOTE: -fno-reorder-functions is essential to prevent the linker moving the entry point!
	-- Also note, -Os can't be used if klua and ulua are false, because it insists on converting
	-- division into __aeabi_uidiv and friends (presumably to save space)
	platOpts = "-mcpu=arm1176jzf-s -mabi=aapcs -mno-unaligned-access -nostartfiles -Os -fno-reorder-functions",

	entryPoint = "build/pi/piboot.c",

	sources = {
		"build/pi/atags.c",
		"build/pi/gpio.c",
		"build/pi/uart.c",
		"build/pi/irq.c",
		"build/pi/pitft.c",
	},

	extraStdInc = "build/pi/stdinc",

	klua = true,
	ulua = true,
	malloc = true,

	textSectionStart = 0xF8008000,
	bssSectionStart = 0x00007000,
	maxCodeSize = 512*1024, -- We've only allowed 512KB in the memory map

}
