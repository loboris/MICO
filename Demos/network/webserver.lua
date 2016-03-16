-- Web Server demo

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("= Simple WEB server demo =")

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
if webskt ~= nil then
	-- close the socket if it existed
	net.close(webskt)
	tmr.delayms(100)
end

webskt = net.new(net.TCP,net.SERVER)
if webskt < 0 then
	print("Error creating socket")
	return
end

led1 = 17
gpio.mode(led1,gpio.OUTPUT)
adcpin = 1 -- set to 0 if you don't want to show adc measurement

function webrecv(clt, d)
	--print("[RECEIVED:\r\n"..d.."]") 
	local buf, method, vars, _GET, _on, resp, _v, _vs
	_vs = ""
	if adcpin > 0 then
		_v = adc.readV(adcpin,8)
		if _v ~= nil then
			_vs = "Voltage on pin "..adcpin..string.format(" = %1.2f V", _v)
		end
	end
	buf = ""
	_, _, method, path, vars = string.find(d, "([A-Z]+) (.+)?(.+) HTTP")
	if method == nil then
		_, _, method, path = string.find(d, "([A-Z]+) (.+) HTTP")
	end
	_GET = {}
	if (vars ~= nil) then
		--print("VARS: ["..vars.."]")
		for k, v in string.gmatch(vars, "(%w+)=(%w+)&*") do
			_GET[k] = v
		end
	end
	
	_on = '<span style="color:brown;border-width:1px;border-style:solid;border-color:black">'
	if (_GET.pin == "ON1") then
		gpio.write(led1, gpio.LOW)
		_on = _on.."ON"
	elseif (_GET.pin == "OFF1") then
		gpio.write(led1, gpio.HIGH)
		_on = _on.."OFF"
	else
		if gpio.read(led1) == 0 then
			_on = _on.."ON"
		else
			_on = _on.."OFF"
		end
	end
	_on = _on..'</span> '

	buf = buf..'<!DOCTYPE html><html><body bgcolor="#C7EDF8"><h1>Welcome to <font color="red">WiFiMCU</font> Web Server</h1>'
	buf = buf..'<p><font color="blue">LED</font> is '.._on..'&nbsp;&nbsp;turn it&nbsp; <a href=\"?pin=ON1\"><button>ON</button></a>'
	buf = buf..'&nbsp;or&nbsp;<a href=\"?pin=OFF1\"><button>OFF</button></a></p>'
	if _vs ~= "" then
		buf = buf..'<p></p><p><font color="darkblue">'.._vs..'</font></p>'
	end
	buf = buf..'<p></p><p><small><i>Demo by LoBo 02/2016</i></small></p></body></html>'
	resp = "HTTP/1.1 200 OK\r\nServer: WiFiMCU\r\nContent-Type:text/html\r\nContent-Length: "
	resp = resp..tostring(string.len(buf)).."\r\nConnection: close\r\n\r\n"..buf
	net.send(clt, resp)
	collectgarbage()
end

net.on(webskt,"receive", webrecv)
net.on(webskt,"accept", function(clt,ip,port) print("ACCEPT: "..ip..":"..port..", clt: "..clt) end)
--net.on(webskt,"sent", function(clt) print("SENT") net.close(clt) end)
net.on(webskt,"sent", function(clt) net.close(clt) end)

webport = 80 -- port on which the web server will listen
local stat = net.start(webskt, webport) 
if stat < 0 then
	print("Error:", stat)
	return
end

local _ip
_ip = wifi.sta.getip()
print("Web server started, skt: "..webskt..", listening on ".._ip..":"..webport.."\r\n")
print("Execute: net.close(webskt) to stop it.")
