#!/usr/local/bin/lua5.3

--[[**
Converts an XBM to the column-packed format used by the TiLDA screen.

Syntax:

	./xbm2colpacked.lua < img.xbm
]]


name, width, height = nil, nil, nil
bytes = {}
packedBytes = {}

-- require "bit32"
--local band, bor, lshift, rshift, bnot = bit32.band, bit32.bor, bit32.lshift, bit32.rshift, bit32.bnot

-- bit32 compat
local function band(a, b) return a & b end
local function bor(a, b) return a | b end
local function lshift(a, b) return a << b end
local function rshift(a, b) return a >> b end
local function bnot(a) return ~a end

local function getBit(bitIdx)
	local byte = bytes[rshift(bitIdx, 3) + 1]
	if byte == nil then return nil end
	return band(byte, lshift(1, band(bitIdx, 7)))
end

local function byteForPageAndColumn(page, column)
	local result = 0
	local x = column
	local y = page*8
	for yidx = 0, 7 do
		local bit = getBit((y + yidx) * stride + x)
		if bit and bit == 0 then
			-- bit == 0 means black in XBM terms, meaning we need to set the bit
			-- to 1 in the format the TiLDA screen uses.
			result = bor(result, lshift(1, yidx))
		end
	end
	return result
end


for line in io.lines() do
	local n, w = line:match("#define (.*)_width (.*)")
	if n then
		name = n:match("(.*)_xbm") or n
		width = assert(tonumber(w))
		stride = band(width + 7, bnot(7))
	else
		local h = line:match("#define .*_height (.*)")
		if h then height = assert(tonumber(h)) end
	end

	if line:match("^%s*0x") then
		for byte in line:gmatch("0x(%x%x)") do
			table.insert(bytes, tonumber(byte, 16))
		end
	end
end

n = #bytes
assert(n*8 >= width * height, "Not enough data, need "..tostring(width * height).." bits, only got "..tostring(n*8))

for page = 0, (height + 7) / 8 - 1 do
	local lineStr = {}
	for column = 0, width - 1 do
		table.insert(packedBytes, byteForPageAndColumn(page, column))
	end
end
assert(#packedBytes >= n, "packedBytes shorter than bytes??")

print(string.format("#define %s_width %d", name, width))
print(string.format("#define %s_npages %d", name, #packedBytes / width))
print("static const unsigned char "..name.."[] = {")
for i = 1, #packedBytes, 12 do
	local line = { "\t" }
	for j = 0, 11 do
		local b = packedBytes[i + j]
		if not b then break end
		if j > 0 then table.insert(line, " ") end
		table.insert(line, string.format("0x%02x,", b))
	end
	print(table.concat(line))
end
print("};")
