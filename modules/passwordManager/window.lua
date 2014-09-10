require "misc"
require "bitmap"

--[[**
`Window` objects handle input and handle laying out controls.

The convention for controls being rendered by a `Window` is they must support
the following members and member functions:

* `x`, `y`: Coordinates of top left of control
* `bitmap`: The bitmap the control will render in to. Must be set before calling
  `draw()`.
* `control:draw()`: Called by the window to render the control.
* `control:width()`: Should return the width of the control, that is the extent
  it will draw to in `bitmap`. Will only be called once `bitmap` is set.
* `control:height()`: As per width().
* `enabled`: if exactly equal to `false`, controls will not be eligible for
  touch events even if they implement `handleActivated` etc.

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
* `control:handleDragged(x, y)`: Called after a `setPressed(true)` each time
  a new down touch even occurs. Only called if control also implements
  `handleActivated()`.
]]

local Colour = bitmap.Colour
local class = misc.class
local array = misc.array

Window = class {
	bitmap = nil,
	backgroundColour = Colour.Purple,
	controls = nil,
	debugDrawing = false,
	dragging = false,
	focusedPressedControl = nil,
	focusCapturedControl = nil,
}

function Window:init()
	local _ENV = self + _ENV
	if not controls then
		controls = array()
	end
	if not self.bitmap then
		self.bitmap = bitmap.create()
		if backgroundColour then
			bitmap:setColour(backgroundColour)
			bitmap:drawRect(0, 0, bitmap:width(), bitmap:height())
		end
	end
end

function Window:__tostring()
	if self.bitmap then
		return string.format("Window %dx%d", self.bitmap:width(),
			self.bitmap:height())
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
	return self.bitmap:width()
end

function Window:height()
	return self.bitmap:height()
end

local function rng(lower, val, upper)
	return val >= lower and val < upper
end

local function sendControlEvent(fn, ...)
	-- It's ok if a control doesn't implement a given event, and thus if fn is
	-- nil
	if fn == nil then return end
	-- if type(fn) ~= "function" then return end
	local ok, err = xpcall(fn, debug.traceback, ...)
	if not ok then
		print(err)
	end
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
	local _ENV = self + _ENV

	local c
	if flags > 0 then
		c = findControlForCoords(x, y)
	else
		c = focusedPressedControl
	end
	-- print(string.format("Input %d,%d,%d c=%s fcc=%s fpc=%s dragging=%s", x, y, flags, tostring(c), tostring(focusCapturedControl), tostring(focusedPressedControl), tostring(dragging)))

	if flags == 0 then
		-- Pen up
		if c and c == focusCapturedControl then
			sendControlEvent(c.setPressed, c, false)
			if c.enabled ~= false then
				-- If enabled is nil or true, we send the event
				sendControlEvent(c.handleActivated, c)
			end
		end
		dragging = false
		focusCapturedControl = nil
	elseif dragging == false then
		-- Pen down
		dragging = true
		focusCapturedControl = c
		if c and c.setPressed then
			focusedPressedControl = c
			sendControlEvent(c.setPressed, c, true, x, y)
		elseif debugDrawing then
			bitmap:drawRect(x, y, 1, 1)
		end
	else
		-- Dragging
		if focusCapturedControl then
			if focusedPressedControl then
				if focusedPressedControl ~= c then
					-- Dragged out of focusedPressedControl
					sendControlEvent(focusedPressedControl.setPressed, focusedPressedControl, false)
					focusedPressedControl = nil
				elseif focusedPressedControl.handleDragged then
					sendControlEvent(focusedPressedControl.handleDragged, focusedPressedControl, x, y)
				end
			elseif focusCapturedControl == c and c.setPressed then
				-- Dragged back in to focusCapturedControl
				focusedPressedControl = c
				sendControlEvent(c.setPressed, c, true, x, y)
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
	b:drawRect(0, 0, b:width(), b:height())
	self.controls = array()
end
