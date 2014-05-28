config = {
	machine = { "host" },
}

local function mkdirForFile(path)
	local dir = path:match("(.+)/.-")
	build.mkdir(dir)
end

local function scanFileForInlineDocs(file)
	local f = assert(io.open(file, "r"))
	-- Scan file line-by-line for either /** (C style) or --[[** (Lua style)
	local mdFile, mdf
	local inDocs = false
	local docLines = {}
	for l in f:lines() do
		if inDocs then
			local lastLine = l:match("^%s*(.*)%*/") or l:match("^%s*(.*)%]%]")
			if lastLine then
				table.insert(docLines, lastLine)
				inDocs = false
			else
				table.insert(docLines, l)
			end
		elseif l == "/**" or l == "--[[**" then
			-- Found docs
			if not mdf then
				mdFile = build.objForSrc(file, ".md")
				mkdirForFile(mdFile)
				mdf = assert(io.open(mdFile, "w"))
			end
			inDocs = true
		elseif next(docLines) then
			-- We're waiting for the first line after the docs, which should be the function
			-- signature - unless it's a documentation in Lua of a native function, in which
			-- case it will be the last line of the doc instead
			local header = l:match("(.*)[{;]") or l -- Chop the { if there is one
			header = header:gsub("%-%-native", "") -- Chop native comment
			local id = header:match("function ([%w_.:]+)") or header:match("([%w_]+)%(")
			if id then
				id = id:gsub("[:.]", "_")
				mdf:write(string.format('<h3><a name="%s"><code>%s</code></a></h3>\n\n', id, header))
			elseif #header > 0 then
				mdf:write("<h3>", header, "</h3>\n\n")
			end
			mdf:write(table.concat(docLines, "\n"))
			mdf:write("\n")
			docLines = {}
		end
	end
	if mdf then
		mdf:close()
	end
	return mdFile
end

local function docForSrc(source)
	local objDir = "bin/obj-"..config.name.."/"
	if source:sub(1, #objDir) == objDir then
		-- It's already in the obj dir, strip that
		source = source:sub(#objDir + 1)
	end
	local result = "bin/doc/" .. source:gsub("%.%a+$", ".html")
	mkdirForFile(result)
	return result
end

local headerTemplate = [[
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
	<meta http-equiv="content-type" content="text/html; charset=utf-8" />
	<title>TITLE</title>
	<link rel="stylesheet" type="text/css" href="STYLESHEET" />
</head>
<body>

<p>
	<a href="INDEX">[Index]</a>
</p>

]]

local footer = "</body></html>"

local function doBuild()
	build.mkdir("bin/doc")
	build.exec(string.format("cp %s %s", build.qrp("build/doc/lua.css"), build.qrp("bin/doc/")))

	 -- Ugh I hate how inconsistant Lua modules are - this one returns a function
	local markdown = require("build/doc/markdown")
	local origSources = {} -- Map of .md files to orig files, where relevant
	local mdSources = {}

	-- Iterate over all possible files
	local cmd = [[find . -path "./bin*" -prune -o \( -name "*.md" -o -name "*.lua" -o -name "*.c" -o -name "*.h" \) -print]]
	local inlineFiles = {}
	local p = io.popen(cmd)
	for filename in p:lines() do
		filename = filename:match("^%./(.*)") or filename -- Chop off the "./" from the front
		if filename:match(".md$") then
			table.insert(mdSources, filename)
		else
			table.insert(inlineFiles, filename)
		end
	end
	p:close()

	for _, f in ipairs(inlineFiles) do
		local mdFile = scanFileForInlineDocs(f)
		if mdFile then
			origSources[mdFile] = f
			table.insert(mdSources, mdFile)
		end
	end
	table.sort(mdSources)

	-- Create index
	local indexFile = build.objForSrc("index.md", ".md")
	local index = assert(io.open(indexFile, "w"))
	index:write("# LuPi index\n\n")
	for _, file in ipairs(mdSources) do
		local origFile = origSources[file] or file
		local relPath = build.makeRelativePath(docForSrc(file), indexFile)
		index:write(string.format("* [%s](%s)\n", origFile, relPath))
	end
	index:close()
	table.insert(mdSources, indexFile)

	for _, file in ipairs(mdSources) do
		local f = assert(io.open(file))
		local src = assert(f:read("*a"))
		f:close()
		local result = markdown(src)
		local h1 = result:match("<h1>(.-)</h1>")
		local title = h1 or origSources[file] or file
		local outFile = docForSrc(file)
		local relCss = build.makeRelativePath("bin/doc/lua.css", outFile)
		local relIdx = build.makeRelativePath("bin/doc/index.html", outFile)
		local header = headerTemplate:gsub("TITLE", title, 1)
		header = header:gsub("STYLESHEET", relCss, 1)
		header = header:gsub("INDEX", relIdx, 1)

		f = assert(io.open(outFile, "w"))
		assert(f:write(header))
		if not h1 then
			-- There wasn't any proper title, so put one in
			assert(f:write("<h1>", title, "</h1>\n\n"))
		end
		assert(f:write(result))
		assert(f:write(footer))
		f:close()
	end
end

link = doBuild -- The only overridable build step provided by build.lua is called 'link'
