#!/usr/local/bin/lua

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
	"lua/lutf8lib.c",
	"lua/loadlib.c",
	"lua/linit.c",
}

luaModules = {
	"modules/init.lua",
	"modules/misc.lua",
	"modules/oo.lua",
	"modules/interpreter.lua",
	"modules/spin.lua",
	{ path = "modules/membuf/membuf.lua", native = "modules/membuf/membuf.c" },
	{ path = "modules/membuf/types.lua" },
	{ path = "modules/membuf/print.lua" },
	{ path = "modules/int64.lua", native = "usersrc/int64.c" },
	{ path = "modules/runloop.lua", native = "usersrc/runloop.c" },
	{ path = "modules/ipc.lua", native = "usersrc/ipc.c" },
	{ path = "modules/timerserver/init.lua" },
	{ path = "modules/timerserver/server.lua", native = "modules/timerserver/timers.c" },
	{ path = "modules/timerserver/local.lua", native = true },
	"modules/symbolParser.lua",
	{ path = "modules/bitmap/bitmap.lua", native = "modules/bitmap/bitmap_lua.c" },
	{ path = "modules/bitmap/transform.lua" },
	{ path = "modules/input/input.lua", native = "modules/input/input.c" },
	"modules/passwordManager/textui.lua",
	"modules/passwordManager/gui.lua",
	"modules/passwordManager/keychain.lua",
	{ path = "modules/passwordManager/uicontrols.lua", strip = false },
	{ path = "modules/passwordManager/window.lua", strip = false },
	{ path = "modules/bapple/bapple.lua", native = "modules/bapple/bapple.c" },
	{ path = "modules/tetris/tetris.lua", native = "modules/tetris/tetris.c", strip = nil },
	{ path = "modules/flash/flash.lua", native = "modules/flash/flash.c" },
	"modules/luazero.lua",
	{ path = "modules/ymodem.lua" },
	{ path = "modules/bootMenu.lua", strip = nil },
}

local kluaCopts = {} -- Filled in later

kluaDebuggerModule = {
	path = "modules/kluadebugger.lua",
	native = "usersrc/kluadebugger.c",
	copts = kluaCopts,
}

local function armOnly() return machineIs("arm") end
local function armv7mOnly() return machineIs("armv7-m") end
local function kluaPresent() return config.klua end
local function uluaPresent() return config.ulua end
local function notFullyHosted() return not config.fullyHosted end
local function bootMenuOnly() return bootMode > 1 end
local function modulesPresent() return config.ulua or (config.klua and config.kluaIncludesModules) end
local function ukluaPresent() return modulesPresent() and (config.klua or config.ulua) end
local function useMalloc() return config.malloc end
local function useUluaHeap() return not useMalloc() end

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
		-- "-DNO_MALLOC_STATS=1", -- Avoids fprintf dep
		"-DMALLOC_INSPECT_ALL=1",
		"-DMALLOC_FAILURE_ACTION=", -- no errno
		"-DUSE_LOCKS=1",
	},
	enabled = useMalloc,
}

memCmpThumb2 = {
	path = "usersrc/memcmp_thumb2.s",
	user = true,
	-- Gcc doesn't like .s (lowercase) files which require preprocessing
	-- unless you override the type with -x
	copts = { "-x assembler-with-cpp" },
	enabled = armv7mOnly,
}

