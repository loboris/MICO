
-- OPTIONAL, you can use initialization parameters
-- if the default does not work with your oled display

initp = {
    0xAE,	    -- display off
    0xD5,0x80,  -- clock div
    0xA8,0x3F,  -- multiplex (height-1
    0xD3,0x00,  -- display offset
    0x40,       -- start line
    0x8D,0x14,  -- charge pump, 0x14 on; 0x10 off
    0x20,0x00,  -- memory address mode (0 horizontal; 1 vertical; 2 page)
    0x22,0,7,	-- page start&end address
    0x21,0,127,	-- column start&end address
    0xA1,	    -- seg remap 1
    0xC8,	    -- remapperd mode (0xC0 = normal mode)
    0xDA,0x12,	-- seq com pins (64 line 0x12; 32 line 0x02)
    0x81,0xCF,	-- contrast (extVcc 0x9F; charge pump 0xCF)
    0xD9,0xF1,	-- precharge (extVcc 0x22; charge pump 0xF1)
    0xDB,0x40,	-- vcomh detect level (0x20 default)
    0xA4,	    -- disable display all on
    --0xA6,	    -- display normal (not inverted)
    0xA7,	    -- inverted display
    0xAF	    -- display on
}


-- select the display interface (spi or i2c)
-- =========================================

spi.setup(2,{mode=3, cs=12, speed=15000})  -- hardware spi
--spi.setup(0,{mode=0, cs=12, speed=5000, mosi=8, sck=16 })  -- software spi

--i2c.setup(1,0,1)   -- hardware i2c
--i2c.setup(0,11,3)  -- software i2c

oled_OK = oled.init(2,14)          -- hardware spi
--oled_OK = oled.init(0,14,initp)    -- software spi with initialization

--oled_OK = oled.init(4)         -- hardware i2c
--oled_OK = oled.init(4,initp)   -- hardware i2c with initialization
--oled_OK = oled.init(3)         -- software i2c
--oled_OK = oled.init(3,initp)   -- software i2c with initialization

initp = nil

if oled_OK ~= 0 then
	print("Oled init error!")
	return
end

oled.inverse(0)
oled.fontsize(16)
oled.charspace(1)
oled.fixedwidth(1)
oled.write(0,0,0," WiFiMCU")
oled.write(0,2,0," LUA")
oled.fontsize(8)
oled.write(38,3,0,"interpreter")
oled.fixedwidth(0)
oled.inverse(1)
oled.write(0,5,0," \r")
oled.write(0,6,0," Hi from WiFiMCU \r")
oled.inverse(0)
