require "ipc"

InitMsg = 1
After = 2

local TimerSession = {}

function TimerSession:__index(key)
	return TimerSession[key]
end

function connect()
	local session = ipc.connect("time")
	setmetatable(session, TimerSession)
	return session
end

--[[**
Calls `completionFn` after `msecs` milliseconds.
]]
function TimerSession:after(msecs, completionFn)
	ipc.send(self, After, {msecs}, completionFn)
end

function TimerSession:sendInitMsg(completionFn)
	ipc.send(self, InitMsg, completionFn)
end
