local socket = require("socket")

udp = socket.udp()
udp:setpeername("192.168.178.108", 8000)
--udp:settimeout(0)
--udp:setblocking(0)

message = 'Hi from udp client, my time is '..os.date("%Y-%m-%d %H:%M:%S")

udp:send(message)

local canread = socket.select({udp}, nil, 2)
if udp == canread[1] then
	data = udp:receive()

	if data then
		print("Received: ["..data.."]")
	end
else
	print("No response received")
end
