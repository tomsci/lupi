--[[**
This module contains the first user-side code executed after boot up.

Modify the `main()` function to do things at bootup. Modify
`commandsToRunInInterpreter` to run commands in the context of the interpreter
(so they will appear on screen and in the history) before the main interpreter
loop starts.
]]

local EValBootMode = 1 -- See exec.h

local commandsToRunInInterpreter = [[
]]

function main()
	lupi.createProcess("timerserver.server")
	local bootMode = lupi.getInt(EValBootMode)
	if bootMode == string.byte('y') then
		-- Yield tests - if it's working right, the yields should make the
		-- prints interleaved
		lupi.createProcess("test.yielda")
		lupi.createProcess("test.yieldb")
	elseif bootMode == string.byte('t') then
		lupi.createProcess("test.init")
	elseif bootMode == string.byte('b') then
		require("bitmap.tests").main()
	end
	local interpreter = require("interpreter")
	local hadPreCmd = false
	for l in commandsToRunInInterpreter:gmatch("[^\n]+") do
		if hadPreCmd == false then
			interpreter.printPrompt()
			hadPreCmd = true
		end
		print(l)
		pcall(interpreter.executeLine, l)
	end
	if hadPreCmd then
		print("") -- Otherwise we'll get two lua prompts on one line
	end
	return interpreter.main()
end
