-- init spi first
-- swspi = 1

if swspi == 1 then
	spi.setup(0,{mode=0, cs=12, speed=10000, mosi=8, sck=16 })
	lcd_OK = lcd.init(0,14,3,lcd.PORTRAIT_FLIP)
else
	spi.setup(2,{mode=0, cs=12, speed=50000})
	lcd_OK = lcd.init(2,14,3,lcd.PORTRAIT_FLIP)
end

if lcd_OK ~= 0 then
	print("LCD nit initialized")
	return
end

tmr.wdclr()

function dispFont()
	lcd.clear()
	tmr.wdclr()
	local y = 0
	local tx = "Hi from LoBo"
	if maxx < maxy then
		lcd.setcolor(lcd.ORANGE)
		lcd.setfont(lcd.FONT_7SEG)
		lcd.write(0,0,"1234567890")
		y = 60
	end
	local x = 0
	local i = 0
	for i=1, 3, 1 do
		tmr.wdclr()
		lcd.setcolor(lcd.GREEN)
		lcd.setfont(lcd.FONT_8x8)
		lcd.write(x,y,tx)
		y = y + lcd.getfontheight()
		lcd.setcolor(lcd.CYAN)
		lcd.setfont(lcd.FONT_BIG)
		lcd.write(x,y,tx)
		tmr.wdclr()
		y = y + lcd.getfontheight()
		lcd.setcolor(lcd.YELLOW)
		lcd.setfont(lcd.FONT_SMALL)
		lcd.write(x,y,tx)
		tmr.wdclr()
		y = y + lcd.getfontheight()
		lcd.setcolor(lcd.RED)
		lcd.setfont(lcd.FONT_DEJAVU18)
		lcd.write(x,y,tx)
		tmr.wdclr()
		y = y + lcd.getfontheight()
		lcd.setcolor(lcd.BLUE)
		lcd.setfont(lcd.FONT_DEJAVU24)
		lcd.write(x,y,tx)
		tmr.wdclr()
		y = y + lcd.getfontheight() + 4
		if i == 1 then 
			x = lcd.CENTER
		end
		if i == 2 then
			x = lcd.RIGHT
		end
	end
	tmr.wdclr()
	lcd.setrot(0);
	lcd.setcolor(lcd.CYAN)
	lcd.setfont(lcd.FONT_SMALL)
end

function fontDemo(sec, rot)
	lcd.clear()
	tmr.wdclr()
	lcd.rect(0,0,maxx-1,miny,lcd.DARKGREY,lcd.DARKGREY)
	if rot > 0 then
		lcd.write(lcd.CENTER,1,"rotated font DEMO")
	else
		lcd.write(lcd.CENTER,1,"font DEMO")
	end
	local tx = "WiFiMCU"
	local x, y, color
	lcd.setrot(0)
	local n = tmr.tick() + (sec*1000)
	while tmr.tick() < n do
		if rot == 1 then
			lcd.setrot(math.floor(mcu.random(359)/5)*5);
		end
		lcd.setcolor(mcu.random(0xFFFF))
		lcd.setfont(lcd.FONT_8x8)
		x = mcu.random(maxx-8)
		y = mcu.random(maxy-lcd.getfontheight(),miny)
		lcd.write(x,y,tx)
		lcd.setcolor(mcu.random(0xFFFF))
		lcd.setfont(lcd.FONT_BIG)
		x = mcu.random(maxx-8)
		y = mcu.random(maxy-lcd.getfontheight(),miny)
		lcd.write(x,y,tx)
		lcd.setcolor(mcu.random(0xFFFF))
		lcd.setfont(lcd.FONT_SMALL)
		x = mcu.random(maxx-8)
		y = mcu.random(maxy-lcd.getfontheight(),miny)
		lcd.write(x,y,tx)
		lcd.setcolor(mcu.random(0xFFFF))
		lcd.setfont(lcd.FONT_DEJAVU18)
		x = mcu.random(maxx-8)
		y = mcu.random(maxy-lcd.getfontheight(),miny)
		lcd.write(x,y,tx)
		lcd.setcolor(mcu.random(0xFFFF))
		lcd.setfont(lcd.FONT_DEJAVU24)
		x = mcu.random(maxx-8)
		y = mcu.random(maxy-lcd.getfontheight(),miny)
		lcd.write(x,y,tx)
		tmr.wdclr()
	end
	lcd.setrot(0);
	lcd.setcolor(lcd.CYAN)
	lcd.setfont(lcd.FONT_SMALL)
