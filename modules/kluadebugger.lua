--[[**
The KLua Debugger
=================

This is an interactive command-line debugger that is invoked when the kernel
crashes. It reuses the user interpreter and membuf modules to provide an
interactive Lua prompt that can view all the kernel data structures. It also
contains various helper functions to aid debugging.

In terms of implementation, it runs with interrupts disabled in System mode,
which means it is privileged and can access the entire address space, but can
still make SVC calls. This is important because it means it can reuse the same
Lua binary that is used user-side.

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

Variables
---------

    TheSuperPage, sp    the SuperPage
    Al                  the PageAllocator

Syntax
------

Most of the kernel data structs have their members declared to MemBuf, meaning
the members can accessed using normal Lua member syntax. Ie:

    print(TheSuperPage.currentProcess.name) -- Prints the name of the current
                                            -- process

As a convenience, calling a function from the command line automatically prints
any returned values, so "GetProcess(0)" is equivalent to print(GetProcess(0)).
]]

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
	print(helpText:sub(7, #helpText)) -- Skip documentation comment bit
end

--local band, bnot = bit32.band, bit32.bnot
--local function roundPageDown(addr) return band(addr, bnot(KPageSize-1)) end

local function roundDown(addr, size)
	-- With integer division (which is the only type we have atm) this should do the trick
	local n = addr / size
	return n * size
end

function stack(obj)
	local addr
	local t = type(obj)
	if t == "nil" then addr = TheSuperPage.crashRegisters.r13
	elseif t == "userdata" then
		addr = obj.savedRegisters.r13 -- assume a thread membuf
		local p = processForThread(obj)
		if TheSuperPage.currentProcess ~= p then
			print("(Switching process to "..p.name..")")
		end
	elseif t == "number" then addr = obj
	else
		error("Bad type to stack function")
	end

	-- Figure out what type of stack addr is, and thus how big it is
	local stackTop
	if addr <= 0x4000000 then
		-- Check if it's a user stack or an svc stack
		local stackAreaStart = roundDown(addr+100, bit32.lshift(1, USER_STACK_AREA_SHIFT))
		local svc = addr >= stackAreaStart - 100 and addr < stackAreaStart + KPageSize
		if svc then
			stackTop = stackAreaStart + KPageSize
		else
			stackTop = stackAreaStart + 2 * KPageSize + USER_STACK_SIZE
		end
	elseif addr >= KKernelStackBase - 100 and addr <= KKernelStackBase + KKernelStackSize then
		stackTop = roundDown(addr, KKernelStackSize) + KKernelStackSize
	else
		stackTop = roundDown(addr, KPageSize) + KPageSize
	end

	local stackMem = newmem(addr, stackTop - addr)
	print(stackMem:words())
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

local PageType = makeEnum(membuf.getType("PageType")._values)

function pageStats()
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
