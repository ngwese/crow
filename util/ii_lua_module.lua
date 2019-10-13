local descriptor = require('util/ii_descriptor')
get_offset = descriptor.get_offset

function lua_cmds(f)
    local c = ''
    local i2c_address = descriptor.base_address(f)
    local impl = 'ii.set'
    if f.set_impl ~= nil then
        impl = f.set_impl
    end
    for _,v in ipairs( f.commands ) do
        c = c .. 'function ' .. f.lua_name .. '.' .. v.name .. '(...)' .. impl .. '('
          .. i2c_address .. ',' .. v.cmd .. ',...)end\n'
    end
    return c
end

function lua_getters(f)
    local i2c_address = descriptor.base_address(f)
    local g = f.lua_name .. '.g={\n'
    for _,v in ipairs( f.commands ) do
        if v.get == true then
            g = g .. '\t[\'' .. v.name .. '\']=' .. (v.cmd + descriptor.get_offset) .. ',\n'
        end
    end
    if f.getters ~= nil then
        for _,v in ipairs( f.getters ) do
            g = g .. '\t[\'' .. v.name .. '\']=' .. v.cmd .. ',\n'
        end
    end
    g = g .. '}\n'
    local impl = 'ii.get'
    if f.get_impl ~= nil then
        impl = f.get_impl
    end

    g = g .. 'function ' .. f.lua_name .. '.get(name,...)' .. impl .. '('
      .. i2c_address .. ',' .. f.lua_name .. '.g[name],...)end\n'
    return g
end

function lua_events(f)
    local e = f.lua_name .. '.e={\n'
    for _,v in ipairs( f.commands ) do
        if v.get == true then
            e = e .. '\t[' .. (v.cmd + descriptor.get_offset) .. ']=\'' .. v.name .. '\',\n'
        end
    end
    if f.getters ~= nil then
        for _,v in ipairs( f.getters ) do
            e = e .. '\t[' .. v.cmd .. ']=\'' .. v.name .. '\',\n'
        end
    end
    e = e .. '}\n'
    return e
end

function make_lua(f)
    local l = 'local ' .. f.lua_name .. '={}\n\n'
            .. lua_cmds(f)
            .. lua_getters(f)
            .. lua_events(f)
            .. 'function ' .. f.lua_name
                .. '.event(e,data)ii.e(\'' .. f.lua_name .. '\',e,data)end\n'
            .. 'print\'' .. f.lua_name .. ' loaded\'\n'
            .. 'return ' .. f.lua_name .. '\n'
    return l
end

local in_file = arg[1]
local out_file = arg[2]
do
    o = io.open( out_file, 'w' )
    o:write( make_lua( dofile( in_file)))
    o:close()
end

-- example usage:
-- lua util/ii_lua_module.lua lua/ii/jf.lua build/ii_jf.lua
