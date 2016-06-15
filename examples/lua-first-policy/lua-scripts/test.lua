--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)

local ffi = require 'ffi'

ffi.cdef[[
void lpm_check_ports();
void lpm_setup(const int mode, const int socketid);
int lpm_ipv4_route_add(uint32_t ip, uint8_t depth, uint8_t next_hop, const int socketid);
int lpm_ipv4_route_del(uint32_t ip, uint8_t depth, const int socketid);
void lpm_ipv4_route_del_all(const int socketid);
uint8_t lpm_lookup_single_packet_with_ipv4(unsigned int ip, int socketid);
int lpm_lookup_single_packet(struct rte_mbuf *m, uint8_t portid, int socketid);
void lpm_lookup(int nb_rx, struct rte_mbuf **pkts_burst, uint8_t portid, uint16_t *dst_port, const int socketid);
void *lpm_get_ipv4_grantor_lookup_struct(const int socketid);
]]

--[[
local text = "helloworld"
local c_str = ffi.new("char[?]", #text)
ffi.copy(c_str, text)
]]

local llpm = ffi.load('./libgfp_lpm.so')

ip1 = 1000
ip2 = 2000
ip3 = 3000

function test()
	print("LLPM");
	llpm.lpm_setup(0, 0)
	print("table init");
	llpm.lpm_ipv4_route_add(ip1, 32, 1, 0)
	llpm.lpm_ipv4_route_add(ip2, 32, 2, 0)
	llpm.lpm_ipv4_route_add(ip3, 32, 3, 0)
	print("The next hop for " .. ip1 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip1, 0))
	print("The next hop for " .. ip2 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip2, 0))
	print("The next hop for " .. ip3 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip3, 0))
	llpm.lpm_ipv4_route_del(ip1, 32, 0)
	print("The next hop for " .. ip1 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip1, 0))
	print("The next hop for " .. ip2 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip2, 0))
	print("The next hop for " .. ip3 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip3, 0))
	llpm.lpm_ipv4_route_del_all(0)
	print("The next hop for " .. ip1 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip1, 0))
	print("The next hop for " .. ip2 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip2, 0))
	print("The next hop for " .. ip3 .. " is: " .. llpm.lpm_lookup_single_packet_with_ipv4(ip3, 0))
end
