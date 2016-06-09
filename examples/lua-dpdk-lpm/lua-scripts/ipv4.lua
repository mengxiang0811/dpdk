--This module declares methods to manage the IPv4 LPM rules table
local M = {}

lrt = {}

local rules_v4 = {
    {["ipv4"] = {1, 1, 1, 0}, ["len"] = 24, ["nh"] = 0},
    {["ipv4"] = {2, 1, 1, 0}, ["len"] = 24, ["nh"] = 1},
    {["ipv4"] = {3, 1, 1, 0}, ["len"] = 24, ["nh"] = 2},
    {["ipv4"] = {4, 1, 1, 0}, ["len"] = 24, ["nh"] = 3},
    {["ipv4"] = {5, 1, 1, 0}, ["len"] = 24, ["nh"] = 4},
    {["ipv4"] = {6, 1, 1, 0}, ["len"] = 24, ["nh"] = 5},
    {["ipv4"] = {7, 1, 1, 0}, ["len"] = 24, ["nh"] = 6},
    {["ipv4"] = {8, 1, 1, 0}, ["len"] = 24, ["nh"] = 7},
}

local function ip_check(a)
    if (type(a) == "number") and (a >= 0 and a <= 255) then
        return true
    else
        return false
    end
end

local function to_ipv4(t)
    local ip = -1

    if ip_check(t[1]) and ip_check(t[2]) and ip_check(t[3]) and ip_check(t[4]) then
        ip = (t[1] * 2^24) + (t[2] * 2^16) + (t[3] * 2^8) + (t[4])
    end

    return ip
end

function M.lpm_setup()
    for idx, entry in ipairs(rules_v4) do
        local ip = to_ipv4(entry["ipv4"])
        local len = entry["len"]
        local nh = entry["nh"]

        lrt[#lrt + 1] = {ip, len, nh}
    end

    return #lrt
end

function M.lookup(ip)
    num = to_ipv4(ip)

    if num == -1 then
        return -1 --invalid ipv4
    end

    for idx, entry in ipairs(lrt) do
        --print(num / 2^(32 - entry[2]) .. "  " .. entry[1]/2^(32 - entry[2]))
        if math.floor(num / 2^(32 - entry[2])) == math.floor(entry[1] / 2^(32 - entry[2])) then
            return entry[3]
        end
    end

    return 255 --default next hop is 255
end

return M
