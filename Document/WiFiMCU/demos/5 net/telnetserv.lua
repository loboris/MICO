--telnet demo
print("------telnet LUA interpreter demo------")

-- connect wifi if not connected ---
local ln=wifi.sta.getlink()
if ln ~= 'connected' then
	print("Starting wifi...")
	local cfg={}
	cfg.ssid="MySSID"
	cfg.pwd="MyPassword"
	wifi.startsta(cfg)
	cfg=nil
	local tmo = 0
	while ln ~= 'connected' do
		tmo = tmo + 1
		if tmo > 100 then break end
		tmr.delayms(100)
		ln=wifi.sta.getlink()
	end
	if ln == 'connected' then
		print("WiFi started in " .. string.format("%.1f",tmo/10) .. " second(s)")
	else
		print("WiFi not connected")
		return
	end
	tmo=nil
end

-- create new tcp server socket ---
skt = net.new(net.TCP,net.SERVER)
net.start(skt,2323) 

-- define ON functions ---
net.on(skt,"accept",function(clt,ip,port)
	print("accept: client ip: "..ip..", port: "..port..", clt: "..clt)
	net.send(clt,"Welcome to WiFiMCU LUA interpreter\r\ntype quit to exit.\r\n$> ")
end)

--net.on(skt,"sent",function(clt) print("sent:clt:"..clt) end)

net.on(skt,"disconnect",function(clt) print("disconnect:clt:"..clt) end)

net.on(skt,"receive",function(clt,d)
	local _res = "?"
	if string.sub(d,1,4) == "quit" then
		net.send(clt,"Bye.\r\n")
		print("disconnect:clt:"..clt)
		net.close(clt)
	else
		if string.sub(d,1,1) == "=" then
			_res = dostring("print("..string.sub(d,2)..")",1,"net")
		else
			_res = dostring(d,1,"net")
		end
		if _res == nil then _res = "??" end
		net.send(clt,_res.."\r\n$> ")
	end
end)

local _ip
_ip = wifi.sta.getip()
print("telnet server started, skt: "..skt..", ip: ".._ip.."\r\n")
