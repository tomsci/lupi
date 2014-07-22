--[[**
Contains functions for looking up symbol names and addresses.

Note this module is unusual in that it is also used by the build system.
]]

symbols = nil
debug = false
local symbolCache

function setSymbols(arg)
	symbols = arg
end

function loadSymbolsFromSymbolsModule()
	local s = require("symbols")
	setSymbols(s.symbols)
end

function getSymbolsFromReadElf(elfFile)
	local f = assert(io.open(elfFile, "r"))
	local syms = {}
	local n = 0
	for line in f:lines() do
		local addr, size, name = line:match("^ +[0-9]+: (%x+) +(%d+).+ ([a-zA-Z._]+)$")
		if addr then
			addr = tonumber(addr, 16)
			size = tonumber(size, 10)
			if addr ~= 0 then
				table.insert(syms, { addr = addr, size = size, name = name })
				n = n + 1
			end
		end
	end
	table.sort(syms, function(a,b) return a.addr < b.addr end)
	f:close()
	syms.n = n
	symbols = syms
	return syms
end

local sizeFail = (0x80000000 < 0)

-- Make sure compiled constants are being handled same as runtime
assert((tonumber("80000000", 16) < 0) == (0x80000000 < 0))

function lt(a, b)
	if not sizeFail then return a < b end

	-- Otherwise, the curse of signed 32-bit integers strikes again
	local abig, bbig = a < 0 and 1 or 0, b < 0 and 1 or 0
	local aa, bb = bit32.band(a, 0x7FFFFFF), bit32.band(b, 0x7FFFFFF)
	--if debug then print(string.format("a=%x b=%x abig=%d bbig=%d aa=%x bb=%x", a, b, abig, bbig, aa, bb)) end
	return abig < bbig or aa < bb
end

function findSymbol(addr)
	local function lowerBoundSearch(i, j)
		if debug then print(string.format("lowerBoundSearch(%d, %d)", i, j)) end
		-- When we're running in OS we don't have math library, but equally
		-- in that case division is always integer so we don't need it
		local floor = math and math.floor or function(n) return n end
		local pivot = floor((i+j)/2)
		if debug then print(string.format("pivot %d = %x", pivot, symbols[pivot].addr)) end
		if lt(addr, symbols[pivot].addr) then
			if pivot == 1 then return nil end -- Before first symbol
			return lowerBoundSearch(i, pivot-1)
		else
			-- Check if it's between pivot and pivot+1, if so return pivot
			local nextSym = symbols[pivot+1]
			if nextSym == nil then
				-- It's off the end of the symbol table
				return nil
			elseif lt(addr, nextSym.addr) then
				return symbols[pivot]
			else
				return lowerBoundSearch(pivot+1, j)
			end
		end
	end
	return lowerBoundSearch(1, symbols.n)
end

function addressDescription(addr)
	local sym = findSymbol(addr)
	if sym and addr - sym.addr < 64*1024 then
		if KUserHeapBase and addr > KUserHeapBase and sym.addr < KUserHeapBase then
			-- Then it's not a useful symbol
			return ""
		end
		-- No point if the symbol is miles away from the addres
		return string.format("%s + %d", sym.name, addr - sym.addr)
	else
		return ""
	end
end

function findSymbolByName(name)
	if not symbolCache then
		symbolCache = {}
		for _, sym in ipairs(symbols) do
			symbolCache[sym.name] = sym
		end
	end
	return symbolCache[name]
end

function dumpSymbolsTable()
	local tbl = { "symbols = {\n" }
	for _, sym in ipairs(symbols) do
		local s = string.format('	{ addr = 0x%x, name = %q, size = %d },\n',
			sym.addr, sym.name, sym.size)
		table.insert(tbl, s)
	end
	table.insert(tbl, string.format("	n = %d\n}\n", symbols.n))
	return table.concat(tbl)
end

return _ENV -- Gah
