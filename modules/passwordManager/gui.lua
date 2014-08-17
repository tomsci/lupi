require "runloop"
require "input"
require "bitmap"
local uicontrols = require "passwordManager.uicontrols"
local window = require "passwordManager.window"

local Colour = bitmap.Colour
local Button = uicontrols.Button
local Window = window.Window

function main()
	local rl 
	if not runloop.current then
		rl = runloop.new()
	end

	win = Window()
	win.debugDrawing = true
	input.registerInputObserver(function (...) win:gotInput(...) end)

	but = Button {
		text = "Hello world",
		x = 10,
		y = 10,
		handleActivated = function(self)
			print("I'M A BUTTON!")
		end
	}
	win:addControl(but)

	disbut = Button {
		enabled = false,
		x = 10,
		y = 50,
		text = "I am greyed out",
	}
	win:addControl(disbut)

	win:redraw()

	if rl then
		rl:run()
	end
end
