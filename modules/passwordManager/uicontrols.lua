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
	hpadding = 10,
	vpadding = 7,
	edgeColour = nil,
	text = nil,
	enabled = true,
	pressed = false,
	x = 0,
	y = 0,
}

function Button:init()
	if not self.edgeColour then
		self.edgeColour = Colour.Black
	end
end

function Button:__tostring()
	return string.format("Button %q", self.text)
end

function Button:width()
	if self.fixedWidth then
		return self.fixedWidth
	else
		return self.hpadding * 2 + self.bitmap:getTextSize(self.text)
	end
end

function Button:height()
	if self.fixedHeight then
		return self.fixedHeight
	else
		local _, h = self.bitmap:getTextSize(self.text)
		return self.vpadding * 2 + h
	end
end

function Button:setWidth(w)
	self.fixedWidth = w
end

function Button:setHeight(h)
	self.fixedHeight = h
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

function Button:contentSize()
	return self.bitmap:getTextSize(self.text)
end

function Button:draw()
	local _ENV = -self
	assert(self.bitmap, "No bitmap to draw to!")
	local contentw, contenth = self:contentSize()
	local w, h
	if fixedWidth then
		w = fixedWidth
	else
		w = contentw + hpadding * 2
	end
	if fixedHeight then
		h = fixedHeight
	else
		h = contenth + vpadding * 2
	end
	local bg = pressed and textColour
		or (not enabled and disabledBackgroundColour) or backgroundColour
	bitmap:setColour(bg)
	bitmap:setBackgroundColour(bg)
	bitmap:drawRect(x, y, w, h)

	if edgeColour then
		bitmap:setColour(edgeColour)
		bitmap:drawBox(x, y, w, h)
	end

	local fg = pressed and backgroundColour
		or (not enabled and disabledTextColour) or textColour
	bitmap:setColour(fg)
	self:drawContent(contentw, contenth)
end

function Button:drawContent(w, h)
	local _ENV = -self
	local lpad = fixedWidth and (fixedWidth - w)/2 or hpadding
	local tpad = fixedHeight and (fixedHeight - h)/2 or vpadding
	-- Plus one is because text hugs the top
	bitmap:drawText(text, x + lpad, y + tpad + 1)
end

Checkbox = class {
	super = Button,
	checked = false,
}

local checkboxSize = 9
local checkboxGap = 5

function Checkbox:contentSize()
	local w, h = self.super.contentSize(self)
	w = w + checkboxSize + checkboxGap
	return w, h
end

function Checkbox:drawContent(w, h)
	local _ENV = -self
	local cbx = x + hpadding
	local cby = y + (self:height() - checkboxSize) / 2
	local b = self.bitmap

	b:drawBox(cbx, cby, checkboxSize, checkboxSize)
	if checked then
		b:drawLine(cbx, cby, cbx + checkboxSize - 1, cby + checkboxSize - 1)
		b:drawLine(cbx + checkboxSize - 1, cby, cbx, cby + checkboxSize - 1)
	end

	local tpad = fixedHeight and (fixedHeight - h)/2 or vpadding
	bitmap:drawText(text, cbx + checkboxSize + checkboxGap, y + tpad + 1)
end

function Checkbox:handleActivated()
	self.checked = not self.checked
	self:draw()
end
