require "bitmap"
require "runloop"
require "input"
local timers = require "timerserver.local"

local blockw, blockh = 6, 6
local inset = 2

-- These coordinates correspond to where the bricks are located in tetris.xbm
-- They consist of 2 pairs of numbers: this first describing the x position and
-- width of the top 2 blocks of the brick; and the second describing the bottom
-- 2 blocks. For the line (which is solely on the top row), the second pair has
-- a width of zero.
bricks = {
	block = { { 0, 2 }, { 0, 2 } },
	backl = { { 2, 1 }, { 2, 3 } },
	line  = { { 3, 4 }, { 3, 0 } },
	ell   = { { 7, 1 }, { 5, 3 } },
	ess   = { { 9, 2 }, { 8, 2 } },
	tee   = { { 11, 1 }, { 10, 3 } },
	backs = { { 12, 2 }, { 13, 2 } },
}

function drawBrick(p, x, y)
	local firstRect = p[1]
	local secondRect = p[2]
	local xdelta = secondRect[1] - firstRect[1]
	if xdelta < 0 then
		-- First rect isn't leftmost part of the brick, so increment x so that
		-- the leftmost part is drawn at the original x
		x = x - xdelta
	end

	bmp:drawXbm(xbm, inset + x * blockw, y * blockh, firstRect[1] * blockw, 0, firstRect[2] * blockw, blockh)
	if secondRect[2] > 0 then
		bmp:drawXbm(xbm, inset + (x + xdelta) * blockw, (y + 1) * blockh,
			secondRect[1] * blockw, blockh, secondRect[2] * blockw, blockh)
	end
end

function clearBrick(p, x, y)
	local col = bmp:getColour()
	bmp:setColour(bmp:getBackgroundColour())
	drawBrick(p, x, y)
	bmp:setColour(col)
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

function brickSolidAt(brick, x, y)
	-- y must be either 0 or 1 so can be used to index into brick (which is one-based hence the +1)
	local minx = min(brick[1][1], brick[2][1])
	local rect = brick[y + 1]
	return rect[2] > 0 and x >= rect[1] - minx and x < rect[1] - minx + rect[2]
end

-- Here endeth the static-ish functions

-- 1st line is at bottom, ie y block coord (height-1)
-- Each line is an array of integers representing the type of block in that column
lines = {}

function currentBrickWidth()
	return brickWidth(current)
end

function currentBrickHeight()
	return brickHeight(current)
end

function currentBlockSolidAt(x, y)
	return brickSolidAt(current, x, y)
end

function init()
	local rl = runloop.current or runloop.new()

	bmp = bitmap.create()
	bmp:setRotation(90)
	width = bmp:width() / blockw
	height = bmp:height() / blockh
	curIdx = "block"
	current = bricks[curIdx]
	curX = width / 2
	curY = 0
	dropping = false
	paused = false

	local w, h = bmp:width()-1, bmp:height()-1
	bmp:drawLine(0, 0, 0, h)
	bmp:drawLine(0, h, w, h)
	bmp:drawLine(w, 0, w, h)

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
	input.registerInputObserver(buttonPressed, 4)

	timers.init()
end

function start()
	timers.after(tick, 1000)
	drawBrick(current, curX, curY)
	bmp:blit()
end

function main()
	init()
	start()
	collectgarbage()
	runloop.current:run()
end

-- Returns true if moving current brick to position (x,y) would overlap another block
local function hitTest(x, y)
	for dy = 0, currentBrickHeight() - 1 do
		for dx = 0, currentBrickWidth() - 1 do
			if currentBlockSolidAt(dx, dy) then
				local xx, yy = x + dx, y + dy
				if xx < 0 or xx >= width or yy >= height then
					return true -- out of bounds
				end
				local line = lines[height - 1 - yy]
				local blk = line and line[xx]
				if blk and blk ~= 0 then return true end
			end
		end
	end
	return false
end

local function landed()
	print("Landed!")
	for dy = 0, currentBrickHeight() - 1 do
		for dx = 0, currentBrickWidth() - 1 do
			if currentBlockSolidAt(dx, dy) then
				block = 1 --TODO
				local x, lineIdx = curX + dx, height - 1 - (curY + dy)
				local line = lines[lineIdx]
				if line == nil then
					lines[lineIdx] = { [x] = block }
				else
					line[x] = block
				end
			end
		end
	end

	-- Now check for full lines
	-- for i, line in ipairs(lines

	curIdx, current = next(bricks, curIdx)
	if not curIdx then curIdx, current = next(bricks) end
	curY = 0
	curX = width / 2

end

function moveCurrent(dx, dy)
	clearBrick(current, curX, curY)
	curX = curX + dx
	curY = curY + dy
	drawBrick(current, curX, curY)
end

function tick()
	if paused then return end
	timers.after(tick, 1000)
	moveBrickDown()
end

function moveBrickDown()
	-- Check if brick can drop
	local wouldHit = hitTest(curX, curY + 1)

	if wouldHit then
		landed()
	else
		moveCurrent(0, 1)
	end
	bmp:blit()
	collectgarbage()
	--lupi.memStats()
end

function drop()
	if paused or not dropping then return end
	timers.after(drop, 100)
	moveBrickDown()
end

Up, Down, Left, Right, A, B, Select, Light = 0, 1, 2, 3, 4, 5, 6, 7

function buttonPressed(op, btn)
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
	if op ~= input.ButtonPressed then return end -- Not bovvered
	if btn == Left then
		if hitTest(curX - 1, curY) == false then
			moveCurrent(-1, 0)
		end
	elseif btn == Right then
		if hitTest(curX + 1, curY) == false then
			moveCurrent(1, 0)
		end
	elseif btn == Light then
		-- DEBUG
		bmp:blit(0, 0, bmp:rawWidth(), bmp:rawHeight())
	elseif btn == A then
		paused = true
		bmp:setColour(bitmap.Colour.White)
		bmp:drawRect(0, 30, 64, 30)
		bmp:setColour(bitmap.Colour.Black)
		bmp:drawTextCentred("Also...", 0, 30, 64, 30)
	elseif btn == B then
		paused = true
		require "bapple"
		bmp:drawXbm(bapple.xbm, 10, 0, 0, 0, 48, 54)
		bmp:drawXbm(bapple.xbm, 20, 54, 48, 0, 21, 54)
	end
	bmp:blit()
end
