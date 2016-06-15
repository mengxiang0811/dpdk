--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)

local ffi = require 'ffi'

ffi.cdef[[
void lpm_setup(const int mode, const int socketid);
int lpm_ipv4_route_add(uint32_t ip, uint8_t depth, uint8_t next_hop, const int socketid);
int lpm_ipv4_route_del(uint32_t ip, uint8_t depth, const int socketid);
void lpm_ipv4_route_del_all(const int socketid);
uint8_t lpm_lookup_single_packet_with_ipv4(unsigned int ip, int socketid);
]]

--[[
local text = "helloworld"
local c_str = ffi.new("char[?]", #text)
ffi.copy(c_str, text)
]]

local llpm = ffi.load('./libgfp_lpm.so')

ip = 1000

function test()
	print("LLPM");
llpm.lpm_setup(0, 0)
	print("table init");
llpm.lpm_ipv4_route_add(ip, 32, 3, 0)
	print("The next hop is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip, 0))
end
