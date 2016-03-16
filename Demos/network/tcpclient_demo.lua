-- TCP client demo
-- you can use "tcpecho.lua" as TCP server

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("= TCP CLIENT DEMO =")

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
if tcpskt ~= nil then
	-- close the socket if it existed
	net.close(tcpskt)
	tmr.delayms(100)
end
tcpskt = net.new(net.TCP,net.CLIENT)
if tcpskt < 0 then
	print("Error creating socket")
	return
end

-- setup your tcp server port and IP/domain
-- or use "loboris.eu", there is tcp echo server running on it
if tcp_server == nil then
	--tcp_server = "192.168.178.68"
	tcp_server = "loboris.eu"
end
tcp_port = 50002

-- === We can use tke socket without callback functions ===
prntitle("Sending data to TCP server without CB functions")
-- Configure socket, wait for IP address
stat = net.start(tcpskt, tcp_port, tcp_server, {wait=4})
if stat == 0 then
    stat, res = net.send(tcpskt, "Hi, i'm the tcp client")
	if (stat == 0) and (res ~= nil) then
		print("RESPONSE:\r\n["..res.."]")
	else
		print("No response")
	end
else
	print("Error:", stat)
	return
end


-- === Or we can use callback functions ===
net.close(tcpskt)
tmr.delayms(1000)
tcpskt = net.new(net.TCP,net.CLIENT)
if tcpskt < 0 then
	print("Error creating socket")
	return
end

-- we have to wait "connect" event to send message
net.on(tcpskt,"connect",function(skt)
		print("CONNECTED: "..skt)
		if net.send(skt, tcpmsg) ~= 0 then
			print("Error")
			tcpok=0 -- for timer: do not send
		end
	end)

--net.on(tcpskt,"dnsfound",function(skt,ip) print("DNSFOUND: "..skt..", ip: "..ip) end)

net.on(tcpskt,"sent",function(skt)
		print("MSG SENT: "..skt)
	end)

net.on(tcpskt,"disconnect",function(skt)
		print("DISCONNECTED: "..skt)
		if net.status(skt) < 0 then
			tcpok=0 -- for timer: do not send
		end
	end)

net.on(tcpskt,"receive",function(skt,d)
		print("RECEIVED:\r\n["..d.."]\r\n")
		tcpok=1 -- for timer to know it can send
	end)

-- flag for sending
tcpok = 0
tcp_nsend = 3
tcpmsg = "Hi from WiFiMCU"

prntitle("Sending data to TCP server using CB functions")
-- Configure socket, after connect the message will be send
stat = net.start(tcpskt, tcp_port, tcp_server)
if stat ~= 0 then
	print("Error:", stat)
	return
end

-- send message every 4 seconds, max 3 times
function tcp_tmr_cb()
    if (tcpok == 0) or (tcp_nsend == 0) then
		if tcp_nsend == 0 then
			print("Finished")
			net.close(tcpskt) -- close and free socket
			tmr.stop(tcptmr)
		else
			print("Message not sent "..tostring(tcp_nsend))
			tcp_nsend = tcp_nsend - 1
		end
	else
		-- reconnect socket, after connect the message will be send
		tcpmsg = "Hi from timer "..tostring(tcp_nsend)
		stat = net.start(tcpskt, tcp_port, tcp_server)
		if stat ~= 0 then
			print("Error:", stat)
			print("Finished")
			net.close(tcpskt)
			tmr.stop(tcptmr)
		end
		tcp_nsend = tcp_nsend - 1
	end
end

tcptmr = tmr.find()

if tcptmr ~= nil then
	tmr.start(tcptmr, 4000, tcp_tmr_cb)
else
	print("No free timers")
end
