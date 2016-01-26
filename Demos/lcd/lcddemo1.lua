-- init spi first
-- swspi = 1  -- software SPI, slow!

if dispType == nil then
	dispType = lcd.ILI7341
	-- dispType = lcd.ST7735B -- probably will work
	-- dispType = lcd.ST7735  -- try if not
	-- dispType = lcd.ST7735G -- or this one
end

if swspi == 1 then
	spi.setup(0,{mode=0, cs=12, speed=10000, mosi=8, sck=16 })
	lcd_OK = lcd.init(0,14,dispType,lcd.PORTRAIT_FLIP)
else
	spi.setup(2,{mode=0, cs=12, speed=50000})
	lcd_OK = lcd.init(2,14,dispType,lcd.PORTRAIT_FLIP)
end

if lcd_OK ~= 0 then
	print("LCD nit initialized")
	return
end

tmr.wdclr()

function spiro()
	lcd.clear()
	local n = mcu.random(19, 2)
	local r = mcu.random(100, 20)
	local colour = 0 -- rainbow()

	local i
	local sx, sy, x0, y0, y1, y1
	for i = 0, (359 * n), 1 do
		sx = math.cos((i / n - 90) * 0.0174532925)
		sy = math.sin((i / n - 90) * 0.0174532925)
		x0 = sx * (120 - r) + 159
		y0 = sy * (120 - r) + 119

		sy = math.cos(((i % 360) - 90) * 0.0174532925)
		sx = math.sin(((i % 360) - 90) * 0.0174532925)
		x1 = sx * r + x0
		y1 = sy * r + y0
		lcd.putpixel(x1, y1, lcd.hsb2rgb(i,1,1)) --colour
	end
  
	r = mcu.random(100,20)  -- r = r / random(4,2)
	for i = 0, (359 * n), 1 do
		sx = math.cos((i / n - 90) * 0.0174532925);
		sy = math.sin((i / n - 90) * 0.0174532925);
		x0 = sx * (120 - r) + 159
		y0 = sy * (120 - r) + 119


		sy = math.cos(((i % 360) - 90) * 0.0174532925)
		sx = math.sin(((i % 360) - 90) * 0.0174532925)
		x1 = sx * r + x0;
		y1 = sy * r + y0;
		lcd.putpixel(x1, y1, lcd.hsb2rgb(i,1,1)) --colour
	end

	tmr.wdclr()
end

--fullDemo(6, 1)
