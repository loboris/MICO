--wifi demo
print("------wifi demo------")

--advanced sta mode
--ap_ssid="routerSSID" --change to your ap ssid
--ap_pwd="routerKEY"   --change to your ap pwd
ap_ssid="LoBoInternet" --change to your ap ssid
ap_pwd="1412lobo956"   --change to your ap pwd

cfg={ssid = ap_ssid,
	pwd = ap_pwd,
	dhcp = 'disable',
	ip='192.168.178.222',
	netmask='255.255.255.0',
	gateway='192.168.178.1',
	dnsSrv='8.8.8.8'}
	
function wifi_sta_cb(info)
	print("\r\n[CB WiFi_STA]: "..info)
	if wifi.startsta() == true then
		print(" wifi.sta.getlink(): "..wifi.sta.getlink())
		--get ip address of sta mode
		print("   wifi.sta.getip(): "..wifi.sta.getip())
		--get adv ip information of sta mode
		print("wifi.sta.getipadv():")
		print(wifi.sta.getipadv())
	end
end

-- check if already connected
if wifi.startsta() == true then
	-- stop it
	print("Wifi sta started, stoping...")
	wifi.sta.stop()
	tmr.delayms(2000)
end

print("Start Wifi sta mode.")
wifi.startsta(cfg, wifi_sta_cb)

cfg=nil
ap_ssid=nil
ap_pwd=nil

print("Input command string: wifi.sta.stop() to stop sta")
--wifi.sta.stop()

--Enable IEEE power save mode
--wifi.powersave()
