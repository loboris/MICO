--ftp demo
-- This demo shows how to use ftp functions without callback functions

function prntitle(titl)
	print("\r\n"..string.rep("=",string.len(titl)).."\r\n"..titl.."\r\n"..string.rep("=",string.len(titl)))
end

prntitle("=  FTP DEMO, NO CALLBACK  =")

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

local stat, tbllist, n, i, data, fd

-- ** You can turn debug messages on to get detailed info on ftp operations
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

prntitle("Change remote directory to 'demos/net' and receive small file")
stat = ftp.chdir("demos/network")
if stat == 0 then
	stat = ftp.recv("webserver.lua")
	if stat > 0 then
		print("File received, length: ", stat)
	else
		print("Error receiving file: ", stat)
	end
else
	print("Error changing directory: ", stat)
end

prntitle("Create short local file and send it to server")
-- Change remote directory to ftp home
ftp.chdir("/")

file.close() -- just in case
fd = file.open("test.txt", "w")
if fd >= 0 then
	file.write(fd, "This is my test file to be written to ftp server")
	file.close(fd)
	--file.slist()

	stat = ftp.send("test.txt")
	if stat > 0 then
		print("File sent, length: ", stat)

		prntitle("Receive it back and show")
		stat, data = ftp.recv("test.txt", 1)
		if stat < 0 then
			print("Error receiving the file: ", stat)
		else
			print("Received "..stat.." byte(s)")
			print(data)
		end
	else
		print("Error sending file: ", stat)
	end

	prntitle("Append the string to the remote file")
	stat = ftp.sendstring("test.txt", "\r\nThis data is appended.", 1)
	if stat > 0 then
		print("String appended to file, length: ", stat)

		prntitle("Receive the file to string and print it")
		stat, data = ftp.recv("test.txt", 1)
		if stat < 0 then
			print("Error receiving the file: ", stat)
		else
			print("Received "..stat.." byte(s)")
			print(data)
		end
	else
		print("Error appending string to file: ", stat)
	end
else
	print("Error creating file")
end

prntitle("Receive bigger remote file to local file")
ftp.chdir("demos/lcd")

stat = ftp.recv("wifimcu.img")
if stat < 0 then
	print("Error receiving the file: ", stat)
else
	print("Received "..stat.." byte(s)")
end

prntitle("Create large local file and send it to server")
-- Change remote directory to ftp home
ftp.chdir("/")
tmr.wdclr()
file.close() -- just in case
fd = file.open("longtest.txt", "w")
if fd >= 0 then
	-- create file of 40000 bytes
	for i=1,1000,1 do
		file.write(fd, "1234567890qwertzuiopASDFGHJKLYyxcvbnm=\r\n")
	end
	file.close(fd)
	tmr.wdclr()

	stat = ftp.send("longtest.txt")
	if stat > 0 then
		print("File sent, length: ", stat)
	else
		print("Error sending file: ", stat)
	end
else
	print("Error creating file")
end

prntitle("Try to get too BIG file, will be truncated!")

file.remove("bigfile.bin") -- if exists

stat = ftp.recv("bigfile.bin")
if stat < 0 then
	print("Error receiving the file: ", stat)
else
	print("Received "..stat.." byte(s)")
	print("*** DELETE 'bigfile.bin' TO FREE SPACE ***")
	print('execute: file.remove("bigfile.bin")')
end


-- end ftp session
stat = ftp.stop()
print("FTP session ended, result: ", stat)
