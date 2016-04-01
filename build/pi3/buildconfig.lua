config = {
	machine = { "aarch64", "host" },

	include = "pi3.h",
	userInclude = "pi3user.h",

	-- platOpts = "-target aarch64-none-elf -mno-unaligned-access -Os",

	entryPoint = "build/pi3/pi3boot.c",

	sources = {
		"k/cpumode_aarch64.c",
		"k/debug.c",
		"build/pi/gpio.c",
		"build/pi/uart.c",
		-- "build/pi/irq.c",
		-- "build/pi/pitft.c",
	},

	-- extraStdInc = "build/pi/stdinc",

	klua = false,
	ulua = false,
	malloc = false,
	lp64 = true,

	-- textSectionStart = 0xF8008000,
	-- bssSectionStart = 0x00007000,
	-- maxCodeSize = 512*1024, -- We've only allowed 512KB in the memory map

	textSectionStart = 0x8000,
}

config.compiler = function(stage, config, opts)
	local qrp, join = build.qrp, build.join
	local exe = "clang"
	-- Figured out by trial, error and "clang -v"
	local clangOpts = "-cc1 -triple arm64-apple-macosx10.11.0 -emit-obj -mthread-model single -target-abi darwinpcs -nostdsysteminc -nobuiltininc -ffreestanding -fmax-type-align=16 -fdiagnostics-show-option -fcolor-diagnostics -vectorize-loops -vectorize-slp -x c"
	local langOpts = "-std=c99 -Wall -Werror -Wno-error=unused-function"
	local output = "-o "..qrp(opts.destination)
	local allOpts = join {
		clangOpts,
		langOpts,
		join(opts.source.copts),
		join(opts.extraArgs),
		output
	}
	if opts.listing then
		allOpts = allOpts.." -g"
	end
	local cmd = string.format("%s %s %s ", exe, allOpts, qrp(opts.source.path))
	return cmd
end

config.linker = function(stage, config, opts)
	local result = opts.outDir.."kernel.macho"
	local mapFile = opts.outDir.."kernel.map"
	local cmd = string.format("/usr/bin/ld %s -segaddr __TEXT 0x%x -o %s %s",
		"-Z -preload -arch arm64 -e _start -map "..build.qrp(mapFile),
		config.textSectionStart,
		build.qrp(result),
		build.join(opts.objs)
	)
	local ok = build.exec(cmd)
	if not ok then error("Link failed!") end
	opts.mapFile = mapFile
	return result
end

config.postLinker = function(stage, config, opts)
	-- Post-linking Mach-O files is so much fun. We've configured the linker
	-- such that the only sections listed in the file are ones we want, so use
	-- the map file to pull them out. Having a way to extrace the whole __TEXT
	-- section would be far too simple.

	local symParser = require("modules/symbolParser")
	symParser.getSymbolsFromMachOMapFile(opts.mapFile)
	-- print(symParser.dumpSymbolsTable())

	local cmd = string.format("segedit %s -extract ", build.qrp(opts.linkedFile))
	local outFile = opts.outDir.."kernel.img"
	local outf = assert(io.open(outFile, "wb"))
	for _, section in ipairs(symParser.sections) do
		local sectFile = opts.linkedFile.."."..(section.name:gsub(" ", "."))
		local ok = build.exec(cmd..section.name.." "..build.qrp(sectFile))
		assert(ok, "Extract of section "..section.name.." failed!")
		local f = io.open(sectFile, "rb")
		local data = assert(f:read("a"))
		local offset = outf:seek()
		if opts.verbose then
			print(string.format("Writing section %q (addr=%x len=%x) to image offset %x", section.name, section.addr, section.size, offset))
		end
		assert(#data == section.size, "Section "..section.name.." size doesn't match map file size!")
		assert(offset == section.addr - config.textSectionStart, "Section "..section.name.." isn't at the right offset!")
		assert(outf:write(data))
		f:close()
		os.remove(sectFile)
	end
	outf:close()

	return outFile
end
