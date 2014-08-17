--[[**
This function behaves like `ipairs`, except that it returns a third value, which
is a function that removes the current value without messing up the iteration.
Usage:

	local tbl = { "a", "b", "c" }
	for i, val, remove in misc.iter(tbl) do
		if val == "b" then
			remove()
			-- Table is now { "a", "c" } and the next time
			-- around the loop, val will be "c"
		end
	end

]]
function iter(tbl)
	local deleted = false
	local i = 0
	local function remove()
		table.remove(tbl, i)
		deleted = true
	end
	local function f(state, lastIdx)
		if deleted then
			i = lastIdx
			deleted = false
		else
			i = lastIdx + 1
		end
		local obj = tbl[i]
		if obj == nil then return nil end
		return i, obj, remove
	end
	return f, tbl, 0
end

arrayMt = {
	remove = table.remove,
	insert = table.insert,
	sort = table.sort,
	concat = table.concat,
	unpack = table.unpack,
	iter = iter,
}
arrayMt.__index = arrayMt

--[[**
Sets a metatable on `arg` that makes the standard table functions and `iter`
available as member functions. For example:

	local t = array { "a", "b", "c" }
	t:remove(2)   -- equivalent to table.remove(t, 2)
	t:insert(2, "b") -- equivalent to table.insert(t, 2, "b")
	for k,v,del in t:iter() do -- equivalent to misc.iter(t)
		-- ...
	end

`array()` is equivalent to `array({})`.
]]
function array(arg)
	local result = arg or {}
	setmetatable(result, arrayMt)
	return result
end

--[[**
Rounds `val` down to the nearest multiple of `size`, considering `val` as if it
were unsigned (ie values with the top bit set are considered large positive).
]]
function roundDownUnsigned(val, size)
	-- With integer division (which is the only type we have atm) this should do the trick
	local n = val / size
	-- Ugh because Lua is compiled with 32-bit signed ints only, if val is
	-- greater than 0x8000000, then the above divide which rounds towards zero,
	-- will actually round *up* when the number is considered as unsigned
	-- Therefore, subtract one from n in this case
	if val < 0 then n = n - 1 end
	return n * size
end

local sizeFail = (0x80000000 < 0)

-- Make sure compiled constants are being handled same as runtime
assert((tonumber("80000000", 16) < 0) == (0x80000000 < 0))

--[[**
Helper function that returns true if a is strictly less than b, considering
both to be unsigned.
]]
function lessThanUnsigned(a, b)
	if not sizeFail then return a < b end

	-- Otherwise, the curse of signed 32-bit integers strikes again
	local abig, bbig = a < 0 and 1 or 0, b < 0 and 1 or 0
	local aa, bb = bit32.band(a, 0x7FFFFFF), bit32.band(b, 0x7FFFFFF)
	--if debug then print(string.format("a=%x b=%x abig=%d bbig=%d aa=%x bb=%x", a, b, abig, bbig, aa, bb)) end
	return abig < bbig or aa < bb
end

local function instanciate(classObj, args)
	local obj = args or {}
	setmetatable(obj, classObj)
	if obj.init then
		obj:init()
	end
	return obj
end

local classObjMt = {
	__call = instanciate
}

function memberEnv(obj)
	if obj._memberEnv == nil then
		assert(obj._globalScope, "object must define a _globalScope")
		-- Return an object that will try the object followed by the globalscope
		local mt = {
			__index = function(_, key)
				local result = obj[key]
				if result == nil then result = obj._globalScope[key] end
				return result
			end,
			__newindex = function(_, key, value)
				obj[key] = value
			end,
		}
		obj._memberEnv = {}
		setmetatable(obj._memberEnv, mt)
	end
	return obj._memberEnv
end

--[[**
Support for object-style tables, with class-like semantics. `class` is used to
define a Class object which may be instanciated by calling it as a function.
The objects returned by this instanciation support calling into the Class object
(via the usual metatable `__index` fallback mechanism). They also support a
few special members. The first is `init()`, which if defined will be called on
every newly-instanciated object.

	Button = class {
		-- Members with default vals go here if desired
		pos = 1,
		text = "woop"
	}

	function Button:init()
		-- Do stuff
	end

	function Button:draw()
		-- ...
	end

	button = Button() -- default args. Will call init()
	button2 = Button { pos = 2 } -- Will also call init()

The second special member is `super` which can be used to chain together a
(single-inheritance) class heirarchy.

	Checkbox = class {
		super = Button,
		checked = false,
	}

	function Checkbox:draw()
		-- Some custom stuff
		self.super.draw(self) -- and call through to super if you want
	end

Finally, there is a special member `_globalScope` which can modifies the
behaviour of the class metatable and can be used to avoid having to use the
prefix `self.` in front of member variables, when used inside a member function.
You may
continue to use the `self.member` syntax to disambiguate between a member and a
similarly-named item in the global scope, and for calling other member functions
using the `self:someFunction()` syntax.

	UnselfishButton = class {
		super = Button,
		_globalScope = _ENV,
	}

	function UnselfishButton:draw()
		-- This odd-looking syntax gives us an env that can access self
		-- variables directly
		local _ENV = -self

		print("Drawing "..text) -- no need to say self.text
		super.draw(self) -- No need to say self.super
	end

If the "`-self`" syntax looks just too horrible, you may also use:

	local _ENV = misc.memberEnv(self)
]]
function class(classObj)
	classObj.__index = function(obj, key)
		local result = classObj[key]
		if result == nil and classObj.super ~= nil then
			result = classObj.super[key]
		end
		return result
	end
	classObj.__unm = function(obj)
		return memberEnv(obj)
	end
	setmetatable(classObj, classObjMt)
	return classObj
end


-- Because we're used by the build system too, indirectly via symbolParser.lua,
-- we have to adhere by the Lua 5.2 module convention
return {
	array = array,
	class = class,
	iter = iter,
	lessThanUnsigned = lessThanUnsigned,
	roundDownUnsigned = roundDownUnsigned,
}
