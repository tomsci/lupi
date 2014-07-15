--[[**
This module contains the first user-side code executed after boot up.

Modify the main() function to do things at bootup.
]]

local EValBootMode = 1 -- See exec.h

function main()
	lupi.createProcess("timerserver.server")
	local bootMode = lupi.getInt(EValBootMode)
	if bootMode == string.byte('y') then
		-- Yield tests - if it's working right, the yields should make the
		-- prints interleaved
		lupi.createProcess("test.yielda")
		lupi.createProcess("test.yieldb")
	end
	--lupi.createProcess("test")
	return require("interpreter").main()
end
