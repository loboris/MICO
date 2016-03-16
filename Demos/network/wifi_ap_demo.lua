--wifi demo
print("------wifi demo------")

--simple ap mode
cfg = {ssid = 'WiFiMCU_Wireless', pwd = ''}

wifi.ap.stop()
stat = wifi.startap(cfg)
cfg=nil

if stat == true then
	print ("Started.")
	--get ip address of sta mode
	print("   wifi.ap.getip(): "..wifi.ap.getip())
	--get adv ip information of sta mode
	print("wifi.ap.getipadv():")
	print(wifi.ap.getipadv())
	print("Input command string: wifi.ap.stop() to stop ap")
else
	print("Not started!")
end

--wifi.ap.stop()

--Enable IEEE power save mode
--wifi.powersave()
