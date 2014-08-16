require "runloop"
require "input"
require "bitmap"
local uicontrols = require "passwordManager.uicontrols"
local window = require "passwordManager.window"

local Colour = bitmap.Colour
local Button = uicontrols.Button

function main()
	local rl 
	if not runloop.current then
		rl = runloop.new()
	end

	win = window.Window() -- Well this syntax doesn't look confusing
	win.debugDrawing = true
	input.registerInputObserver(function (...) win:gotInput(...) end)
	local b = win.bitmap
	b:setColour(Colour.Purple)
	b:drawRect(0, 0, b:getWidth(), b:getHeight())
	b:blit()

	but = Button {
		text = "Hello world",
		x = 10,
		y = 10,
		handleActivated = function(self)
			print("I'M A BUTTON!")
		end
	}
	win:addControl(but)
	win:redraw()

	if rl then
		rl:run()
	end
end
