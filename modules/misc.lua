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

local sizeFail = (0x80000000 < 0)

--[[**
Rounds `val` down to the nearest multiple of `size`, considering `val` as if it
were unsigned (ie values with the top bit set are considered large positive).
]]
function roundDownUnsigned(val, size)
	-- With integer division (which is the only type we have atm)
	-- this should do the trick
	local n = val / size
	-- Ugh because Lua is compiled with 32-bit signed ints only, if val is
	-- greater than 0x8000000, then the above divide which rounds towards zero,
	-- will actually round *up* when the number is considered as unsigned
	-- Therefore, subtract one from n in this case
	if sizeFail and val < 0 then n = n - 1 end
	return n * size
end

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
	-- if debug then
	-- 	print(string.format(
	-- 		"a=%x b=%x abig=%d bbig=%d aa=%x bb=%x", a, b, abig, bbig, aa, bb))
	-- end
	return abig < bbig or aa < bb
end

local function instantiate(classObj, args)
	local obj = args or {}
	setmetatable(obj, classObj)
	if obj.init then
		obj:init()
	end
	return obj
end

local classObjMt = {
	__call = instantiate
}

function memberEnv(obj, env)
	if obj._memberEnv == nil then
		-- Return an object that will try the object followed by the globalscope
		local mt = {
			__index = function(_, key)
				local result = obj[key]
				if result == nil then result = env[key] end
				return result
			end,
			__newindex = function(_, key, value)
				obj[key] = value
			end,
		}
		local mEnv = {}
		obj._memberEnv = mEnv

		-- Finally, migrate member functions into _memberEnv
		-- Note this does assume they're all member fns taking an implicit first
		-- parameter, hmm...
		for name, fn in pairs(obj) do
			if type(fn) == "function" then
				mEnv[name] = function(...) return fn(obj, ...) end
			end
		end

		setmetatable(mEnv, mt)
	end
	return obj._memberEnv
end

-- Helper fn to make pairs(obj) return exactly the same as what classObj.__index
-- will return non-nil for
local function nextInClass(origObj, currentlyIterating, idx, seenKeys)
	local nextKey, nextVal = next(currentlyIterating, idx)
	if nextKey == nil then
		if rawequal(currentlyIterating, origObj) then
			currentlyIterating = getmetatable(origObj)
		else
			-- Iterating a classObj
			currentlyIterating = currentlyIterating._super
		end
		if currentlyIterating ~= nil then
			nextKey, nextVal = next(currentlyIterating, nil)
		end
	end
	if nextKey ~= nil then
		if seenKeys[nextKey] then
			-- We've already seen this key in more-derived class (ie this
			-- version is hidden) so we shouldn't return this in the iterator)
			return nextInClass(origObj, currentlyIterating, nextKey, seenKeys)
		end
		seenKeys[nextKey] = true
	end
	return currentlyIterating, nextKey, nextVal
end


--[[**
Support for object-style tables, with class-like semantics. `class` is
used to define a Class object which may be instantiated by calling it as a
function. `class()`, `class(nil)`, `class {}`, and `class({})` are all
equivalent. The objects returned by this instantiation support calling into the
Class object (via the usual metatable `__index` fallback mechanism). They also
support a few special operations. The first is the `init()` member function,
which if defined will be called on every newly-instantiated object.

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

The second special member is `_super` which can be used to chain together a
(single-inheritance) class hierarchy.

	Checkbox = class {
		_super = Button,
		checked = false,
	}

	local cb = Checkbox()
	cb:draw() -- Will call Button.draw(cb) if Checkbox doesn't define a draw fn

Note that `_super` should not be accessed as if it were a normal member, because
it will not behave the way you might expect super to behave when called from the
member of a class which is not the leaf of the class hierarchy. If you really
want a super variable in a member function, you must access it via the
explicitly-named class object, as in:

	function SomeClass:someFn()
		local super = SomeClass._super
		super:someFn()
	end

Instances are given a `__pairs` metamethod meaning that you can iterate over
all its members, its class members and functions, and its superclass's. As
with normal `pairs`, the order is not defined, although all object members will
come before class members. If a member is present in both the the instance and
its class or superclass, it is only returned once.

Finally, there is a a metamethod defined as a shorthand to allow you to skip
using the `self.` prefix inside member functions. The shorthand
`local _ENV = self + _ENV` is equivalent to
`local _ENV = misc.memberEnv(self, _ENV)` and can
be read as "give me a new `_ENV` which accesses both `self` and `_ENV`".
Specifying either of these in a member function means you can skip the `self.`
on any member access or member function call for the remainder of the function
(or more accurately, the remainder of the scope of the `_ENV` declaration). You
may continue to use the `self.member` syntax to disambiguate between a member
and a similarly-named item in the global scope, or if you want to call a static
member function (eg `self.someFunctionThatDoesntExpectASelfArg()`). Any
statement of the form `variable = value` will be translated to
`self.variable = value`, even if `self.variable` is currently nil and
`_ENV.variable` isn't.

	UnselfishButton = class {
		_super = Button,
	}

	function UnselfishButton:draw()
		-- Sets the _ENV to one that can access self directly
		local _ENV = self + _ENV    -- or misc.memberEnv(self, _ENV)

		print("Drawing "..text)     -- no need to say self.text
		drawContent()               -- no need to say self:drawContent()
	end

Note that for performance reasons, member functions are only checked the first
time `memberEnv()` or `self + _ENV` is called for a given object, so any
functions added to `self` or self's class after this point will not be callable
without prefixing them with `self:`.
]]
function class(classObj)
	classObj.__index = function(obj, key)
		local fallback = classObj
		local result
		while result == nil and fallback ~= nil do
			result = fallback[key]
			fallback = fallback._super
		end
		return result
	end
	classObj.__add = function(obj, env)
		return memberEnv(obj, env)
	end
	classObj.__pairs = function(obj)
		local currentlyIterating = obj
		local seenKeys = { __index = true, __add = true, __pairs = true }
		local nextFn = function(_, idx)
			local c, k, v = nextInClass(obj, currentlyIterating, idx, seenKeys)
			currentlyIterating = c
			return k, v
		end
		return nextFn, obj, nil
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
