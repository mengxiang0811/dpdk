package.cpath = package.cpath .. ";./?.a;/home/developer/dpdk/x86_64-native-linuxapp-gcc/lib/?.so.2.1;"
print(package.cpath)

local ffi = require 'ffi'

ffi.cdef[[
int get_lcore(__attribute__((unused)) void *arg);
]]

--[[
local text = "helloworld"
local c_str = ffi.new("char[?]", #text)
ffi.copy(c_str, text)
]]

local hello = ffi.load('./libhelloworld.so')

hello.get_lcore(nil)
