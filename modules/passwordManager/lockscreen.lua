--require "window"
require "bitmap"
require "oo"

local class = oo.class
local Colour = bitmap.Colour
local bgcol = Colour.Purple

LockSlider = class {
	bitmap = nil,
	x = 0,
	y = 0,
	values = nil,
	currentValue = 0,
}

function LockSlider:init()
	self.values = {}
end

function LockSlider:setPressed(flag, x, y)
	if not flag then
		print("LockSlider currentValue = "..self.currentValue)
	end
	self:draw()
end

function LockSlider:handleDragged(x, y)
	self.currentValue = ((x - self.x) * 10) / self:width()
	--self:draw()
end

function LockSlider:handleActivated()
	-- Don't care but have to define it
end

function LockSlider:width()
	return self.bitmap:width() - 10
end

function LockSlider:height()
	return 50
end

local knobw = 30
local knobh = 15

function LockSlider:draw()
	local _ENV = self + _ENV
	local w = self:width()
	local b = bitmap
	-- Draw slider
	b:setColour(bgcol)
	b:drawRect(x, y, w, self:height())
	b:setColour(Colour.Grey)
	b:drawLine(x, y + 25, x + w - 1, y + 25)

	-- Draw the knob thingy
	local knobx = x + ((currentValue * w) / 10)
	b:setColour(Colour.Black)
	local knoby = y + 25 - knobh/2
	b:drawBox(knobx, knoby, knobw, knobh)
	b:setColour(Colour.White)
	b:drawRect(knobx + 1, knoby + 1, knobw - 2, knobh - 2)
end


function displayLockScreen(window)
	window:clear()
	lockSlider = LockSlider()
	window:addControl(lockSlider)
	lockSlider:draw()
end
