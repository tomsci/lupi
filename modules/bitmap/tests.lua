require "bitmap"

function main()
	b = bitmap.create()
	b:setAutoBlit(true)

	b:setColour(bitmap.Colour.Green)
	b:drawRect(10, 200, 15, 15)
	b:setColour(bitmap.Colour.Black)
	b:drawText("Hello, world!", 50, 200)

	b:setColour(bitmap.rgbColour(0, 0, 0xFF))
	centrex, centrey = 160, 120
	centre = { centrex, centrey }

	-- All the quads from 0 onwards
	b:drawLine(centre, centrex + 50, centrey + 20)
	b:drawLine(centre, centrex + 20, centrey + 50)
	b:drawLine(centre, centrex - 20, centrey + 50)
	b:drawLine(centre, centrex - 50, centrey + 20)
	b:drawLine(centre, centrex - 50, centrey - 20)
	b:drawLine(centre, centrex - 50, centrey - 20)
	b:drawLine(centre, centrex - 20, centrey - 50)
	b:drawLine(centre, centrex + 20, centrey - 50)
	b:drawLine(centre, centrex + 50, centrey - 20)

end
