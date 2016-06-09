--package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
--print(package.cpath)

function lua_getlcore()
    local ffi = require 'ffi'

    ffi.cdef[[
    int get_lcore(void);
    ]]

    --int get_lcore(__attribute__((unused)) void *arg);
    --[[
    local text = "helloworld"
    local c_str = ffi.new("char[?]", #text)
    ffi.copy(c_str, text)
    --]]

    local hwl = ffi.load('./libhelloworld.so')

    if hwl == nil then
        print("Load ./libhelloworld.so error!")
    else
        hwl.get_lcore()
    end
end

do
    local LPM = require("ipv4")
    num_entry = LPM.lpm_setup()
    print("init the LPM module ... ")
    print("\ttotal number of entries is: ".. num_entry)

    print("Loopup ipv4 = 1.1.1.0, next hop = " .. LPM.lookup({1, 1, 1, 0}))
    print("Loopup ipv4 = 1.1.1.2, next hop = " .. LPM.lookup({1, 1, 1, 2}))
    print("Loopup ipv4 = 1.1.1.255, next hop = " .. LPM.lookup({1, 1, 1, 255}))
    print("Loopup ipv4 = 1.1.1.256, next hop = " .. LPM.lookup({1, 1, 1, 256}))
    
    print("Loopup ipv4 = 8.1.1.0, next hop = " .. LPM.lookup({8, 1, 1, 0}))
    print("Loopup ipv4 = 8.1.1.2, next hop = " .. LPM.lookup({8, 1, 1, 2}))
    print("Loopup ipv4 = 8.1.1.255, next hop = " .. LPM.lookup({8, 1, 1, 255}))
    print("Loopup ipv4 = 8.1.1.256, next hop = " .. LPM.lookup({8, 1, 1, 256}))
end
