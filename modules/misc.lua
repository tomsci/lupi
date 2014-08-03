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
		t:insert(2, "b") -- equivalent to table.insert(2, "b")
		for k,v,del in t:iter() do -- equivalent to common.iter(t)
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

-- Because we're used by the build system too, indirectly via symbolParser.lua,
-- we have to adhere by the Lua 5.2 module convention
return {
	lessThanUnsigned = lessThanUnsigned,
	roundDownUnsigned = roundDownUnsigned,
}
