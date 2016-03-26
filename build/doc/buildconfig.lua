config = {
	machine = { "host" },
}

local function mkdirForFile(path)
	local dir = path:match("(.+)/.-")
	build.mkdir(dir)
end

local function fixupLinksInLine(line, filename, lineNum)
	local function fixupLink(link)
		local path, anchor = link:match("^([^#]*)(#.*)$")
		if not path then
			path = link
			anchor = ""
		end
		if path:match("^[a-z]+:") or path:match("%.html$") then
			-- It's a real URL, or one already to an html file, do not touch
			return link
		end
		-- Now check that the file exists (the empty path means a link within
		-- the current file, so no need to check that)
		if #path > 0 then
			local dir = build.removeLastPathComponent(filename)
			local absPath = build.makeAbsolutePath(path, dir)
			local f = io.open(absPath, "r")
			if not f then
				error(string.format("%s:%d: Link destination not found: '%s'",
					filename, lineNum, path), 0)
			end
			f:close()
		end
		-- Finally, fix it up to its html-ified name
		return path:gsub("%.[a-z]+$", ".html")..anchor
	end
	-- Check for out-of-line links of the form [linktext]: URL
	local text, url = line:match("^%[(.*)%]: (.*)")
	if text then
		return string.format("[%s]: %s", text, fixupLink(url))
	end

	-- Now search for inline links of the form [linktext](linkurl)
	local function repl(text, url)
		return string.format("[%s](%s)", text, fixupLink(url))
	end
	line = line:gsub("%[([^%]]+)%]%(([^)]+)%)", repl)
	return line
end

local function getIdentifierFromText(txt)
	-- Where 'txt' is a function definition/declaration in C or Lua, or a
	-- C #define
	local id = txt:match("function ([%w_.:]+)") or txt:match("([%w_]+)%(")
	return id
end

