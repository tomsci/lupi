require "runloop"
require "input"
require "bitmap"

local uicontrols = require "passwordManager.uicontrols"
local window = require "passwordManager.window"
local keychain = require "passwordManager.keychain"
local lockscreen = require "passwordManager.lockscreen"

--BEGIN DEBUG
for i = 1, 30 do
	table.insert(keychain.items, { url = "Filler item "..i, pass="Item"..i.."pass" })
end

local Colour = bitmap.Colour
local Button, Checkbox = uicontrols.Button, uicontrols.Checkbox
local Window = window.Window

function main()
	local rl 
	if not runloop.current then
		rl = runloop.new()
	end

	win = Window()
	-- win.debugDrawing = true
	input.registerInputObserver(function (...) win:gotInput(...) end)

	assert(xpcall(displayMainList, debug.traceback))
	-- assert(xpcall(lockscreen.displayLockScreen, debug.traceback, win))

	if rl then
		rl:run()
	end
end

function drawDebug()
	but = Button {
		text = "Hello world",
		x = 10,
		y = 10,
		handleActivated = function(self)
			print("I'M A BUTTON!")
		end
	}
	win:addControl(but)

	disbut = Button {
		enabled = false,
		x = 10,
		y = 50,
		text = "I am greyed out",
	}
	win:addControl(disbut)
	win:redraw()
end

function displayMainList(startingAt)
	win:clear()
	if not startingAt then startingAt = 1 end

	local b = win.bitmap
	local topBarHeight = 20
	b:setColour(Colour.White)
	b:setBackgroundColour(win.backgroundColour)
	b:drawTextCentred("Password manager", 0, 0, b:width(), topBarHeight)
	b:setColour(Colour.Black)
	b:drawLine(0, topBarHeight, b:width()-1, topBarHeight)

	prevButton = Button {
		x = 5,
		text = "Previous",
		enabled = (startingAt > 1),
		bitmap = b,
		handleActivated = gotoPrevious,
	}
	prevButton.y = b:height() - prevButton:height() - 5

	nextButton = Button {
		y = prevButton.y,
		text = "Next",
		enabled = false,
		bitmap = b,
		handleActivated = gotoNext,
	}
	nextButton.x = b:width() - nextButton:width() - 5

	displayCheckbox = Checkbox {
		text = "Display",
		x = prevButton.x + prevButton:width() + 5,
		y = prevButton.y,
		checked = true, -- Until we have PS/2 support
	}

	win:addControl(prevButton)
	win:addControl(nextButton)
	win:addControl(displayCheckbox)

	local items = keychain.items
	local numItems = #items
	currentPageStart = startingAt
	local row = 0
	local rowHeight = 30
	maxRows = (win:height() - topBarHeight - 10 - nextButton:height()) / rowHeight
	local rowWidth = win:width() / 2
	local x = 0
	for i = currentPageStart, currentPageStart + maxRows*2 do
		local item = items[i]
		if not item then break end
		local but = Button {
			text = item.url,
			x = x,
			y = topBarHeight + 5 + row * (rowHeight - 1), -- we overlap borders
			fixedHeight = rowHeight,
			fixedWidth = rowWidth,
			edgeColour = Colour.Grey,
			vpadding = 10,
			setPressed = itemButtonPressed,
			item = item,
		}
		win:addControl(but)
		row = row + 1
		if row == maxRows then
			if x == 0 then
				-- Move to second column
				x = rowWidth
				row = 0
			else
				break
			end
		end
	end
	currentNumItems = row + (x > 0 and 1 or 0) * maxRows
	nextButton:setEnabled(numItems > currentPageStart-1 + currentNumItems)
	win:redraw()
end

function gotoPrevious()
	displayMainList(currentPageStart - maxRows*2)
end

function gotoNext()
	displayMainList(currentPageStart + currentNumItems)
end

function itemButtonPressed(itemButton, flag)
	if displayCheckbox.checked then
		local b = win.bitmap
		b:setColour(win.backgroundColour)
		b:drawRect(0, prevButton.y, win:width(), prevButton:height())
		b:setBackgroundColour(win.backgroundColour)
		b:setColour(Colour.White)
		if flag then
			b:drawTextCentred(itemButton.item.pass, 0, prevButton.y, b:width(), prevButton:height())
		else
			prevButton:draw()
			displayCheckbox:draw()
			nextButton:draw()
		end
		win:redraw()
	end
	Button.setPressed(itemButton, flag)
end
