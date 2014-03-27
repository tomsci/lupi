config = {
	toolchainPrefix = "arm-none-eabi-",
	machine = { "arm", "armv6", "arm1176jzf-s" },

	include = "pi.h",
	userInclude = "piuser.h",

	--# NOTE: -fno-reorder-functions is essential to prevent the linker moving the entry point!
	platOpts = "-mcpu=arm1176jzf-s -mabi=aapcs -nostartfiles -Os -fno-reorder-functions",

	entryPoint = "build/pi/piboot.c",

	extraKernelSources = {
		"k/mmu_arm.c",
		"build/pi/uart.c",
	},

	extraStdInc = "build/pi/stdinc",

	klua = true,
}
