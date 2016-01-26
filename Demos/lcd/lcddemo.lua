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

function header(tx)
	tmr.wdclr()
    mcu.random(1000,0,1)
	maxx, maxy = lcd.getscreensize()
	lcd.clear()
	lcd.setcolor(lcd.CYAN)
	if maxx < 240 then
		lcd.setfont(lcd.FONT_SMALL)
	else
		lcd.setfont(lcd.FONT_DEJAVU12)
	end
	miny = lcd.getfontheight() + 5
	lcd.rect(0,0,maxx-1,miny-1,lcd.OLIVE,{8,16,8})
	lcd.settransp(1)
	lcd.write(lcd.CENTER,2,tx)
	lcd.settransp(0)
end

function dispFont(sec)
	header("DISPLAY FONTS")

	local tx
	if maxx < 240 then
		tx = "WiFiMCU"
	else
		tx = "Hi from LoBo"
	end
	local starty = miny + 4
	if maxx < maxy then
		-- display only in portrait mode
		lcd.setcolor(lcd.ORANGE)
		lcd.setfont(lcd.FONT_7SEG)
		lcd.write(0,starty,"1234567890")
		starty = starty + lcd.getfontheight() + 4
	end

	local x,y
	local n = tmr.tick() + (sec*1000)
	while tmr.tick() < n do
		y = starty
		x = 0
		local i,j
		for i=1, 3, 1 do
			for j=0, lcd.FONT_7SEG-1, 1 do
				tmr.wdclr()
				lcd.setcolor(mcu.random(0xFFFF))
				lcd.setfont(j)
				lcd.write(x,y,tx)
				y = y + lcd.getfontheight()
				if y > (maxy-lcd.getfontheight()) then
					break
				end
			end
			y = y + 2
			if y > (maxy-lcd.getfontheight()) then
				break
			end
			if i == 1 then 
				x = lcd.CENTER
			end
			if i == 2 then
				x = lcd.RIGHT
			end
		end
	end
	tmr.wdclr()
end

function fontDemo(sec, rot)
	local tx = "FONTS"
	if rot > 0 then
		tx = "ROTATED "..tx
	end
	header(tx)

	lcd.setclipwin(0,miny,maxx,maxy)
	tx = "WiFiMCU"
	local x, y, color, i
	local n = tmr.tick() + (sec*1000)
	while tmr.tick() < n do
		if rot == 1 then
			lcd.setrot(math.floor(mcu.random(359)/5)*5);
		end
		for i=0, lcd.FONT_7SEG-1, 1 do
			lcd.setcolor(mcu.random(0xFFFF))
			lcd.setfont(i)
			x = mcu.random(maxx-8)
			y = mcu.random(maxy-lcd.getfontheight(),miny)
			lcd.write(x,y,tx)
		end
		tmr.wdclr()
	end
	lcd.resetclipwin()
	lcd.setrot(0)
end

function lineDemo(sec)
	header("LINE DEMO")

	lcd.setclipwin(0,miny,maxx,maxy)
	local n = tmr.tick() + (sec*1000)
	local x1, x2,y1,y2,color
	while tmr.tick() < n do
		x1 = mcu.random(maxx-4)
		y1 = mcu.random(maxy-4,miny)
		x2 = mcu.random(maxx-1)
		y2 = mcu.random(maxy-1,miny)
		color = mcu.random(0xFFFF)
		lcd.line(x1,y1,x2,y2,color)
		tmr.wdclr()
    end;
	lcd.resetclipwin()
end;

function circleDemo(sec,dofill)
	local tx = "CIRCLE"
	if dofill > 0 then
		tx = "FILLED "..tx
	end
	header(tx)

	lcd.setclipwin(0,miny,maxx,maxy)
	local n = tmr.tick() + (sec*1000)
	local x, y, r, color, fill
	while tmr.tick() < n do
		x = mcu.random(maxx-2,4)
		y = mcu.random(maxy-2,miny+2)
		if x < y then
			r = mcu.random(x,2)
		else
			r = mcu.random(y,2)
		end
		color = mcu.random(0xFFFF)
		if dofill > 0 then
			fill = mcu.random(0xFFFF)
			lcd.circle(x,y,r,color,fill)
		else
			lcd.circle(x,y,r,color)
		end
		tmr.wdclr()
    end;
	lcd.resetclipwin()
end;

function rectDemo(sec,dofill)
	local tx = "RECTANGLE"
	if dofill > 0 then
		tx = "FILLED "..tx
	end
	header(tx)

	lcd.setclipwin(0,miny,maxx,maxy)
	local n = tmr.tick() + (sec*1000)
	local x, y, w, h, color, fill
	while tmr.tick() < n do
		x = mcu.random(maxx-2,4)
		y = mcu.random(maxy-2,miny)
		w = mcu.random(maxx-x,2)
		h = mcu.random(maxy-y,2)
		color = mcu.random(0xFFFF)
		if dofill > 0 then
			fill = mcu.random(0xFFFF)
			lcd.rect(x,y,w,h,color,fill)
		else
			lcd.rect(x,y,w,h,color)
		end
		tmr.wdclr()
    end;
	lcd.resetclipwin()
