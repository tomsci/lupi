#!/usr/local/bin/lua

if _VERSION ~= "Lua 5.2" then
	error "Lua 5.2 is required to run this script"
end
local cmdargs = {...}

baseDir = "/Users/tomsci/Documents/lupi/"
verbose = false
listing = false
config = nil

local function loadConfig(c)
	local env = {
		print = print, error = error, ipairs = ipairs, pairs = pairs, table = table, string = string, os = os,
		build = _G, -- Yeah yeah, hacky but gets the job done
	}
	local f = assert(loadfile(baseDir.."build/"..c.."/buildconfig.lua", nil, env))
	f()
	local config = env.config
	if config.toolchainPrefix and not config.cc then
		config.cc = config.toolchainPrefix .. "gcc"
		config.as = config.toolchainPrefix .. "as"
		config.ld = config.toolchainPrefix .. "ld"
		config.objcopy = config.toolchainPrefix .. "objcopy"
		config.objdump = config.toolchainPrefix .. "objdump"
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
	local ok = exec(cmd)
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

	local overallOpts = source.listing and "-S" or "-c"
	local sysOpts = source.hosted and "-ffreestanding" or "-ffreestanding -nostdinc -nostdlib"
	if listing then
		--# Debug is required to do interleaved listing
		sysOpts = sysOpts .. " -g"
	end
	local langOpts = "-std=c99 -funsigned-char -Wall -Werror -Wno-error=unused-function"
	local platOpts = config.platOpts or ""

	local extraArgsString = join(extraArgs)

	local suff = source.listing and ".s" or ".o"
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
	local ok = exec(cmd)
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

function build_kernel()
	local sysIncludes = {
		"k/inc",
		"userinc/lupi",
	}
	local sources = {
		"k/boot.c",
		"k/debug.c",
		"k/pageAllocator.c",
		"k/process.c",
		"k/svc.c",
	}
	for _, src in ipairs(config.extraKernelSources or {}) do
		table.insert(sources, src)
	end
	local userIncludes = {
		"-isystem "..qrp("userinc"),
		"-I "..qrp("luaconf"),
		"-isystem "..qrp("lua"),
	}
	if config.extraStdInc then
		table.insert(userIncludes, 1, "-isystem "..qrp(config.extraStdInc))
	end
	if config.userInclude then
		table.insert(userIncludes, 1, "-include "..qrp("build/"..config.name.."/"..config.userInclude))
	end

	local includes = {}
	if config.klua or config.ulua then
		for _,src in ipairs(luaSources) do
			table.insert(sources, { path = src, user = true })
		end
		for _, src in ipairs(generateLuaModulesSource()) do
			table.insert(sources, { path = src, user = true })
		end
		if machineIs("arm") then
			table.insert(sources, { path = "usersrc/memcpy_arm.S", user = true })
			table.insert(sources, { path = "usersrc/memcmp_arm.S", user = true })
		end
		table.insert(sources, { path = "usersrc/crt.c", user = true })
		if config.klua then
			table.insert(includes, "-DKLUA")
			table.insert(userIncludes, "-DKLUA")
		else -- ulua
			table.insert(sources, mallocSource)
		end
	end
	if not config.klua then
		table.insert(sources, { path = "usersrc/ulua.c", user = true })
		table.insert(sources, { path = "usersrc/uexec.c", user = true })
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
		--# klua.c is a special case as it sits between kernel and user code, and needs to access bits of both. It is primarily user (has user includes) but also has access to the platform -include file
		local includes = { "-DKLUA" }
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
			--# I need your clothes, your boots and your run-time EABI helper functions
			--# TODO fix hardcoded path!
			table.insert(args, "/Users/tomsci/Documents/gcc-arm/gcc-arm-none-eabi-4_8-2013q4/lib/gcc/arm-none-eabi/4.8.3/armv6-m/libgcc.a")
		end
		--table.insert(args, "-Ttext 0x8000 -Tbss 0x28000")
		-- The only BSS we have is in malloc.c, userside, so we can set to a user address
		table.insert(args, "-Ttext 0xF8008000 -Tbss 0x00007000")
		local cmd = string.format("%s %s -o %s", config.ld, join(args), qrp(elf))
		local ok = exec(cmd)
		if not ok then error("Link failed!") end

		local img = outDir .. "kernel.img"
		local cmd = string.format("%s %s -O binary %s", config.objcopy, qrp(elf), qrp(img))
		local ok = exec(cmd)
		if not ok then error("Objcopy failed!") end

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
	--#"lua/liolib.c",
	--#"lua/lmathlib.c",
	--#"lua/loslib.c",
	"lua/lstrlib.c",
	"lua/ltablib.c",
	"lua/loadlib.c",
	"lua/linit.c",
}

luaModules = {
	"modules/test.lua",
	"modules/interpreter.lua",
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

function build_lua()
	local includes = {
		"-isystem", qrp("userinc"),
		"-I", qrp("luaconf"),
	}

	mkdir("bin/obj-"..config.name.."/lua")
	for _, source in ipairs(luaSources) do
		compilec(source, includes)
	end
end

function generateLuaModulesSource()
	local dirs = calculateUniqueDirsFromSources("bin/obj-"..config.name.."/", luaModules)
	for dir, _ in pairs(dirs) do
		mkdir(dir)
	end

	local results = {}
	local modMap = {}
	local sizeMap = {}
	for i, module in ipairs(luaModules) do
		local cname = "KLuaModule_"..module:gsub("%A", "_")
		local modName = module:gsub("^modules/(.*).lua", "%1")
		modMap[modName] = cname
		local f = assert(io.open(module, "r"))
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
		sizeMap[cname] = #data
		local str = data:gsub('[\\"\n]', rep)
		f:close()
		local outName = objForSrc(module, ".c")
		local outf = assert(io.open(outName, "w+"))
		outf:write("// Autogenerated from "..module.."\n")
		outf:write("char const*const "..cname.." = \"\\\n")
		outf:write(str)
		outf:write('";\n')
		outf:close()
		table.insert(results, outName)
	end

	local modEntryPoint = objForSrc("modules/entryPoint.c", ".c")
	local f = assert(io.open(modEntryPoint, "w+"))
	local fn = {
		"",
		"const char* getLuaModule(const char* moduleName, int* modSize) {",
	}
	f:write([[
// Autogenerated from modules list
#include <stddef.h>
#include <string.h>

]])
	local tst = [[
	if (strcmp(moduleName, "%s") == 0) {
		*modSize = %d;
		return %s;
	}
]]
	for modName, cname in pairs(modMap) do
		f:write("extern char const*const "..cname..";\n")
		table.insert(fn, tst:format(modName, sizeMap[cname], cname))
	end
	table.insert(fn, "	return NULL;")
	table.insert(fn, "}")
	f:write(table.concat(fn, "\n"))
	f:close()
	table.insert(results, modEntryPoint)

	return results
end


function run()
	local platforms = {}
	for i,a in ipairs(cmdargs) do
		if a == "-v" or a == "--verbose" then
			verbose = true
		elseif a == "-l" or a == "--listing" then
			listing = true
		else
			table.insert(platforms, a)
		end
	end

	if not next(platforms) then
		platforms = { "hosted" }
	end

	for _, platform in ipairs(platforms) do
		if platform == "clean" then
			local cmd = "rm -rf "..qrp("bin")
			exec(cmd)
		else
			config = loadConfig(platform)
			build_kernel()
			--build_lua()
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
