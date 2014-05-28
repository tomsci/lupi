
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
Returns the members table for a specific async request
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

function run()
	assert(current, "No runloop constructed")
	current:run()
end

local function iter(tbl)
	local deleted = false
	local i = 0
	local function remove()
		table.remove(tbl, i)
		deleted = true
	end
	local function f(state, lastIdx)
		if deleted then
			i = lastIdx
			deleted = false
		else
			i = lastIdx + 1
		end
		local obj = tbl[i]
		if obj == nil then return nil end
		return i, obj, remove
	end
	return f, tbl, 0
end

function RunLoop:run()
	while true do
		local numReqs = self:waitForAnyRequest()
		-- Now search the list and complete exactly numReqs requests
		for i, req, removeReqFromTable in iter(self.pendingRequests) do
			if numReqs == 0 then
				break
			end
			local r = req:getResult()
			if r ~= nil then
				-- req has completed
				numReqs = numReqs - 1
				removeReqFromTable()
				self:handleCompletion(req, r)
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

function RunLoop:handleCompletion(obj, result)
	local fn = obj.completionFn or error("No completion function for AsyncRequest!")
	obj:clearFlags()
	local ok, err = xpcall(obj.completionFn, debug.traceback, obj, result)
	if not ok then
		print(err)
	elseif obj.requestFn then
		self:queue(obj) -- This will call requestFn
	end
end

--[[**
Creates a new async request. Does *not* automatically call `queue`.
]]
--native function RunLoop:newAsyncRequest(membersOrNil)

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
