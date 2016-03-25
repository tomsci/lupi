require "bitmap"
require "runloop"
require "input"
require "misc"
local timers = require "timerserver.local"
local trans = require "bitmap.transform"

local Transform, RotateTransform = trans.Transform, trans.RotateTransform
local iter = misc.iter

local blockw, blockh = 6, 6
local inset = 2

-- These coordinates correspond to where the bricks are located in tetris.xbm
-- They consist of 2 pairs of numbers: the first describing the x position and
-- width of the top block(s) of the brick; and the second describing the bottom
-- block(s). For the line (which is solely on the top row), the second pair has
-- a width of zero.
bricks = {
	O = { { 0, 2 }, { 0, 2 } },
	J = { { 2, 1 }, { 2, 3 } },
	I = { { 3, 4 }, { 3, 0 } },
	L = { { 7, 1 }, { 5, 3 } },
	S = { { 9, 2 }, { 8, 2 } },
	T = { { 11, 1 }, { 10, 3 } },
	Z = { { 12, 2 }, { 13, 2 } },
}

brickList = { bricks.O, bricks.J, bricks.I, bricks.L, bricks.S, bricks.T, bricks.Z }

-- Returns the top left index of brick in the xbm
function xOriginForBrick(brick)
	local firstRect = brick[1]
	local secondRect = brick[2]
	local baseX = firstRect[1]
	local xdelta = secondRect[1] - firstRect[1]
	if xdelta < 0 then
		-- First rect isn't leftmost part of the brick
		baseX = baseX + xdelta
	end
	return baseX
end

-- blockX and blockY are baseRotated block coords
function drawBrick(brick, blockX, blockY, rotation)
	local baseX = blockX * blockw
	local baseY = blockY * blockh

	local firstRect = brick[1]
	local secondRect = brick[2]
	local xdelta = secondRect[1] - firstRect[1]
	local firstOffset, secondOffset = 0, xdelta * blockw
	if xdelta < 0 then
		-- First rect isn't leftmost part of the brick, so fix offsets
		firstOffset = -secondOffset
		secondOffset = 0
	end

	local x1, y1 = rotation:transform(baseX, baseY, 0, 0)
	-- printf("Block coords %d,%d -> base coords (%d,%d) -> bmp coords (%d,%d)", blockX, blockY, baseX, baseY, x1, y1)

	bmp:drawXbm(xbm, x1 + firstOffset, y1, firstRect[1] * blockw, 0, firstRect[2] * blockw, blockh)
	if secondRect[2] > 0 then
		bmp:drawXbm(xbm, x1 + secondOffset, y1 + blockh,
			secondRect[1] * blockw, blockh, secondRect[2] * blockw, blockh)
	end
end

local function min(a, b) return b < a and b or a end
local function max(a, b) return b > a and b or a end

-- These assume brick is non-rotated, ie in same orientation as in tetris.xbm
function brickWidth(brick)
	local firstRect = brick[1]
	local secondRect = brick[2]
	local minx = min(firstRect[1], secondRect[1])
	local maxx = max(firstRect[1] + firstRect[2], secondRect[1] + secondRect[2])
	return maxx - minx
end

function brickHeight(brick)
	return brick[2][2] > 0 and 2 or 1
end

function brickSizeInPixels(brick)
	return brickWidth(brick) * blockw, brickHeight(brick) * blockh
end

function brickSize(brick)
	return brickWidth(brick), brickHeight(brick)
end

function brickSolidAt(brick, x, y)
	assert(y == 0 or y == 1, "y ("..tostring(y)..") must be 0 or 1")
	-- y must be either 0 or 1 so can be used to index into brick (which is one-based hence the +1)
	local minx = min(brick[1][1], brick[2][1])
	local rect = brick[y + 1]
	return rect[2] > 0 and x >= rect[1] - minx and x < rect[1] - minx + rect[2]
end

-- Here endeth the static-ish functions

-- Each line is an array of integers representing the type of block in that column
-- Empty columns represented by nil
-- Is in block idx order, ie lines[0] is the topmost line and lines[height-1] the bottom
-- A completely empty line may be represented by nil or {}
lines = nil

function currentBrickWidth()
	if current.angle % 180 == 0 then
		return brickWidth(current.brick)
	else
		return brickHeight(current.brick)
	end
end

function currentBrickHeight()
	if current.angle % 180 == 0 then
		return brickHeight(current.brick)
	else
		return brickWidth(current.brick)
	end
end

function currentBlockSolidAt(x, y)
	return brickSolidAt(current.brick, current.rotation:transform(x, y))
end

function clearCurrentBrick()
	local col = bmp:getColour()
	bmp:setColour(bmp:getBackgroundColour())
	drawCurrentBrick()
	bmp:setColour(col)
