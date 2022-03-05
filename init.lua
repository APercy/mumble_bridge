local port
local udp = nil
local data, msg_or_ip, port_or_nil
local clients = {}
local wait_timer = 0
local timer_context = 0

local function get_server_info()
    local retVal = {}
    retVal.address = "your.server.adress"
    retVal.port = "30000"
    return retVal
end

local function get_mumble_context(player)
    local retVal = ""
    local serverinfo = get_server_info()
    if player then
        retVal = "mumble id "..player:get_player_name().."\n"..
        "mumble context "..serverinfo.address..":"..serverinfo.port.."\n"
    end
    return retVal
end

local function getSpatialData(player)
    if player then
        local name = player:get_player_name()
        if name then
            local player_pos = player:get_pos() or {x=0, y=0, z=0}
            local player_look = {x=0, y=0, z=0}
            local camera_pos = {x=0, y=0, z=0}
            local camera_look = {x=0, y=0, z=0}

            camera_pos = {x=player_pos.x, y=player_pos.y, z=player_pos.z}
            camera_pos.y = camera_pos.y + 1.5
            camera_look = player:get_look_dir()
            player_look = camera_look

            if camera_look and player_pos then
                local retval = "p p ["..(player_pos.x).." "..(player_pos.y).." "..(player_pos.z).."]\n"..
                "p l ["..(player_look.x).." "..(player_look.y).." "..(player_look.z).."]\n"..
                "c p ["..(camera_pos.x).." "..(camera_pos.y).." "..(camera_pos.z).."]\n"..
                "c l ["..(camera_look.x).." "..(camera_look.y).." "..(camera_look.z).."]\n"
                return retval
            end
        end
    end
    return nil
end

local function sanitizeNick(data_received)
    return string.gsub(data_received, "%c", ""):sub( 1, 20 )
end

local function setClient(uid, data_nick, data_ip, data_port)
    for _, c in pairs(clients) do
        if c.nick == data_nick then
            clients[uid] = nil
            break
        end
    end
    clients[uid] = { nick = sanitizeNick(data_nick),ip = data_ip, port = data_port }
end

-- require the socket and gets udp
if minetest.request_insecure_environment then
     insecure_environment = minetest.request_insecure_environment()
     if insecure_environment then
        local old_path = insecure_environment.package.path
        local old_cpath = insecure_environment.package.cpath
        
        insecure_environment.package.path = insecure_environment.package.path.. ";" .. "external/?.lua"
        insecure_environment.package.cpath = insecure_environment.package.cpath.. ";" .. "external/?.so"
        --overriding require to insecure require to allow modules to load dependencies
        local old_require = require
        require = insecure_environment.require

        --load modules
        local socket = require("socket")
        --reset changes
        require = old_require
        --reset changes
        insecure_environment.package.path = old_path
        insecure_environment.package.cpath = old_cpath

        -- find out ip
        port = 44000
        --ip = assert(socket.dns.toip(host))
        -- create a new UDP object
        udp = assert(socket.udp())
        if udp then
            udp:settimeout(0)
            udp:setsockname('*', port)
        end
    end
end

-- loop forever waiting for clients
minetest.register_globalstep(function(dtime)
    if udp then
        wait_timer = wait_timer + dtime
        if wait_timer > 0.5 then wait_timer = 0.5 end
        timer_context = timer_context + dtime
        if timer_context > 10 then timer_context = 10 end
        
        if wait_timer >= 0.5 then
            wait_timer = 0
            local uid = nil
            data, msg_or_ip, port_or_nil = udp:receivefrom()
            if data then
                --remove control characters and limmit to 20 chars
                local nick = sanitizeNick(data)
                --[[lets check if the player is the same that is connected
                to prevent remote monitoring by another player]]--
                if msg_or_ip == minetest.get_player_ip(nick) then
                    uid = nick.."@"..msg_or_ip..":"..port_or_nil
                    --so register the client as position receiver
                    setClient(uid, nick, msg_or_ip, port_or_nil)
                    --minetest.chat_send_all("connected as: " .. data .. msg_or_ip, port_or_nil)
                    --send the contect data to client
                    local player = minetest.get_player_by_name(nick)
                    local data_to_send = get_mumble_context(player)
                    pcall(udp:sendto(data_to_send, msg_or_ip, port_or_nil))
                end
            end
            
            for _, c in pairs(clients) do
                --minetest.chat_send_all(c.nick)
                local player = minetest.get_player_by_name(c.nick)
                if player then
                    --minetest.chat_send_all("ok")
                    local data_to_send = getSpatialData(player)
                    if data_to_send then
                        pcall(udp:sendto(data_to_send, c.ip, c.port))
                    end
                end
            end
        end
    end
end)


