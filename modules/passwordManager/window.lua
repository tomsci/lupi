require "misc"
require "bitmap"

--[[**
`Window` objects handle input and handle laying out controls
]]

local Colour = bitmap.Colour
local class = misc.class
local array = misc.array

Window = class {
	_globalScope = _ENV,
	bitmap = nil,
	backgroundColour = Colour.Purple,
	controls = nil,
	debugDrawing = false,
	dragging = false,
	focusedPressedControl = nil,
	focusCapturedControl = nil,
}

function Window:init()
	local _ENV = -self
	if not controls then
		controls = array()
	end
	if not self.bitmap then
		self.bitmap = bitmap.create()
		if backgroundColour then
			bitmap:setColour(backgroundColour)
			bitmap:drawRect(0, 0, bitmap:getWidth(), bitmap:getHeight())
		end
	end
end

function Window:__tostring()
	if self.bitmap then
		return string.format("Window %dx%d", self.bitmap:getWidth(),
			self.bitmap:getHeight())
	else
		return "Window"
	end
end

function Window:addControl(c)
	self.controls:insert(c)
	c.bitmap = self.bitmap
	c:draw()
end

function Window:width()
	return self.bitmap:getWidth()
end

function Window:height()
	return self.bitmap:getHeight()
end

local function rng(lower, val, upper)
	return val >= lower and val < upper
end

function Window:findControlForCoords(x, y)
	-- Only consider controls with a handleActivated fn
	for _, control in self.controls:iter() do
		if control.handleActivated and
			rng(control.x, x, control.x + control:width()) and
			rng(control.y, y, control.y + control:height()) then
			return control
		end
	end
	return nil
end

function Window:gotInput(flags, x, y)
	local _ENV = -self

	local c
	if flags > 0 then
		c = self:findControlForCoords(x, y)
	else
		c = focusedPressedControl
	end
	-- print(string.format("Input %d,%d,%d c=%s fcc=%s fpc=%s dragging=%s", x, y, flags, tostring(c), tostring(focusCapturedControl), tostring(focusedPressedControl), tostring(dragging)))

	if flags == 0 then
		-- Pen up
		if c and c == focusCapturedControl then
			if c.setPressed then c:setPressed(false) end
			c:handleActivated()
		end
		dragging = false
		focusCapturedControl = nil
	elseif dragging == false then
		-- Pen down
		dragging = true
		focusCapturedControl = c
		if c and c.setPressed then
			focusedPressedControl = c
			c:setPressed(true, x, y)
		elseif debugDrawing then
			bitmap:drawRect(x, y, 1, 1)
		end
	else
		-- Dragging
		if focusCapturedControl then
			if focusedPressedControl then
				if focusedPressedControl ~= c then
					-- Dragged out of focusedPressedControl
					focusedPressedControl:setPressed(false)
					focusedPressedControl = nil
				end
			elseif focusCapturedControl == c and c.setPressed then
				-- Dragged back in to focusCapturedControl
				focusedPressedControl = c
				c:setPressed(true, x, y)
			end
		elseif debugDrawing then
			bitmap:drawRect(x, y, 1, 1)
		end
	end
	bitmap:blit()
end

function Window:redraw()
	-- This will only redraw the dirty regions
	self.bitmap:blit()
end

function Window:clear()
	local b = self.bitmap
	b:setColour(self.backgroundColour)
	b:drawRect(0, 0, b:getWidth(), b:getHeight())
	self.controls = array()
	self:redraw()
end
