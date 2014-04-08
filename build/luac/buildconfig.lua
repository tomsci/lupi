config = {
	cc = "gcc",
	platOpts = "-Os -arch i386 -DLUACONF_FULL_FAT_STDIO",
	machine = { "host" },

	fullyHosted = true,

	sources = {
		{ path = "lua/luac.c", user = true },
	},

	lua = true,
}

function link(objs)
	local quotedObjs = {}
	for i, obj in ipairs(objs) do
		quotedObjs[i] = build.qrp(obj)
	end
	local out = build.qrp("bin/luac")
	local cmd = string.format("gcc -arch i386 -Os -o %s %s ", out, build.join(objs))
	local ok = build.exec(cmd)
	if not ok then error("Link failed!") end
end
