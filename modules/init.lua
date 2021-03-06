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

local function doMain()
	collectgarbage("setpause", 125) -- Start cycle when mem usage gets to 125% of prev
	collectgarbage("setstepmul", 250) -- Slightly more agressive than the default 200

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
	elseif bootMode == 3 then
		return lupi.createProcess("passwordManager.textui")
	elseif bootMode == 4 then
		lupi.createProcess("passwordManager.gui")
	elseif bootMode == 5 then
		local tetris = require("tetris")
		return tetris.main()
	elseif bootMode == 6 then
		require("bootMenu").main()
	elseif bootMode == string.byte('m') then
		require("test.memTests").test_mem()
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
	collectgarbage()
	return interpreter.main()
end

function main()
	local ok, err = xpcall(doMain, debug.traceback)
	if not ok then
		print("Unable to perform init action")
		print(err)
	end
end
