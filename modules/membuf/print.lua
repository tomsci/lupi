require "membuf"

MemBuf = membuf.MemBuf

local function min(x,y)
	return x < y and x or y
end

local function safech(ch)
	if ch >= 32 and ch < 127 then
		return string.char(ch)
	else
		return "."
	end
end

--[[**
Returns a hexdump-style string of the first `length` bytes of the MemBuf (or of
the entire MemBuf if length is nil).

If `printer` is non-nil, the output is printed line-by-line by repeatedly
calling `printer(line)` rather than being returned as one big string.
]]
function MemBuf:hex(length, printer)
	if length == nil then length = self:getLength() end

	local result = {}
	local function arrayConcat(str)
		table.insert(result, str)
	end
	local outputFn = printer or arrayConcat
	for i = 0, length-1, 16 do
		local line = {}
		table.insert(line, string.format("%08X: ", self:getAddress() + i))
		local len = min(length - i, 16)
		for j = 0, len-1 do
			table.insert(line, string.format("%02X ", self:getByte(i+j)))
		end
		table.insert(line, " ")
		for j = 0, len-1 do
			table.insert(line, safech(self:getByte(i+j)))
		end
		outputFn(table.concat(line))
	end

	if printer == nil then
		return table.concat(result, "\n")
	end
end

--[[**
Like [hex()](#hex) but interprets the data as 32-bit words. This means
little-endian integers are displayed in a more readable fashion.

If `printer` is non-nil, the output is printed line-by-line by repeatedly
calling `printer(line)` rather than being returned as one big string.
]]
function MemBuf:words(length, printer)
	if length == nil then length = self:getLength() end

	local result = {}
	local function arrayConcat(str)
		table.insert(result, str)
	end
	local outputFn = printer or arrayConcat
	for i = 0, length-1, 16 do
		local line = {}
		table.insert(line, string.format("%08X: ", self:getAddress() + i))
		local len = min(length - i, 16)
		for j = 0, len-1, 4 do
			if len-j < 4 then
				for k = 0, len-j-1 do
					table.insert(line, string.format("%02X ", self:getByte(i+j+k)))
				end
			else
				table.insert(line, string.format("%08X ", self:getInt(i+j)))
			end
		end
		table.insert(line, " ")
		for j = 0, len-1 do
			table.insert(line, safech(self:getByte(i+j)))
		end
		outputFn(table.concat(line))
	end

	if printer == nil then
		return table.concat(result, "\n")
	end
end