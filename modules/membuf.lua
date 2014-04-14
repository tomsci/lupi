-- Pretty much everything here is native code...

--# Note that for getByte and getInt, indexes are *ZERO-BASED* because we're talking memory, not Lua data structures.

print "Loading membuf.lua..."
print(MemBuf)
assert(MemBuf)

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

function MemBuf:hex(length)
	if length == nil then length = self:length() end

	local result = {}
	for i = 0, length-1, 16 do
		table.insert(result, string.format("%08X: ", self:address() + i))
		local len = min(length - i, 16)
		for j = 0, len-1 do
			table.insert(result, string.format("%02X ", self:getByte(i+j)))
		end
		table.insert(result, " ")
		for j = 0, len-1 do
			table.insert(result, safech(self:getByte(i+j)))
		end
		table.insert(result, "\n");
	end

	return table.concat(result)
end

function MemBuf:words(length)
	if length == nil then length = self:length() end

	local result = {}
	for i = 0, length-1, 16 do
		table.insert(result, string.format("%08X: ", self:address() + i))
		local len = min(length - i, 16)
		for j = 0, len-1, 4 do
			if len-j < 4 then
				for k = 0, len-j-1 do
					table.insert(result, string.format("%02X ", self:getByte(i+j+k)))
				end
			else
				table.insert(result, string.format("%08X ", self:getInt(i+j)))
			end
		end
		table.insert(result, " ")
		for j = 0, len-1 do
			table.insert(result, safech(self:getByte(i+j)))
		end
		table.insert(result, "\n");
	end

	return table.concat(result)
end
