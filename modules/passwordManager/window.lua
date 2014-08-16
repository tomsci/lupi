require "misc"
require "bitmap"

--[[**
`Window` objects handle input and handle laying out controls
]]

local Colour = bitmap.Colour
local class = misc.class
local array = misc.array

Window = class {
	bitmap = nil,
	controls = nil,
	debugDrawing = false,
	dragging = false,
	focusedPressedControl = nil,
	focusCapturedControl = nil,
}

function Window:init()
	if not self.controls then
		self.controls = array()
	end
	if not self.bitmap then
		self.bitmap = bitmap.create()
		self.bitmap:setBackgroundColour(Colour.White)
	end
end

function Window:__tostring()
	return string.format("Window %dx%d", self.bitmap:getWidth(),
		self.bitmap:getHeight())
end

function Window:addControl(c)
	self.controls:insert(c)
	c.bitmap = self.bitmap
	c:draw()
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
	local c
	if flags > 0 then
		c = self:findControlForCoords(x, y)
	else
		c = self.focusedPressedControl
	end
	-- print(string.format("Input %d,%d,%d c=%s fcc=%s fpc=%s", x, y, flags,
	-- 	tostring(c), tostring(self.focusCapturedControl), tostring(self.focusedPressedControl)))

	if flags == 0 then
		-- Pen up
		if c and c == self.focusCapturedControl then
			if c.setPressed then c:setPressed(false) end
			c:handleActivated()
		end
		self.dragging = false
		self.focusCapturedControl = nil
	elseif self.dragging == false then
		-- Pen down
		self.dragging = true
		self.focusCapturedControl = c
		if c and c.setPressed then
			self.focusedPressedControl = c
			c:setPressed(true, x, y)
		elseif self.debugDrawing then
			self.bitmap:drawRect(x, y, 1, 1)
		end
	else
		-- Dragging
		if self.focusCapturedControl then
			if self.focusedPressedControl then
				if self.focusedPressedControl ~= c then
					-- Dragged out of focusedPressedControl
					self.focusedPressedControl:setPressed(false)
					self.focusedPressedControl = nil
				end
			elseif self.focusCapturedControl == c and c.setPressed then
				-- Dragged back in to focusCapturedControl
				self.focusedPressedControl = c
				c:setPressed(true, x, y)
			end
		elseif self.debugDrawing then
			self.bitmap:drawRect(x, y, 1, 1)
		end
	end
	self.bitmap:blit()
end

function Window:redraw()
	-- This will only redraw the dirty regions
	self.bitmap:blit()
end
