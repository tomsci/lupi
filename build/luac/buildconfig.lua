config = {
	platOpts = "-Os -DLUACONF_FULL_FAT_STDIO",
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

function config.compiler(stage, config, opts)
	opts.compiler = "gcc -arch "..(config.lp64 and "x86_64" or "i386")
	return build.cc(stage, config, opts)
end

function config.link(stage, config, opts)
	local quotedObjs = {}
	for i, obj in ipairs(opts.objs) do
		quotedObjs[i] = build.qrp(obj)
	end
	local out = build.qrp(config.lp64 and "bin/luac64" or "bin/luac")
	local arch = config.lp64 and "x86_64" or "i386"
	local cmd = string.format("gcc -arch %s -Os -o %s %s ", arch, out, build.join(quotedObjs))
	local ok = build.exec(cmd)
	if not ok then error("Link failed!") end
end
