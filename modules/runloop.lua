require "misc"

--[[**
The run loop is responsible for dispatching asynchronous callbacks on the
calling thread. It's like pretty much every other event loop ever.

Individual tasks are represented by `AsyncRequest` objects which are constructed
using the `RunLoop:newAsyncRequest()` API (or some higher-level equivalent).
AsyncRequests must define at a minimum, a `completionFn` which is called when
the request is completed. It has the signature

	function completionFn(asyncRequestObj, result)
		-- result will currently always be an integer
	end

and is called using `pcall` so calling `error()` from within a completion
function is allowable and will cause a stacktrace to be printed. The second
member that AsyncRequests may define is a `requestFn`. This is optional, and is
useful for repeating requests - if defined the run loop will automatically call
it when you `queue()` the object and again whenever the request completes, so
you don't have to bother doing it yourself. A request that does not define a
`requestFn` is responsible for calling `Runloop:queue(obj)` followed by whatever
the asynchronous function is, every time. It is an error to pass to a function
an `AsyncRequest` that has not been `queue()d`.

`AsyncRequest` objects are backed by a C `struct AsyncRequest` and may
therefore be used in C API calls.

Example usage:

	require "runloop"

	function main()
		-- Create run loop
		local rl = runloop.new()

		-- Set up some async sources
		local chreq = rl:newAsyncRequest({
			completionFn = someFunction,
			requestFn = lupi.getch_async,
		})

		-- Add them to the run loop
		rl:queue(chreq)

		-- Finally, start the run loop
		rl:run()
	end
]]

-- AsyncRequest and RunLoop objects setup by native code

function AsyncRequest:__index(key)
	local members = AsyncRequest.getMembers(self)
	local m = members[key]
	if m ~= nil then return m end
	return AsyncRequest[key]
end

function AsyncRequest:__newindex(key, value)
	AsyncRequest.getMembers(self)[key] = value
end

--[[**
Returns the members table for a specific async request. This is used internally
by `AsyncRequest`.
]]
--native function AsyncRequest.getMembers(asyncRequest)

--[[**
If the request has been completed, returns the result otherwise nil. The non-nil
result will currently always be an integer.
]]
--native function AsyncRequest:getResult()

--[[**
Sets the result of the request, depending on the type of the argument:

* If result is nil, clears the `completed` flag. No I'm not sure why you'd need
  this either.
* If result is an integer, sets the `KAsyncFlagIntResult` flag.
* Any other type will (currently) cause an error.
]]
--native function AsyncRequest:setResult(result)

--[[**
Sets the pending flag. Is called automatically by
[RunLoop:queue(obj)](#RunLoop_queue).
]]
--native function AsyncRequest:setPending()

--[[**
Returns true if the message is not in use (from the kernel's point of view).
]]
--native function AsyncRequest:isFree()

--[[**
Clears all flags on the request. Called by the runloop just before the request's
`completionFn` is called.
]]
--native function AsyncRequest:clearFlags()

--[[**
Creates a new RunLoop.
]]
function RunLoop.new()
	local rl = {
		pendingRequests = {},
	}
	setmetatable(rl, RunLoop)
	if not current then
		current = rl
	end
	return rl
end

new = RunLoop.new
-- current set by RunLoop.new

--[[**
Convenience function equivalent to `runloop.current:run()`.
]]
function run()
	assert(current, "No runloop constructed")
	current:run()
end

--[[**
Starts the event loop running. Does not return.
]]
function RunLoop:run()
	assert(#self.pendingRequests > 0,
		"Starting a run loop with no pending requests makes no sense...")
	local waitForAnyRequest = self.waitForAnyRequest
	local queue = self.queue
	local function innerLoop()
		while true do
			local numReqs = waitForAnyRequest()
			-- Now search the list and complete exactly numReqs requests
			for i, req, removeReqFromTable in misc.iter(self.pendingRequests) do
				if numReqs == 0 then
					break
				end
				local r = req:getResult()
				if r ~= nil then
					-- req has completed
					numReqs = numReqs - 1
					removeReqFromTable()
					--self:handleCompletion(req, r)
					local fn = req.completionFn or error("No completion function for AsyncRequest!")
					req:clearFlags()
					fn(req, r)
					if req.requestFn then
						queue(self, req) -- This will call requestFn
					end
				end
			end
			if numReqs ~= 0 then
				print("Uh oh numReqs not zero at end of RunLoop:"..numReqs)
				print("Pending requests:")
				for _, req in ipairs(self.pendingRequests) do
					print(tostring(req))
				end
				error("Bad numReqs")
			end
		end
	end
	while true do
		-- Use a nested inner loop so we don't have to call xpcall every time
		-- we service a new completion
		local ok, err = xpcall(innerLoop, debug.traceback)
		if not ok then
			print(err)
		end
	end
end

--[[**
Creates a new async request. Does *not* automatically call `queue`. The
C memory allocation for the `AsyncRequest` struct will be increased by
`extraSize` if it is specified.
]]
--native function RunLoop:newAsyncRequest([members, [extraSize]])

--[[**
Sets the given async request as pending. If `obj` has a `requestFn`, this
function is called with `obj` as the only argument. If it doesn't, the caller is
responsible for making the request as appropriate *after* calling `queue`.
]]
function RunLoop:queue(obj)
	obj:setPending()
	table.insert(self.pendingRequests, obj)
	if obj.requestFn then
		obj:requestFn()
	end
end

--[[**
Blocks the thread until any request completes. Returns the number of completed
requests (which will always be >= 1).
]]
--native function RunLoop:waitForAnyRequest()
