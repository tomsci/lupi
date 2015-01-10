--[[**
Graphical boot menu for the TiLDA.
]]

require "input"
require "runloop"
require "bitmap"

local uicontrols = require "passwordManager.uicontrols"

local Window = require("passwordManager.window").Window
local Colour = bitmap.Colour
local Button = uicontrols.Button

Items = {
	{ title = "Tetris", module = "tetris.tetris" },
	{ title = "???", module = "woop" },
	{ title = "Profit!", module = "woop" },
}

local function selected(button)
	lupi.replaceProcess(button.module)
end

-- Basically everything except the direction keys should behave as Select
local mappings = {
	[input.A] = input.Select,
	[input.B] = input.Select,
	[input.Start] = input.Select,
}
local function gotInput(op, x, y)
	if op == input.ButtonPressed then
		x = mappings[x] or x
	end
	win:gotInput(op, x, y)
end

function main()
	local rl = runloop.new()
	win = Window {
		backgroundColour = Colour.White,
	}
	local bmp = win.bitmap
	bmp:setRotation(90)
	-- win.debugDrawing = true
	input.registerInputObserver(gotInput)

	local y = 0
	local _, texth = bmp:getTextSize(" ")
	bmp:setColour(Colour.Black)
	local version = lupi.getString("Version")
	bmp:drawText(version, 0, y)
	y = y + texth
	bmp:drawLine(0, y, bmp:width(), y)
	y = y + 3

	-- We will simulate a list box with a bunch of buttons. Should be good
	-- enough for now, until we get enough entries to need it to scroll, at
	-- which point this will need ripping out...
	for i, item in ipairs(Items) do
		local button = Button {
			x = 0,
			y = y,
			text = item.title,
			module = item.module,
			fixedWidth = win:width(),
			hpadding = 1,
			vpadding = 2,
			handleActivated = selected,
		}
		button.edgeColour = nil
		win:addControl(button)
		y = y + button:height()
		item.button = button
	end

	win:setKeyFocus(Items[1].button)
	win.bitmap:blit()
end
