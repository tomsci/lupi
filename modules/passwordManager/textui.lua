require "runloop"

function main()
	local rl = runloop.new()
	local chreq = rl:newAsyncRequest({
		completionFn = function(req, ch)
			if gotChar == nil then
				print("Got a char in an unexpected state")
			else
				gotChar(ch)
			end
		end,
		requestFn = lupi.getch_async,
	})
	rl:queue(chreq)
	displayMainMenu()
	rl:run()
end

keychainItems = {
	"http://www.foobar.com",
	"http://amazon.co.uk",
	"ssid:WoopWifi",
}

function displayMainMenu()
	print([[
Password manager
================

a - add new password
l - lookup

Select an operation: [al] ]])

	gotChar = function(ch)
		ch = string.char(ch)
		if ch == 'l' then
			print(ch)
			displayLookupMenu()
		end
	end
end

function displayLookupMenu()
	print([[

Lookup an item
   0. (or ESC) Back to previous menu]])

	-- TODO proper impl
	for i, item in ipairs(keychainItems) do
		printf("% 4d. %s", i, item)
	end
	gotChar = function(ch)
		local idx = ch - string.byte('0')
		if idx == 0 or ch == 27 then
			displayMainMenu()
		elseif idx > 0 and idx <= #keychainItems then
			displayItemMenu(keychainItems[idx])
		end
	end
end

function displayItemMenu(item)
	printf([[

Keychain item: %s

p, ENTER - print password
       d - delete item
  0, ESC - back
]], item)
	gotChar = function(ch)
		if ch == 27 or ch == string.byte('0') then
			displayLookupMenu()
		elseif ch == string.byte('p') or ch == 13 then
			print("TODO magic")
		end
	end
end
