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
		"build/tilda/board.c",
		"build/tilda/pio.c",
		"build/tilda/lcd.c",
		"build/tilda/flash.c",
		"build/tilda/audio.c",
	},

	extraStdInc = "build/tilda/stdinc",

	klua = true,
	-- Not enough ram for our wasteful klua allocator to load modules
	-- kluaIncludesModules = true,
	ulua = true,
	-- malloc = true,

	textSectionStart = 0x00080000, -- Must match KKernelCodeBase
	bssSectionStart = 0x20087DE0, -- Must match KUserBss
	maxCodeSize = 512*1024, -- We only have 512KB of flash

}
