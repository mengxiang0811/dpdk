--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)

local ffi = require 'ffi'

ffi.cdef[[
void lpm_check_ports();
void lpm_setup(const int mode, const int socketid);
int lpm_ipv6_route_add(uint8_t *ip, uint8_t depth, uint8_t next_hop, const int socketid);
int lpm_ipv6_route_del(uint8_t *ip, uint8_t depth, const int socketid);
void lpm_ipv6_route_del_all(const int socketid);
uint8_t lpm_lookup_single_packet_with_ipv6(uint8_t ip6_addr[16], int socketid);
]]

local e1 = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
local ip1 = ffi.new("uint8_t[16]", e1)

local e2 = {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
local ip2 = ffi.new("uint8_t[16]", e2)

local e3 = {3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
local ip3 = ffi.new("uint8_t[16]", e3)

local llpm = ffi.load('./libgfp_lpm.so')

function ipv6_test()
	print("LLPM: IPv6");
	llpm.lpm_setup(0, 0)
	print("table init");
	llpm.lpm_ipv6_route_add(ip1, 48, 1, 0)
	llpm.lpm_ipv6_route_add(ip2, 48, 2, 0)
	llpm.lpm_ipv6_route_add(ip3, 48, 3, 0)
	print("The next hop for ip1 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip1, 0))
	print("The next hop for ip2 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip2, 0))
	print("The next hop for ip3 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip3, 0))

	llpm.lpm_ipv6_route_del(ip1, 48, 0)
	print("The next hop for ip1 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip1, 0))
	print("The next hop for ip2 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip2, 0))
	print("The next hop for ip3 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip3, 0))

	llpm.lpm_ipv6_route_del_all(0)
	print("The next hop for ip1 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip1, 0))
	print("The next hop for ip2 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip2, 0))
	print("The next hop for ip3 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip3, 0))
	
	llpm.lpm_ipv6_route_add(ip1, 48, 1, 0)
	llpm.lpm_ipv6_route_add(ip2, 48, 2, 0)
	llpm.lpm_ipv6_route_add(ip3, 48, 3, 0)
end

function llpm_ipv6_lookup()
	print("The next hop for ip1 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip1, 0))
	print("The next hop for ip2 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip2, 0))
	print("The next hop for ip3 is: " .. llpm.lpm_lookup_single_packet_with_ipv6(ip3, 0))
end
