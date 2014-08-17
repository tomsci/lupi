require "input"
require "bitmap"
require "misc"

local Colour = bitmap.Colour
local class = misc.class

Button = class {
	_globalScope = _ENV,
	bitmap = nil,
	backgroundColour = Colour.White,
	disabledBackgroundColour = bitmap.rgbColour(0xCC, 0xCC, 0xCC),
	textColour = Colour.Black,
	disabledTextColour = bitmap.rgbColour(0x33, 0x33, 0x33),
	padding = 10,
	edgeColour = Colour.Black,
	text = nil,
	enabled = true,
	pressed = false,
	x = 0,
	y = 0,
}

function Button:__tostring()
	return string.format("Button %q", self.text)
end

function Button:width()
	return self.padding * 2 + self.bitmap:getTextSize(self.text)
end

function Button:height()
	local _, h = self.bitmap:getTextSize(self.text)
	return self.padding * 2 + h
end

function Button:setPressed(flag)
	--print(tostring(self).." pressed "..tostring(flag))
	if not self.enabled then return end
	self.pressed = flag
	self:draw()
end

function Button:setEnabled(flag)
	self.enabled = flag
	self:draw()
end

function Button:handleActivated()
	print(string.format("Button %q activated", self.text))
end

function Button:draw()
	local _ENV = -self
	assert(self.bitmap, "No bitmap to draw to!")
	local w, h = bitmap:getTextSize(text)
	w = w + padding * 2
	h = h + padding * 2
	local x, y = x, y
	local bg = pressed and textColour or
		not enabled and disabledBackgroundColour or
		backgroundColour
	bitmap:setColour(bg)
	bitmap:setBackgroundColour(bg)
	bitmap:drawRect(x, y, w, h)

	bitmap:setColour(edgeColour)
	bitmap:drawLine(x, y, x + w, y)
	bitmap:drawLine(x + w, y, x + w, y + h)
	bitmap:drawLine(x, y + h, x + w, y + h)
	bitmap:drawLine(x, y, x, y + h)

	local fg = pressed and backgroundColour or
		not enabled and disabledTextColour or
		textColour
	bitmap:setColour(fg)
	-- Plus one is because text hugs the top
	bitmap:drawText(text, x + padding, y + padding + 1)
end
