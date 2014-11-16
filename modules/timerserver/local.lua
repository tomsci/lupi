--[[**
A cutdown version of timerserver.lua for use when timers are managed locally
rather than in a separate thread/process.
]]

require "misc"
require "runloop"
require "int64"

local iter, array = misc.iter, misc.array
local dbg = false

-- sorted array of { time = int64, msg = msg }
local timers = array {
	{ time = int64.MAX } -- end of list marker
}
local timerRequest

local function timerExpired(timerRequest)
	local t = lupi.getUptime()
	if dbg then print("[timers] timerExpired at "..t) end
	local completed = array()
	for _, timer, removeFromTimers in timers:iter() do
		if t >= timer.time then
			completed:insert(timer)
			removeFromTimers()
		end
	end

	-- Rerequest
	runloop.current:queue(timerRequest)
	setNewTimerCallback(timerRequest, timers[1].time)

	for _, timer in completed:iter() do
		timer.msg()
	end
end



function after(msg, msecs)
	if dbg then print("[timers] after "..msecs) end
	-- TODO we should probably resolve the targetTime client-side
	local t = { time = lupi.getUptime() + msecs, msg = msg }
	timers:insert(1, t)
	if t.time < timers[2].time then
		-- Need to tell kernel
		setNewTimerCallback(timerRequest, t.time)
	else
		timers:sort(function(l,r) return l.time < r.time end)
	end
end

function init()
	local loop = runloop.current
	assert(loop, "Must have set up a runloop before calling init()")
	timerRequest = loop:newAsyncRequest({
		completionFn = timerExpired,
	})
	loop:queue(timerRequest)
end
