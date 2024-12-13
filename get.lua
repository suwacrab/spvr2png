local oslib = {}

local printf = function(str,...) print(str:format(...)) end

oslib.exec = function(cmd) 
	local d = os.execute(cmd)
	if not d then
		printf("oslib: error: exec failed -> '%s'",cmd)
		error("exec fail")
	end
end
oslib.execa = function(...)
	local str = table.concat( {...}," ")
	return oslib.exec(str)
end
oslib.execaP = function(...)
	local str = table.concat( {...}," ")
	local handle = io.popen(str,"rb")
	handle:close()
end
oslib.execf = function(str,...)
	return oslib.execa(str:format(...))
end

for i = 1,30 do
	local filename_src = ("workdata\\EYECATCHSRC\\EYECATCH%02d.PVR"):format(i)
	local filename_out = ("workdata\\EYECATCH\\eyecatch%02d.png"):format(i)
	print(filename_src,filename_out)
	oslib.execaP("bin\\spvr2png.exe",
		"-s",filename_src,
		"-d",filename_out,
		"&&",
		"magick",
		filename_out,
		"-rotate -90",
		"-crop 480x640+0+384",
		filename_out
	)	
end
