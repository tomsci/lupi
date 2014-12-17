require "interpreter"
require "runloop"
require "bit32"
require "luazero"
local at, strmid, rng = luazero.at, luazero.strmid, luazero.rng
local timers = require("timerserver.local")

local interpreterGotChar = interpreter.gotChar

longTimeout = 60
mediumTimeout = 5
shortTimeout = 1

soh = 0x01
stx = 0x02
eot = 0x04
ack = 0x06
nak = 0x15
can = 0x18
sub = 0x1A
iac = 0xFF
cancel = "\x18\x18\x18"

local Session = {}
Session.__index = Session

function crc16(data)
	local crc = 0
	local length = #data
	for i = 1, length do
		crc = bit32.bxor(crc, data:byte(i) * 256)
		for j = 1, 8 do
			if bit32.band(crc, 0x8000) > 0 then
				crc = bit32.bxor(bit32.lshift(crc, 1), 0x1021)
			else
				crc = bit32.lshift(crc, 1)
			end
		end
	end
	return bit32.band(crc, 0xFFFF)
end

function send(val)
	local t = type(val)
	dbg(string.format("Sending %s", tostring(val)))
	if t == "number" then
		putch(val)
	elseif t == "string" then
		for i = 1, #val do
			putch(val:byte(i))
		end
	else
		error("Bad value "..tostring(val))
	end
end

local debugData = {}

function dbg(str)
	table.insert(debugData, string.format("% 8d %s", lupi.getUptime():lo(), str))
end

function dumpDebug()
	for _,s in ipairs(debugData) do
		print(s)
	end
	debugData = {}
end

function fail(str)
	dbg(str)
	lupi.suppressPrints(false)
	error(str)
end

function Session:receiveByte(timeout)
	local ret = self:receiveWithTimeout(1, timeout)
	if ret then ret = at(ret, 0) end
	return ret
end

function Session:receiveWithTimeout(targetLen, timeout)
	local receiveBuf = { n = 0 }
	local timedOut = false
	local function gotByte(b)
		dbg(string.format("gotByte %d", b))
		assert(receiveBuf.n < targetLen) -- Don't think this can happen
		local idx = receiveBuf.n + 1
		receiveBuf[idx] = string.char(b)
		receiveBuf.n = idx
		if idx == targetLen then
			-- Then exit the nested runloop
			receiveBuf.exit = true
		end
	end
	local function timerExpired()
		timedOut = true
		receiveBuf.exit = true
	end
	timers.after(timerExpired, timeout * 1000)
	-- We receive bytes by hijacking the interpreter's gotChar fn. It's easier
	-- to do that than implement proper multiple client support. (The proper
	-- way to do it would be to push the calls to getch_async out of interpreter
	-- and into the 'input' module).
	interpreter.gotChar = gotByte
	-- Need to do this otherwise it'll never be active in the nested run loop
	if not interpreter.getChRequest:isPending() then
		runloop.current:queue(interpreter.getChRequest)
	end
	-- receiveBuf is also used as the exit cond
	runloop.current:run(receiveBuf)
	interpreter.gotChar = interpreterGotChar
	if timedOut then
		dbg("Timed out from receiveWithTimeout")
		return nil
	else
		return table.concat(receiveBuf)
	end
end

function Session:sendSync()
	self.crc = true
	local syncByte = string.byte("C")
	while true do
		for retry = 1, 5 do
			send(syncByte)
			local b = self:receiveByte(mediumTimeout)
			if b == soh then
				dbg("Using 128 byte block size")
				self.blockSize = 128
				return
			elseif b == stx then
				dbg("Using 1024 byte block size")
				self.blockSize = 1024
				return
			elseif b == can then
				send(ack)
				fail("Sender cancelled")
			elseif b == eot then
				dbg("Received EOT")
				send(ack)
				return eot
			elseif b == iac then
				fail("Oh no telnet!")
			end
		end
		-- if syncByte == string.byte("C") then
		-- 	dbg("Sender doesn't support CRC?")
		-- 	self.crc = false
		-- 	syncByte = nak
		-- else
			send(cancel)
			fail("Failed to sync")
		-- end
	end
end

function Session:checkSize()
	return (self.crc and 2 or 1)
