-- Note that Int64s should not be used as indexes into Lua tables - table indexing uses the
-- definition of raw equality, so two int64s with the same value will *not* index to the same
-- value. Convert to an int first, or something.

function Int64:__tostring()
	return string.format("%08X%08X", self:hi(), self:lo())
end

Int64.__index = Int64
