#!/usr/local/bin/lua

if _VERSION ~= "Lua 5.2" then
	error "Lua 5.2 is required to run this script"
end
local cmdargs = arg

local env = {}
local envMt = { __index = _G }
setmetatable(env, envMt)
_ENV = env

baseDir = nil -- set up in calculateBaseDir()
verbose = false
listing = false
preprocess = false
config = nil
compileModules = false
jobs = { n = 0 }
maxJobs = 1
debugSchedulingEntryPoint = false
bootMode = 0

local function loadConfig(c)
	local env = {
		build = _ENV,
	}
	local envMt = {
		__index = _G,
	}
	setmetatable(env, envMt)
	local f = assert(loadfile(baseDir.."build/"..c.."/buildconfig.lua", nil, env))
	f()
	local config = env.config
	if config.toolchainPrefix and not config.cc then
		config.cc = config.toolchainPrefix .. "gcc"
		config.as = config.toolchainPrefix .. "as"
		config.ld = config.toolchainPrefix .. "ld"
		config.objcopy = config.toolchainPrefix .. "objcopy"
		config.objdump = config.toolchainPrefix .. "objdump"
		config.readelf = config.toolchainPrefix .. "readelf"
	end
	config.name = c
	if config.lua == nil then
		config.lua = config.klua or config.ulua
	end
	if not config.link then
		config.link = env.link
	end

	return env.config
end

function exec(cmd)
	if verbose then print(cmd) end
	return os.execute(cmd)
end

function waitForJob(job)
	local ok, exitType, num = job.handle:close()
	if not ok or num ~= 0 then
		print("Job failed: "..job.cmd)
		error("Build aborted due to failed job")
	end
end

function parallelExec(cmd)
	if maxJobs == 1 then return exec(cmd) end

	if verbose then print(cmd) end
	if jobs.n >= maxJobs then
		-- Wait for oldest job to finish - not the finest strategy but the best we can
		-- really do without notifications or a non-blocking isFinished API
		waitForJob(jobs[1])
		table.remove(jobs, 1)
	else
		jobs.n = jobs.n + 1
	end
	local h = io.popen(cmd, "r")
	table.insert(jobs, {cmd = cmd, handle = h})
	return true
end

function waitForAllJobs()
	for _, job in ipairs(jobs) do
		waitForJob(job)
	end
	jobs = { n = 0 }
end

function mkdir(relPath)
	exec("mkdir -p "..qrp(relPath))
end

function quote(path)
	return string.format("%q", path)
end

--# quoted relative path
function qrp(relPath)
	return quote(baseDir..relPath)
end

function join(array)
	return table.concat(array or {}, " ")
end

function prefixAndFlatten(option, includes)
	local result = {}
	for i, inc in ipairs(includes) do
		result[i] = option..inc
	end
	return join(result)
end

function flattenOpts(opts)
	local result = {}
	local i = 1
	for k,v in pairs(opts) do
		result[i] = k .. "=" .. v
		i = i+1
	end
	return join(result)
end

