--[[**
MemBuf
======

MemBufs represent raw chunks of native memory. They can be used as the
interface between Lua code and lower-level C functions that operate on pointers
to memory. MemBufs have both an address and a length, and unless otherwise
explicitly stated below, any attempt to access outside of the defined bounds
will cause an error. They cannot normally be created from Lua code (except for
when the klua debugger is running), only from native code using
[mbuf_new()](../userinc/lupi/membuf.h#mbuf_new).

]]

--[[**
Returns the byte of memory located at self:getAddress() + offset, as a number.
If an `_accessFn` is set, this will be used to read the memory, otherwise this
function performs the equivalent of:

	return *((char*)address + offset)

Note therefore that offset is *zero-based* because it is an offset in memory and
not a Lua table index. An error will be thrown if the location lies outside of
the MemBuf.
]]
--native function MemBuf:getByte(offset)

--[[**
Returns the 32-bit value located at self:getAddress() + offset, as a number. The
value must be word aligned.

In every other way, behaves the same as [getByte()](#getByte).
]]
--native function MemBuf:getInt(offset)

--[[**
Like [getInt()](#getInt) but returns an [Int64](int64.lua).
]]
--native function MemBuf:getInt64(offset)

--[[**
Returns the length of the buffer.
]]
--native function MemBuf:getLength()

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

local lvl = 0 -- Used while printing

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

function MemBuf._declareType(type, size)
	if not MemBuf._types then MemBuf._types = {} end
	MemBuf._types[type] = { _type=type, _size=size }
end

function getType(name)
	return MemBuf._types[name]
end

function setPointerDescriptionFunction(fn)
	MemBuf.pointerDescFn = fn
end

function MemBuf:checkType(type)
	local selfType = self:getType()
	assert(selfType._type == type, "MemBuf is of type"..selfType._type.." not "..type)
	return true
end

function MemBuf._declareMember(type, memberName, offset, size, memberType)
	local t = MemBuf._types[type]
	--# The "type" table t serves two purposes. In its integer keys it stores the members in order.
	--# Its non-integer keys also acts as a lookup of memberName to member
	--# (In addition to _type and _size)
	local member = { name=memberName, offset=offset, size=size, type=memberType }
	t[memberName] = member
	table.insert(t, member)
end

function MemBuf._declareValue(type, value, name)
	local t = MemBuf._types[type]
	if not t._values then t._values = {} end
	t._values[value] = name
end

--# Note mustn't invoke self's metatable in this function because it is used by __index!
function MemBuf._valueForMember(self, m)
	local obj = MemBuf._objects[MemBuf.getAddress(self) + m.offset]
	if obj and m.offset > 0 then
		--# It's an embedded object - note we can't currently display embedded objects at
		--# offset zero because there's no way of distinguishing it from self in the _objects table
		return obj
	elseif m.type == "char[]" then
		local tbl = {}
		local i = 0
		while i < m.size do
			local ch = MemBuf.getByte(self, m.offset + i)
			if ch == 0 then break end
			table.insert(tbl, safech(ch))
			i = i + 1
		end
		return table.concat(tbl)
	elseif m.size == 1 then
		return MemBuf.getByte(self, m.offset)
	elseif m.size == 4 then
		return MemBuf.getInt(self, m.offset)
	elseif m.size == 8 then
		return MemBuf.getInt64(self, m.offset)
	else
		return nil
	end
end

function MemBuf:_descriptionForMember(m, printer, linePrefix)
	local linePrefix = string.format("%s%s = ", linePrefix or "", m.name)

	local val = self:_valueForMember(m)
	local str
	local t = type(val)
	if t == "string" then
		str = '"'..val..'"'
	elseif t == "number" then
		if m.size == 1 then
			str = string.format("%x", val)
		elseif m.size == MemBuf._PTR_SIZE then
			-- See if it's a pointer to an object we know about
			local ptr = MemBuf._objects[val]
			local desc = MemBuf.pointerDescFn and MemBuf.pointerDescFn(val) or ""
			if ptr then
				str = string.format("%08X (%s*) %s", val, ptr:getType()._type, desc)
			else
				str = string.format("%08x %s", val, desc)
			end
		else
			str = string.format("%08x", val)
		end
	elseif t == "userdata" then
		if getmetatable(val) == MemBuf then
			MemBuf.description(val, printer, linePrefix)
			return
		else
			str = tostring(val)
		end
	else
		str = "??"
	end

	local desc = string.format("%s%s", linePrefix, str)
	-- See if type has any decription of the value
	local memberType = MemBuf._types[m.type]
	local valueName = memberType and memberType._values and memberType._values[val]

	if valueName then
		desc = desc .. string.format(" (%s)", valueName)
	end
	printer(desc)
end

--[[**
Returns a string representation of the MemBuf. If the MemBuf has a type, then
its members will be enumerated and listed. Otherwise it returns a single-line
description.
]]
function MemBuf:__tostring()
	local result = {}
	local function arrayConcat(str)
		table.insert(result, str)
	end
	self:description(arrayConcat)
	return table.concat(result, "\n")
end

--[[**
The textual representation of MemBuf objects can be dozens of lines long, and
on severely memory-contrained devices the simple approach of constructing the
text as one big string (via `tostring()`) then printing it can take too much
memory to even run.

Instead, MemBufs support a streaming `description` function which outputs each
line as it's constructed by calling `printer(line)`. `linePrefix` is an optional
parameter used when you already have some text that should appear on the first
line of the output.

For example the following code:

	local buf = MemBuf.null
	buf:description(print, "This is a ")

would print the following:

	This is a MemBuf(00000000, 0)
]]
function MemBuf:description(printer, linePrefix)
	local bufType = self:getType()
	if bufType then
		-- Do we have any members?
		if bufType[1] == nil then
			-- If not, assume we've got some values declared, eg we're representing an enum
			local val = self:_valueForMember({ offset = 0, size = self:getLength() })
			local result = bufType._values and bufType._values[val]
			if result then
				printer(string.format("%s%s (%s)", linePrefix or "", tostring(val), result))
				return
			end
		else
			printer(string.format("%s%08X %s:", linePrefix or "", self:getAddress(), bufType._type))
			lvl = lvl + 1
			for _, m in ipairs(bufType) do
				local linePrefix = string.format("%08X:%s",
					self:getAddress() + m.offset,
					string.rep("| ", lvl))
				self:_descriptionForMember(m, printer, linePrefix)
			end
			lvl = lvl - 1
			return
		end
	end

	printer(string.format("MemBuf(%08X, %d)", self:getAddress(), self:getLength()))
end

function MemBuf.__index(mbuf, key)
	-- First see if our obj has a type - making sure not to invoke anything that will
	-- end up calling back into __index!
	local t = MemBuf.getType(mbuf)
	local member = t and t[key]
	if member and member.size == 4 then
		local addr = MemBuf.getInt(mbuf, member.offset)
		local obj = MemBuf._objects[addr]
		if obj then return obj end
	end
	if member then
		-- Just return the member val
		return MemBuf._valueForMember(mbuf, member)
	end

	-- As a fallback, behave like a normal metatable __index and return MemBuf
	return MemBuf[key]
end

function MemBuf._newObject(obj)
	local t = obj:getType()
	if t == nil then
		--# Anonymous untyped objects don't go in the global array
		return
	end

	--# If there is a type, check it agrees with the obj len otherwise it's an error
	if t._size ~= obj:getLength() then
		error("Mismatch between object length and type size")
	end

	if not MemBuf._objects then MemBuf._objects = {} end
	MemBuf._objects[obj:getAddress()] = obj

	--# Check for embedded objects and declare them too
	for i, member in ipairs(t) do
		if member.type then
			--print(string.format("subobject of %s at %d+%d %s %s", t._type, member.offset, member.size, member.type, member.name))
			obj:sub(member.offset, member.size, member.type)
		end
	end
end
