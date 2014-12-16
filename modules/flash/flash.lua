require "bit32"
local timers = require "timerserver.local"

FlashErase = 1
FlashStatus = 2
FlashRead = 3

handle = lupi.driverConnect('FLSH')

local function checkEraseCompletion()
	local status = lupi.driverCmd(handle, FlashStatus)
	printf("status=%d", status)
	if bit32.band(status, 1) == 0 then
		print("Erase completed")
	else
		timers.after(checkEraseCompletion, 1000)
	end
end

function erase()
	lupi.driverCmd(handle, FlashErase)
	timers.after(checkEraseCompletion, 1000)
end

--native function doReadTest(handle)
--native function doWriteTest(handle)

-- TESTS
function read()
	return doReadTest(handle, 0)
end

function write()
	doWriteTest(handle, 0x20)
end

function getStatus()
	return lupi.driverCmd(handle, FlashStatus)
end