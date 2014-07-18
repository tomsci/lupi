assert(true) -- Ie that assert itself, as something in the global table, is accessible!
assert(_ENV ~= _G)

x = 12345
assert(_G.x == nil)

local interp = require "interpreter"
assert(type(interp) == "table")
assert(_ENV.interpreter == interp)
assert(_G.interpreter == nil) -- but that we don't pollute the global namespace every time a module does a require
assert(debug.traceback ~= nil)
assert(debug.debug == nil)

require "runloop"
function main(...)

	print("I'm still special!\n")
	runloop.new()
	local timerserver = require("timerserver")
	print("Connecting...")
	local t = timerserver.connect()
	print("Test is connected to server")

	t:sendInitMsg(function(result)
		print("Test got response, yay", result);
		t:after(5000, function() print("[test] after completed!") end)
	end)
	runloop.run()
end

