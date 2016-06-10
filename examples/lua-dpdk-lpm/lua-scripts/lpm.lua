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
int get_lcore();
int poll(struct pollfd *fds, unsigned long nfds, int timeout);
int printf(const char *fmt, ...);
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

local function sleep(s)
	ffi.C.poll(nil, 0, s * 1000)
end

function llpm_lookup()

    local nh = 0
    local lc = llpm.get_lcore()

    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({2, 1, 1, 0}), 0)

    if nh ~= 1 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 2.1.1.0, next hop = %g\n\n", lc, nh)
    --print("+++end lcore " .. lc)

    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({4, 1, 1, 2}), 0)
    if nh ~= 3 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 4.1.1.2, next hop = %g\n\n", lc, nh)
    --print("---lcore " .. lc .. " Loopup ipv4 = 4.1.1.2, next hop = " .. nh)
    --print("+++end lcore " .. lc)

    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({6, 1, 1, 2}), 0)
    if nh ~= 5 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 6.1.1.2, next hop = %g\n\n", lc, nh)
    
    sleep(math.random(1, 3))
    nh = llpm.lpm_entry_lookup(to_ipv4({8, 1, 1, 255}), 0)
    if nh ~= 7 then 
    	print("Error nh!!!")
    end
    ffi.C.printf("---lcore %g Loopup ipv4 = 8.1.1.255, next hop = %g\n\n", lc, nh)
    --print("---lcore " .. lc .. " Loopup ipv4 = 8.1.1.255, next hop = " .. nh)
    --print("+++end lcore " .. lc)

--[[
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
--]]
end
