require "misc"
require "oo"
require "bitmap"
require "input"

--[[**
`Window` objects handle [input][], focus and laying out controls.

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
call to `draw()`. They should not call bitmap:blit() however, it is the
responsibility of the Window or higher-level framework to do that.

In addition, the following optional functions may be implemented by controls
that can receive input:

* `control:handleActivated()`: Called when a touch press event (a touch down
  followed by a touch up) occurs within the control, or if the control is
  highlighted then activated via an `input.ButtonPressed` event for the key
  `input.Select`.
* `control:setPressed(bool)`: Called with argument `true` when a touch down
  event occurs within the control's bounds. May be called repeatedly if the
  touch drags in and out of the control. Only called if control also implements
  `handleActivated()`.
* `control:handleDragged(x, y)`: Called after a `setPressed(true)` each time
  a new down touch even occurs. Only called if control also implements
  `handleActivated()`.
* `control:setKeyFocus(bool)`: Called when a control gains or loses keyboard
  focus. Controls are only eligible for key focus if they are not disabled and
  also implement `handleActivated()`. Only one control in a given window can
  have focus at any given time.

[input]: ../input/input.lua

]]

local Colour = bitmap.Colour
local class = oo.class
local array = misc.array

Window = class {
	bitmap = nil,
	backgroundColour = Colour.Purple,
	controls = nil,
	debugDrawing = false,
	dragging = false,
	focusedPressedControl = nil,
	focusCapturedControl = nil,
	keyFocusedControl = nil,
}

function Window:init()
	local _ENV = self + _ENV
	if not self.controls then
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

local function sendControlEvent(obj, fnName, ...)
	local fn = obj[fnName]
	-- It's ok if a control doesn't implement a given event, and thus if fn is
	-- nil
	if fn == nil then return end
	-- if type(fn) ~= "function" then return end
	local ok, err = xpcall(fn, debug.traceback, obj, ...)
	if not ok then
		print(err)
	end
end

function Window:findControlForCoords(x, y)
	-- Only consider controls with a handleActivated fn
	for _, control in self.controls:iter() do
		if control.handleActivated and
			control.enabled ~= false and
			rng(control.x, x, control.x + control:width()) and
			rng(control.y, y, control.y + control:height()) then
			return control
		end
	end
	return nil
end

local function controlCentre(c)
	return c.x + c:width() / 2, c.y + c:height() / 2
end

local function controlEligibleForFocus(c)
	return c.setKeyFocus ~= nil
		and c.handleActivated ~= nil
		and c.enabled ~= false
end

-- These fns return true if (cx, cy) is in the specified quadrant relative to
-- the point (x, y).

function TopQuadrant(x, y, cx, cy)
	-- Below both lines (as y grows downwards)
	return cy - cx <= y - x and cy + cx <= y + x
end

function LeftQuadrant(x, y, cx, cy)
	-- Above rising line, below falling line
	return cy - cx >= y - x and cy + cx <= y + x
end

function RightQuadrant(x, y, cx, cy)
	-- Below rising line, above falling line
	return cy - cx <= y - x and cy + cx >= y + x
end

function BottomQuadrant(x, y, cx, cy)
	-- Above rising line, above falling line
	local result = cy - cx >= y - x and cy + cx >= y + x
	-- printf("BottomQuadrant: (%d,%d) is below (%d,%d): %s", cx, cy, x, y, tostring(result))
	return result
end

--[[**
Return a positive integer representing the distance from (x,y) to the specified
control. If `control` is not in the given quadrant, relative to x and y, then
return nil. The four quadrants around a point X look like:

	\ T /
	 \ /
	L X R
	 / \
	/ B \
]]
function Window:getDistanceToControl(x, y, quadrant, control)
	local cx, cy = controlCentre(control)
	if quadrant(x, y, cx, cy) then
		local dx, dy = x - cx, y - cy
		return dx * dx + dy * dy
	else
		return nil
	end
end

local dirToQuadrant = {
	[input.Up] = TopQuadrant,
	[input.Down] = BottomQuadrant,
	[input.Left] = LeftQuadrant,
	[input.Right] = RightQuadrant,
}

local FAR_AWAY = 0x7FFFFFFF

