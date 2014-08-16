require "input"
require "bitmap"
require "misc"

local Colour = bitmap.Colour
local class = misc.class

Button = class {
	bitmap = nil,
	backgroundColour = Colour.White,
	textColour = Colour.Black,
	padding = 10,
	edgeColour = Colour.Black,
	text = nil,
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
	self.pressed = flag
	self:draw()
end

function Button:handleActivated()
	print(string.format("Button %q activated", self.text))
end

function Button:draw()
	assert(self.bitmap, "No bitmap to draw to!")
	local bmp = self.bitmap
	local w, h = bmp:getTextSize(self.text)
	w = w + self.padding * 2
	h = h + self.padding * 2
	local x, y = self.x, self.y
	local bg = self.pressed and self.textColour or self.backgroundColour
	bmp:setColour(bg)
	bmp:setBackgroundColour(bg)
	bmp:drawRect(x, y, w, h)

	bmp:setColour(self.edgeColour)
	bmp:drawLine(x, y, x + w, y) 
	bmp:drawLine(x + w, y, x + w, y + h) 
	bmp:drawLine(x, y + h, x + w, y + h) 
	bmp:drawLine(x, y, x, y + h) 

	bmp:setColour(self.pressed and self.backgroundColour or self.textColour)
	-- Plus one is because text hugs the top
	bmp:drawText(x + self.padding, y + self.padding + 1, self.text)
end
