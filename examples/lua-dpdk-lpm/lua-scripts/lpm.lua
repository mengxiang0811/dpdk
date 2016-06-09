--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)

print("Load the ffi library")

ffi = require 'ffi'
--Test the LPM table
LPM = require("ipv4")

print("Load the lpm library")
llpm = ffi.load('./liblpm_rules.so')
--llpm = ffi.load('./librte_lpm.so')

ffi.cdef[[
int lpm_table_init(int socket_id);
int lpm_entry_add(unsigned int ip, int depth, int next_hop, int socketid);
int lpm_entry_lookup(unsigned int ip, int socketid);
]]

function lpm_table_init(socketid)
	--local lrt = LPM.lpm_setup()
	LPM.lpm_setup()
	llpm.lpm_table_init(socketid)
    --populate the LPM table
    for idx, entry in ipairs(lrt) do
        ret = llpm.lpm_entry_add(entry[1], entry[2], entry[3], socketid)
        if ret < 0 then
            print( "Unable to add entry " .. idx .. " to the l3fwd LPM table on socket " .. socketid)

            print("\tLPM: Adding route " .. entry[1] .. " " .. entry[2] .. " " .. entry[3])
        end
    end
end

function lpm_init()
    if llpm == nil then
        print("Load ./liblpm_rules.so error!")
    else
        --setup the LPM table
        lpm_table_init(0)
    end
end

function llpm_setup()
    print("init the LPM module ... ")
    lpm_init()
    print("\ttotal number of entries is: ".. #lrt)
end

function llpm_lookup()
    nh = llpm.lpm_entry_lookup(to_ipv4({1, 1, 1, 0}), 0)
    print("Loopup ipv4 = 1.1.1.0, next hop = " .. nh)

    nh = llpm.lpm_entry_lookup(to_ipv4({1, 1, 1, 2}), 0)
    print("Loopup ipv4 = 1.1.1.2, next hop = " .. nh)

    nh = llpm.lpm_entry_lookup(to_ipv4({1, 1, 1, 255}), 0)
    print("Loopup ipv4 = 1.1.1.255, next hop = " .. nh)

    nh = llpm.lpm_entry_lookup(to_ipv4({1, 1, 1, 256}), 0)
    print("Loopup ipv4 = 1.1.1.256, next hop = " .. nh)

    nh = llpm.lpm_entry_lookup(to_ipv4({8, 1, 1, 0}), 0)
    print("Loopup ipv4 = 8.1.1.0, next hop = " .. nh)

    nh = llpm.lpm_entry_lookup(to_ipv4({8, 1, 1, 2}), 0)
    print("Loopup ipv4 = 8.1.1.2, next hop = " .. nh)

    nh = llpm.lpm_entry_lookup(to_ipv4({8, 1, 1, 255}), 0)
    print("Loopup ipv4 = 8.1.1.255, next hop = " .. nh)

    nh = llpm.lpm_entry_lookup(to_ipv4({8, 1, 1, 256}), 0)
    print("Loopup ipv4 = 8.1.1.256, next hop = " .. nh)
end
