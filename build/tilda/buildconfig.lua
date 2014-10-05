config = {
	toolchainPrefix = "arm-none-eabi-",
	machine = { "armv7-m", "cortex-m3" },

	include = "tilda.h",
	userInclude = "tildauser.h",

	-- NOTE: -fno-reorder-functions is essential to prevent the linker moving the entry point!
	platOpts = "-mthumb -march=armv7-m -mcpu=cortex-m3 -mabi=aapcs -mno-unaligned-access -nostartfiles -Os -fno-reorder-functions",

	entryPoint = "build/tilda/tildaboot.c",

	sources = {
		"build/tilda/uart.c",
		"build/tilda/pio.c",
		--"build/tilda/lcd.c",
	},

	extraStdInc = "build/tilda/stdinc",

	klua = true,
	ulua = false,

	textSectionStart = 0x00080000, -- Must match KKernelCodeBase
	bssSectionStart = 0x20073000, -- Must match KUserBss

}
