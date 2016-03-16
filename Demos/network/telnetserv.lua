--telnet demo

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("= Telnet LUA interpreter demo =")

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

tdbg = 1 -- set to 1 if you want received data printed

-- create new tcp server socket ---
if tskt ~= nil then
	-- close the socket if it existed
	print("Already running")
	return
end
tskt = net.new(net.TCP,net.SERVER)
if tskt < 0 then
	print("Error creating socket")
	return
end

-- define ON functions ---

net.on(tskt,"accept",function(clt,ip,port)
	print("Accepted: client ip: "..ip..", port: "..port..", clt: "..clt)
	net.send(clt,"Welcome to WiFiMCU LUA interpreter\r\ntype quit to exit.\r\n$> ")
end)

--net.on(tskt,"sent",function(clt) print("SENT: "..clt) end)

net.on(tskt,"disconnect",function(clt)
		if tdbg == 1 then print("Client disconnected: "..clt) end
	end)

net.on(tskt,"receive",function(clt,d)
	if tdbg == 1 then print("RECEIVED: ["..d.."]") end
	local _res = "?"
	if string.sub(d,1,4) == "quit" then
		net.send(clt,"Bye.\r\n")
		if tdbg == 1 then print("Client ended session: "..clt) end
		net.close(clt)
	else
		if string.sub(d,1,1) == "=" then
			_res = dostring("print("..string.sub(d, 2)..")", 1, "telnet")
		else
			_res = dostring(d, 1, "telnet")
		end
		if _res == nil then _res = "??" end
		net.send(clt,_res.."\r\n$> ")
	end
end)

tport = 2323  -- port on which the telnet server will listen
-- start the server
stat = net.start(tskt, tport) 
if stat < 0 then
	print("Error:", stat)
	return
end

local _ip
_ip = wifi.sta.getip()
print("Telnet server started, skt: "..tskt..", listening on ".._ip..":"..tport.."\r\n")
