--mqtt demo
function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

printtitle("= MQTT DEMO =")

-- Set mqtt.debug to 1 if you want to see debug messages
--mqtt.debug(1)

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


function cb_messagearrived1(topic,len,message)
	print('** [Message Arrived from loboris]\r\n topic: '..topic..' \r\n message: '..message)
end
 
function cb_messagearrived2(topic,len,message)
	print('** [Message Arrived from mosquitto]\r\n topic: '..topic..' \r\n message: '..message)
end

--[[function cb_messagearrived3(topic,len,message)
	print('** [Message Arrived from local]\r\n topic: '..topic..' \r\n message: '..message)
end]]--
 
function cb_connected(clt)
	print("** mqtt #"..clt.." connected.")
	-- If client is not yet subscribed (first connect), subscribe to topic(s)
	if clt == clt0 then
		if mqtt.issubscribed(clt0,"test") == 0 then
			mqtt.subscribe(clt0,"test",mqtt.QOS0)
		end
		if mqtt.issubscribed(clt0,"news") == 0 then
			mqtt.subscribe(clt0,"news",mqtt.QOS0)
		end
	elseif clt == clt1 then
		if mqtt.issubscribed(clt1,"wifimcu") == 0 then
			mqtt.subscribe(clt1,"wifimcu",mqtt.QOS0)
		end
	--[[elseif clt == clt2 then
		if mqtt.issubscribed(clt2,"wifimcunews") == 0 then
			mqtt.subscribe(clt2,"wifimcunews",mqtt.QOS0)
		end]]--
	end
end

function cb_disconnected(clt, flag)
    if flag == 1 then
		-- client was disconnected, but wil try to reconnect
		print("** mqtt #"..clt.." disconnected, will reconnect")
	elseif flag == 2 then
		-- client reconnect failed after max retries
		print("** mqtt #"..clt.." reconnect failed.")
		-- client cannot connect, we can close it
		mqtt.close(clt)
	end
end

clt0=mqtt.new("wifimcuclt", "wifimcu", "wifimculobo")
clt1=mqtt.new("wifimcuclt", "", "")
--clt2=mqtt.new("wifimcuclt", "wifimcu", "wifimculobo")

if clt0 >= 0 then
	mqtt.on(clt0,'message',cb_messagearrived1)
	mqtt.on(clt0,'connect',cb_connected)
	mqtt.on(clt0,'offline',cb_disconnected)
end

if clt1 >= 0 then
	mqtt.on(clt1,'message',cb_messagearrived2)
	mqtt.on(clt1,'connect',cb_connected)
	mqtt.on(clt1,'offline',cb_disconnected)
end

-- your local broker
--[[if clt2 >= 0 then
	mqtt.on(clt2,'message',cb_messagearrived3)
	mqtt.on(clt2,'connect',cb_connected)
	mqtt.on(clt2,'offline',cb_disconnected)
end]]--


print(mcu.mem())

print('Starting MQTT clients...')
if clt0 >= 0 then
	if mqtt.start(clt0,"loboris.eu",1883) ~= 0 then
		print("** Error starting mqtt client loboris.eu")
	end
end
if clt1 >= 0 then
	if mqtt.start(clt1,"test.mosquitto.org",1883) ~= 0 then
		print("** Error starting mqtt client test.mosquitto.org")
	end
end
--[[if clt2 >= 0 then
	if mqtt.start(clt2,"192.168.178.63",1883) ~= 0 then
		print("** Error starting mqtt client 192.168.178.63")
	end
end]]--

--mqtt.unsubscribe(clt0,"test")
--mqtt.unsubscribe(clt1,"wifimcu")
--mqtt.publish(clt0,"test",mqtt.QOS0, 'hi from wifimcu')
--mqtt.publish(clt1,"wifimcu",mqtt.QOS0, 'hi from wifimcu')

