assert(true) -- Ie that assert itself, as something in the global table, is accessible!
assert(_ENV ~= _G)

x = 12345
assert(_G.x == nil)

local interp = require "interpreter"
assert(type(interp) == "table")
assert(_ENV.interpreter == interp)
assert(_G.interpreter == nil) -- but that we don't pollute the global namespace every time a module does a require

function main(...)

	print("I'm still special!\n")

end

