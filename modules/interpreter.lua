if lupi then
	require "runloop"
end

prompt = "lua> "

-- Not accessible to interpreter - too much possibility to break stuff
local history = {}
local historyIdx = 1 -- unless cycling through history, is always one past the end of history
local cursor
local line

local function c(str)
	--# So you can for example write c'\n' to mean "the character code for \n"
	return string.byte(str)
end

local function CTRL(charstr)
	return c(charstr) - c('A') + 1
end

-- getch and putch functions are implemented natively by klua.c/ulua.c
-- should maybe support io module instead?

local function safeputch(ch)
	if ch >= 32 and ch < 127 then
		putch(ch)
	else
		putch(c'.')
	end
end

function printPrompt()
	local ok, p = pcall(tostring, prompt)
	if not ok then p = "You think you're so clever don't you? >" end
	for i = 1, #p do
		putch(p:byte(i))
	end
end

local function moveCursor(delta)
	while delta > 0 do
		--print(cursor, #line)
		if cursor > #line then return end
		cursor = cursor + 1
		putch(27); putch(c'['); putch(c'C')
		delta = delta - 1
	end
	while delta < 0 do
		if cursor <= 1 then return end
		cursor = cursor - 1
		putch(27); putch(c'['); putch(c'D')
		delta = delta + 1
	end
end

local function setCursor(pos)
	moveCursor(pos - cursor)
end

local function saveLineToHistory(lineString)
	line = {}
	cursor = 1
	if #lineString == 0 then return end -- Nothing doing
	table.insert(history, lineString)
	historyIdx = #history + 1
end

local function backspace()
	if cursor > 1 then
		moveCursor(-1)
		table.remove(line, cursor)
		-- Redraw rest of line in new pos
		for i = cursor, #line do
			safeputch(line[i]:byte())
		end
		putch(c' ')
		-- And restore cursor
		for i = cursor, #line + 1 do
			putch(8)
		end
	end
end

local function clearLine()
	setCursor(#line + 1)
	while cursor > 1 do
		backspace()
	end
end

function handleCtrlA()
	setCursor(1)
end

function handleCtrlE()
	setCursor(#line + 1)
end

local function handleChar(ch)
	local handleCtrlFn = ch < 30 and _ENV["handleCtrl"..(string.char(ch - 1 + c'A'))]
	if handleCtrlFn then
		handleCtrlFn(ch)
	else
		table.insert(line, cursor, string.char(ch))
		safeputch(ch)
		if cursor < #line then
			-- Have to reprint everything following cursor on the line
			for i = cursor+1, #line do
				safeputch(line[i]:byte())
			end
			for i = cursor+1, #line do
				putch(8)
			end
		end
		cursor = cursor + 1
	end
end

local function setLineToHistory(idx)
	clearLine()
	local str = history[idx]
	if str == nil then
		historyIdx = #history + 1
	else
		for i = 1, #str do
			handleChar(str:byte(i))
		end
		historyIdx = idx
	end
end

local function handleEscape()
	local ch = getch()
	if ch == c'[' then
		ch = getch()
		if ch == c'A' then
			-- history prev
			setLineToHistory(historyIdx - 1)
		elseif ch == c'B' then
			-- history next
			setLineToHistory(historyIdx + 1)
		elseif ch == c'C' then
			-- right
			moveCursor(1)
		elseif ch == c'D' then
			-- left
			moveCursor(-1)
		end
	else
		handleChar(ch)
	end
end

local function handleResult(ok, ...)
	if not ok then
		local err = select(1, ...)
		print("Error: "..err)
	elseif select("#", ...) > 0 then
		print("==>", ...)
	end
end

function executeLine(lineString)
	saveLineToHistory(lineString) -- even if it didn't compile, it still goes in the history
	-- Like the standard Lua interpreter, support "=" as a synonym for "return"
	lineString = lineString:gsub("^=", "return ")
	if lineString:match("%)$") and not lineString:match("=") and not lineString:match("^return ") then
		-- Assume it's a function call and prepend an implicit return statement
		lineString = "return "..lineString
	end
	local fn, err = load(lineString, "<stdin>", nil, _ENV)
	if fn then
		handleResult(pcall(fn))
	else
		print("Error: "..err)
	end
	printPrompt()
end

local function gotChar(ch)
	if string.char(ch) == "\r" then
		print("")
		local lineString = table.concat(line)
		executeLine(lineString)
	elseif ch == 8 then
		backspace()
	elseif ch == 27 then
		handleEscape()
	else
		handleChar(ch)
	end
end

function main()
	line = {}
	cursor = 1
	printPrompt()

	if lupi ~= nil then
		local rl = runloop.current or runloop.new()
		local chreq = rl:newAsyncRequest({
			completionFn = function(req, ch)
				gotChar(ch)
			end,
			requestFn = lupi.getch_async,
		})
		rl:queue(chreq)
		rl:run()
	else
		--# Run blocking (klua doesn't support async)
		while true do
			gotChar(getch())
		end
	end
end

-- This is only used (when on pi) to validate the output of running hosted luac.
function dump(modName)
	local function printstring(str)
		for i = 1, #str do
			putch(str:byte(i))
		end
	end
	local data = string.dump(getModuleFn(modName))
	for i = 1, #data, 16 do
		for j = 0, 15 do
			printstring(string.format("%02x ", data:byte(i+j)))
		end
		print("")
	end
end
