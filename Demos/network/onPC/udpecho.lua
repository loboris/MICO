-- UDP server to be used with WiFiMCU udp client demo

local socket = require("socket")

host = "*"
port = 50007
print("Binding to host '" ..host.. "' and port " ..port.. "...")

udp = assert(socket.udp())
assert(udp:setsockname(host, port))
--assert(udp:settimeout(100))
ip, port = udp:getsockname()
assert(ip, port)

print("Waiting packets on " .. ip .. ":" .. port .. "...")

while 1 do
	local canread = socket.select({udp}, nil, 1)
	if udp == canread[1] then
		dgram, ip, port = udp:receivefrom()
		if dgram then
			local repl = string.format("Welcome to WiFiMCU UDP server, your IP is %s,\r\nYour message was:\r\n%s", ip, dgram)
			udp:sendto(repl, ip, port)
			print("Received: [" .. dgram .. "] from " .. ip .. ":" .. port)
		else
			print("Empty message from "..ip)
		end
	end
end
