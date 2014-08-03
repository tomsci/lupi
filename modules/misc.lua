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
