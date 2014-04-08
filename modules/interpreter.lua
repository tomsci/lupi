prompt = "lua> "

-- getch and putch functions are implemented natively by klua.c
-- should maybe support io module instead?

local function safeputch(ch)
	if ch >= 32 and ch < 127 then
		putch(ch)
	else
		putch(string.byte("."))
	end
end

local function printPrompt()
	local ok, p = pcall(tostring, prompt)
	if not ok then p = "You think you're so clever don't you? >" end
	for i = 1, #p do
		putch(p:byte(i))
	end
end

function main()
	printPrompt()
	local line = {}
	while true do
		local ch = getch()
		if string.char(ch) == "\r" then
			print("")
			local fn, err = load(table.concat(line), "<stdin>")
			line = {}
			if fn then
				local ok, err = pcall(fn)
				if not ok then
					print("Error: "..err)
				end
			else
				print("Error: "..err)
			end
			printPrompt()
		elseif ch == 8 then
			-- Backspace
			if next(line) then
				table.remove(line)
				putch(ch)
				putch(string.byte(" "))
				putch(ch)
			end
		else
			table.insert(line, string.char(ch))
			safeputch(ch)
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
