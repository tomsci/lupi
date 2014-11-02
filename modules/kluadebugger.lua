--[[**
The KLua Debugger
=================

This is an interactive command-line debugger that is invoked when the kernel
crashes. It reuses the user interpreter and membuf modules to provide an
interactive Lua prompt that can view all the kernel data structures. It also
contains various helper functions to aid debugging.

In terms of implementation, it runs with interrupts disabled in System mode, (on
ARMv6) or in privileged thread mode with BASEPRI set to only allow SVC (on
ARMv7-M), meaning it can access the entire address space, but can still make SVC
calls. This is important because it means it can reuse the same Lua binary that
is used user-side.

The available syntax is below.

]]

-- Note the weird bracket syntax so this is both a string we can print at
-- runtime, and also something that looks like a documentation comment

local helpText = [[
--[[**
Functions
---------

    help()              prints this text.
    print(), p()        Standard print function
    GetProcess(idx)     returns the Process with the given index.
    switch_process(p)   switches to the process p. If p is an integer, it is
                        treated as a convenience for
                        switch_process(GetProcess(p))
    mem(addr, len)      if there is an existing membuf for this adress, returns
                        it. Otherwise returns nil.
    newmem(addr, len [,type])  creates a MemBuf from an arbitrary address and
                        length, and optionally the supplied type.
    ustack()            Convenience for
                        stack(TheSuperPage.currentThread.savedRegisters.r13)
    stack()             Convenience for stack(TheSuperPage.crashRegisters.r13)
    stack(obj)          Prints a stacktrace for obj. Obj can be a thread (in
                        which case thread.savedRegisters.r13 is used) or a
                        number (which is assumed to be a stack pointer) or nil
                        (in which case TheSuperPage.crashRegisters.r13 is used).
    pageStats()         Print stats about the PageAllocator.
    reboot(), ^X        Reboot the device.

    buf:getAddress()    returns the address of the MemBuf.
    buf:getLength()     returns the length of the MemBuf in bytes.
    buf:getInt(offset)  returns *(int*)(buf:getAddress() + offset)
    buf:getByte(offset) returns *(byte*)(buf:getAddress() + offset)
    buf:hex()           returns a hexdumped string of the given buf. General
                        usage is to call print(buf:hex()).
    buf:words()         returns a hexdumped string of the given buf assuming
                        its contents are word-sized. Upshot being that
                        word-sized values on a little-endian machine are
                        displayed correctly.

    s(addr)             If the kernel was built with symbols enabled, equivalent
                        of symbols.addressDescription(addr).

    c.FUNC(args...)     If symbols are enabled, you can (attempt to) execute any
                        FUNC which appears in the symbol table, takes 0-3 32-bit
                        arguments and returns a 32-bit value. Any argument or
                        return which is a pointer to a known MemBuf will be
                        converted as appropriate. For example:
                        c.processForThread(TheSuperPage.currentThread)

Variables
---------

    TheSuperPage, sp    the SuperPage
    Al                  the PageAllocator
    symbols             if the kernel was built with symbols enabled, the
                        symbolParser module will be set up and the symbols
                        loaded. If not, this will be nil.
    scb                 On ARMv7-M platforms, this will point to the System
                        Control Block. Otherwise will be nil.

Syntax
------

Most of the kernel data structs have their members declared to MemBuf, meaning
the members can accessed using normal Lua member syntax. Ie:

    print(TheSuperPage.currentProcess.name) -- Prints the name of the current
                                            -- process

As a convenience, calling a function from the command line automatically prints
any returned values, so "GetProcess(0)" is equivalent to print(GetProcess(0)).
]]

if OhGodNoRam then helpText = nil end -- Should free up a bit of space

local require = require -- Make sure we keep this even after we've changed _ENV
local membuf = require("membuf")

-- We're not a module, just a collection of helper fns that should be global
_ENV = _G
local interpreter = require("interpreter")

-- Conveniences
sp = TheSuperPage
p = print

function help()
	print("The klua debugger.")
	if helpText then
		print(helpText:sub(7, #helpText)) -- Skip documentation comment bit
	end
end

local function loadSymbols()
	local ok = pcall(require, "symbols")
	if not ok then return end -- No point loading symbolParser if no symbols
	local symbolParser = require("symbolParser")
	symbolParser.loadSymbolsFromSymbolsModule()
	symbols = symbolParser
	s = symbols.addressDescription
	membuf.setPointerDescriptionFunction(symbols.addressDescription)
end

loadSymbols()

local function getUintArg(arg)
	if type(arg) == "userdata" then
		-- Assume it's a MemBuf
		return arg:getAddress()
	else
		return tonumber(arg)
	end
end

function executableCFunction(addr)
	local f = function(arg1, arg2, arg3)
		local ret = executeFn(addr, getUintArg(arg1), getUintArg(arg2), getUintArg(arg3))
		-- See if this is an object ptr
		local obj = mem(ret)
		return obj or ret
	end
	return f
end

c = {}
local cmt = {}
cmt.__index = function(c, fnName)
	if symbols then
		local s = symbols.findSymbolByName(fnName)
		if not s then error("Can't find symbol "..fnName) end
		return executableCFunction(s.addr)
	else
		error("Can't use dynamic execution functionality unless symbols are included")
	end
end
setmetatable(c, cmt)

-- Copied from misc.roundDownUnsigned to break the dependancy on misc
local sizeFail = (0x80000000 < 0)
function roundDown(val, size)
	-- With integer division (which is the only type we have atm)
	-- this should do the trick
	local n = val / size
	-- Ugh because Lua is compiled with 32-bit signed ints only, if val is
	-- greater than 0x8000000, then the above divide which rounds towards zero,
	-- will actually round *up* when the number is considered as unsigned
	-- Therefore, subtract one from n in this case
	if sizeFail and val < 0 then n = n - 1 end
	return n * size
end

function stack(obj)
	local addr
	local t = type(obj)
	if t == "nil" then addr = TheSuperPage.crashRegisters.r13
	elseif t == "userdata" and obj:checkType("Thread") then
		addr = obj.savedRegisters.r13
		local p = processForThread(obj)
		if p and TheSuperPage.currentProcess ~= p then
			print("(Switching process to "..p.name..")")
			switch_process(p)
		end
	elseif t == "number" then addr = obj
	else
		error("Bad type to stack function")
	end

	-- Figure out what type of stack addr is, and thus how big it is
	local stackTop
	if addr <= 0x4000000 and addr > 0 then
		-- Check if it's a user stack or an svc stack
		local stackAreaStart = roundDown(addr+100, bit32.lshift(1, USER_STACK_AREA_SHIFT))
		local svc = addr >= stackAreaStart - 100 and addr < stackAreaStart + KPageSize
		if svc then
			stackTop = stackAreaStart + KPageSize
		else
			stackTop = stackAreaStart + 2 * KPageSize + USER_STACK_SIZE
		end
	else
		stackTop = roundDown(addr, KPageSize) + KPageSize
	end

	print(string.format("(Calcuating stack from %x to %x)", addr, stackTop))
	local stackMem = newmem(addr, stackTop - addr)
	if symbols then
		for offset = 0, stackMem:getLength()-4, 4 do
			local stackData = stackMem:getInt(offset)
			print(string.format("%08X: %08x %s", addr+offset,
				stackData, symbols.addressDescription(stackData)))
		end
	else
		print(stackMem:words())
	end
end

function ustack()
	return stack(TheSuperPage.currentThread.savedRegisters.r13)
end

local function makeEnum(values)
	local result = {}
	for k,v in pairs(values) do
		result[k] = v
		result[v] = k
	end
	return result
end

local PageType = Al and makeEnum(membuf.getType("PageType")._values)

function pageStats()

	assert(Al, "No page allocator on this platform!")

	-- Unfortunately, doing the actual iterating over the page info is too slow in lua.
	-- Maybe can revisit this if icache ever starts working
	--[[
	local n = Al.numPages
	--TODO the pageList offset is almost certainly wrong by now
	local pageList = mem(Al:getAddress() + 8, n)
	local count = {}
	for i = 0, n-1 do
		local t = pageList:getByte(i)
		count[t] = (count[t] or 0) + 1
	end
	]]
	local count = pageStats_getCounts()

	local function printCount(text, num)
		if not num then num = 0 end
		local pad = string.rep(" ", 18 - text:len())
		print(string.format("%s:%s%d (%d kB)", text, pad, num, num * 4))
	end

	printCount("Total used RAM", TheSuperPage.totalRam/4096 - count[PageType.KPageFree])
	printCount("Free pages", count[PageType.KPageFree])
	printCount("Section 0 pages", count[PageType.KPageSect0])
	printCount("Allocator", count[PageType.KPageAllocator])
	printCount("Process pages", count[PageType.KPageProcess])
	printCount("User PDEs", count[PageType.KPageUserPde])
	printCount("User PTs", count[PageType.KPageUserPt])
	printCount("User mem", count[PageType.KPageUser])
	printCount("klua heap", count[PageType.KPageKluaHeap])
	printCount("KernPtForProcPts", count[PageType.KPageKernPtForProcPts])
	printCount("Shared pages", count[PageType.KPageSharedPage])
	printCount("User stack pages", count[PageType.KPageThreadSvcStack])
end

function memStats()
	local k, b = collectgarbage("count")
	print(string.format("Lua memory usage = %d", k*1024 + b))
	printMallocStats()
end

function interpreter.handleCtrlX()
	-- The interpreter will check for this fn if the user hits ctrl-X
	print("reboot")
	-- Some extra newlines because otherwise the reboot will occur before the
	-- uart buffers have completed flushing and the "reboot" text will be
	-- truncated
	print("")
	print("")
	print("")
	print("")
	reboot()
end

-- print(collectgarbage("count"))
-- collectgarbage()
-- print(collectgarbage("count"))
