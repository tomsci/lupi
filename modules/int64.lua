--[[**
Int64
=====

The Int64 type is a userdata containing a C `int64` (`long long`). Int64s are
only returned by a few native APIs, and can not (currently) be created by Lua
code. Int64s are immutable (from Lua code) and support the usual range of
operators which always create a new object.

Note that Int64s should not be used as indexes into Lua tables - table indexing
uses the definition of raw equality, so two Int64s with the same value will
*not* index to the same value even though we define an __eq metamethod. Convert
to a string first, or if performance is more important than readability, use
[rawval()](#Int64_rawval) to return the raw value as a Lua string.
]]

--[[**
Returns the value of the Int64 as a string of 16 hex degits. Usage:

		tostring(theInt64)
]]
function Int64:__tostring()
	return string.format("%08X%08X", self:hi(), self:lo())
end

Int64.__index = Int64

--[[**
Returns true if both `Int64`s represent the same integer value.
]]
function Int64.__eq(l, r)
	return l:lo() == r:lo() and l:hi() == r:hi()
end

--[[**
Returns a binary blob representing the Int64's value. This is a copy, and is
the most compact form if you want to use a 64-bit index into a table. It doesn't
have many other uses. The returned value is a string and can be tested for
equality with the rawval of another Int64, but it cannot be converted back into
an Int64 object.
]]
--native function Int64:rawval()
