--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)

ffi = require 'ffi'

ffi.cdef[[
struct rte_lpm;
struct rte_lpm_config;
struct rte_lpm *rte_lpm_create(const char *name, int socket_id, const struct rte_lpm_config *config);
int rte_lpm_add(struct rte_lpm *lpm, uint32_t ip, uint8_t depth, uint32_t next_hop);
static int rte_lpm_lookup(struct rte_lpm *lpm, uint32_t ip, uint32_t *next_hop);
]]

llpm = ffi.load('./librte_lpm.so')

IPV4_L3FWD_LPM_MAX_RULES = 1024
IPV4_L3FWD_LPM_NUMBER_TBL8S =  2^8

function lpm_table_init(socket_id)

    local config_ipv4 = ffi.new("struct rte_lpm_config[1]", 1)
    config_ipv4[0].max_rules = IPV4_L3FWD_LPM_MAX_RULES
    config_ipv4[0].number_tbl8s = IPV4_L3FWD_LPM_NUMBER_TBL8S
    config_ipv4[0].flags = 0

    local pv4_l3fwd_lpm_lookup_struct = ffi.new("struct rte_lpm[1]", 1)
    
    name = "IPV4_L3FWD_LPM_" .. socket_id --socket 0
    ipv4_l3fwd_lpm_lookup_struct[socket_id] = llpm.rte_lpm_create(name, socket_id, config_ipv4);

    if ipv4_l3fwd_lpm_lookup_struct[socket_id] == nil then
        print("create the lookup struct error!!!")
        return
    end

    --populate the LPM table
    for idx, entry in ipairs(lrt) do
        ret = rte_lpm_add(ipv4_l3fwd_lpm_lkup_struct[socketid], entry[1], entry[2], entry[3])
        if ret < 0 then
            print( "Unable to add entry " .. i .. " to the l3fwd LPM table on socket " .. socketid)

            print("LPM: Adding route " entry[1] .. " " .. entry[2] .. " " .. entry[3])
        end
    end
end

function lpm_init()
    if llpm == nil then
        print("Load ./librte_lpm.so error!")
    else
        --setup the LPM table
        lpm_table_init(0)
    end
end

--Test the LPM table
do
    local LPM = require("ipv4")
    num_entry = LPM.lpm_setup()
    print("init the LPM module ... ")
    print("\ttotal number of entries is: ".. num_entry)

    local nh = ffi.new("uint32_t[1]", 1)
    llpm.rte_lpm_lookup(pv4_l3fwd_lpm_lookup_struct, to_ipv4({1, 1, 1, 0}), nh)
    print("Loopup ipv4 = 1.1.1.0, next hop = " .. nh[0])

    print("Loopup ipv4 = 1.1.1.0, next hop = " .. LPM.lookup({1, 1, 1, 0}))
    print("Loopup ipv4 = 1.1.1.2, next hop = " .. LPM.lookup({1, 1, 1, 2}))
    print("Loopup ipv4 = 1.1.1.255, next hop = " .. LPM.lookup({1, 1, 1, 255}))
    print("Loopup ipv4 = 1.1.1.256, next hop = " .. LPM.lookup({1, 1, 1, 256}))

    print("Loopup ipv4 = 8.1.1.0, next hop = " .. LPM.lookup({8, 1, 1, 0}))
    print("Loopup ipv4 = 8.1.1.2, next hop = " .. LPM.lookup({8, 1, 1, 2}))
    print("Loopup ipv4 = 8.1.1.255, next hop = " .. LPM.lookup({8, 1, 1, 255}))
    print("Loopup ipv4 = 8.1.1.256, next hop = " .. LPM.lookup({8, 1, 1, 256}))

    lpm_init()
end
