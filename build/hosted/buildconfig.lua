config = {
	cc = "gcc",
	include = "hosted.h",
	platOpts = "-O0 -g -arch i386",

	extraKernelSources = {
		{ path = "build/hosted/entry_point.c", hosted = true },
		{ path = "usersrc/tests.c", user = true },
	},

	klua = true,
}
