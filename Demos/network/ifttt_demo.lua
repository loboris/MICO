-- Demo using IFTTT Maker chanel
-- Check this to learn how to setup yor maker channel:
-- http://www.makeuseof.com/tag/ifttt-connect-anything-maker-channel/

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("= IFTTT DEMO =")

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
if iftttskt ~= nil then
	-- close the socket if it existed
	net.close(iftttskt)
	tmr.delayms(100)
end
iftttskt = net.new(net.TCP,net.CLIENT)
if iftttskt < 0 then
	print("Error creating socket")
	return
end

my_event = "wifimcu"
my_key = "dmvNkqKV9bJbr2b4J1u02F"

-- Connect to IFTT maker server using GET request
print("Sending event "..my_event.." using GET request")
stat = net.start(iftttskt, 80,"maker.ifttt.com",{wait=5})
if stat == 0 then
    stat,res = net.send(iftttskt, "GET /trigger/"..my_event.."/with/key/"..my_key.."?value1=1234&value2=7.88")
	if stat == 0 then
		print(res)
	else
		print("Error:", stat)
	end
end

tmr.wdclr()
tmr.delayms(5000)
tmr.wdclr()

-- You can also use POST request, the data must be send as json string
print("Sending event "..my_event.." using POST request")
data = '{"value1" : "ID=wifimcu7", "value2" : "Time='..rtc.getasc()..'", "value3" : "T=12.4 C"}'
stat = net.start(iftttskt, 80,"maker.ifttt.com","http",{wait=5})
if stat == 0 then
    stat,res = net.send(iftttskt, "POST /trigger/"..my_event.."/with/key/"..my_key, data)
	if stat == 0 then
		print(res)
	else
		print("Error:", stat)
	end
end

