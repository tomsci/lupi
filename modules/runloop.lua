
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

function RunLoop.new()
	local rl = {
		pendingRequests = {},
	}
	setmetatable(rl, RunLoop)
	return rl
end

new = RunLoop.new

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
		assert(numReqs == 0, "Oh no numReqs="..numReqs)
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

function RunLoop:queue(obj)
	obj:setPending()
	table.insert(self.pendingRequests, obj)
	if obj.requestFn then
		obj:requestFn()
	end
end
