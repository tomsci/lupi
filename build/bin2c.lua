#!/usr/local/bin/lua

f = assert(io.open(arg[1], "rb"))
if arg[2] then
	chunkSize = tonumber(arg[2])
	outf = assert(io.open(arg[1]..".chunk0.c", "w+b"))
else
	outf = io.stdout
end

data = f:read("*a")
local len = #data
header = "static const unsigned char data[] = {\n"
if chunkSize then
	outf:write("#define dataStart 0\n")
end
outf:write(header)

local bytesWritten = 0
local n = 0
for i = 1, #data, 16 do
	local line = { "\t" }
	for j = 0, 15 do
		if i+j > len then break end
		table.insert(line, string.format("0x%02X,", data:byte(i+j)))
		if j ~= 15 then
			table.insert(line, " ")
		end
		bytesWritten = bytesWritten + 1
	end
	outf:write(table.concat(line))
	outf:write('\n')
	if chunkSize and bytesWritten == chunkSize then
		outf:write("};\n")
		outf:close()
		n = n + 1
		bytesWritten = 0
		outf = assert(io.open(string.format("%s.chunk%d.c", arg[1], n), "w+b"))
		outf:write(string.format("#define dataStart %d\n", n * chunkSize))
		outf:write(header)
	end
end
f:close()
outf:write("};\n")
