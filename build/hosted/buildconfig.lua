config = {
	cc = "gcc",
	include = "hosted.h",
	machine = { "host" },
	platOpts = "-O0 -g -arch i386",

	sources = {
		"k/debug.c",
		{ path = "build/hosted/entry_point.c", hosted = true },
		{ path = "usersrc/tests.c", user = true },
	},

	klua = true,
}

function link(objs)
	local quotedObjs = {}
	for i, obj in ipairs(objs) do
		quotedObjs[i] = build.qrp(obj)
	end
	local out = build.qrp("bin/lupik")
	local cmd = string.format("gcc -arch i386 -g -o %s %s ", out, build.join(objs))
	local ok = build.exec(cmd)
	if not ok then error("Link failed!") end
end
