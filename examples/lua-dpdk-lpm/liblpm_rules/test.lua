--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)

local ffi = require 'ffi'

ffi.cdef[[
int lpm_table_init(int socket_id);
int lpm_entry_add(unsigned int ip, int depth, int next_hop, int socketid);
int lpm_entry_lookup(unsigned int ip, int socketid);
]]

--[[
local text = "helloworld"
local c_str = ffi.new("char[?]", #text)
ffi.copy(c_str, text)
]]

local llpm = ffi.load('./liblpm_rules.so')

ip = 1000

function test()
	print("LLPM");
llpm.lpm_table_init(0)
	print("table init");
llpm.lpm_entry_add(ip, 32, 1, 0)
	print("add entry");
	print("The next hop is: " .. llpm.lpm_entry_lookup(ip, 0))
end
