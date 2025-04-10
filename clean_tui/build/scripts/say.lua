-- say.lua - Script for the 'say' command
-- This command sends a message to everyone in the current room

-- Command metadata
local script = {
    help = "say <message> - Speak a message to everyone in the room",
    description = "Broadcasts a message to all players in your current location",
    
    -- Main command function
    run = function(args)
        if not args or args == "" then
            return "Say what?"
        end
        
        -- Return a formatted message
        -- In a real implementation, this would broadcast to all players in the room
        return string.format("You say: \"%s\"", args)
    end
}

-- Return the script table
return script