end

function lineDemo(sec)
	lcd.clear()
	tmr.wdclr()
	lcd.rect(0,0,maxx-1,miny,lcd.DARKGREY,lcd.DARKGREY)
	lcd.write(lcd.CENTER,1,"line DEMO")
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
end;

function circleDemo(sec,dofill)
	lcd.clear()
	tmr.wdclr()
	lcd.rect(0,0,maxx-1,miny,lcd.DARKGREY,lcd.DARKGREY)
	if dofill > 0 then
		lcd.write(lcd.CENTER,1,"filled circle DEMO")
	else
		lcd.write(lcd.CENTER,1,"circle DEMO")
	end
	local n = tmr.tick() + (sec*1000)
	local x, y, r, color, fill
	while tmr.tick() < n do
		x = mcu.random(maxx-2,4)
		y = mcu.random(maxy-2,miny+2)
		if x < y then
			if x > math.floor(maxx / 2) then
				r = mcu.random(maxx-x,2)
			else
				r = mcu.random(x,2)
			end
		else
			if y > math.floor(maxy / 2) then
				r = mcu.random(maxy-y,2)
			else
				r = mcu.random(y-miny-4,2)
			end
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
end;

function rectDemo(sec,dofill)
	lcd.clear()
	tmr.wdclr()
	lcd.rect(0,0,maxx-1,miny,lcd.DARKGREY,lcd.DARKGREY)
	if dofill > 0 then
		lcd.write(lcd.CENTER,1,"filled rectangle DEMO")
	else
		lcd.write(lcd.CENTER,1,"rectangle DEMO")
	end
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
end;

function triangleDemo(sec,dofill)
	lcd.clear()
	tmr.wdclr()
	lcd.rect(0,0,maxx-1,miny,lcd.DARKGREY,lcd.DARKGREY)
	if dofill > 0 then
		lcd.write(lcd.CENTER,1,"filled triangle DEMO")
	else
		lcd.write(lcd.CENTER,1,"triangle DEMO")
	end
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
end;

function pixelDemo(sec, orient)
	lcd.clear()
	tmr.wdclr()
	lcd.rect(0,0,maxx-1,miny,lcd.DARKGREY,lcd.DARKGREY)
	lcd.write(lcd.CENTER,1,"putpixel DEMO")
	local n = tmr.tick() + (sec*1000)
	local x, y, color
	while tmr.tick() < n do
		x = mcu.random(maxx-1)
		y = mcu.random(maxy-1,miny)
		color = mcu.random(0xFFFF)
		lcd.putpixel(x,y,color)
		tmr.wdclr()
    end;
	if (orient == lcd.LANDSCAPE) or (orient == lcd.LANDSCAPE_FLIP) then
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

function lcdDemo(orient)
	lcd.setorient(orient)
	maxx, maxy = lcd.getscreensize()
	miny = lcd.getfontheight() + 4

	dispFont()
	tmr.delayms(4000)
	fontDemo(4,0)
	tmr.delayms(1000)
	fontDemo(4,1)
	tmr.delayms(1000)
	lineDemo(4,1)
	tmr.delayms(1000)
	circleDemo(4,0)
	tmr.delayms(1000)
	circleDemo(4,1)
	tmr.delayms(1000)
	rectDemo(4,0)
	tmr.delayms(1000)
	rectDemo(4,1)
	tmr.delayms(1000)
	triangleDemo(4,0)
	tmr.delayms(1000)
	triangleDemo(4,1)
	tmr.delayms(1000)
	pixelDemo(6, orient)
	tmr.delayms(1000)
	tmr.wdclr()
end

function fullDemo(rpt)
	while rpt > 0 do
		lcd.setrot(0);
		lcd.setcolor(lcd.CYAN)
		lcd.setfont(lcd.FONT_SMALL)

		lcdDemo(lcd.LANDSCAPE)
		tmr.delayms(6000)
		tmr.wdclr()
		lcdDemo(lcd.PORTRAIT_FLIP)

		lcd.setcolor(lcd.YELLOW)
		lcd.write(lcd.CENTER,maxy-lcd.getfontheight() - 4,"That's all folks!")
		rpt = rpt - 1
	end
end

fullDemo(1)
