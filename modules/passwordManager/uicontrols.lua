require "input"
require "bitmap"
require "misc"

--[[**
Some basic UI controls.

The convention for controls being rendered by a [window](window.html)
is they must support the following members and member functions:

* `x`, `y`: Coordinates of top left of control
* `bitmap`: The bitmap the control will render in to. Must be set before calling
  `draw()`.
* `control:draw()`: Called by the window to render the control.
* `control:width()`: Should return the width of the control, that is the extent
  it will draw to in `bitmap`. Will only be called once `bitmap` is set.
* `control:height()`: As per width().

Controls may draw into their bitmap whenever they wish, for example when their
state changes, but should always fully render their contents as a result of a
call to `draw()`.

In addition, the following optional functions may be implemented by controls
that can receive input:

* `control:handleActivated()`: Called when a touch even occurs within the
  control.
* `control:setPressed(bool)`: Called with argument `true` when a touch down
  event occurs within the control's bounds. May be called repeatedly if the
  touch drags in and out of the control. Only called if control also implements
  `handleActivated()`.
]]

local Colour = bitmap.Colour
local class = misc.class

--[[**
A basic momentary-push button.
]]
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

--[[**
Call to disable a button. Disabled buttons are drawn greyed out and do not
respond to input.
]]
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

--[[**
A subclass of Button which draws a checkbox. The current state of the checkbox
is in the member `checked` and may be set by calling
`checkbox:setChecked(flag)`.
]]
Checkbox = class {
	super = Button,
	checked = false,
	checkboxSize = 9,
	checkboxGap = 5,
}

function Checkbox:contentSize()
	local w, h = self.super.contentSize(self)
	w = w + self.checkboxSize + self.checkboxGap
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
	self:setChecked(not self.checked)
end

function Checkbox:setChecked(flag)
	self.checked = flag
	self:draw()
end
