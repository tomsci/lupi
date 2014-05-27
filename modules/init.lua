--[[**
This module contains the first user-side code executed after boot up.

Modify its main function to do things at bootup.
]]

function main()
	lupi.createProcess("test")
	return require("interpreter").main()
end
