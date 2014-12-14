--[[**
A pure-Lua implementation of Affine Transformations for use by Bitmaps in
coordinate translations.

To construct an arbitrary transform of the form:

	x' = a * x + b * y + tx
	y' = c * x + d * y + ty

do:

	t = Transform(a, b, c, d, tx, ty)

There is also a convenience constructor function for rotating systems which have
origin (0,0) rather than being centered at (0,0), which preserve the original
width and height to ensure coordinates are always appropriately transformed into
the positive x-y domain. When subsequent rotations are applied to it using
`applyToRotation()` a transform constructed in this way will automatically
recalculate appropriate tx and ty values:

	-- degrees must be a multiple of 90
	rt = RotateTransform(degrees, width, height)
	newRt = RotateTransform(270):applyToRotation(rt)
	-- newRt has tx,ty such that positive coordinates from (0,0) to
	-- (width, height) will always be translated to positive values
]]


Transform = {
	a = 1,
	b = 0,
	c = 0,
	d = 1,
	tx = 0,
	ty = 0
}

Transform.__index = Transform
local transformMt = {
	__call = function(theTransformObj, ...)
		local ret = {}
		setmetatable(ret, Transform)
		ret:set(...)
		return ret
	end
}
setmetatable(Transform, transformMt)

function RotateTransform(degrees, width, height)
	-- printf("RotateTransform(%d, %d, %d)", degrees, width, height)
	local t = Transform()
	t.width = width
	t.height = height
	t:setRotation(degrees)
	return t
end

--[[**
Transform the supplied coordinates. If `w` and `h` are not supplied, they
are assumed to be `1`. Returns 4 results: `newx, newy, neww, newh`.
]]
function Transform:transform(x, y, w, h)
	if not w then w = 1 end
	if not h then h = 1 end
	local x1 = self.a * x + self.b * y + self.tx
	local y1 = self.c * x + self.d * y + self.ty

	local x2 = self.a * (x + w) + self.b * (y + h) + self.tx
	local y2 = self.c * (x + w) + self.d * (y + h) + self.ty
	-- Some basic arithmetic means we can simplify this
	-- local x2 = x1 + self.a * w + self.b * h
	-- local y2 = y1 + self.c * w + self.d * h

	if x2 < x1 then
		x1, x2 = x2, x1
	end
	if y2 < y1 then
		y1, y2 = y2, y1
	end

	-- printf("Transform (%d,%d) %dx%d -> (%d,%d) %dx%d (%s)", x, y, w, h, x1, y1, x2-x1, y2-y1, tostring(self))
	return x1, y1, x2 - x1, y2 - y1
end

function Transform:inverse()
	return Transform(
		self.a,
		-self.b,
		-self.c,
		self.d,
		-self.tx * self.a + self.ty * self.b,
		-self.ty * self.d + self.tx * self.c
	)
end

function Transform:__eq(other)
	local l,r = self,other
	return l.a == r.a and
		l.b == r.b and
		l.c == r.c and
		l.d == r.d and
		l.tx == r.tx and
		l.ty == r.ty
end

function Transform:__tostring()
	return string.format("a=%d b=%d c=%d d=%d tx=%d ty=%d",
		self.a, self.b, self.c, self.d, self.tx, self.ty
	)
end

function Transform:set(a, b, c, d, tx, ty)
	self.a = a
	self.b = b
	self.c = c
	self.d = d
	self.tx = tx or 0
	self.ty = ty or 0
end

function Transform:get()
	return self.a, self.b, self.c, self.d, self.tx, self.ty
end

function Transform:setRotation(degrees)
	-- For a rotation, a == d and b == -c
	assert(degrees % 90 == 0, "degrees must be a multiple of 90")
	degrees = degrees % 360
	local width = self.width or 0
	local height = self.height or 0

	if degrees == 0 then
		self:set(1, 0, 0, 1)
	elseif degrees == 90 then
		self:set(0, -1, 1, 0)
	elseif degrees == 180 then
		self:set(-1, 0, 0, -1)
	elseif degrees == 270 then
		self:set(0, 1, -1, 0)
	else
		error("Non-right-angle rotation "..tostring(degrees).." not supported")
	end

	twidth, theight = self:transform(width, height, 0, 0)
	if twidth < 0 then
		self.tx = width
	end
	if theight < 0 then
		self.ty = height
	end
	-- print("twidth,theight", twidth, theight)
	-- printf("Setting tx=%d, ty=%d for rotation %d", self.tx, self.ty, degrees)
end

--[=[

--[[*
Generic matrix multiply function to combine two Affine coordinate transforms:

	[ a b tx ] [ A B TX ]   [ aA+bC aB+bD aTX+bTY+tx ]
	[ c d ty ] [ C D TX ] = [ cA+dC cB+dD cTX+dTY+ty ]
	[ 0 0  1 ] [ 0 0  1 ]   [     0     0          1 ]

Returns the resulting transform, does not modify `self` or `other`.
]]
function Transform:combine(other)
	local l, r = self, other
	return Transform(
		l.a * r.a + l.b * r.c,
		l.a * r.b + l.b * r.d,
		l.c * r.a + l.d * r.c,
		l.c * r.b + l.d * r.d,
		l.a * r.tx + l.b * r.ty + l.tx,
		l.c * r.tx + l.d * r.ty + l.ty
	)
end
]=]

-- Note that `other` must have width and height set correctly, and self need not
function Transform:applyToRotation(other)
	-- Like combine(), but assumes both transforms are 90' rotations with
	-- translations intended to keep x and y positive. Applies the
	-- scaling then recalculates the appropriate translation
	local l, r = self, other
	local result = Transform(
		l.a * r.a + l.b * r.c,
		l.a * r.b + l.b * r.d,
		l.c * r.a + l.d * r.c,
		l.c * r.b + l.d * r.d,
		0,
		0
	)
	local testx, testy = result:transform(2,2)
	local tx = testx < 0 and r.width or 0
	local ty = testy < 0 and r.height or 0
	result.tx, result.ty = tx, ty
	result.width, result.height = r.width, r.height
	return result
end
