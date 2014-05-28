require "runloop"
require "ipc"
require "timerserver"

-- TODO: plenty

local ops = {
	[timerserver.InitMsg] = function(msg)
		print("[timerserver] Got init message", msg)
		ipc.complete(msg, 0)
	end,
}

function main()
	local loop = runloop.new()
	local ret = ipc.startServer(loop, "time", ops)
	print("[timerserver] started ok", ret)
	loop:run()
end
