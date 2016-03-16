--http demo

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("=                      HTTP DEMO                       =")
prntitle("= Send multiple http requests using Callback functions =")

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

-- cleans the string containing html farmated text
function hclean(html)
	-- list of strings to replace (the order is important to avoid conflicts)
	local cleaner = {
		{ "&amp;", "&" }, -- decode ampersands
		{ "&#151;", "-" }, -- em dash
		{ "&#146;", "'" }, -- right single quote
		{ "&#147;", "\"" }, -- left double quote
		{ "&#148;", "\"" }, -- right double quote
		{ "&#150;", "-" }, -- en dash
		{ "&#160;", " " }, -- non-breaking space
		{ "<br ?/?>", "\r\n" }, -- all <br> tags whether terminated or not (<br> <br/> <br />) become new lines
		{ "</p>", "\r\n" }, -- ends of paragraphs become new lines
		{ "[  ]+", " " }, -- reduce all multiple spaces with a single space
		{ "<title>", "Title: " }, -- title
		{ "</title>", "" }, -- title
		{ "[\n\n]+", "\r\n" }, -- reduce all multiple new lines with a single new line
		{ "[\r\n\r\n]+", "\r\n" }, -- reduce all multiple new lines with a single new line
		{ "^\n*", "" }, -- trim new lines from the start...
		{ "\n*$", "" }, -- ... and end
		{ "(%b<>)", "" }, -- all other html elements are completely removed (must be done last)
	}

	-- clean html from the string
	for i=1, #cleaner do
		local cleans = cleaner[i]
		html = string.gsub( html, cleans[1], cleans[2] )
	end
	
	return html
end

-- Define callback functions

-- Connected to server
function sktconn(skt)
	print("CONNECTED: "..skt)
	-- send request to the server from requests table
	local rq = req[nreq]
	if rq ~= nil then
		if rq[4] ~= nil then
			prntitle(rq[4])
		end
		if rq[5] ~= nil then
			net.send(httpskt, rq[2], rq[5]) -- POST data
		else
			net.send(httpskt, rq[2])
		end
	end
end

-- Response received
function sktrecv(skt, dta, hdr)
	local rq = req[nreq]
	local clnh = false
	if rq ~= nil then
		clnh = rq[3]
	end
	if clnh then
		print(hclean(dta))
	else
		print(dta)
	end
	if printhdr and (dhr ~= nil) then
		print("HEADER: \r\n["..hdr.."]\r\n")
	end
	-- connect for next request from requests table
	nreq = nreq + 1
	rq = req[nreq]
	if rq ~= nil then
		net.start(httpskt, 80, webserv, rq[1])
	end
end


-- Create new socket
if httpskt ~= nil then
	-- close the socket if it existed
	net.close(httpskt)
	tmr.delayms(100)
end

httpskt = net.new(net.TCP,net.CLIENT) 
if httpskt < 0 then
	print("Error creating socket")
	return
end

-- Set callback functions
net.on(httpskt,"connect", sktconn)
net.on(httpskt,"receive",sktrecv)
net.on(httpskt,"disconnect",function(skt) print("DISCONNECTed: "..skt) end)
--net.on(httpskt,"dnsfound",function(skt,ip) print("DNS FOUND: skt: "..skt..", ip: "..ip) end)
--net.on(httpskt,"sent",function(skt) print("SENT: skt: "..skt) end)

-- Set requests to be send to the server
-- {options, request, cleanhtml, info, postdata}
req = {
	{{http=1}, "GET /", false, "HTML PAGE WITHOUT HTML CLEANING"},
	{{http=1}, "GET /", true, "HTML PAGE WITH HTML CLEANED"},
	{{http=1}, "GET /wifimcu/wifimcu.txt", false, "SPECIFIC TEXT PAGE"},
	{{http=2}, "GET /wifimcu/wifimcu.py?user=wifimcu&pass=wf1234&data=7.9&par1=10", false, "GET REQUEST WITH PARAMS"},
	{{http=2}, "POST /wifimcu/wifimcu.py", false, "POST REQUEST", {data="T=7.4;V=5.2;H=34", time=rtc.getasc(), id=7782}}
}

-- connect to this web server
webserv = "loboris.eu"
-- do not print http headers
printhdr = false
-- socket options

-- Connect for the first request, next requests will be handled by "receive" handler
nreq = 1
net.start(httpskt, 80, webserv, {http=1})
