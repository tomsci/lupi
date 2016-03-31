config = {
	machine = { "aarch64", "host" },

	include = "pi3.h",
	userInclude = "pi3user.h",

	-- platOpts = "-target aarch64-none-elf -mno-unaligned-access -Os",

	entryPoint = "build/pi3/pi3boot.c",

	sources = {
		"k/cpumode_aarch64.c",
		-- "build/pi/gpio.c",
		-- "build/pi/uart.c",
		-- "build/pi/irq.c",
		-- "build/pi/pitft.c",
	},

	-- extraStdInc = "build/pi/stdinc",

	klua = false,
	ulua = false,
	malloc = false,

	-- textSectionStart = 0xF8008000,
	-- bssSectionStart = 0x00007000,
	-- maxCodeSize = 512*1024, -- We've only allowed 512KB in the memory map

	textSectionStart = 0x8000,
}

config.compiler = function(stage, config, opts)
	local qrp, join = build.qrp, build.join
	local exe = "clang"
	-- Figured out by trial, error and "clang -v"
	local clangOpts = "-cc1 -triple arm64-apple-macosx10.11.0 -emit-obj -mthread-model single -target-abi darwinpcs -backend-option -aarch64-strict-align -nostdsysteminc -nobuiltininc -ffreestanding -fmax-type-align=16 -fdiagnostics-show-option -fcolor-diagnostics -vectorize-loops -vectorize-slp -x c"
	local langOpts = "-std=c99 -Wall -Werror -Wno-error=unused-function"
	local output = "-o "..qrp(opts.destination)
	local allOpts = join {
		clangOpts,
		langOpts,
		join(opts.source.copts),
		join(opts.extraArgs),
		output
	}
	local cmd = string.format("%s %s %s ", exe, allOpts, qrp(opts.source.path))
	return cmd
end

config.linker = function(stage, config, opts)
	local result = opts.outDir.."kernel.macho"
	local cmd = string.format("/usr/bin/ld %s -segaddr __TEXT 0x%x -o %s %s",
		"-Z -preload -arch arm64 -e _start",
		config.textSectionStart,
		build.qrp(result),
		build.join(opts.objs)
	)
	local ok = build.exec(cmd)
	if not ok then error("Link failed!") end
	return result
end
