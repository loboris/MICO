--http demo

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("=                    HTTP DEMO                     =")
prntitle("= Send multiple http requests waiting for response =")

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

-- connect to web site, send request and wait response
-- the response is in "hdata" global variable
function request(url, opt, pdata)
	local stat;
	-- connect the socket
	if opt ~= nil then
		stat = net.start(httpskt, 80, webserv, opt)
	else
		stat = net.start(httpskt, 80, webserv)
	end
	if stat < 0 then
		print("Error connecting", stat)
		return -1
	end
	-- send request & receive response
	if pdata ~= nil then
		stat,hdata = net.send(httpskt, url, pdata)
	else
		stat,hdata = net.send(httpskt, url)
	end
	if stat < 0 then
		print("Error receiving data", stat)
		return -1
	end
	return 0
end
-- We don't use CB functions in this demo

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

--net.on(httpskt,"disconnect",function(skt) print("DISCONNECTed: "..skt) end)

-- connect to this web server
webserv = "loboris.eu"
-- do not print http headers
printhdr = false
hdata = ""


if request("GET /", {http=1,wait=4}) < 0 then return end
prntitle("Simple html response:")
print(hdata)

if request("GET /", {http=1,wait=4}) < 0 then return end
prntitle("Html response with cleaned markup:")
print(hclean(hdata))

if request("GET /wifimcu/wifimcu.txt", {http=1,wait=4}) < 0 then return end
prntitle("Get text page:")
print(hdata)

if request("GET /wifimcu/wifimcu.py?user=wifimcu&pass=wf1234&data=7.9&par1=10", {http=2,wait=4}) < 0 then return end
prntitle("GET request with params:")
print(hdata)

postdata = {data="T=7.4;V=5.2;H=34", time=rtc.getasc(), id=7782}
if request("POST /wifimcu/wifimcu.py", {http=2,wait=4}, postdata) < 0 then return end
prntitle("POST request with params:")
print(hdata)

postdata = nil
hdata = nil

