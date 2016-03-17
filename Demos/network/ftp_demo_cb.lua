--ftp demo
-- This demo shows how to use ftp functions without callback functions

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("=  FTP DEMO, USING CALLBACK FUNCTIONS  =")

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

local stat, tbllist, n, i, fd

-- You can turn debug messages on to get detailed info
-- on ftp operations
--ftp.debug(0)
ftp.debug(1)

-- Stop ftp if it was active
ftp.stop()

prntitle("Configuring ftp server")
-- Configure ftp server
-- There is ftp server on "loboris.eu" which you can use for testing
stat = ftp.new("loboris.eu",21,"wifimcu","wifimcu")
if stat ~= 0 then
	print("Error configuring ftp: ", stat)
	return
else
	print("OK.")
end

prntitle("Login to server")
stat = ftp.start()
if stat ~= 0 then
	print("Error connecting to ftp server: ", stat)
	return
else
	print("Connected.")
end

prntitle("Get and print file list (only names)")
ftp.list(1,0)

prntitle("Get and print file list (detailes)")
ftp.list(0,0)

prntitle("Get file list from directory 'demos/network' to table")
tbllist, n = ftp.list(1,1,"demos/network")
if n > 0 then
	print("'demos/network' directory list:")
	for i=1,n,1 do
		print(tbllist[i])
	end
else
	print("No list received, error: ", n)
end

prntitle("Create short local file and send it to server")

fd = file.open("test.txt", "w")
if fd >= 0 then
	file.write(fd, "This is my test file to be written to ftp server")
	file.close(fd)
	--file.slist()

	stat = ftp.send("test.txt")
	if stat > 0 then
		print("File sent, length: ", stat)
	else
		print("Error sending file: ", stat)
	end

	prntitle("Append the string to the remote file")
	stat = ftp.sendstring("test.txt", "\r\nThis data is appended.", 1)
	if stat > 0 then
		print("String appended to file, length: ", stat)
	else
		print("Error appending string to file: ", stat)
	end
else
	print("Error creating file")
end

-- define callback function for receive event
function ftp_cb_recv(stat, data)
	if stat > 0 then
		prntitle("CB: FILE RECEIVED, length: "..stat)
		print(data)
	else
		print("\r\nCB: Error receiving file: ", stat)
	end
	ftp.stop()
end

prntitle("= Receive the file to string using callback function =")

ftp.stop()
tmr.delayms(2000)

prntitle("Configuring ftp server")
stat = ftp.new("loboris.eu",21,"wifimcu","wifimcu")
if stat ~= 0 then
	print("Error configuring ftp: ", stat)
	return
else
	print("OK.")
end

stat = ftp.on("receive", ftp_cb_recv)
if stat < 0 then
	print("Error configuring callback function: ", stat)
	ftp.stop()
	return
end

prntitle("Login to server")
stat = ftp.start()
if stat ~= 0 then
	print("Error connecting to ftp server: ", stat)
	return
else
	print("Connected, sending file receive request.")
	ftp.recv("test.txt", 1)
	-- callback function will be called after the file is received
end