function objForSrc(source, suffix)
	local dir = "bin/obj-"..config.name.."/"
	local newname = source:gsub("%.%a+$", suffix or ".o")
	if source:sub(1, #dir) == dir then
		--# It's already in the obj dir, just replace suffix
		return newname
	else
		return dir..newname
	end
end

function makePathComponents(path)
	local pathComponents = { path:match("^/") and "" }
	for component in path:gmatch("[^/]+") do
		table.insert(pathComponents, component)
	end
	return pathComponents
end

function makeRelativePath(path, relativeTo)
	local pathComponents = makePathComponents(path)
	local relativeToComponents = makePathComponents(relativeTo)
	-- Remove common parents
	while (pathComponents[1] == relativeToComponents[1] and pathComponents[1] ~= nil) do
		table.remove(pathComponents, 1)
		table.remove(relativeToComponents, 1)
	end
	local result = string.rep("../", #relativeToComponents - 1) .. (table.concat(pathComponents, "/"))
	return result
end

function makeAbsolutePath(path, relativeTo)
	if path:match("^/") then return path end -- already absolute
	local pc = makePathComponents(relativeTo)
	for _, c in ipairs(makePathComponents(path)) do
		table.insert(pc, c)
	end
	-- Normalise
	local i = 1
	while i <= #pc do
		if pc[i] == "." then
			table.remove(pc, i)
		elseif pc[i] == ".." and i > 1 then
			table.remove(pc, i)
			table.remove(pc, i-1)
			i = i - 1
		else
			i = i + 1
		end
	end
	result = table.concat(pc, "/")
	return result
end

function lastPathComponent(path)
	local pc = makePathComponents(path)
	return pc[#pc]
end

function removeLastPathComponent(path)
	local pc = makePathComponents(path)
	return table.concat(pc, "/", 1, #pc - 1)
end

function removeExtension(path)
	return path:match("(.*)%.(.*)") or path
end

local function machineIs(m)
	for _, type in ipairs(config.machine or {}) do
		if m == type then return true end
	end
	return false
end


function assemble(file)
	assert(file:match("%.s$"), "Can only assemble .s files")
	local obj = objForSrc(file)
	local cmd = string.format("%s %s -o %s ", config.as, qrp(file), qrp(obj))
	local ok = parallelExec(cmd)
	if not ok then
		error("Assemble failed for "..source, 2)
	end
	return obj
end

function compilec(source, extraArgs)
	--# source can be a string if there's no additional options needed
	if type(source) == "string" then
		source = { path = source }
	end

	local overallOpts = preprocess and "-E" or source.listing and "-S" or "-c"
	local sysOpts = (source.hosted or config.fullyHosted) and "-ffreestanding" or "-ffreestanding -nostdinc -nostdlib"
	if listing then
		--# Debug is required to do interleaved listing
		sysOpts = sysOpts .. " -g"
	end
	local langOpts = "-std=c99 -funsigned-char -Wall -Werror -Wno-error=unused-function"
	local platOpts = config.platOpts or ""

	local extraArgsString = join(extraArgs)

	local suff = preprocess and ".i" or source.listing and ".s" or ".o"
	local obj = objForSrc(source.path, suff)
	local output = "-o "..qrp(obj)
	local opts = join {
		overallOpts,
		sysOpts,
		langOpts,
		platOpts,
		join(source.copts),
		extraArgsString,
		output
	}

	local cmd = string.format("%s %s %s ", config.cc, opts, qrp(source.path))
	local ok = parallelExec(cmd)
	if not ok then
		error("Compile failed for "..source.path, 2)
	end
	if listing and not source.listing and not source.path:match("%.S$") then
		--# Rerun the fun with listing enabled. Bit of a hack this.
		--# Don't do it for *.S files, gcc gets confused and dumps them to stdout
		source.listing = true
		compilec(source, extraArgs)
	end
	return obj
end

function calculateUniqueDirsFromSources(prefix, sources)
	local result = {}
	for _, source in ipairs(sources) do
		local path = type(source) == "string" and source or source.path
		local dir = path:gsub("/[^/]+$", "")
		if not (#prefix > 0 and dir:sub(1, #prefix) == prefix) then
			--# If the source matches the prefix it's assumed not to be relevant
			--# This is so autogenerated sources in the bin dir (which is prefix) don't cause lots
			--# of unnecessary empty dirs to get created
			result[prefix..dir] = 1
		end
	end
	return result
end

local function findLibgcc()
	-- First find where gcc is
	local h = io.popen("which "..config.cc)
	local path = h:read("*l")
	assert(h:close())
	-- Then look for libgcc in ../lib/gcc relative to the gcc binary
	local binDir = removeLastPathComponent(path)
	local libDir = makeAbsolutePath("../lib/gcc", binDir)
	-- Search for a libgcc.a in a dir matching one of the entries in
	-- config.machine. Have to do repeated searches to ensure we pick up the
	-- most specific one
	local foundLib
	for i = #config.machine, 1, -1 do
		local path = string.format("*/%s/libgcc.a", config.machine[i])
		local cmd = string.format("find %s -path %s", quote(libDir), quote(path))
		h = io.popen(cmd)
		for p in h:lines() do
			foundLib = p
		end
		assert(h:close())
		if foundLib then break end
	end
	assert(foundLib, "Cannot locate libgcc.a")
	return foundLib
end

function build_kernel()
	local sysIncludes = {
		"k/inc",
		"userinc/lupi",
	}
	local sources
	if machineIs("host") then
		--# We make no assumptions about what host wants to compile by default
		sources = {}
	else
		sources = {
			{ path = "k/boot.c", copts = { "-DBOOT_MODE="..bootMode } },
			"k/debug.c",
			--"k/lock.c",
			"k/pageAllocator.c",
			"k/process.c",
			"k/scheduler.c",
			"k/svc.c",
			"k/kipc.c",
		}
	end
	if machineIs("arm") then
		table.insert(sources, "k/mmu_arm.c")
	end

	for _, src in ipairs(config.sources or {}) do
		table.insert(sources, src)
	end
	local userIncludes = {
		"-isystem "..qrp("userinc"),
		"-isystem "..qrp("lua"),
		"-I "..qrp("luaconf"),
	}
	if config.fullyHosted then
		table.remove(userIncludes, 1) -- Don't include userinc
	end
	if config.extraStdInc then
		table.insert(userIncludes, 1, "-isystem "..qrp(config.extraStdInc))
	end
	if config.userInclude then
		table.insert(userIncludes, 1, "-include "..qrp("build/"..config.name.."/"..config.userInclude))
	end

	local includes = {}
	local includeModules = config.klua or config.ulua
	if config.lua then
		for _,src in ipairs(luaSources) do
			table.insert(sources, { path = src, user = true })
		end
		if includeModules then
			if config.ulua and config.klua then
				table.insert(luaModules, "modules/kluadebugger.lua")
			end
			if bootMode ~= 0 then
				table.insert(sources, "k/bootmenu.c")
				table.insert(sources, "testing/atomic.c")
			end
			for _, src in ipairs(generateLuaModulesSource()) do
				table.insert(sources, { path = src, user = true })
			end
		end
		if machineIs("arm") then
			table.insert(sources, { path = "usersrc/memcpy_arm.S", user = true })
			table.insert(sources, { path = "usersrc/memcmp_arm.S", user = true })
		end
		if not config.fullyHosted then
			table.insert(sources, { path = "usersrc/crt.c", user = true })
			table.insert(sources, { path = "usersrc/uklua.c", user = true })
		end
		if config.ulua then
			table.insert(sources, mallocSource)
			if config.klua then
				--# ulua and klua can both be true when we're using klua as a kernel debugger
				--# In this config we're primarily ulua (don't define KLUA in the kernel) but
				--# klua.c still gets compiled
				table.insert(includes, "-DKLUA_DEBUGGER")
			end
		elseif config.klua then
			table.insert(includes, "-DKLUA")
			table.insert(userIncludes, "-DLUACONF_USE_PRINTK")
		end
	end
	if config.ulua then
		table.insert(sources, { path = "usersrc/ulua.c", user = true })
		table.insert(sources, { path = "usersrc/uexec.c", user = true })
		-- Add any modules with native code
		for _, module in ipairs(luaModules) do
			if type(module) == "table" and module.native then
				table.insert(sources, { path = module.native, user = true })
			end
		end
	end
	if debugSchedulingEntryPoint then
		table.insert(sources, { path = "testing/debugSchedulingEntryPoint.c", user = true })
		table.insert(userIncludes, "-DDEBUG_CUSTOM_ENTRY_POINT")
	end

	if config.include then
		table.insert(includes, "-include "..qrp("build/"..config.name.."/"..config.include))
	end
	for _,inc in ipairs(sysIncludes) do
		table.insert(includes, "-isystem")
		table.insert(includes, qrp(inc))
	end

	local dirs = calculateUniqueDirsFromSources("bin/obj-"..config.name.."/", sources)
	for dir, _ in pairs(dirs) do
		mkdir(dir)
	end

	local objs = {}
	if config.entryPoint then
		--# Make sure it gets compiled first
		local obj
		if config.entryPoint:match("%.s$") then
			obj = assemble(config.entryPoint)
		else
			obj = compilec(config.entryPoint, includes)
		end
		table.insert(objs, obj)
	end

	if config.klua then
		table.insert(sources, { path = "usersrc/kluaHeap.c", user = true })
		--# klua.c is a special case as it sits between kernel and user code, and needs to access bits of both. It is primarily user (has user includes) but also has access to the platform -include file
		local includes = { }
		if config.ulua then
			table.insert(includes, "-DULUA_PRESENT")
			table.insert(includes, "-DKLUA_DEBUGGER")
		end
		if config.include then
			table.insert(includes, "-include "..qrp("build/"..config.name.."/"..config.include))
		end
		table.insert(includes, "-isystem "..qrp("userinc"))
		table.insert(includes, "-I "..qrp("luaconf"))
		table.insert(includes, "-isystem "..qrp("lua"))
		table.insert(includes, "-isystem "..qrp("k/inc"))

		local obj = compilec({
			path = "usersrc/klua.c",

		}, includes)
		table.insert(objs, obj);
	end

	for i, source in ipairs(sources) do
		if type(source) == "string" then
			source = { path = source }
		end
		if source.path:match("%.s$") then
			table.insert(objs, assemble(source.path))
		else
			table.insert(objs, compilec(source, source.user and userIncludes or includes))
		end
	end

	waitForAllJobs()

	if preprocess then
		--# We're done
		return
	end

	if config.link then
		config.link(objs)
	else
		--# The proper code - link time!
		local outDir = "bin/" .. config.name .. "/"
		mkdir(outDir)
		local elf = outDir .. "kernel.elf"
		local args = {}
		for i, obj in ipairs(objs) do
			args[i] = qrp(obj)
		end
		if config.klua or config.ulua then
			-- I need your clothes, your boots and your run-time EABI helper functions
			table.insert(args, findLibgcc())
		end
		-- The only BSS we have is userside, so we can set to a user address
		table.insert(args, "-Ttext 0xF8008000 -Tbss 0x00007000")
		local cmd = string.format("%s %s -o %s", config.ld, join(args), qrp(elf))
		local ok = exec(cmd)
		if not ok then error("Link failed!") end

		local img = outDir .. "kernel.img"
		local cmd = string.format("%s %s -O binary %s", config.objcopy, qrp(elf), qrp(img))
		local ok = exec(cmd)
		if not ok then error("Objcopy failed!") end

		local readElfOutput = outDir.."kernel.txt"
		cmd = string.format("%s -a %s > %s", config.readelf, qrp(elf), qrp(readElfOutput))
		ok = exec(cmd)
		if not ok then error("Readelf failed!") end
		checkElfSize(readElfOutput, 0xF8008000, 0x00040000)

		if listing then
			cmd = string.format("%s -d --section .text --section .rodata --source -w %s > %s", config.objdump, qrp(elf), qrp(outDir .. "kernel.s"))
			exec(cmd)
		end
	end
end

luaSources = {
	"lua/lapi.c",
	"lua/lcode.c",
	"lua/lctype.c",
	"lua/ldebug.c",
	"lua/ldo.c",
	"lua/ldump.c",
	"lua/lfunc.c",
	"lua/lgc.c",
	"lua/llex.c",
	"lua/lmem.c",
	"lua/lobject.c",
	"lua/lopcodes.c",
	"lua/lparser.c",
	"lua/lstate.c",
	"lua/lstring.c",
	"lua/ltable.c",
	"lua/ltm.c",
	"lua/lundump.c",
	"lua/lvm.c",
	"lua/lzio.c",
	"lua/lauxlib.c",
	"lua/lbaselib.c",
	"lua/lbitlib.c",
	"lua/lcorolib.c",
	"lua/ldblib.c",
	--"lua/liolib.c",
	--"lua/lmathlib.c",
	--"lua/loslib.c",
	"lua/lstrlib.c",
	"lua/ltablib.c",
	"lua/loadlib.c",
	"lua/linit.c",
}

luaModules = {
	"modules/init.lua",
	"modules/common.lua",
	"modules/test.lua",
	"modules/interpreter.lua",
	"modules/spin.lua",
	{ path = "modules/membuf.lua", native = "usersrc/membuf.c" },
	{ path = "modules/int64.lua", native = "usersrc/int64.c" },
	{ path = "modules/runloop.lua", native = "usersrc/runloop.c" },
	{ path = "modules/ipc.lua", native = "usersrc/ipc.c" },
	{ path = "modules/timerserver/init.lua" },
	{ path = "modules/timerserver/server.lua", native = "modules/timerserver/timers.c" },
}

mallocSource = {
	path = "usersrc/malloc.c",
	user = true,
	copts = {
		"-DHAVE_MMAP=0",
		"-DHAVE_MREMAP=0",
		"-DHAVE_MORECORE=1",
		"-DMORECORE=sbrk",
		"-DUSE_BUILTIN_FFS=1",
		"-DLACKS_UNISTD_H",
		"-DLACKS_SYS_PARAM_H",
		"-DNO_MALLOC_STATS=1", -- Avoids fprintf dep
		"-DMALLOC_FAILURE_ACTION=", -- no errno
	},
}

function generateLuaModulesSource()
	local dirs = calculateUniqueDirsFromSources("bin/obj-"..config.name.."/", luaModules)
	for dir, _ in pairs(dirs) do
		mkdir(dir)
	end

	local results = {}
	local modulesMap = {}
	for i, luaModule in ipairs(luaModules) do
		if type(luaModule) == "string" then luaModule = { path = luaModule } end
		local module = luaModule.path
		local modName = module:gsub("^modules/(.*).lua", "%1"):gsub("/", ".")
		local cname = "KLua_module_"..modName:gsub("%W", "_")
		local nativeFn = luaModule.native and "init_module_" .. modName:gsub("%W", "_")
		local moduleEntry = { name = modName, src = luaModule.path, cname = cname, nativeInit = nativeFn}
		table.insert(modulesMap, moduleEntry)
		if compileModules then
			local obj = objForSrc(module, ".luac")
			local compileCmd = "bin/luac -o "..qrp(obj).." "..module
			local ok = parallelExec(compileCmd)
			if not ok then error("Failed to precompile module "..module) end
			moduleEntry.obj = obj

		else
			--# Include modules as plain text
			local f = assert(io.open(baseDir..module, "r"))
			local backslash = '\\'
			local doubleBackslash = backslash:rep(2)
			local newline = '\n'
			local escapedNewline = [[\n\]]..newline
			local escapedQuote = [[\"]]
			--# Thank goodness backslash isn't a special char in lua gsub!
			local rep = {
				[backslash] = doubleBackslash,
				[newline] = escapedNewline,
				['"'] = escapedQuote,
			}
			local data = f:read("*a")
			moduleEntry.size = #data
			moduleEntry.str = data:gsub('[\\"\n]', rep)
			f:close()
		end
	end

	if compileModules then
		-- We have to wait for the luac jobs to complete
		waitForAllJobs()
		for _, moduleEntry in ipairs(modulesMap) do
			local f = assert(io.open(moduleEntry.obj, "r"))
			local rep = function(char)
				return string.format([[\x%02x]], string.byte(char))
			end
			local data = f:read("*a")
			moduleEntry.size = #data
			moduleEntry.str = data:gsub(".", rep)
			f:close()
		end
	end
	for _, moduleEntry in ipairs(modulesMap) do
		local outName = objForSrc(moduleEntry.src, ".c")
		local outf = assert(io.open(baseDir..outName, "w+"))
		outf:write("// Autogenerated from "..moduleEntry.src.."\n")
		outf:write("const char "..moduleEntry.cname.."[] = \"\\\n")
		outf:write(moduleEntry.str)
		outf:write('";\n')
		outf:close()
		table.insert(results, outName)
	end

	local modEntryPoint = objForSrc("modules/entryPoint.c", ".c")
	local f = assert(io.open(baseDir..modEntryPoint, "w+"))
	local fn = {
		"",
		"static const LuaModule modules[] = {"
	}
	f:write([[
// Autogenerated from modules list
#include <lupi/module.h>
#include <stddef.h>
#include <string.h>

]])

	local post = [[
const LuaModule* getLuaModule(const char* moduleName) {
	for (int i = 0; i < sizeof(modules)/sizeof(LuaModule); i++) {
		if (strcmp(modules[i].name, moduleName) == 0) {
			return &modules[i];
		}
	}
	return NULL;
}
]]

	local modFmt = '\t{ .name="%s", .data=%s, .size=%d, .nativeInit=%s },'
	for _, mod in pairs(modulesMap) do
		f:write("extern const char "..mod.cname.."[];\n")
		if mod.nativeInit then
			f:write("extern int "..mod.nativeInit.."(lua_State* L);\n")
		end
		table.insert(fn, modFmt:format(mod.name, mod.cname, mod.size, mod.nativeInit or "NULL"))
	end
	table.insert(fn, "};\n\n")
	f:write(table.concat(fn, "\n"))
	f:write(post)
	f:close()
	table.insert(results, modEntryPoint)

	waitForAllJobs() -- To make sure the c files have been created before we try to compile them
	return results
end

function checkElfSize(readElfOutput, codeBase, maxCodeSize)
	local f = assert(io.open(readElfOutput))
	local found
	for line in f:lines() do
		--# Looking for the .rodata section
		--#   [ 3] .rodata           PROGBITS        f8024e90 024e90 006f70
		local addr, size = line:match("^  %[%s*%d+%] %.rodata%s+PROGBITS%s+(%w+) %w+ (%w+)")
		if addr then
			found = true
			local endPos = tonumber(addr, 16) + tonumber(size, 16)
			assert(endPos <= codeBase + maxCodeSize, string.format("Kernel code size is too big! %X > %X", endPos - codeBase, maxCodeSize))
			break
		end
	end
	assert(found, "Couldn't find .rodata segment!")
	f:close()
end

local function calculateBaseDir()
	local me = makeAbsolutePath(arg[0], os.getenv("PWD"))
	local cmps = makePathComponents(me)
	table.remove(cmps) -- remove build.lua
	table.remove(cmps) -- remove build dir
	baseDir = table.concat(cmps, "/").."/"
end

function run()
	calculateBaseDir()
	local platforms = {}
	for i,a in ipairs(cmdargs) do
		if a == "-v" or a == "--verbose" then
			verbose = true
		elseif a == "-l" or a == "--listing" then
			listing = true
		elseif a == "-m" or a == "--modules" then
			compileModules = true
		elseif a == "-p" or a == "--preprocess" then
			preprocess = true
		elseif a == "-j" then
			maxJobs = assert(tonumber(cmdargs[i+1]))
			table.remove(cmdargs, i+1)
		elseif a == "-b" or a == "--bootmode" then
			bootMode = assert(tonumber(cmdargs[i+1]))
			table.remove(cmdargs, i+1)
		else
			table.insert(platforms, a)
		end
	end

	if not next(platforms) then
		platforms = { "pi" }
	end

	for _, platform in ipairs(platforms) do
		if platform == "clean" then
			local cmd = "rm -rf "..qrp("bin")
			exec(cmd)
		else
			config = loadConfig(platform)
			build_kernel()
			for _,step in ipairs(config.buildSteps or {}) do
				step()
			end
		end
	end
end

--
local ok, err = pcall(run)
if not ok then
	print(err)
	os.exit(false)
end
