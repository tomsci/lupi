require "runloop"
require "membuf"
require "bit32"

--[[**
Module to interface with the touchscreen driver in the kernel. Currently this
module is also responsible for calibrating the raw output from the touchscreen
ADC.
]]

local observer

-- Calibration data
xmin, xmax = 250, 3800
ymin, ymax = 230, 3700

-- Cached globals
tscWidth = nil
tscHeight = nil
rotated = false

buttonStates = {}

-- Constants
TouchUp = 0
TouchDown = 1
ButtonDown = 3
ButtonPressed = 4
ButtonUp = 5

--[[**
Function to take the 12-bit raw sample data from the TSC's ADC and convert to
screen coordinates. Calibration data is currently hard-coded in the
`xmin, xmax, ymin, ymax` variables, and this function also transforms the data
to be correct for the current screen orientation. Returns `screenX, screenY`.
]]
function calibratedCoords(x, y)
	if not tscWidth then
		tscWidth = lupi.getInt(lupi.ScreenWidth)
		tscHeight = lupi.getInt(lupi.ScreenHeight)
		-- TODO need to know which way we're rotated!
		rotated = tscWidth > tscHeight
		if rotated then
			-- We want to put w and h back to how the TSC sees them (ie unrotated)
			tscWidth, tscHeight = tscHeight, tscWidth
		end
	end
	-- Real basic for now
	x, y = ((x - xmin) * tscWidth) / (xmax-xmin), ((y-ymin) * tscHeight) / (ymax-ymin)
	if rotated then
		return tscHeight - y, x
	else
		return x, y
	end
end

local function dummyObserver(op, x, y)
	if op == TouchDown then
		print(string.format("gotInput %d,%d", x, y))
	elseif op == TouchUp then
		print("Touch up")
	elseif op == ButtonDown then
		print(string.format("Button %d down", x))
	elseif op == ButtonPressed then
		print(string.format("Button %d pressed", x))
	elseif op == ButtonUp then
		print(string.format("Button %d up", x))
	end
end

local function handleKeypress(buttonMask, timestamp)
	-- print(string.format("handleKeypress %x", buttonMask))
	for i = 0, 31 do
		local b = buttonStates[i]
		if not b then
			b = {}
			buttonStates[i] = b
		end
		local bset = bit32.band(buttonMask, bit32.lshift(1, i)) ~= 0
		if b.pressed then
			-- TODO autorepeat
			if not bset then
				b.pressed = false
				observer(ButtonUp, i)
			end
		else
			if bset then
				b.pressed = true
				b.t = timestamp
				observer(ButtonDown, i)
				observer(ButtonPressed, i)
			end
		end
	end
end

local function gotInput(inputRequest, numSamples)
	for i = 0, numSamples-1 do
		local offset = i*3*4
		local flags = inputRequest.data:getInt(offset)
		if flags == 2 then
			local buttonMask = inputRequest.data:getInt(offset + 4)
			local timestamp = inputRequest.data:getInt(offset + 8)
			return handleKeypress(buttonMask, timestamp)
		end
		local xy = inputRequest.data:getInt(offset + 4)
		local x = bit32.rshift(xy, 16)
		local y = bit32.band(xy, 0xFFFF)
		observer(flags, calibratedCoords(x, y))
	end
end

local function createInputRequest(bufSize)
	observer = dummyObserver
	inputRequest = newInputRequest(runloop.current, bufSize)
	-- inputRequest.requestFn is filled in by newInputRequest
	inputRequest.completionFn = gotInput
	runloop.current:queue(inputRequest)
end

-- Only used for testing
function main()
	local rl = runloop.new()
	createInputRequest()
	rl:run()
end

--[[**
The only function most clients will need to call. Creates a new input request,
adds it to the current run loop, and sets its observer to `fn`. Example usage:

	function handleInput(flags, x, y)
		if flags == 0 then
			-- Touch up event, ignore x and y
		else
			-- Do stuff with x and y
		end
	end

	input.registerInputObserver(handleInput)

There must be a run loop created before calling this function. Do not call more
than once. `bufSize` is an optional parameter used to override the default
buffer size of 128 samples.
]]
function registerInputObserver(fn, bufSize)
	assert(inputRequest == nil)
	assert(runloop.current, "Run loop must be set up before calling registerInputObserver")
	createInputRequest(bufSize)
	observer = fn or dummyObserver
end

--[[**
Create a new async request object that receives touch input data from the
kernel. If `maxSamples` is not specified it defaults to something sensible. The
data buffer the samples are read in to is available as a membuf in the `data`
member.
]]
--native function newInputRequest(runloop [, maxSamples])