end;

function triangleDemo(sec,dofill)
	local tx = "TRIANGLE"
	if dofill > 0 then
		tx = "FILLED "..tx
	end
	header(tx)

	lcd.setclipwin(0,miny,maxx,maxy)
	local n = tmr.tick() + (sec*1000)
	local x1, y1, x2, y2, x3, y3, color, fill
	while tmr.tick() < n do
		x1 = mcu.random(maxx-2,4)
		y1 = mcu.random(maxy-2,miny)
		x2 = mcu.random(maxx-2,4)
		y2 = mcu.random(maxy-2,miny)
		x3 = mcu.random(maxx-2,4)
		y3 = mcu.random(maxy-2,miny)
		color = mcu.random(0xFFFF)
		if dofill > 0 then
			fill = mcu.random(0xFFFF)
			lcd.triangle(x1,y1,x2,y2,x3,y3,color,fill)
		else
			lcd.triangle(x1,y1,x2,y2,x3,y3,color)
		end
		tmr.wdclr()
    end;
	lcd.resetclipwin()
end;

function pixelDemo(sec)
	header("PUTPIXEL")

	lcd.setclipwin(0,miny,maxx,maxy)
	local n = tmr.tick() + (sec*1000)
	local x, y, color
	while tmr.tick() < n do
		x = mcu.random(maxx-1)
		y = mcu.random(maxy-1,miny)
		color = mcu.random(0xFFFF)
		lcd.putpixel(x,y,color)
		tmr.wdclr()
    end;
	lcd.resetclipwin()
	if (maxx > maxy) then
		if file.open("nature_160x123.img",'r') then
			file.close()
			lcd.image(math.floor((maxx-160) / 2),miny + 4,160,123,"nature_160x123.img")
		else
			lcd.write(lcd.CENTER,miny+4,"Image not found")
		end
	else
		if file.open("newyear_128x96.img",'r') then
			file.close()
			lcd.image(math.floor((maxx-128) / 2),miny + 4,128,96,"newyear_128x96.img")
		else
			lcd.write(lcd.CENTER,miny+4,"Image not found")
		end
	end
	tmr.wdclr()
end;

function intro(sec)
	tmr.wdclr()
	maxx, maxy = lcd.getscreensize()
	local inc = 360 / maxy
	local i
	for i=0,maxy-1,1 do
		lcd.line(0,i,maxx-1,i,lcd.hsb2rgb(i*inc,1,1))
	end
	lcd.setrot(0);
	lcd.setcolor(lcd.BLACK)
	lcd.setfont(lcd.FONT_DEJAVU18)
	local y = (maxy/2) - (lcd.getfontheight() / 2)
	lcd.settransp(1)
	lcd.write(lcd.CENTER,y,"WiFiMCU")
	y = y + lcd.getfontheight()
	lcd.write(lcd.CENTER,y,"LCD demo")
	lcd.settransp(0)
	for i=1, sec, 1 do
		tmr.delayms(1000)
		tmr.wdclr()
	end
end;

function lcdDemo(sec, orient)
	lcd.setorient(orient)

	intro(sec)
	dispFont(sec)
	tmr.delayms(2000)
	fontDemo(sec,0)
	tmr.delayms(2000)
	fontDemo(sec,1)
	tmr.delayms(2000)
	lineDemo(sec,1)
	tmr.delayms(2000)
	circleDemo(sec,0)
	tmr.delayms(2000)
	circleDemo(sec,1)
	tmr.delayms(2000)
	rectDemo(sec,0)
	tmr.delayms(2000)
	rectDemo(sec,1)
	tmr.delayms(2000)
	triangleDemo(sec,0)
	tmr.delayms(2000)
	triangleDemo(sec,1)
	tmr.delayms(2000)
	pixelDemo(sec, orient)
	tmr.delayms(2000)
	tmr.wdclr()
end

function fullDemo(sec, rpt)
	while rpt > 0 do
		lcd.setrot(0);
		lcd.setcolor(lcd.CYAN)
		lcd.setfont(lcd.FONT_DEJAVU12)

		lcdDemo(sec, lcd.LANDSCAPE)
		tmr.delayms(5000)
		tmr.wdclr()
		lcdDemo(sec, lcd.PORTRAIT_FLIP)
		tmr.delayms(5000)
		tmr.wdclr()

		lcd.setcolor(lcd.YELLOW)
		lcd.write(lcd.CENTER,maxy-lcd.getfontheight() - 4,"That's all folks!")
		rpt = rpt - 1
	end
end

fullDemo(6, 1)
