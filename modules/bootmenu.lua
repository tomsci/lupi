BootMode = {
	normal = 0,
	--atomicsTest = 1,
}

function main()
	print [[
Boot menu:
 Enter, 0: Start interpreter
        1: Start klua debugger
        a: Run atomics unit tests
]]

	while true do
		local ch = string.char(getch())
		if ch == '\r' then
			return 0
		elseif ch:match('[0-9]') then
			return tonumber(ch)
		elseif ch == 'a' then
			--return BootMode.atomicsTest
		end
	end
end