function Window:moveKeyFocus(direction)
	-- direction must be one of input.{Up,Down,Left,Right}
	local x, y
	if self.keyFocusedControl then
		-- Start at the centre of the currently focussed control
		x, y = controlCentre(self.keyFocusedControl)
	elseif direction == input.Up then
		-- Start at bottom middle
		x = self.bitmap:width() / 2
		y = self.bitmap:height()
	elseif direction == input.Down then
		-- Start at top middle
		x = self.bitmap:width() / 2
		y = 0
	elseif direction == input.Left then
		-- Start at centre right
		x = self.bitmap:width()
		y = self.bitmap:height() / 2
	elseif direction == input.Right then
		-- Start at centre left
		x = 0
		y = self.bitmap:height() / 2
	end
	local quadrant = assert(dirToQuadrant[direction])
	-- Find nearest control in the appropriate direction
	local nearestControl = nil
	local nearestControlDistance = FAR_AWAY
	local distanceFn = self.getDistanceToControl
	for i, c in self.controls:iter() do
		if c ~= self.keyFocusedControl and controlEligibleForFocus(c) then
			local distance = distanceFn(self, x, y, quadrant, c) or FAR_AWAY
			if distance < nearestControlDistance then
				nearestControlDistance = distance
				nearestControl = c
			end
		end
	end
	if nearestControl then
		-- printf("Moving focus from (%d,%d) to control %s", x, y, tostring(nearestControl))
		self:setKeyFocus(nearestControl)
	end
end

--[[**
Set keyboard focus in the window to the given control. Any previously-focussed
control will receive a call to `setKeyFocus(false)`. `control` must implmenent
the `setKeyFocus(bool)` function. If `control` is nil, removes focus from any
previous control.
]]
function Window:setKeyFocus(control)
	assert(control == nil or controlEligibleForFocus(control), string.format("Control %q not focussable", tostring(control)))
	if self.keyFocusedControl == control then return end
	if self.keyFocusedControl then
		sendControlEvent(self.keyFocusedControl, "setKeyFocus", false)
	end
	self.keyFocusedControl = control
	if control then
		sendControlEvent(control, "setKeyFocus", true)
	end
end

function Window:gotInput(op, x, y)
	local _ENV = self + _ENV

	local c
	if op == input.TouchDown then
		c = findControlForCoords(x, y)
	elseif op == input.TouchUp then
		c = focusedPressedControl
	elseif op == input.ButtonPressed then
		if x == input.Select and keyFocusedControl ~= nil then
			sendControlEvent(keyFocusedControl, "handleActivated")
		elseif dirToQuadrant[x] ~= nil then
			-- It's a direction key
			moveKeyFocus(x)
		end
		bitmap:blit()
		return
	elseif op == input.ButtonDown or op == input.ButtonUp then
		-- Not bothered
		return
	else
		-- Something we don't know how to handle
		print("Got unknown input:", op, x, y)
		return
	end
	-- printf("Input %d,%d,%d c=%s fcc=%s fpc=%s dragging=%s", x, y, op, tostring(c), tostring(focusCapturedControl), tostring(focusedPressedControl), tostring(dragging)))

	if op == input.TouchUp then
		-- Pen up
		if c and c == focusCapturedControl then
			sendControlEvent(c, "setPressed", false)
			sendControlEvent(c, "handleActivated")
		end
		dragging = false
		focusCapturedControl = nil
	elseif dragging == false then
		-- Pen down
		dragging = true
		focusCapturedControl = c
		if c and c.setPressed then
			focusedPressedControl = c
			sendControlEvent(c, "setPressed", true, x, y)
		elseif debugDrawing then
			bitmap:drawRect(x, y, 1, 1)
		end
	else
		-- Dragging
		if focusCapturedControl then
			if focusedPressedControl then
				if focusedPressedControl ~= c then
					-- Dragged out of focusedPressedControl
					sendControlEvent(focusedPressedControl, "setPressed", false)
					focusedPressedControl = nil
				elseif focusedPressedControl.handleDragged then
					sendControlEvent(focusedPressedControl, "handleDragged", x, y)
				end
			elseif focusCapturedControl == c and c.setPressed then
				-- Dragged back in to focusCapturedControl
				focusedPressedControl = c
				sendControlEvent(c, "setPressed", true, x, y)
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
