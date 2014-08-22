assert(true) -- Ie that assert itself, as something in the global table, is accessible!
assert(_ENV ~= _G)

x = 12345
assert(_G.x == nil)

local interp = require "interpreter"
assert(type(interp) == "table")
assert(_ENV.interpreter == interp)
assert(_G.interpreter == nil) -- but that we don't pollute the global namespace every time a module does a require
assert(debug.traceback ~= nil)
assert(debug.debug == nil)

require "runloop"
require "misc"
function main(...)

	print("I'm still special!\n")
	runloop.new()
	local timerserver = require("timerserver")
	print("Connecting...")
	local t = timerserver.connect()
	print("Test is connected to server")

	t:sendInitMsg(function(result)
		print("Test got response, yay", result);
		t:after(5000, function() print("[test] after completed!") end)
	end)
	testClass()
	runloop.run()
end


local function checkMembersMatch(obj, expected)
	local seenMembers = {}
	for k,v in pairs(obj) do
		table.insert(seenMembers, {k, v})
	end
	table.sort(seenMembers, function(l, r) return l[1] < r[1] end)
	for i, tuple in ipairs(seenMembers) do
		assert(tuple[1] == expected[i][1], "Expected key "..expected[i][1].." got "..tuple[1])
		assert(tuple[2] == expected[i][2], "Expected value "..tostring(expected[i][2]).." got "..tostring(tuple[2]))
	end
	assert(#seenMembers == #expected, "Expected "..#expected.." members, got "..#seenMembers)
end

function testClass()
	local class = misc.class

	local TestClass = class {
		nonOverriddenMember = "kitty",
		member = 1,
	}
	assert(TestClass.member == 1)
	local instance = TestClass()
	assert(instance.member == 1)
	instance = TestClass { member = 2 }
	assert(instance.member == 2)
	assert(TestClass.member == 1)

	TestClass.fn = function(obj) obj.member = 3 end
	assert(instance.fn ~= nil)
	instance:fn()
	assert(instance.member == 3)

	local expected = {
		{ "fn", TestClass.fn },
		{ "member", 3 },
		{ "nonOverriddenMember", "kitty" },
	}
	-- Check pairs works, otherwise memberEnv() won't be able to make functions
	checkMembersMatch(instance, expected)

	local Subclass = class {
		_super = TestClass,
		member = 10,
		submember = 100,
	}
	TestClass.overriddenFn = function(obj) obj.member = 4 end
	Subclass.overriddenFn = function(obj) obj.member = 40 end
	local subinst = Subclass { member = 11 }

	assert(subinst.member == 11)
	subinst:overriddenFn()
	assert(subinst.member == 40)

	local expected = {
		{ "_super", TestClass },
		{ "fn", TestClass.fn },
		{ "member", 40 },
		{ "nonOverriddenMember", "kitty" },
		{ "overriddenFn", Subclass.overriddenFn },
		{ "submember", 100 },
	}
	checkMembersMatch(subinst, expected)

	-- Finally, test member fns
	subinst.member = 12
	local menv = misc.memberEnv(subinst, _ENV)
	menv.overriddenFn()
	assert(subinst.member == 40)
end