-- Note, doesn't include boot.c which is added programmatically
kernelSources = {
	{ path = "k/cpumode_arm.c", enabled = armOnly },
	{ path = "k/cpumode_armv7m.c", enabled = armv7mOnly },
	{ path = "k/scheduler_arm.c", enabled = armOnly },
	{ path = "k/scheduler_armv7m.c", enabled = armv7mOnly },
	{ path = "k/mmu_arm.c", enabled = armOnly },
	{ path = "k/mmu_armv7m.c", enabled = armv7mOnly },
	"k/debug.c",
	"k/atomic.c",
	"k/pageAllocator.c",
	"k/process.c",
	"k/scheduler.c",
	"k/svc.c",
	"k/kipc.c",
	"k/ringbuf.c",
	"k/uart_common.c",
	"k/driver_common.c",
	{ path = "usersrc/memcpy_arm.S", user = true, enabled = armOnly },
	{ path = "usersrc/memcmp_arm.S", user = true, enabled = armOnly },
	{ path = "usersrc/memcpy_thumb2.c", user = true, enabled = armv7mOnly },
	memCmpThumb2,
	{ path = "usersrc/crt.c", user = true, enabled = notFullyHosted },
	{ path = "usersrc/strtol.c", user = true, enabled = notFullyHosted },
	{ path = "usersrc/uklua.c", user = true, enabled = ukluaPresent },
	{ path = "modules/bitmap/bitmap.c", user = true, enabled = modulesPresent },
	{ path = "usersrc/ulua.c", user = true, enabled = uluaPresent },
	{ path = "usersrc/uexec.c", user = true },
	mallocSource,
	{ path = "usersrc/uluaHeap.c", user = true, enabled = useUluaHeap },
	{ path = "usersrc/kluaHeap.c", user = true, enabled = kluaPresent },
	{ path = "k/bootMenu.c", enabled = bootMenuOnly },
	{ path = "testing/atomic.c", enabled = bootMenuOnly },
}

