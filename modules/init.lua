-- This module contains the first user-side code executed after boot up.
-- Modify its main function to do things at startup.

function main()
	return require("interpreter").main()
end