end

function drawCurrentBrick()
	drawBrick(current.brick, current.x, current.y, current.drawTransform)
end

function init()
	local rl = runloop.current or runloop.new()

	if bmp then
		-- Reset everything
		bmp:clear()
	else
		bmp = bitmap.create()
		baseRotation = RotateTransform(90, bmp:rawWidth(), bmp:rawHeight())
		input.registerInputObserver(buttonPressed, 4)
		timers.init()
	end
	bmp:setTransform(baseRotation:get())
	bottomInset = inset + 2 * blockh

	width = bmp:width() // blockw
	height = (bmp:height() - bottomInset) // blockh
	score = 0
	level = 1
	lineCount = 0
	if not lastKeypressTime then lastKeypressTime = 0 end
	lines = {}
	local brick = nextBrick()
	current = {
		brick = brick,
		x = width // 2 - 1,
		y = 0,
		angle = 0,
		rotation = RotateTransform(0, brickSize(brick)),
		drawTransform = Transform()
	}
	current.drawTransform.tx = current.drawTransform.tx + inset
	playing = true
	dropping = false
	paused = false
	tickPeriod = 1000
	shouldPlayAudio, audioDriver = pcall(lupi.driverConnect, "BEEP")

	redrawPlayArea(true)

	bmp:blit()

	-- drawBrick(bricks.block, 0, 0)
	-- bmp:blit()
	-- drawBrick(bricks.backl, 2, 2)
	-- drawBrick(bricks.ell, 5, 2)
	-- drawBrick(bricks.ess, 7, 3)
	-- drawBrick(bricks.backs, 0, 4)
	-- drawBrick(bricks.tee, 3, 4)
	-- drawBrick(bricks.line, 6, 5)
	-- lupi.memStats()
end

function start()
	-- weightless = true; current.brick = bricks.I -- DEBUG
	-- weightless = true; current.brick = bricks.L -- DEBUG

	if not weightless then
		timers.after(tick, tickPeriod)
		ticking = true
	end
	drawCurrentBrick()
	bmp:blit()
	if shouldPlayAudio then
		playAudio()
	end
end

function main()
	init()
	playing = false
	bmp:drawTextCentred("Press A", 0, 20, bmp:width(), 0)
	bmp:blit()
	collectgarbage()
	runloop.current:run()
end

--[[**
Returns true if moving current brick to position (x,y) would overlap another
block or cause the brick to go out of bounds.
]]
function hitTest(x, y)
	for dy = 0, currentBrickHeight() - 1 do
		for dx = 0, currentBrickWidth() - 1 do
			if currentBlockSolidAt(dx, dy) then
				local xx, yy = x + dx, y + dy
				if xx < 0 or xx >= width or yy >= height then
					return true -- out of bounds
				end
				local line = lines[yy]
				local blk = line and line[xx]
				if blk then return true end
			end
		end
	end
	return false
end

-- Get an opaque identifier which encasulates which block of the XBM is drawn
-- for (x,y) and in what orientation
local function idForCurrentBrickOffset(x, y)
	local basex, basey = current.rotation:transform(x, y)
	assert(basey == 0 or basey == 1)
	assert(basex <= 3)
	local blockx = xOriginForBrick(current.brick) + basex
	local blocky = basey
	return current.angle * 32 + blockx + basey * 16
end

local function removeFullLines()
	local linesCompleted = 0
	for i = 0, height-1 do
		local full = true
		local line = lines[i]
		if line then
			for j = 0, width-1 do
				if line[j] == nil then
					full = false
					break
				end
			end
			if full then
				linesCompleted = linesCompleted + 1
				lines[i] = nil
			end
		end
	end
	score = score + 10 * linesCompleted * linesCompleted
	lineCount = lineCount + linesCompleted
	if lineCount >= level * 10 then
		level = level + 1
	end

	-- TODO animate fullLines

	-- Shuffle em up, ugh this is why I had lines counting the opposite way so I could use table.remove...
	local i = height-1
	local numSkipped = 0
	while i >= 0 do
		if lines[i] == nil then
			-- printf("Skipping line %i", i)
			numSkipped = numSkipped+1
		elseif numSkipped > 0 then
			-- printf("Moving line %d to %d", i, i + numSkipped)
			lines[i+numSkipped] = lines[i]
			lines[i] = nil
		end
		i = i - 1
	end
	redrawPlayArea()
	-- Restore bmp rotation
	bmp:setTransform(baseRotation:get())
end

