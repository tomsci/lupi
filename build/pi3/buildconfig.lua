config = {
	cc = "clang",
	machine = { "aarch64" },

	include = "pi3.h",
	userInclude = "pi3user.h",

	platOpts = "-target aarch64-none-aapcs -mno-unaligned-access -Os",

	entryPoint = "build/pi3/pi3boot.c",

	sources = {
		-- "build/pi/gpio.c",
		-- "build/pi/uart.c",
		-- "build/pi/irq.c",
		-- "build/pi/pitft.c",
	},

	-- extraStdInc = "build/pi/stdinc",

	klua = true,
	ulua = true,
	malloc = true,

	textSectionStart = 0xF8008000,
	bssSectionStart = 0x00007000,
	maxCodeSize = 512*1024, -- We've only allowed 512KB in the memory map

}

config.link = function()
	print("TODO: link!")
end
