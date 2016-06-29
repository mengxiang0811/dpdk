--[[
--]]

print("------>>>>>>load the ffi library")

ffi = require 'ffi'
--Test the LPM table
LPM = require("ipv4")

ffi.cdef[[
void lpm_check_ports();

void lpm_init(const int is_ipv6_on, const int is_numa_on);

void lpm_setup_lcore(const int lcoreid);

int lpm_ipv4_route_add(uint32_t ip, uint8_t depth, uint8_t next_hop, const int lcoreid);

int lpm_ipv4_route_del(uint32_t ip, uint8_t depth, const int lcoreid);

void lpm_ipv4_route_del_all(const int lcoreid);

uint8_t lpm_lookup_single_packet_with_ipv4(unsigned int ip, int lcoreid);

int lpm_lookup_single_packet(struct rte_mbuf *m, uint8_t portid, int lcoreid);

void lpm_lookup(int nb_rx, struct rte_mbuf **pkts_burst, uint8_t portid, uint16_t *dst_port, const int lcoreid);

void lpm_change_grantor_active_lookup_structure(const int lcoreid);

void *lpm_get_ipv4_grantor_lookup_struct(const int lcoreid);

int get_lcore();
int poll(struct pollfd *fds, unsigned long nfds, int timeout);
int printf(const char *fmt, ...);
]]

print("------>>>>>>load the lpm library")
llpm = ffi.load('./libgfp_lpm.so')

function lpm_table_init(lcore_id)
	llpm.lpm_setup_lcore(lcore_id)
    --populate the LPM table
    for idx, entry in ipairs(lrt) do
        ret = llpm.lpm_ipv4_route_add(entry[1], entry[2], entry[3], lcore_id)
        if ret < 0 then
            print( "Unable to add entry " .. idx .. " to the l3fwd LPM table on lcore " .. lcoreid)

            print("\tLPM: Adding route " .. entry[1] .. " " .. entry[2] .. " " .. entry[3])
        end
    end
end

function lpm_init()
	--local lrt = LPM.lpm_setup()
	LPM.lpm_setup()
end

--Function to setup the LPM table
function llpm_setup()
    print("init the LPM module ... ")
    lpm_init()
    llpm.lpm_init(1, 1);
    print("\ttotal number of entries is: ".. #lrt)
end

--Function to lookup one single packet
function llpm_simple_lookup(pkt, lcore_id)
    return llpm.lpm_lookup_single_packet(pkt, 100, lcore_id)
end

--Function to lookup a set of packets
function llpm_opt_lpm_lookup(nb_rx, pkts_burst, dst_port, lcore_id)
	llpm.lpm_lookup(nb_rx, pkts_burst, 254, dst_port, lcore_id)
end

---------------------------------------------------------------------------------
---------------------------------------------------------------------------------
---------------------------------------------------------------------------------
---------------------------------------------------------------------------------
---------------------------------------------------------------------------------
---------------------------------------------------------------------------------
---------------------------------------------------------------------------------
---------------------------------------------------------------------------------

local function sleep(s)
	ffi.C.poll(nil, 0, s * 1000)
end

function llpm_lookup()

    local nh = 0
    local lcore_id = llpm.get_lcore()

    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({2, 1, 1, 0}), lcore_id)

    if nh ~= 1 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 2.1.1.0, next hop = %g\n\n", lcore_id, nh)
    --print("+++end lcore " .. lc)

    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({4, 1, 1, 2}), lcore_id)
    if nh ~= 3 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 4.1.1.2, next hop = %g\n\n", lcore_id, nh)

    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({6, 1, 1, 2}), lcore_id)
    if nh ~= 5 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 6.1.1.2, next hop = %g\n\n", lcore_id, nh)
    
    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({8, 1, 1, 255}), lcore_id)
    if nh ~= 7 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 8.1.1.255, next hop = %g\n\n", lcore_id, nh)
end
