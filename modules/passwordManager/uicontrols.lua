require "bitmap"
require "oo"

--[[**
Some basic UI controls. See [window.lua](window.lua) for the interface that all
UI controls must support in order to be rendered in a `Window`.
]]

local Colour = bitmap.Colour
local class = oo.class

--[[**
A basic momentary-push button.
]]
Button = class {
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
	keyFocus = false,
	x = 0,
	y = 0,
	fixedWidth = nil,
	fixedHeight = nil,
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

function Button:setKeyFocus(flag)
	assert(self.enabled ~= false)
	self.keyFocus = flag
	self:draw()
end

--[[**
Call to disable a button. Disabled buttons are drawn greyed out and do not
respond to input.
]]
function Button:setEnabled(flag)
	self.enabled = flag
	self:draw()
end

function Button:handleActivated()
	printf("Button %q activated", self.text)
end

function Button:contentSize()
	return self.bitmap:getTextSize(self.text)
end

function Button:draw()
	local _ENV = self + _ENV
	assert(self.bitmap, "No bitmap to draw to!")
	local contentw, contenth = self:contentSize()
	local w = fixedWidth or contentw + hpadding * 2
	local h = fixedHeight or contenth + vpadding * 2
	local bg = pressed and textColour
		or (not enabled and disabledBackgroundColour) or backgroundColour
	bitmap:setColour(bg)
	bitmap:setBackgroundColour(bg)
	bitmap:drawRect(x, y, w, h)

	if edgeColour then
		bitmap:setColour(edgeColour)
		bitmap:drawBox(x, y, w, h)
		if keyFocus then
			-- If we have a button edge, draw focus box tight around the content
			bitmap:setColour(textColour)
			bitmap:drawBox(x + (w - contentw) / 2, y + (h - contenth) / 2, contentw + 1, contenth + 1)
		end
	elseif keyFocus then
		-- If there's no button edge, and we have key focus, draw it where the
		-- edge would be
		bitmap:setColour(textColour)
		bitmap:drawBox(x, y, w, h)
	end

	local fg = pressed and backgroundColour
		or (not enabled and disabledTextColour) or textColour
	bitmap:setColour(fg)
	self:drawContent(contentw, contenth)
end

function Button:drawContent(w, h)
	local _ENV = self + _ENV
	local lpad = fixedWidth and (fixedWidth - w)/2 or hpadding
	local tpad = fixedHeight and (fixedHeight - h)/2 or vpadding
	bitmap:drawText(text, x + lpad, y + tpad)
end

--[[**
A subclass of `Button` which draws a checkbox. The current state of the checkbox
is in the member `checked` and may be set by calling
`checkbox:setChecked(boolean)`.
]]
Checkbox = class {
	_super = Button,
	checked = false,
	checkboxSize = 9,
	checkboxGap = 5,
}

function Checkbox:contentSize()
	local w, h = Button.contentSize(self)
	w = w + self.checkboxSize + self.checkboxGap
	return w, h
end

function Checkbox:drawContent(w, h)
	local _ENV = self + _ENV
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
	self:setChecked(not self.checked)
end

function Checkbox:setChecked(flag)
	self.checked = flag
	self:draw()
end
