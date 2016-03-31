config = {
	platOpts = "-Os -arch i386 -DLUACONF_FULL_FAT_STDIO",
	machine = { "host" },

	fullyHosted = true,

	sources = {
		{ path = "lua/luac.c", user = true },
		-- In order to be consistant between how luac parses constants and
		-- how they are done at runtime, use the same version of strtol in both
		{ path = "usersrc/strtol.c", user = true },
	},

	malloc = true,
	lua = true,
}

function config.link(stage, config, opts)
	local quotedObjs = {}
	for i, obj in ipairs(opts.objs) do
		quotedObjs[i] = build.qrp(obj)
	end
	local out = build.qrp("bin/luac")
	local cmd = string.format("gcc -arch i386 -Os -o %s %s ", out, build.join(quotedObjs))
	local ok = build.exec(cmd)
	if not ok then error("Link failed!") end
end