end

function Session:protocolOverhead()
	return 3 + self:checkSize()
end

function Session:verifyBlock(block)
	if self.crc then
		local calculatedCrc = crc16(strmid(block, 2, self.blockSize))
		local receivedCrc = at(block, self.blockSize + 2) * 256 + at(block, self.blockSize + 3)
		if calculatedCrc ~= receivedCrc then
			dbg(string.format("crc check failed, calculated %x received %x", calculatedCrc, receivedCrc))
			return false
		end
	else
		local sum = 0
		for i in rng(2, self.blockSize + 2) do
			sum = (sum + at(block, i)) % 256
		end
		local received = at(block, self.blockSize + 2)
		if sum ~= received then
			dbg(string.format("checksum failed, calculated %x received %x", sum, received))
			return false
		end
	end
end

function Session:receiveBlock(firstBlock, repetition)
	dbg("Receiving block")
	local isFinal = false
	if not firstBlock then
		for retry = 1, 5 do
			dbg("Receiving first byte of block")
			local b = self:receiveByte(mediumTimeout)
			if b == nil then
				dbg("Timed out, sending NAK")
				send(nak)
				-- and retry
			elseif b == soh then
				self.blockSize = 128
				break
			elseif b == stx then
				self.blockSize = 1024
				break
			elseif b == eot then
				dbg("Got last block")
				send(ack)
				return "", true
			elseif b == can then
				dbg("Sender cancelled")
				send(ack)
				error("Sender cancelled")
			end
			-- Otherwise, retry
			if retry == 5 then
				fail("Failed to receive valid block")
			end
		end
	end

	dbg("Receiving remainder of block")
	local block = self:receiveWithTimeout(self.blockSize + self:protocolOverhead() - 1, longTimeout) -- minus one because we've read the first byte already
	if block == nil then
		dbg("Timed out, retrying")
		send(nak)
		return self:receiveBlock(false, (repetition or 0) + 1)
	end

	local b0, b1 = block:byte(1, 2)
	-- I don't know why the packetNumber-1 check is in here!
	if b0 == bit32.band(bit32.bnot(b1), 0xFF) and (b0 == self.packetNumber or b0 == self.packetNumber - 1) and self:verifyBlock(block) then
		dbg("Got block OK")
		send(ack)
		if at(block, 0) == self.packetNumber then
			dbg(string.format("Packet number matched (%d)", self.packetNumber))
			send(ack)
			self.packetNumber = self.packetNumber + 1
			-- Strip header and checksum
			return strmid(block, 2, #block - self:checkSize() - 2)
		else
			-- Do not understand logic here. As far as I can tell, if we got the previous packet again,
			-- we ack it and return an empty block. So basically we gloss over an undesired retransmit
			return "", false
		end
	elseif repetition >= 5 then
		send(cancel)
		fail("Block not successfully received after 5 retries, cancelling")
	else
		dbg("Block not successfully received, retrying")
		send(nak)
		return self:receiveBlock(false, (repetition or 0) + 1)
	end
	return block, isFinal
end

function receive()
	assert(runloop.current, "Must have a runloop to call receive")
	local session = {}
	setmetatable(session, Session)
	local finished = false
	local receivedFiles = {}
	lupi.suppressPrints(true)
	while not finished do
		if session:sendSync() == eot then
			finished = true
			break
		end
		self.packetNumber = 0
		-- First packet contains just filename and size
		local block, isFinal = self:receiveBlock(true)
		local filename = block:match("^([^\0]*)\0")
		if filename then
			local size = block:match("^(0-9+)", #filename + 1 + 1)
			assert(size, "Failed to extract size from packet 0")
			size = tonumber(size)
			--local fileBytes = 0
			while not isFinal do
				block, isFinal = self:receiveBlock(false)
				--fileBytes = fileBytes + #block
				-- TODO do something with the data
			end
			dbg("Finished receiving file")
			table.insert(receivedFiles, filename)
		else
			-- Empty filename indicates session finished
			finished = true
		end
	end
	lupi.suppressPrints(false)
	if #receivedFiles == 1 then
		print(string.format("Received file %q ok.", receivedFiles[1]))
	end
end
