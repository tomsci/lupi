require "misc"
require "runloop"
require "ipc"
require "timerserver"
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
	local completed = array()
	for _, timer, removeFromTimers in timers:iter() do
		if t >= timer.time then
			completed:insert(timer)
			removeFromTimers()
		end
	end

	for _, timer in completed:iter() do
		ipc.complete(timer.msg, 0)
	end

	-- Rerequest
	runloop.current:queue(timerRequest)
	setNewTimerCallback(timerRequest, timers[1].time)
end

local function after(msg, msecs)
	if dbg then print("[timerserver] after "..msecs) end
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

local ops = {
	[timerserver.InitMsg] = function(msg)
		if dbg then print("[timerserver] Got init message", msg) end
		ipc.complete(msg, 0)
	end,
	[timerserver.After] = after,
}

function main()
	local loop = runloop.new()
	timerRequest = loop:newAsyncRequest({
		completionFn = timerExpired,
	})
	loop:queue(timerRequest)

	local ret = ipc.startServer(loop, "time", ops)
	if dbg then print("[timerserver] started ok", ret) end
	loop:run()
end
