require "bit32"
local timers = require "timerserver.local"
require "runloop"

FlashErase = 1
FlashStatus = 2
FlashRead = 3

handle = lupi.driverConnect('FLSH')

function erase()
	lupi.driverCmd(handle, FlashErase)
	waitForWriteComplete(1000)
end

--native function doReadTest(handle)
--native function doWriteTest(handle)

-- TESTS
function read()
	return doReadTest(handle, 0)
end

function writeTestData()
	doWriteTest(handle, 0x20)
end

function getStatus()
	return lupi.driverCmd(handle, FlashStatus)
end

function waitForWriteComplete(pollTime)
	local stopper = {}
	local function checkWriteCompletion()
		local status = lupi.driverCmd(handle, FlashStatus)
		if bit32.band(status, 1) == 0 then
			stopper.exit = true
		else
			timers.after(checkWriteCompletion, pollTime)
		end
	end
	collectgarbage()
	timers.after(checkWriteCompletion, 2)
	runloop.current:run(stopper)
end

--[[**
If `INCLUDE_TETRISDATA` is defined in flash.c, this function writes a chunk of
data from the ROM to the secondary flash chip.
]]
--native function writePageFromData(handle, offset)

function writeData()
	local offset = 0
	local moreData = true
	while moreData do
		moreData = writePageFromData(handle, offset)
		-- Check every 2ms, spec sheet says takes up to 5ms
		waitForWriteComplete(2)
		offset = offset + 256
	end
	print("*** Done ***")
end
