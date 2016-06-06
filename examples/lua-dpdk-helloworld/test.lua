--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)
function lua_getlcore()
	--print("In test.lua - 0")
	local ffi = require 'ffi'

	--print("In test.lua - 1")
	ffi.cdef[[
	int get_lcore(void);
	]]

	--int get_lcore(__attribute__((unused)) void *arg);
	--[[
	local text = "helloworld"
	local c_str = ffi.new("char[?]", #text)
ffi.copy(c_str, text)
	]]

	--print("In test.lua - 2")
	local hwl = ffi.load('./libhelloworld.so')
	
	if hwl == nil then
		print("Load ./libhelloworld.so error!")
	else

		hwl.get_lcore()
	end
	--print("In test.lua - 4")
end
