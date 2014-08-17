-- Bitmap object setup by native code

create = Bitmap.create -- For convenience

function Bitmap:__tostring()
	return string.format("Bitmap %dx%d", self:getWidth(), self:getHeight())
end

--[[**
Create a new bitmap.
]]
--native function Bitmap.create(width, height)

--[[**
Get height of the bitmap.
]]
--native function Bitmap:getHeight()

--[[**
Get width of the bitmap.
]]
--native function Bitmap:getWidth()

--[[**
Gets the current foreground (pen) colour.
]]
--native function Bitmap:getColour()

--[[**
Sets the current foreground (pen) colour for future drawing operations.
`colour` can be either an integer (in 16-bit 565 format) or a symbolic constant
like `Colour.Red`.
]]
--native function Bitmap:setColour(colour)

--[[**
Gets the current background colour.
]]
--native function Bitmap:getBackgroundColour()

--[[**
Like `setColour()` but sets the background colour, which is only used when
drawing text.
]]
--native function Bitmap:setBackgroundColour(colour)

--[[**
Draw a solid rectangle using the current foreground colour.
]]
--native function Bitmap:drawRect(x, y, w, h)

--[[**
Draw text in the current foreground colour. The non-text pixels within the text
rect will be set to the background colour. Instead of passing x and y as
separate parameters, you may pass an array with two entries, for example:

	local pos = { x, y }
	bmp:drawText(pos, text)
]]
--native function Bitmap:drawText(x, y, text)

--[[**
Returns the width and height in pixels of the specified text, if it were to be
rendered into this bitmap with the current settings.

	local width, height = bmp:getTextSize("Some text")
]]
--native function Bitmap:getTextSize(text)

--[[**
Draw a line in current foreground colour. You may also pass arrays for the two
points, for example:

	local start = { x0, y0 }
	local end = { x1, y1 }
	bmp:drawLine(start, end)
]]
--native function Bitmap:drawLine(x0, y0, x1, y1)

--[[**
Blit the bitmap to the screen device. If no parameters are supplied, only blits
the invalidated region. The invalidated region is the union of all rects that
have been drawn to since the last call to `blit()`.
]]
--native function Bitmap:blit([x [,y [,w [,h]]]])

local function clamp(n)
	if n < 0 then return 0
	elseif n > 255 then return 255
	else return n
	end
end

local math = math or { floor = function(x) return x end } -- HACK!

--[[**
Converts RGB into a colour value suitable for passing to `Bitmap:setColour()`.
`r`, `g` and `b` must be in the range 0-255.
]]
function rgbColour(r, g, b)
	r = clamp(r) / 8 -- 8 bits to 5 bits
	g = clamp(g) / 4 -- 8 bits to 6 bits
	b = clamp(b) / 8 -- 8 bits to 5 bits
	return math.floor(r) * 2048 + math.floor(g) * 32 + math.floor(b)
end

Colour = {
	Black = rgbColour(0, 0, 0),
	White = rgbColour(0xFF, 0xFF, 0xFF),
	Red = rgbColour(0xFF, 0, 0),
	Green = rgbColour(0, 0xFF, 0),
	Blue = rgbColour(0, 0, 0xFF),
	Purple = 0xE01F,
}
