-- test.lua - A simple test script to verify Lua integration

-- Command metadata
local script = {
    help = "test - A test command to verify script loading",
    description = "Tests the Lua script integration system",
    
    -- Main command function
    run = function(args)
        -- This is a very simple script that just prints information
        -- about the Lua environment and any arguments passed
        
        local info = "Lua script system is working!\n"
        info = info .. string.format("Lua version: %s\n", _VERSION)
        
        if args and args ~= "" then
            info = info .. string.format("Arguments received: \"%s\"", args)
        else
            info = info .. "No arguments provided"
        end
        
        return info
    end
}

-- Return the script table
return script