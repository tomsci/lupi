require "membuf"

local MemBuf = membuf.MemBuf

function MemBuf._declareType(type, size)
	if not MemBuf._types then MemBuf._types = {} end
	MemBuf._types[type] = { _type=type, _size=size }
end

function MemBuf:checkType(type)
	local selfType = self:getType()
	assert(selfType._type == type, "MemBuf is of type"..selfType._type.." not "..type)
	return true
end

function MemBuf._declareMember(type, memberName, offset, size, memberType)
	local t = MemBuf._types[type]
	-- The "type" table t serves two purposes. In its integer keys it stores the members in order.
	-- Its non-integer keys also acts as a lookup of memberName to member
	-- (In addition to _type and _size)

	local member = { name=memberName, offset=offset, size=size, type=memberType }
	if memberType and memberType:match("^BITFIELD#") then
		-- Needs the size in the type to so that a uint16 bitfield can be distinguished from a uint32 one etc.
		member.type = string.format("BITFIELD#%d%s", size, memberType:sub(10))
		-- print(string.format("BITFIELD! type=%s", member.type))
	end

	t[memberName] = member
	table.insert(t, member)
end

function MemBuf._declareValue(type, value, name)
	local t = MemBuf._types[type]
	if not t._values then t._values = {} end
	t._values[value] = name
end

-- Note mustn't invoke self's metatable in this function because it is used by __index!
function MemBuf._valueForMember(self, m)
	local obj = MemBuf._objects[MemBuf.getAddress(self) + m.offset]
	if obj and m.offset > 0 then
		-- It's an embedded object - note we can't currently display embedded objects at
		-- offset zero because there's no way of distinguishing it from self in the _objects table
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
	-- print(string.format("_descriptionForMember %s", tostring(m.type)))
	local valueName
	local bitfield = m.type and m.type:match("^BITFIELD#%d(.*)")
	if bitfield then
		local parts = {}
		local enumType = MemBuf._types[bitfield]
		-- print(string.format("Describing BITFIELD %s of %s!", m.type, bitfield))
		if enumType then
			for enumVal, name in pairs(enumType._values) do
				if bit32.band(val, bit32.lshift(1, enumVal)) > 0 then
					table.insert(parts, name)
				end
			end
			valueName = table.concat(parts, "|")
		end
	else
		local memberType = MemBuf._types[m.type]
		valueName = memberType and memberType._values and memberType._values[val]
	end

	if valueName then
		desc = desc .. string.format(" (%s)", valueName)
	end
	printer(desc)
end

local lvl = 0 -- Used while printing

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
			printer(string.format("%s%08X %s: (%d bytes)",
				linePrefix or "", self:getAddress(), bufType._type, self:getLength()))
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

function MemBuf:getDescriptionString()
	local result = {}
	local function arrayConcat(str)
		table.insert(result, str)
	end
	self:description(arrayConcat)
	return table.concat(result, "\n")
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
		-- Anonymous untyped objects don't go in the global array
		return
	end

	-- If there is a type, check it agrees with the obj len otherwise it's an error
	if t._size ~= obj:getLength() then
		error("Mismatch between object length "..tostring(obj:getLength()).." and the size "..tostring(t._size).." of type "..t._type)
	end

	if not MemBuf._objects then MemBuf._objects = {} end
	MemBuf._objects[obj:getAddress()] = obj

	-- Check for embedded objects and declare them too
	for i, member in ipairs(t) do
		if member.type then
			-- print(string.format("subobject of %s at %d+%d %s %s", t._type, member.offset, member.size, member.type, member.name))
			obj:sub(member.offset, member.size, member.type)
		end
	end
end
