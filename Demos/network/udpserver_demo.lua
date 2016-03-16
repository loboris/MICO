-- UDP Server demo
-- use "udpclient.lua" on your pc to test UDP server

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("= UDP server demo =")

-- ** Start wifi in station mode and wait for connection ***
local wifiok = wifi.startsta()
if wifiok == false then
	print("Waiting for wifi connection...")
	--local wifiok = wifi.startsta({ssid="myRouterSSID", pass="routerKEY", wait=12})
	-- ** You can use default settings if configured in system params
	wifiok = wifi.startsta({wait=12})
	if wifiok == false then
		print("Wifi not connected!")
		return
	end
end

-- Create new server socket
if sudpskt ~= nil then
	-- close the socket if it existed
	net.close(sudpskt)
	tmr.delayms(100)
end

sudpskt = net.new(net.UDP,net.SERVER)
if sudpskt < 0 then
	print("Error creating socket")
	return
end

net.on(sudpskt,"sent",function(clt) print("SENT to clt: "..clt) end)
net.on(sudpskt,"disconnect",function(clt) print("DISCONNECTED clt: "..clt) end)
net.on(sudpskt,"receive",function(clt,d)
	print("RECEIVED clt: "..clt.."\r\n["..d.."]\r\n")
	local stat
	stat = net.send(clt,"Hi from WiFiMCU UDP Server, my time is: "..rtc.getasc().."\r\n")
	print("SEND: "..stat)
end)

sudpport = 8000
stat = net.start(sudpskt, sudpport)
if stat < 0 then
	print("Error:", stat)
else
	local _ip = wifi.sta.getip()
	print("UDP server started, skt: "..sudpskt..", listening on: ".._ip..":"..sudpport.."\r\n")
end
