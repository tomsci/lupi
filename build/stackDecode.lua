#!/usr/local/bin/lua5.3

if #arg ~= 2 then
	print("Syntax: stackDecode.lua <kernel.txt> <dumpfile>")
	os.exit(1)
end

local elf = arg[1]
local dumpFile = arg[2]

local symParser = require("modules/symbolParser")
symParser.getSymbolsFromReadElf(elf)

local f = assert(io.open(dumpFile, "r"))
for line in f:lines() do
	local addr, rest = line:match("(%x%x%x%x%x%x%x%x): (.*)  .*")
	if addr then
		addr = tonumber(addr, 16)
		for word in rest:gsub("(%x) (%x)", "%1%2"):gmatch("%x%x%x%x%x%x%x%x") do
			local value = assert(tonumber(word, 16))
			local desc = symParser.addressDescription(value)
			print(string.format("%x: %08X %s", addr, value, desc))
			addr = addr + 4
		end
	end
end

f:close()