bootMenuModules = {
	"modules/test/init.lua",
	"modules/test/yielda.lua",
	"modules/test/yieldb.lua",
	"modules/bitmap/tests.lua",
	{ path = "modules/test/memTests.lua", native = "testing/memTests.c" },
	"modules/test/emptyModule.lua",
}

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
includeSymbols = false
incremental = false
dependencyInfo = {}
jobs = { n = 0 }
maxJobs = 1
debugSchedulingEntryPoint = false
bootMode = 0
shouldStripLuaModules = false

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
	if verbose then print(string.format("Waiting for %d jobs...", #jobs)) end
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

function machineIs(m)
	for _, type in ipairs(config.machine or {}) do
		if m == type then return true end
	end
	return false
end

local function updateDepsForObj(objFile, commandLine)
	local newObjDeps = {}
	local generatedMatch = objFile:match("bin/obj%-[^/]+/(.*)%.gen%.o$") or
		objFile:match("bin/obj%-[^/]+/(.*)%.luac%.o$")
	if generatedMatch then
		-- It's a generated lua file, so the dependencies that the C compiler
		-- knows about are not important
		table.insert(newObjDeps, generatedMatch..".lua")
	else
		local dfile = objForSrc(objFile, ".d")
		local f = assert(io.open(baseDir..dfile, "r"))
		-- First line is always the makefile target name, boring
		f:read("*l")
		for line in f:lines() do
			local dep = line:match(" "..baseDir.."(.-) ?\\?$")
			assert(dep, "Problem reading dependency file")
			--print(objFile .. " depends on "..dep)
			-- Flatten relative includes like k/../build/pi/gpio.h
			dep = dep:gsub("[^/]+/%.%./", "")
			table.insert(newObjDeps, dep)
		end
		f:close()
	end
	table.sort(newObjDeps)
	newObjDeps.commandLine = commandLine
	return newObjDeps
end

local function getModificationTimes()
	local modificationTimes = {}
	local path = baseDir:match("^(.-)/$") or baseDir -- chomp final slash
	local cmd = "find "..quote(path)..[[ \( -name ".*" -prune \) -o -type f -print0 | xargs -0 stat -f "%m %N"]]
	if verbose then print(cmd) end
	local h = assert(io.popen(cmd, "r"))
	local job = {cmd = cmd, handle = h}
	for line in h:lines() do
		local timestamp, path = line:match("(%d+) "..baseDir.."(.+)")
		--print("Got timestamp", timestamp, path)
		assert(timestamp, "Failed to parse find/xargs/stat data")
		modificationTimes[path] = tonumber(timestamp)
	end
	waitForJob(job)
	return modificationTimes
end

local function loadDependencyCache(cacheFile)
	-- First read in the current state of the filesystem
	local modificationTimes = getModificationTimes()

	-- Now load the cached dependency data
	local moduleTable = {}
	local f = loadfile(cacheFile, nil, moduleTable)
	if not f then
		-- No deps info, consider everything dirty
		dependencyInfo = {}
		return
	end
	f()
	dependencyInfo = moduleTable.dependencies
	-- And mark as clean all object files whose dependencies haven't changed
	for obj, objDeps in pairs(dependencyInfo) do
		local objTime = modificationTimes[obj]
		local clean = (objTime ~= nil) -- File has to exist
		for _, dep in ipairs(clean and objDeps or {}) do
			if modificationTimes[dep] == nil then
				-- A required dependency has gone bye-bye
				if verbose then
					print(obj.." is unclean due to missing modification time for "..dep)
				end
				clean = false
				break
			elseif modificationTimes[dep] > objTime then
				if verbose then
					print(obj.." is unclean due to dependency "..dep.." having changed.")
				end
				clean = false
				break
			end
		end
		objDeps.clean = clean
	end
	-- The entry point effectively depends on every .lua file but this is not
	-- (currently) captured in dependencyCache.lua nor do we correctly re-check
	-- timestamps of files generated during this build, so just always unclean
	-- it for now.
	local ep = dependencyInfo[objForSrc("modules/entryPoint.c")]
	if ep then
		if verbose then print("Uncleaning entryPoint.c") end
		ep.clean = false
	end

	for obj, cmdLine in pairs(moduleTable.commandLines) do
		dependencyInfo[obj].commandLine = cmdLine
	end
end

local function isClean(objFile, cmdLine)
	local clean = dependencyInfo[objFile] and dependencyInfo[objFile].clean
	if clean and cmdLine then
		clean = cmdLine == dependencyInfo[objFile].commandLine
	end
	-- if verbose then
	-- 	print(string.format("isClean %s .clean=%s clean=%s", objFile, dependencyInfo[objFile].clean, clean))
	-- end
	return clean
end

function setDependency(obj, cmdLine)
	local dep = dependencyInfo[obj]
	if not dep then
		dep = {}
		dependencyInfo[obj] = dep
	end
	dep.commandLine = cmdLine
	dep.built = true -- As opposed to merely having been loaded from the cache
end

function makeDirty(obj)
	if dependencyInfo[obj] then
		dependencyInfo[obj].clean = false
	end
end

local function sortedKeys(tbl, sortFn)
	local sorted = {}
	for k in pairs(tbl) do
		table.insert(sorted, k)
	end
	table.sort(sorted, sortFn)
	return sorted
end

local function saveDependencyCache(cacheFile, deps)
	local f = assert(io.open(cacheFile, "w"))
	f:write("-- Autogenerated by build.lua "..os.date().."\n\n")
	f:write("dependencies = {\n")
	local sortedDeps = sortedKeys(deps)
	for _, filename in ipairs(sortedDeps) do
		local objDeps = deps[filename]
		f:write(string.format("\t[%q] = {\n", filename))
		for i, dep in ipairs(objDeps) do
			f:write(string.format("\t\t%q,\n", dep))
		end
		f:write("\t},\n")
	end
	f:write("}\n\ncommandLines = {\n")
	for _, filename in ipairs(sortedDeps) do
		f:write(string.format("\t[%q] = %q,\n", filename, deps[filename].commandLine))
	end
	f:write("}\n")
	f:close()
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

	local overallOpts = preprocess and "-E" or "-c"
	if listing then
		-- This saves intermediary .i (preprocessed) and .s (asm) files
		overallOpts = overallOpts.." -save-temps=obj"
	end
	local sysOpts = (source.hosted or config.fullyHosted) and "-ffreestanding" or "-ffreestanding -nostdinc -nostdlib"
	if listing then
		-- Debug is required for objdump to do interleaved listing
		sysOpts = sysOpts .. " -g"
	end
	if not preprocess and incremental then
		sysOpts = sysOpts .. " -MD"
	end
	local langOpts = "-std=c99 -fstrict-aliasing -Wall -Werror -Wno-error=unused-function"
	local platOpts = config.platOpts or ""

	local extraArgsString = join(extraArgs)

	local suff = preprocess and ".i" or ".o"
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
	if incremental then
		-- Check if we need to build
		local objClean = isClean(obj, cmd)
		if objClean then
			if verbose then
				print("Not compiling "..source.path..", dependency cache reports no changes")
			end
			return obj
		end
	end
	local ok = parallelExec(cmd)
	if not ok then
		error("Compile failed for "..source.path, 2)
	end
	if not preprocess and incremental then
		-- Remember the invocation for later
		setDependency(obj, cmd)
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
	local h = io.popen(config.cc.." -print-libgcc-file-name")
	local path = h:read("*l")
	assert(h:close())
	return path
end

local function findModulesTableSymbol(symbols)
	-- Given how we build the kernel it's generally the last symbol, so search
	-- backwards
	for i = symbols.n, 1, -1 do
		local sym = symbols[i]
		if sym.name == "KLuaModulesTable" then return sym end
	end
	error("Failed to find KLuaModulesTable symbol")
end

local function le32(val)
	return string.char(
		bit32.extract(val, 0, 8),
		bit32.extract(val, 8, 8),
		bit32.extract(val, 16, 8),
		bit32.extract(val, 24, 8)
	)
end

function build_kernel()
	local cacheFile = objForSrc("dependencyCache.lua", ".lua")
	if incremental then
		loadDependencyCache(cacheFile)
	else
		-- We won't be updating it so we should delete it to invalidate
		os.remove(cacheFile)
	end
	local sysIncludes = {
		"k/inc",
		"userinc/lupi",
	}
	local sources
	if machineIs("host") then
		-- We make no assumptions about what host wants to compile by default
		sources = {}
	else
		sources = {
			{ path = "k/boot.c", copts = { "-DBOOT_MODE="..bootMode } },
		}
		for _, s in ipairs(kernelSources) do table.insert(sources, s) end
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
	if useMalloc() then
		table.insert(userIncludes, "-DMALLOC_AVAILABLE");
	end

	local includes = {}
	local includeModules = modulesPresent()
	if config.lua then
		for _,src in ipairs(luaSources) do
			table.insert(sources, { path = src, user = true })
		end
		if includeModules then
			if config.ulua and config.klua then
				table.insert(luaModules, kluaDebuggerModule)
			end
			if bootMode ~= 0 then
				addBootMenuSources(sources, luaModules)
			end
			for _, src in ipairs(generateLuaModulesSource()) do
				table.insert(sources, { path = src, user = true })
			end
		end
		if config.ulua then
			if config.klua then
				-- ulua and klua can both be true when we're using klua as a kernel debugger
				-- In this config we're primarily ulua (don't define KLUA in the kernel) but
				-- klua.c still gets compiled
				table.insert(includes, "-DKLUA_DEBUGGER")
			end
		elseif config.klua then
			table.insert(includes, "-DKLUA")
			table.insert(includes, "-DLUPI_NO_PROCESS")
			table.insert(userIncludes, "-DLUACONF_USE_PRINTK")
		end
	else
		table.insert(includes, "-DLUPI_NO_PROCESS")
	end
	if config.klua then
		-- klua.c is a special case as it sits between kernel and user code,
		-- and needs to access bits of both. It is primarily user (has user
		-- includes) but additionally has access to kernel headers
		if config.ulua then
			table.insert(kluaCopts, "-DULUA_PRESENT")
			table.insert(kluaCopts, "-DKLUA_DEBUGGER")
		end
		if includeModules then
			table.insert(kluaCopts, "-DKLUA_MODULES")
		end
		if config.include then
			table.insert(kluaCopts, "-include "..qrp("build/"..config.name.."/"..config.include))
		end
		table.insert(kluaCopts, "-isystem "..qrp("k/inc"))

		table.insert(sources, {
			path = "usersrc/klua.c",
			user = true,
			copts = kluaCopts,
		})
	end
	if includeModules then
		-- Add any modules with native code
		for _, module in ipairs(luaModules) do
			if type(module) == "table" and type(module.native) == "string" then
				table.insert(sources, { path = module.native, user = true, copts = module.copts })
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

	local allSources = { }
	for i,s in ipairs(sources) do allSources[i] = s end
	if config.entryPoint then table.insert(allSources, config.entryPoint) end
	local dirs = calculateUniqueDirsFromSources("bin/obj-"..config.name.."/", allSources)
	for dir, _ in pairs(dirs) do
		mkdir(dir)
	end

	local objs = {}
	if config.entryPoint then
		-- Make sure it gets compiled first
		local obj
		if config.entryPoint:match("%.s$") then
			obj = assemble(config.entryPoint)
		else
			obj = compilec(config.entryPoint, includes)
		end
		table.insert(objs, obj)
	end

	for i, source in ipairs(sources) do
		if type(source) == "string" then
			source = { path = source }
		end
		if type(source.enabled) == "function" and not source.enabled() then
			-- Skip this file
		elseif source.path:match("%.s$") and source.copts == nil then
			-- If it has custom copts, assume it wants to be compilec()'d
			table.insert(objs, assemble(source.path))
		else
			table.insert(objs, compilec(source, source.user and userIncludes or includes))
		end
	end

	waitForAllJobs()

	if preprocess then
		-- We're done
		return
	end

	if config.link then
		config.link(objs)
	else
		-- The proper code - link time!
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
		assert(config.textSectionStart, "No textSectionStart defined in buildconfig!")
		assert(config.bssSectionStart, "No bssSectionStart defined in buildconfig!")
		table.insert(args, string.format("-Ttext 0x%X -Tbss 0x%X", config.textSectionStart, config.bssSectionStart))
		local cmd = string.format("%s %s -o %s", config.ld, join(args), qrp(elf))
		local ok = exec(cmd)
		if not ok then error("Link failed!") end

		local img = outDir .. "kernel.img"
		local cmd = string.format("%s %s -O binary %s", config.objcopy, qrp(elf), qrp(img))
		local ok = exec(cmd)
		if not ok then error("Objcopy failed!") end

		local readElfOutput = outDir.."kernel.txt"
		cmd = string.format("%s -a -W %s > %s", config.readelf, qrp(elf), qrp(readElfOutput))
		ok = exec(cmd)
		if not ok then error("Readelf failed!") end

		-- Check the entry point ended up in the right place (this can cause evil bugs otherwise)
		local symParser = require("modules/symbolParser")
		local syms = symParser.getSymbolsFromReadElf(readElfOutput)
		local entryPoint = symParser.findSymbolByName("_start")
		assert(entryPoint and bit32.band(entryPoint.addr, -2) == config.textSectionStart, "Linker failed to locate the entry point at the start of the text section")

		local imgFileSize
		if includeSymbols then
			assert(includeModules, "Cannot include the symbols module unless modules are being built")
			-- Symbols get appended on the end of the kernel image, and the
			-- symbols module KLuaModulesTable entry is fixed up to point to it
			local luaFile = objForSrc("modules/symbols.lua", ".lua")
			local moduleText = symParser.dumpSymbolsTable()
			local f = assert(io.open(luaFile, "w+"))
			assert(f:write(moduleText))
			f:close()
			local data
			if compileModules then
				local obj = objForSrc(luaFile, ".luac")
				local compileCmd = "bin/luac -s -o "..qrp(obj).." "..qrp(luaFile)
				local ok = exec(compileCmd)
				if not ok then error("Failed to compile symbols module") end
				f = assert(io.open(obj, "rb"))
				data = f:read("*a")
				f:close()
			else
				data = moduleText
			end

			local moduleTableSymbol = findModulesTableSymbol(syms)
			if verbose then
				print(string.format("symbols: found KLuaModulesTable at %x", moduleTableSymbol.addr))
			end
			local imageFile = assert(io.open(baseDir..img, "r+b"))
			imgFileSize = imageFile:seek("end")
			imageFile:seek("set", moduleTableSymbol.addr - config.textSectionStart+ 4)
			local symbolsAddress = config.textSectionStart + imgFileSize
			assert(imageFile:write(le32(symbolsAddress)))
			assert(imageFile:write(le32(#data)))
			imageFile:seek("end")
			assert(imageFile:write(data))
			imageFile:close()
			imgFileSize = imgFileSize + #data
		end
		if config.maxCodeSize then
			if not imageFileSize then
				-- We've already calculated imgFileSize if we compiled symbols
				local imageFile = assert(io.open(baseDir..img, "r+b"))
				imgFileSize = imageFile:seek("end")
				imageFile:close()
			end
			assert(imgFileSize < config.maxCodeSize, "Kernel image exceeded maximum size "..tonumber(config.maxCodeSize))
		end

		if listing then
			cmd = string.format("%s -d --section .text --section .rodata --source -w %s > %s", config.objdump, qrp(elf), qrp(outDir .. "kernel.s"))
			exec(cmd)
		end

		if incremental then
			local newDeps = {}
			for _, obj in ipairs(objs) do
				newDeps[obj] = updateDepsForObj(obj, dependencyInfo[obj].commandLine)
				dependencyInfo[obj] = nil
			end
			-- Go through anything left in dependencyInfo
			for obj, dep in pairs(dependencyInfo) do
				if dep.built then
					-- Only interested in the stuff we built just now, not
					-- things left over in the cache from a previous build
					newDeps[obj] = dep
				end
			end
			saveDependencyCache(cacheFile, newDeps)
		end
	end
end

function addBootMenuSources(sources, modules)
	for _, m in ipairs(bootMenuModules) do
		table.insert(modules, m)
	end
end

function generateLuaModulesSource()
	local dirs = calculateUniqueDirsFromSources("bin/obj-"..config.name.."/", luaModules)
	for dir, _ in pairs(dirs) do
		mkdir(dir)
	end

	local results = {}
	local modulesList = {}
	for i, luaModule in ipairs(luaModules) do
		if type(luaModule) == "string" then luaModule = { path = luaModule } end
		local module = luaModule.path
		local modName = module:gsub("^modules/(.*).lua", "%1"):gsub("/", ".")
		local cname = "KLua_module_"..modName:gsub("%W", "_")
		local nativeFn = luaModule.native and "init_module_" .. modName:gsub("%W", "_")
		local moduleEntry = { name = modName, src = luaModule.path, cname = cname, nativeInit = nativeFn}
		table.insert(modulesList, moduleEntry)
		if compileModules then
			local obj = objForSrc(module, ".luac")
			local shouldStrip = shouldStripLuaModules
			if luaModule.strip ~= nil then
				-- Overriden on per-module basis
				shouldStrip = luaModule.strip
			end
			local stripArg = shouldStrip and "-s " or ""
			local compileCmd = "bin/luac "..stripArg.."-o "..qrp(obj).." "..module
			local finalObj = objForSrc(module, ".luac.o")
			if incremental and isClean(obj, compileCmd) and isClean(finalObj) then
				-- We can get away with skipping the compilation
				if verbose then print("Skipping compilation of "..module) end
				moduleEntry.obj = obj
				moduleEntry.finalObj = finalObj
			else
				local ok = parallelExec(compileCmd)
				if not ok then error("Failed to precompile module "..module) end
				moduleEntry.obj = obj
				makeDirty(finalObj) -- Important to invalidate this
			end
			setDependency(obj, compileCmd)
		else
			-- Include modules as plain text - note, dependency info is not
			-- checked on this code path
			local f = assert(io.open(baseDir..module, "r"))
			local backslash = '\\'
			local doubleBackslash = backslash:rep(2)
			local newline = '\n'
			local escapedNewline = [[\n\]]..newline
			local escapedQuote = [[\"]]
			-- Thank goodness backslash isn't a special char in lua gsub!
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
		for _, moduleEntry in ipairs(modulesList) do
			local f = assert(io.open(moduleEntry.obj, "rb"))
			if moduleEntry.finalObj then
				-- All we need to do is read the file size, because that's not
				-- saved in the dependency cache
				moduleEntry.size = f:seek("end")
			else
				local rep = function(char)
					return string.format([[\x%02x]], string.byte(char))
				end
				local data = f:read("*a")
				moduleEntry.size = #data
				moduleEntry.str = data:gsub(".", rep)
			end
			f:close()
		end
	end
	for _, moduleEntry in ipairs(modulesList) do
		-- The dep checking is easier if we use different names for the
		-- generated file depending on whether compileModules is set
		local suffix = compileModules and ".luac.c" or ".gen.c"
		local outName = objForSrc(moduleEntry.src,  suffix)
		if moduleEntry.finalObj == nil then
			local outf = assert(io.open(baseDir..outName, "w+"))
			outf:write("// Autogenerated from "..moduleEntry.src.."\n")
			outf:write("const char "..moduleEntry.cname.."[] = \"\\\n")
			outf:write(moduleEntry.str)
			outf:write('";\n')
			outf:close()
		end
		table.insert(results, outName)
	end

	local modEntryPoint = objForSrc("modules/entryPoint.c", ".c")
	local f = assert(io.open(baseDir..modEntryPoint, "w+"))
	local fn = {
		"",
		"static const LuaModule KLuaModulesTable[] = {"
	}
	f:write([[
// Autogenerated from modules list
#include <lupi/module.h>
#include <stddef.h>
#include <string.h>

]])

	local post = [[
const LuaModule* getLuaModule(const char* moduleName) {
	for (int i = 0; i < sizeof(KLuaModulesTable)/sizeof(LuaModule); i++) {
		if (strcmp(KLuaModulesTable[i].name, moduleName) == 0) {
			return &KLuaModulesTable[i];
		}
	}
	return NULL;
}
]]

	if includeSymbols then
		-- Need to make a dummy entry that will be updated at end of build
		-- It has to be first so we can find it easily
		table.insert(modulesList, 1, {
			name = "symbols",
			size = 0,
		})
	end

	local modFmt = '\t{ .name="%s", .data=%s, .size=%d, .nativeInit=%s },'
	for _, mod in pairs(modulesList) do
		if mod.cname then
			f:write("extern const char "..mod.cname.."[];\n")
		end
		if mod.nativeInit then
			f:write("extern int "..mod.nativeInit.."(lua_State* L);\n")
		end
		table.insert(fn, modFmt:format(mod.name, mod.cname or "NULL", mod.size, mod.nativeInit or "NULL"))
	end
	table.insert(fn, "};\n\n")
	f:write(table.concat(fn, "\n"))
	f:write(post)
	f:close()
	table.insert(results, modEntryPoint)

	waitForAllJobs() -- To make sure the c files have been created before we try to compile them
	return results
end

local function calculateBaseDir()
	local me = makeAbsolutePath(arg[0], os.getenv("PWD"))
	local cmps = makePathComponents(me)
	table.remove(cmps) -- remove build.lua
	table.remove(cmps) -- remove build dir
	baseDir = table.concat(cmps, "/").."/"
end

local opts = {
	{ s = 'v', l = "verbose", bool = "verbose" },
	{ s = 'l', l = "listing", bool = "listing" },
	{ s = 'm', l = "modules", bool = "compileModules" },
	{ s = 'p', l = "preprocess", bool = "preprocess" },
	{ s = 's', l = "symbols", bool = "includeSymbols" },
	{ s = 'i', l = "incremental", bool = "incremental" },
	{ s = 'j', l = "jobs", int = "maxJobs" },
	{ s = 'b', l = "bootmode", int = "bootMode" },
	{ s = 't', l = "strip", bool = "shouldStripLuaModules" },
}

function run()
	calculateBaseDir()
	local platforms = {}
	local shortOpts, longOpts = {}, {}
	for _, opt in ipairs(opts) do
		opt.variable = opt.bool or opt.int
		shortOpts[opt.s], longOpts[opt.l] = opt, opt
	end
	for i,a in ipairs(cmdargs) do
		local opt = longOpts[a:match("^%-%-(.+)")]
		local shopt = (opt == nil) and a:match("^%-([^-].*)")
		if opt then
			if opt.int then
				_ENV[opt.variable] = assert(tonumber(cmdargs[i+1]))
				table.remove(cmdargs, i+1)
			else
				_ENV[opt.variable] = true
			end
		elseif shopt then
			-- Iterate through possibly multiple short opts combined together
			local j = 1
			while j <= #shopt do
				local ch = shopt:sub(j, j)
				opt = shortOpts[ch]
				if opt then
					if opt.bool then
						_ENV[opt.variable] = true
						j = j + 1
					elseif opt.int then
						local num = shopt:match("[%xx]+", j+1)
						if num == nil then
							-- Must be in next cmdarg
							num = cmdargs[i+1]
							table.remove(cmdargs, i+1)
						end
						_ENV[opt.variable] = assert(tonumber(num), "Syntax error: Bad number "..(num or shopt:sub(j+1)))
						j = j + 1 + #num
					else
						error("I'm confused")
					end
				else
					error("Unrecognised short option -"..ch)
				end
			end
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
