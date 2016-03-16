-- UDP server to be used with WiFiMCU udp client demo

local socket = require("socket")

host = "*"
port = 50002
print("Binding to host '" ..host.. "' and port " ..port.. "...")
local tcp = socket.tcp()
assert(tcp:bind( host, port ))

tcp:settimeout(0.1)
tcp:listen(5)
print("Waiting on " .. host .. ":" .. port .. "...")

while 1 do
	tclient = tcp:accept()  --allow a new client to connect
	if tclient then
		tclient:settimeout(0)
		local ip, cport;
		ip, cport = tclient:getpeername()
		print( "Client connected from "..ip..":"..cport )
		local canread = socket.select({tclient}, nil, 1)
		if tclient == canread[1] then
			local data, errmsg, partial = tclient:receive()
			if data then
				recv = data
			else
				if partial and #partial > 0 then
					recv = partial
				else
					recv = "No data received"
				end
			end
			print("Received: ["..recv.."]")
			local repl = string.format("Welcome to WiFiMCU TCP server, your IP is %s,\r\nYour message was:\r\n%s", ip, recv)
			tclient:send( repl )
		end
		tclient:close()
	end
end
