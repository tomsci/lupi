local misc = require "misc"
_ENV = misc.fixupEnvIfRunByHostLua(_ENV)

function at(s, idx)
	return s:byte(idx + 1)
end

function strmid(s, pos, len)
	return s:sub(pos + 1, pos + len)
end

function rng(start, lessThanEnd)
	local function step(s, var)
		local ret = var + 1
		if ret < s then return ret
		else return nil
		end
	end
	return step, lessThanEnd, start - 1
end

function test()
	assert(at("abcde", 2) == string.byte("c"))
	assert(strmid("abcdef", 2, 3) == "cde")

	str = ""
	for i in rng(2, 5) do
		str = str .. tostring(i)
	end
	assert(str == "234")
	for i in rng(0, 0) do
		str = nil
	end
	assert(str)
	for i in rng(0, 1) do
		str = str..tostring(i)
	end
	assert(str == "2340")
end

return _ENV
