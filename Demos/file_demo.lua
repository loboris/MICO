--file demo
print("\r\n------file demo------")

--format file system
--file.format()

--write data to a new file
fd = file.open("test.lua","w")
if fd >= 0 then
	file.writeline(fd, "print('hello world1')");
	file.writeline(fd, "print('hello world2')");
	file.writeline(fd, "print('hello world3')");
	file.close(fd)
else
    print("Error creating file!")
end

--read back
fd = file.open("test.lua","r")
if fd >= 0 then
	print(file.readline(fd))
	print(file.readline(fd))
	print(file.readline(fd))
	file.close()
else
    print("Error opening file!")
end

--check file existing or not
if file.exists('test.lua') then
	print("File test.lua exist")
else
	print("File test.lua does not exist")
end

if file.exists('testNO.lua') then
	print("File testNO.lua exist")
else
	print("File testNO.lua does not exist")
end

--rename the file
file.remove("testNew.lua") -- if exists
file.rename("test.lua","testNew.lua")

--show the file system information
print("\r\n--File system infomation--")
free,used,total=file.info();
print("last:  "..free.." bytes")
print("used:  "..used.." bytes")
print("total: "..total.." bytes")

--print file list
print("\r\n-- Print file list --")
file.slist()

-- === Using directories ===

-- goto root directory
file.chdir("/")

-- create some directories
print("\r\n-- Make some dirs and files --")
file.mkdir("testdir")
file.mkdir("progs")
file.mkdir("temp")
file.mkdir("/temp/myDir1")
file.mkdir("/temp/myDir2")
file.mkdir("/temp/myDir3")

function dummyFile(name)
	local fn = file.open(name,"w")
	if fn >= 0 then
		for i=1,10,1 do
			file.writeline(fn,"Test file, line #"..i)
		end
		file.close(fn)
	end
end

-- change to directory
file.chdir("temp")

-- make some files
dummyFile("myFile1") -- in current dir
dummyFile("/testdir/myFile2") -- in /testdir directory
dummyFile("/testdir/myFile3") -- in /testdir directory

file.chdir("/progs")
dummyFile("myLua1.lua")
dummyFile("myLua2.lua")
dummyFile("myLua3.lua")

-- goto root directory
file.chdir("/")

--list the files using table
local cdir, filet, fnlen
print("\r\n-- File list (from table) --")
cdir, filet = file.list(5)
print("Files in directory '"..cdir.."'")
--find max name length, so we can format the output
fnlen = 5
for k,v in pairs(filet) do 
	if string.len(k) > fnlen then
		fnlen = string.len(k)
	end
end

print(string.format("%"..fnlen.."s", "Name:").."  Size:")
for k,v in pairs(filet) do
	if v < 0 then
		-- directory
		print(string.format("%"..fnlen.."s", k).."  <DIR>")
	else
		print(string.format("%"..fnlen.."s", k).."  "..v)
	end
end


-- move file from one dir to another
file.rename("/testdir/myFile2", "/temp/myDir3/myFile2")

-- print directory tree
print("\r\n-- Print files and directories list --")
file.slist(5)

-- remove directory /temp and all files and subdirs in it
print("\r\n-- Remove '/temp' dir --")
file.rmdir("/temp", "removeall")

-- print directory tree
file.slist(5)

--remove the file
--file.remove("test.lua")

--compile the file to lc 
--file.compile("testNew.lua")

--dofile
--dofile('testNew.lua')
--dofile('testNew.lc')


