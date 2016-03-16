--wifi demo
print("------wifi demo------")

--advanced ap mode
cfg={ssid = 'WiFiMCU_Wireless_adv',
	pwd = '12345678',
	ip='192.168.1.1',
	netmask='255.255.255.0',
	gateway='192.168.1.1',
	dnsSrv='192.168.1.1'}
	
function wifi_cb(info)
	print ("AP EVENT:")
	print(info)
	--get ip address of sta mode
	print("   wifi.ap.getip(): "..wifi.ap.getip())
	--get adv ip information of sta mode
	print("wifi.ap.getipadv():")
	print(wifi.ap.getipadv())
end

print("Input command string: wifi.ap.stop() to stop ap")

wifi.ap.stop()
wifi.startap(cfg, wifi_cb)
cfg = nil

--wifi.ap.stop()

--Enable IEEE power save mode
--wifi.powersave()