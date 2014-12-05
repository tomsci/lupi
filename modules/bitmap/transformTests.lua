require("transform")

function printf(...) print(string.format(...)) end

function assertEq(l, r)
	if l ~= r then
		print("l = "..tostring(l))
		print("r = "..tostring(r))
		error("Comparison failed", 2)
	end
end

w = 128
h = 64

rotate90 = RotateTransform(90, w, h)
rotate180 = RotateTransform(180, w, h)
rotate270 = RotateTransform(270, w, h)
rotate0 = RotateTransform(0, w, h)

base = rotate90

brickRotate = rotate180
overall = brickRotate:applyToRotation(base)
assertEq(overall, rotate270)

brickRotate = rotate90
overall = brickRotate:applyToRotation(base)
assertEq(overall, rotate180)

brickRotate = rotate270
overall = brickRotate:applyToRotation(base)
assertEq(overall, rotate0)

-- Base rotation of the 's' piece is as below:
-- | ^^|
-- |^^ |
sSolid = {
	{ false, true, true },
	{ true, true, false },
}

sWidth = 3
sHeight = 2
sRotate90 = RotateTransform(90, sWidth, sHeight)
sRotate180 = sRotate90:applyToRotation(sRotate90)
sRotate270 = sRotate180:applyToRotation(sRotate90)

function solidAt(x, y, rot)
	local basex, basey = rot:transform(x, y)
	local result = sSolid[basey + 1][basex + 1]
	-- if rot == sRotate270 then
	-- 	print(tostring(rot))
	-- 	print(x, y, basex, basey, result)
	-- end
	return result
end

assertEq(solidAt(2, 1, rotate0), false)
assertEq(solidAt(2, 0, rotate0), true)

assertEq(sRotate270, RotateTransform(270, sWidth, sHeight))

-- |> |
-- |>>|
-- | >|
assertEq(solidAt(0, 0, sRotate90), true)
assertEq(solidAt(0, 1, sRotate90), true)
assertEq(solidAt(0, 2, sRotate90), false)

-- | vv|
-- |vv |
assertEq(solidAt(2, 1, sRotate180), false)
assertEq(solidAt(2, 0, sRotate180), true)

-- |< |
-- |<<|
-- | <|
-- assertEq(solidAt(0, 0, sRotate270), true)
assertEq(solidAt(0, 1, sRotate270), true)
assertEq(solidAt(0, 2, sRotate270), false)



