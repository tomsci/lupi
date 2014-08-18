require "runloop"
require "input"
require "bitmap"
local uicontrols = require "passwordManager.uicontrols"
local window = require "passwordManager.window"
local keychain = require "passwordManager.keychain"

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

	displayMainList()

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
		enabled = false,
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
	}

	win:addControl(prevButton)
	win:addControl(nextButton)
	win:addControl(displayCheckbox)

	local items = keychain.items
	currentPageStart = startingAt or 1
	local row = 0
	local rowHeight = 30
	local maxRows = (win:height() - topBarHeight - 10 - nextButton:height()) / rowHeight
	local rowWidth = win:width() / 2
	local x = 0
	for i = currentPageStart, currentPageStart + maxRows*2 do
		local item = items[i]
		if not item then break end
		local but = Button {
			text = item.url,
			x = x,
			y = topBarHeight + 10 + row * (rowHeight - 1), -- we overlap borders
			fixedHeight = rowHeight,
			fixedWidth = rowWidth,
			edgeColour = Colour.Grey,
			vpadding = 10,
			setPressed = itemButtonPressed,
		}
		win:addControl(but)
		row = row + 1
		if row == maxRows then
			if x == 0 then
				-- Move to second column
				x = rowWidth
				row = 0
			else
				error("Shouldn't get here")
				break
			end
		end
	end
	win:redraw()
end

function gotoPrevious()
end

function itemButtonPressed(itemButton, flag)
	--TODO
	Button.setPressed(itemButton, flag)
end