function redrawPlayArea(redrawBorders)
	if redrawBorders then
		bmp:clear()
		local w, h = bmp:width() - 1, bmp:height() - (bottomInset - 1)
		bmp:drawLine(0, 0, 0, h)
		bmp:drawLine(0, h, w, h)
		bmp:drawLine(w, 0, w, h)
		updateScore()
	else
		bmp:clear(inset, 0, bmp:width() - inset*2, bmp:height() - bottomInset)
	end

	for y = 0, height-1 do
		if lines[y] then
			for x = 0, width-1 do
				local id = lines[y][x]
				if id then
					-- printf("Line %d col %d drawing 0x%x", y, x, id)
					local angle = (id & (~31)) // 32
					id = id & 31
					local blockx, blocky = id & 15, (id & 16) // 16

					-- Starting to lose the plot here... condensed code from setBrickRotation
					bmp:setTransform(RotateTransform(angle):applyToRotation(baseRotation):get())
					local drawTransform = RotateTransform(-angle, bmp:width(), bmp:height())
					if angle == 0 then
						drawTransform.tx = drawTransform.tx + inset
					elseif angle == 90 then
						drawTransform.ty = drawTransform.ty - blockw - inset
					elseif angle == 180 then
						drawTransform.tx = drawTransform.tx - blockw - inset
						drawTransform.ty = drawTransform.ty - blockh
					elseif angle == 270 then
						drawTransform.tx = drawTransform.tx - blockh
						drawTransform.ty = drawTransform.ty + inset
					end
					local x1, y1 = drawTransform:transform(x*blockw, y*blockh, 0, 0)
					bmp:drawXbm(xbm, x1, y1, blockx * blockw, blocky * blockh, blockw, blockh)
				end
			end
		end
	end
end

local function landed()
	-- print("Landed!")
	for dy = 0, currentBrickHeight() - 1 do
		for dx = 0, currentBrickWidth() - 1 do
			if currentBlockSolidAt(dx, dy) then
				local blockId = idForCurrentBrickOffset(dx, dy)
				-- printf("BlockId (%d,%d) = %x", dx, dy, blockId)
				local x, lineIdx = current.x + dx, current.y + dy
				local line = lines[lineIdx]
				if line == nil then
					line = {}
					lines[lineIdx] = line
				end
				-- printf("Line %d col %d = 0x%x", lineIdx, x, blockId)
				line[x] = blockId
			end
		end
	end

	current.brick = nextBrick()

	-- And reset brick orientation and location
	setBrickRotation(0, false)
	current.y = 0
	current.x = width / 2 - 1

	-- Call this only after setting rotation to 0
	removeFullLines()
	updateScore()

	if hitTest(current.x, current.y) then
		-- We are full
		playing = false
		bmp:setTransform(baseRotation:get())
		bmp:drawTextCentred("Game over!", 0, 20, bmp:width(), 0)
	end
end

function moveCurrent(dx, dy)
	clearCurrentBrick()
	current.x = current.x + dx
	current.y = current.y + dy
	drawCurrentBrick()
end

function tick()
	ticking = false
	if paused or not playing then return end
	timers.after(tick, tickPeriod - level * 100)
	ticking = true
	moveBrickDown()
	-- lupi.memStats()
end

function moveBrickDown()
	-- Check if brick can drop
	local wouldHit = hitTest(current.x, current.y + 1)

	if wouldHit then
		landed()
	else
		moveCurrent(0, 1)
	end
	bmp:blit()
	collectgarbage()
	-- lupi.memStats()
end

function drop()
	if paused or not dropping or not playing then return end
	timers.after(drop, 100)
	moveBrickDown()
end

function setBrickRotation(angle, performHitTest)
	if performHitTest == nil then
		-- Defaults to true if not specified
		performHitTest = true
	end
	local oldAngle = current.angle
	current.angle = angle % 360
	current.rotation = RotateTransform(-current.angle, brickSize(current.brick))

	if performHitTest and hitTest(current.x, current.y) then
		-- Try a wall kick, ie move block away from wall until it fits
		local kickedX = width - currentBrickWidth()
		if current.x > kickedX and not hitTest(kickedX, current.y) then
			-- Don't call moveCurrent it is confusing to do so during a rotate
			-- Instead update x directly, as the brick is about to be redrawn
			-- anyway
			current.x = kickedX
		else
			-- No room to rotate
			current.angle = oldAngle
			current.rotation = RotateTransform(-current.angle, brickSize(current.brick))
			return false
		end
	end

	bmp:setTransform(RotateTransform(current.angle):applyToRotation(baseRotation):get())
	current.drawTransform = RotateTransform(-current.angle, bmp:width(), bmp:height())

	-- Correct for the centre of rotation not putting it the brick top left in quite the right place
	-- I'm sure there should be a better way of doing this
	-- At same time, add the inset to the appropriate axis
	local r = current.drawTransform
	if current.angle == 0 then
		r.tx = r.tx + inset
	elseif current.angle == 90 then
		r.ty = r.ty - currentBrickWidth() * blockw - inset
	elseif current.angle == 180 then
		r.tx = r.tx - currentBrickWidth() * blockw - inset
		r.ty = r.ty - currentBrickHeight() * blockh
	elseif current.angle == 270 then
		r.tx = r.tx - currentBrickHeight() * blockh
		r.ty = r.ty + inset
	end
	-- printf("setBrickRotation %d -> a=%d b=%d c=%d d=%d tx=%d ty=%d", angle, r.a, r.b, r.c, r.d, r.tx, r.ty)
	return true
