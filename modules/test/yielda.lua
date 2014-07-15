function main()
	print("[test/yielda] starting up")
	lupi.yield() -- Allow init to continue

	for i = 1,20 do
		print("[test/yielda] iteration "..i)
		lupi.yield()
	end
end
