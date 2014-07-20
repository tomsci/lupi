#!/usr/local/bin/lua

local symbolParser = require "modules/symbolParser"
require "bin/obj-pi/modules/symbols"

symbolParser.setSymbols(symbols)

local lt = symbolParser.lt

assert(lt(80000, 0xf8008010))


-- print(symbolParser.addressDescription(0xf8008010))
-- print(symbolParser.addressDescription(0xf8100000))
print(symbolParser.addressDescription(0x00000000))
-- print(symbolParser.addressDescription(0xf800b740))