end

-- 7 is actually 'Light' on the tilda, but hey
Up, Down, Left, Right, A, B, Select, Start = 0, 1, 2, 3, 4, 5, 6, 7

local mask = 0

function buttonPressed(op, btn, timestamp)
	if not playing then
		if op == input.ButtonPressed then
			lastKeypressTime = timestamp
			init()
			if btn == Select then shouldPlayAudio = false end
			start()
		end
		return
	end
	if paused then
		if op == input.ButtonDown then
			mask = mask + btn
			if mask == A + B then
				bapple()
			end
		else
			mask = 0
		end
	end

	lastKeypressTime = timestamp

	if btn == Down then
		if op == input.ButtonDown then
			if not dropping then
				dropping = true
				timers.after(drop, 100)
			end
		elseif op == input.ButtonUp then
			dropping = false
		end
		return
	end
	if op ~= input.ButtonPressed then return end -- Rest of fn only deals in presses
	if btn == Left then
		if hitTest(current.x - 1, current.y) == false then
			moveCurrent(-1, 0)
		end
	elseif btn == Right then
		if hitTest(current.x + 1, current.y) == false then
			moveCurrent(1, 0)
		end
	elseif btn == Start then
		paused = not paused
		if paused then
			bmp:setTransform(baseRotation:get())
			bmp:drawTextCentred("Paused", 0, 20, bmp:width(), 0)
		else
			redrawPlayArea()
			setBrickRotation(current.angle)
			drawCurrentBrick()
		end
		if not paused and not ticking then
			timers.after(tick, tickPeriod)
		end
	elseif btn == A then
		-- Rotate!
		clearCurrentBrick()
		setBrickRotation(current.angle + 90)
		drawCurrentBrick()
	elseif btn == B then
		clearCurrentBrick()
		setBrickRotation(current.angle - 90)
		drawCurrentBrick()
	elseif btn == Select then
		if baseRotation.b == 1 then
			-- 270
			baseRotation = RotateTransform(90, bmp:rawWidth(), bmp:rawHeight())
		else
			-- b = -1 means 90
			baseRotation = RotateTransform(270, bmp:rawWidth(), bmp:rawHeight())
		end
		bmp:setTransform(baseRotation:get())
		redrawPlayArea(true)
		setBrickRotation(current.angle)
		drawCurrentBrick()
	end
	bmp:blit()
	if weightless then
		collectgarbage()
	end
end

--[[
function bapple()
	if not paused then
		bmp:setTransform(baseRotation:get())
		bmp:setColour(bitmap.Colour.White)
		bmp:drawRect(inset, 30, bmp:width() - inset*2, 30)
		bmp:setColour(bitmap.Colour.Black)
		bmp:drawTextCentred("Also...", 0, 30, 64, 30)
		paused = true
	else
		local bapple = require("bapple")
		bmp:drawXbm(bapple.xbm, 10, 0, 0, 0, 48, 54)
		bmp:drawXbm(bapple.xbm, 20, 54, 48, 0, 21, 54)
	end
end
]]

function nextBrick()
	--local idx = (rand() % #brickList) + 1
	local idx = lastKeypressTime % #brickList
	return brickList[idx+1]
end

function updateScore()
	local scoreText = string.format("%d", score)
	local levelText = string.format("%d", level)
	bmp:clear(0, height * blockh + inset, bmp:width(), bottomInset - inset)
	local y = height * blockh + inset + 1
	bmp:drawText(levelText, 1, y)
	local w = bmp:getTextSize(scoreText)
	bmp:drawText(scoreText, bmp:width() - w - 1, y)
end

local KExecDriverAudioPlay = 1
local KExecDriverAudioPlayLoop = 2

local pcmLen = 1702256
-- local pcmLen = 1707228 -- Full length of original PCM

function playAudio()
	lupi.driverCmd(audioDriver, KExecDriverAudioPlayLoop, 0, pcmLen)
end
