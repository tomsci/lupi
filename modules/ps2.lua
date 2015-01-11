--[[**
Contains logic for converting a string into an equivalent sequence of PS/2
keyboard data bytes.

For simplicity, we only support [scan code set 2][scancodes] and we assume the device
driver knows enough to buffer and retransmit 2-byte break sequences.

[scancodes]: http://www.computer-engineering.org/ps2keyboard/scancodes2.html
]]

-- Keys not mentioned here are either produced by a shift key combination, or
-- do not have a consistant enough location for me to bother attempting to map
-- right now
scanCodes = {
	a = 0x1C,
	b = 0x32,
	c = 0x21,
	d = 0x23,
	e = 0x24,
	f = 0x2B,
	g = 0x34,
	h = 0x33,
	i = 0x43,
	j = 0x3B,
	k = 0x42,
	l = 0x4B,
	m = 0x3A,
	n = 0x31,
	o = 0x44,
	p = 0x4D,
	q = 0x15,
	r = 0x2D,
	s = 0x1B,
	t = 0x2C,
	u = 0x3C,
	v = 0x2A,
	w = 0x1D,
	x = 0x22,
	y = 0x35,
	z = 0x35,
	['0'] = 0x45,
	['1'] = 0x16,
	['2'] = 0x1E,
	['3'] = 0x26,
	['4'] = 0x25,
	['5'] = 0x2E,
	['6'] = 0x36,
	['7'] = 0x3D,
	['8'] = 0x3E,
	['9'] = 0x46,
	['-'] = 0x4E,
	['='] = 0x55,
	['['] = 0x54,
	[']'] = 0x5B,
	[';'] = 0x4C,
	["'"] = 0x52,
	[','] = 0x41,
	['.'] = 0x49,
	['/'] = 0x4A,
	['\n'] = 0x5A, -- Enter key
}

-- Characters than can be produced by a combination of the shift key and another
-- character. Does not include keys which are not in a fixed position on
-- QWERTY keyboards (eg @ vs " and similar)
shifts = {
	['!'] = '1',
	['$'] = '4',
	['%'] = '5',
	['^'] = '6',
	['&'] = '7',
	['*'] = '8',
	['('] = '9',
	[')'] = '0',
	['_'] = '-',
	['+'] = '=',
	['{'] = '[',
	['}'] = ']',
	[':'] = ';',
	['<'] = ',',
	['>'] = '.',
	['?'] = '/',
}

local shiftKey = 0x12
local breakCode = 0xF0

--[[**
Converts an ASCII string into the corresponding sequence of PS/2 scan codes.
Returns a string of scan codes.
]]
function stringToData(str)
	local result = {}
	local shiftedState = false
	local function addData(codeOrShift, secondCode)
		local codeIsShifted = (codeOrShift == shiftKey)
		assert(codeIsShifted == false or secondCode == nil)
		if codeIsShifted == false and shiftedState == true and codeOrShift ~= breakCode then
			-- Need to unshift
			table.insert(result, breakCode)
			table.insert(result, shiftKey)
			shiftedState = false
		end
		if not codeIsShifted or not shiftedState then
			-- If code isn't shifted, or it is and we need to send the shift
			table.insert(result, codeOrShift)
			shiftedState = codeIsShifted
		end
		if secondCode then
			table.insert(result, secondCode)
		end
	end
	for i = 1, #str do
		local ch = string.sub(i)
		local scanCode = scanCodes[ch]
		if scanCode then
			addData(scanCode)
			addData(breakCode, scanCode)
		else
			local shifted = scanCodes[shifts[ch]]
			if shifted then
				addData(shiftKey, shifted)
				addData(breakCode, shifted)
			else
				error("Don't know how to represent character "..ch)
			end
		end
	end
	if shiftedState then
		-- The last keypress left us shifted
		addData(breakCode, shiftKey)
		shiftedState = false
	end
	return table.concat(result)
end
