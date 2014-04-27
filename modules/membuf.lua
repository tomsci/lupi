require "int64"

-- The MemBuf object itself and a bunch of primitives are declared in native code

--# Note that for getByte and getInt, indexes are *ZERO-BASED* because we're talking memory, not Lua data structures.

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

function MemBuf:hex(length)
	if length == nil then length = self:getLength() end

	local result = {}
	for i = 0, length-1, 16 do
		table.insert(result, string.format("%08X: ", self:getAddress() + i))
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
	if length == nil then length = self:getLength() end

	local result = {}
	for i = 0, length-1, 16 do
		table.insert(result, string.format("%08X: ", self:getAddress() + i))
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

function MemBuf._declareType(type, size)
	if not MemBuf._types then MemBuf._types = {} end
	MemBuf._types[type] = { _type=type, _size=size }
end

function getType(name)
	return MemBuf._types[name]
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

function MemBuf:_descriptionForMember(m)
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
			if ptr then
				str = string.format("%08X (%s*)", val, ptr:getType()._type)
			else
				str = string.format("%08x", val)
			end
		else
			str = string.format("%08x", val)
		end
	elseif t == "userdata" then
		str = tostring(val)
	else
		str = "??"
	end

	local desc = string.format("%s = %s", m.name, str)
	-- See if type has any decription of the value
	local memberType = MemBuf._types[m.type]
	local valueName = memberType and memberType._values and memberType._values[val]

	if valueName then
		desc = desc .. string.format(" (%s)", valueName)
	end
	return desc
end

function MemBuf:__tostring()
	local bufType = self:getType()
	if bufType then
		--# Do we have any members?
		if bufType[1] == nil then
			--# If not, assume we've got some values declared, eg we're representing an enum
			local val = self:_valueForMember({ offset = 0, size = self:getLength() })
			local result = bufType._values and bufType._values[val]
			if result then
				return string.format("%s (%s)", tostring(val), result)
			end
		else
			local result = { string.format("%08X %s:", self:getAddress(), bufType._type) }
			lvl = lvl + 1
			for _, m in ipairs(bufType) do
				local s = string.format("%08X:%s%s",
					self:getAddress() + m.offset,
					string.rep("| ", lvl),
					self:_descriptionForMember(m)
				)
				table.insert(result, s)
			end
			lvl = lvl - 1
			return table.concat(result, "\n")
		end
	end

	return string.format("MemBuf(%08X, %d)", self:getAddress(), self:getLength())
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
