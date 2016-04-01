assert(loadfile(build.baseDir.."build/luac/buildconfig.lua", nil, _ENV))()
local baseConfig = config

-- Inherit all other settings from luac
config = setmetatable({ lp64 = true }, { __index = baseConfig })
