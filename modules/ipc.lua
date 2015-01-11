require "membuf"
require "runloop"

------------ Server functions ------------

-- Incoming message from a client
local function doHandleMsg(msg, cmd)
	local dataPos, len = getMsgData(msg.page, msg.index)
	if len > 4*4 then
		error("Message too big!")
	end
	local decodedMsg = {
	}
	for i=0,len,4 do -- for (i = 0; i <= len; i+= 4)
		table.insert(decodedMsg, msg.page:getInt(dataPos + i))
	end

	if msg.server.fns[cmd] then
		msg.server.fns[cmd](msg, table.unpack(decodedMsg))
	else
		error("No handler defined for cmd "..cmd)
	end
end

local function handleMsg(msg, val)
	local ok, err = xpcall(doHandleMsg, debug.traceback, msg, val)
	-- Make sure we always requeue the msg before erroring
	msg.server.runloop:queue(msg)
	if not ok then error(err, 0) end
end

local function handleServerMsg(server, ptr)
	--[[
	The result of a server message is either a shared page address (in which case it's a
	connection message, OR .... something else! (dum dum DUUUUM)
	]]
	--printf("Got server message %x", ptr)
	if ptr & 0xFFF == 0 then
		-- Shared page address, connection message from new client
		local p = getSharedPage(ptr)
		server.clientSharedPages[ptr] = p
		local msgs = setupMsgsForClient(p)
		for i, msg in ipairs(msgs) do
			msg.completionFn = handleMsg
			msg.server = server
			msg.index = i-1 -- msg indexes are relative to position in the C IpcPage struct
			msg.page = p
			-- All client msgs are pending at all times
			server.runloop:queue(msg)
		end
	else
		error("unrecognised message "..ptr)
	end
end

function startServer(rl, name, fns)
	local server = rl:newAsyncRequest({
		completionFn = handleServerMsg,
		requestFn = requestServerMessage,
		name = name,
		fns = fns,
		clientSharedPages = {},
		runloop = rl,
	})
	doCreateServer(name)
	rl:queue(server)
	return server
end

--[[**
Completes the given message. `result` must be an integer and `msg` must be a
server-side msg.
]]
--native function complete(msg, result)

------------ Client functions ------------

local function msgCompleted(msg, result)
	local fn = msg.ipcCompletionFn
	msg.ipcCompletionFn = nil
	fn(result)
end

function connect(serverName, numMessages)
	-- First, get a new page for doing IPC
	if numMessages == nil then numMessages = 1 end
	local ipcPage = newSharedPage() -- this is a MemBuf
	ipcPage:setInt(0, numMessages)

	local id, msgs = connectToServer(serverName, ipcPage --[[, msgs]])
	if id < 0 then
		error(string.format("Error %d connecting to server %s", id, serverName))
	end
	-- Hook up the returned msgs (a table of AsyncRequests pointing into the ipcPage)
	-- to behave like real runloop objects
	for _, msg in ipairs(msgs) do
		msg.completionFn = msgCompleted
	end

	local session = {
		ipcPage = ipcPage,
		id = id,
		msgs = msgs,
		runloop = runloop.current,
	}
	setmetatable(session, Session)
	return session
end

function serialiseToPage(server, args)
	-- Currently the only supported args are an array of ints [1]-[4]
	local page = server.ipcPage
	-- Work out where in the page to put the data
	-- TODO! For now always put at end
	local startOfData = server.endPtr or 32
	local max = page:getLength()
	if startOfData + 4 * 4 > max then
		error("Page full, aargh")
	end
	local pos = startOfData
	for i,v in ipairs(args) do
		if i > 4 then error("Too many parameters to IPC!") end
		page:setInt(pos, v)
		pos = pos + 4
	end
	server.endPtr = pos
	return startOfData, server.endPtr - startOfData
end

function send(session, cmd, args, completionFn)
	if type(args) == "function" then
		-- Skipped the args
		completionFn = args
		args = nil
	end
	if session.runloop then
		assert(completionFn, "Session:send() is asynchronous and must be supplied a completionFn")
	else
		assert(completionFn == nil, "Cannot specify a completionFn to Session:send() when there is no run loop")
	end
	-- Find a free message - note this is actually the response msg not the request msg,
	-- because the response is what we're interested in our address space. The C code
	-- is smart enough to do the right thing and actually complete the request msg.
	local msg
	for i, m in ipairs(session.msgs) do
		if m:isFree() then
			msg = m
			break
		end
	end
	assert(msg, "No free message available")
	local startOfData, len = 0, 0
	if args then
		startOfData, len = serialiseToPage(session, args)
	end

	if session.runloop then
		session.runloop:queue(msg)
	end
	msg.ipcCompletionFn = completionFn
	doSendMsg(msg, cmd, startOfData, len)
end
