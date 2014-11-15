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
Returns the 16-bit value located at self:getAddress() + offset, as a number. The
value must be 2-byte aligned.

In every other way, behaves the same as [getByte()](#getByte).
]]
--native function MemBuf:getUint16(offset)

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

function getType(name)
	return MemBuf._types[name]
end

function setPointerDescriptionFunction(fn)
	MemBuf.pointerDescFn = fn
end


--[[**
Returns a string representation of the MemBuf. If the MemBuf has a type, then
its members will be enumerated and listed. Otherwise it returns a single-line
description.
]]
function MemBuf:__tostring()
	if MemBuf.getDescriptionString ~= nil then
		return MemBuf.getDescriptionString(self)
	else
		return string.format("MemBuf(%08X, %d)", self:getAddress(), self:getLength())
	end
end

function MemBuf.__index(mbuf, key)
	-- Note this can be overridden by membuf/types.lua
	return MemBuf[key]
end
