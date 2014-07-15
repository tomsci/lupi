function main()
	print("[test/yieldb] starting up")
	lupi.yield() -- Allow init to continue

	for i = 1,20 do
		print("[test/yieldb] iteration "..i)
		lupi.yield()
	end
end
