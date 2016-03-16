--wifi demo
print("------wifi demo------")

--simple sta mode

-- Start wifi with your router settings

--cfg = {ssid="myRouterSSID", pass="routerKEY", wait=12}
--local wifiok = wifi.startsta({ssid="myRouterSSID", pass="routerKEY", wait=12})

-- ** You can use default settings if configured in system params, recommended!

-- check if already connected
if wifi.startsta() == true then
	-- stop it
	wifi.sta.stop()
	tmr.delayms(200)
	print("Wifi sta stopped.")
end

print("Starting Wifi sta...")
--wifiok = wifi.startsta(cfg) -- using configured settings
wifiok = wifi.startsta({wait=12}) -- using settings from system params

if wifiok == true then
	print ("Started.")
	print("Input command: wifi.sta.stop() to stop sta")
	print(" wifi.sta.getlink(): "..wifi.sta.getlink())
	--get ip address of sta mode
	print("   wifi.sta.getip(): "..wifi.sta.getip())
	--get adv ip information of sta mode
	print("wifi.sta.getipadv():")
	print(wifi.sta.getipadv())
else
	print("Not started!")
	print(" wifi.sta.getlink(): "..wifi.sta.getlink())
end

cfg=nil
ap_ssid=nil
ap_pwd=nil

--wifi.sta.stop()

--Enable IEEE power save mode
--wifi.powersave()
