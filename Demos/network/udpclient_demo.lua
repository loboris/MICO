-- UDP client demo
-- you can use "udpecho.lua" on your PC as UDP server

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("= UDP CLIENT DEMO =")

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

-- Create new socket
if udpskt ~= nil then
	-- close the socket if it existed
	net.close(udpskt)
	tmr.delayms(100)
end
udpskt = net.new(net.UDP,net.CLIENT)
if udpskt < 0 then
	print("Error creating socket")
	return
end

-- setup your udp server port and IP/domain
-- or use "loboris.eu", there is udp echo server running on it
if udp_server == nil then
	--udp_server = "192.168.178.68"
	udp_server = "loboris.eu"
end
udp_port = 50007

-- === We can use tke socket without callback functions ===
prntitle("Sending data to UDP server without CB functions")
-- Configure socket, wait for IP address
stat = net.start(udpskt, udp_port, udp_server, {wait=4})
if stat == 0 then
    stat, res = net.send(udpskt, "Hi, i'm the udp client")
	if (stat == 0) and (res ~= nil) then
		print("RESPONSE:\r\n["..res.."]")
	else
		print("No response")
	end
else
	print("Error:", stat)
end

-- === Or we can use callback functions ===
net.close(udpskt)
tmr.delayms(1000)
udpskt = net.new(net.UDP,net.CLIENT)
if udpskt < 0 then
	print("Error creating socket")
	return
end

-- we have to wait "dnsfound" event before the FIRST send
-- after that we can just use net.send to send the message
net.on(udpskt,"dnsfound",function(skt,ip) print("DNSFOUND: skt: "..skt..", ip: "..ip) net.send(skt, "Hi from WiFiMCU") udpok=1 end)
--net.on(udpskt,"sent",function(skt) print("SENT: skt: "..skt) end)
net.on(udpskt,"disconnect",function(skt) print("DISCONNECTED: skt: "..skt) udpok=0 end)
net.on(udpskt,"receive",function(skt,d) print("RECEIVED:\r\n["..d.."]\r\n") end)

-- flag for sending
udpok = 0
udp_nsend = 3

prntitle("Sending data to UDP server using CB functions")
-- Configure socket, after getting the IP the message will be send
stat = net.start(udpskt, udp_port, udp_server)
if stat ~= 0 then
	print("Error:", stat)
	return
end

-- send message every 4 seconds, max 3 times
function udp_tmr_cb()
    if (udpok == 0) or (udp_nsend == 0) then
		if udp_nsend == 0 then
			print("Finished")
			net.close(udpskt)
			tmr.stop(udptmr)
		else
			print("Message not sent "..tostring(udp_nsend))
			udp_nsend = udp_nsend - 1
		end
	else
		net.send(udpskt, "Hi from timer "..tostring(udp_nsend))
		udp_nsend = udp_nsend - 1
	end
end

udptmr = tmr.find()

if udptmr ~= nil then
	tmr.start(udptmr, 4000, udp_tmr_cb)
else
	print("No free timers")
end