local function scanFileForInlineDocs(file)
	local f = assert(io.open(file, "r"))
	-- Scan file line-by-line for either /** (C style) or --[[** (Lua style)
	local mdFile, mdf
	local inDocs = false
	local docLines = {}
	local lineNum = 1
	for l in f:lines() do
		if inDocs then
			local lastLine = l:match("^%s*(.*)%*/") or l:match("^%s*(.*)%]%]")
			if lastLine then
				l = lastLine
				inDocs = false
			end
			table.insert(docLines, fixupLinksInLine(l, file, lineNum))
		elseif l == "/**" or l == "--[[**" then
			-- Found docs
			if not mdf then
				mdFile = build.objForSrc(file, ".tempmd")
				if build.verbose then
					print("Extracting markdown from "..file.." to "..mdFile)
				end
				mkdirForFile(mdFile)
				mdf = assert(io.open(mdFile, "w"))
			end
			inDocs = true
		elseif next(docLines) then
			-- We're waiting for the first line after the docs, which should be the function
			-- signature - unless it's a documentation in Lua of a native function, in which
			-- case it will be a comment starting "--native"
			local header = l:match("(.*)%s*[{;\\]") or l -- Chop the { if there is one
			header = header:gsub("%-%-native%s*", "") -- Chop native comment
			if #header > 0 then
				if getIdentifierFromText(header) then
					header = "`"..header.."`"
				end
				mdf:write("### ", header, "\n\n")
			end
			mdf:write(table.concat(docLines, "\n"))
			mdf:write("\n")
			docLines = {}
		end
		lineNum = lineNum + 1
	end
	if mdf then
		mdf:close()
	end
	return mdFile
end

local function htmlEscape(txt)
	local rep = {
		["<"] = "&lt;",
		[">"] = "&gt;",
	}
	return txt:gsub("&", "&amp;"):gsub("[<>]", rep)
end

local function processMarkdownFile(mdFile, outputFile)
	if build.verbose then
		print("Processing markdown "..mdFile.." to "..outputFile)
	end
	local f = assert(io.open(mdFile, "r"))
	mkdirForFile(outputFile)
	local outf = assert(io.open(outputFile, "w"))
	local index = {}
	local lineNum = 1
	for line in f:lines() do
		line = fixupLinksInLine(line, mdFile, lineNum)
		local heading, txt = line:match("^(#+)%s*(.*)")
		if heading then
			-- txt might have have `markdown code` markers, so strip them first
			local uncodedTxt = txt:gsub("`", "")
			local isCode = (uncodedTxt ~= txt)
			local id = getIdentifierFromText(uncodedTxt) or txt
			id = id:gsub("[^%w_]", "_")
			local htmlTxt = htmlEscape(uncodedTxt)
			if isCode then
				htmlTxt = "<code>"..htmlTxt.."</code>"
			end
			table.insert(index, { lvl = #heading, txt = uncodedTxt, id = id })
			line = string.format('<h%d><a name="%s">%s</a></h%d>', #heading, id, htmlTxt, #heading)
		end
		outf:write(line)
		outf:write("\n")
		lineNum = lineNum + 1
	end
	f:close()
	outf:close()
	return index
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

local function normaliseHeaderIndex(index)
	-- The idea here is to munge the h1, h2, etc header indexes into a simpler
	-- heirarchy. Such that eg an index with a single h1 doesn't bother with it,
	-- and 'holes' are adjusted down so that a page with eg no h2s doesn't have
	-- a hole in its heirarchy
	local newIdx = {}
	local counts = {0, 0, 0, 0, 0, 0}
	for _, entry in ipairs(index) do
		counts[entry.lvl] = counts[entry.lvl] + 1
	end
	local adjust = {0, 0, 0, 0, 0, 0}
	if counts[1] == 1 then
		-- Need more than 1 h1 to include it in the index
		adjust[1] = -1
	end
	for i = 2, 6 do
		adjust[i] = adjust[i-1]
		if counts[i-1] == 0 then
			adjust[i] = adjust[i] - 1
		end
	end
	if build.verbose then
		print(string.format("Adjustments = %d,%d,%d,%d,%d,%d", table.unpack(adjust)))
	end
	for i, entry in ipairs(index) do
		local newLvl = entry.lvl + adjust[entry.lvl]
		if newLvl > 0 then
			local newEntry = {}
			for k,v in pairs(entry) do newEntry[k] = v end
			newEntry.lvl = newLvl
			table.insert(newIdx, newEntry)
		end
	end
	return newIdx
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
	local indexes = {} -- Map of .md files to index structures
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

	-- We now process all md files even the ungenerated ones to check links
	-- and to create heading indexes
	for i, f in ipairs(mdSources) do
		local processedFile = build.objForSrc(f, ".md")
		local index = processMarkdownFile(f, processedFile)
		origSources[processedFile] = origSources[f] or f
		mdSources[i] = processedFile
		indexes[processedFile] = index
	end

	table.sort(mdSources)

	-- Create index page
	local indexFile = build.objForSrc("index.md", ".md")
	local index = assert(io.open(indexFile, "w"))
	index:write("# Index\n\n")
	for _, file in ipairs(mdSources) do
		local origFile = origSources[file] or file
		local relPath = build.makeRelativePath(docForSrc(file), indexFile)
		index:write(string.format("* [%s](%s)\n", origFile, relPath))
	end
	index:close()
	table.insert(mdSources, indexFile)

	for _, file in ipairs(mdSources) do
		if build.verbose then
			print("Marking down "..file)
		end
		local f = assert(io.open(file))
		local src = assert(f:read("*a"))
		f:close()
		local result = markdown(src)
		local h1, h1pos = result:match("<h1><a [^>]+>(.-)</a></h1>()")
		if not h1 then h1, h1pos = result:match("<h1>(.-)</h1>()") end
		local title = h1 or origSources[file] or file
		local outFile = docForSrc(file)
		local relCss = build.makeRelativePath("bin/doc/lua.css", outFile)
		local relIdx = build.makeRelativePath("bin/doc/index.html", outFile)
		local header = headerTemplate:gsub("TITLE", title, 1)
		header = header:gsub("STYLESHEET", relCss, 1)
		header = header:gsub("INDEX", relIdx, 1)
		local index = indexes[file] and normaliseHeaderIndex(indexes[file])

		f = assert(io.open(outFile, "w"))
		f:write(header)
		if h1 then
			-- Put the index after the first h1
			f:write(result:sub(1, h1pos))
			f:write("\n")
			result = result:sub(h1pos)
		else
			-- There wasn't any proper title, so put one in
			f:write("<h1>", title, "</h1>\n\n")
		end
		if index and next(index) then
			local lvl = 0
			for _, entry in ipairs(index) do
				while lvl < entry.lvl do
					f:write("<ul>\n")
					lvl = lvl + 1
				end
				while lvl > entry.lvl do
					f:write("</ul>\n")
					lvl = lvl - 1
				end
				f:write(string.format('<li><a href="#%s">%s</a></li>\n', entry.id, htmlEscape(entry.txt)))
			end
			while lvl > 0 do
				f:write("</ul>\n")
				lvl = lvl - 1
			end
			f:write("<hr>\n")
		end
		f:write(result)
		f:write(footer)
		f:close()
	end
end

link = doBuild -- The only overridable build step provided by build.lua is called 'link'
