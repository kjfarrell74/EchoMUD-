#include "../include/GameEngine.h"
#include <algorithm>
#include <sstream>  // For stringstream
#include <iostream> // For debugging
#include <fstream>  // For file logging

// Implementation of internal logging function
namespace internal {
    void logDebug(const std::string& msg) {
        // Simple file logging implementation
        static std::ofstream logFile("game_engine_debug.log", std::ios::app);
        if (logFile) {
            logFile << "[DEBUG] " << msg << std::endl;
        }
        // Also log to stderr for debugging
        std::cerr << "[DEBUG] " << msg << std::endl;
    }
}

// Safe string copy function to avoid memory issues
static void safeStringAppend(std::string& dest, const char* src) {
    try {
        dest.append(src);
    } catch (const std::exception& e) {
        std::cerr << "Error appending string: " << e.what() << std::endl;
    }
}

GameEngine::GameEngine(const std::string& playerName)
    : m_player(playerName)
{
    // Register all available commands
    registerCommands();
}

void GameEngine::registerCommands() {
    // Register the 'say' command
    m_commands["say"] = {
        .name = "say",
        .help = "say <message>",
        .description = "Speak aloud in the room for others to hear.",
        .handler = [this](const std::string& args) -> std::string {
            try {
                std::string response;
                response.reserve(args.length() + 10); // Pre-allocate enough memory
                safeStringAppend(response, "You say: '");
                response.append(args);
                safeStringAppend(response, "'");
                return response;
            } catch (const std::exception& e) {
                return std::string("Error processing say command: ") + e.what();
            }
        }
    };
    
    // Register the 'look' command
    m_commands["look"] = {
        .name = "look",
        .help = "look",
        .description = "Look around and examine your surroundings.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            try {
                // Create a fixed-size response buffer to avoid allocations
                char response[512] = {0};
                
                // Get current room name
                const char* roomName = (m_player.currentRoom[0] != '\0') ? 
                    m_player.currentRoom : "an unknown location";
                
                // Create a static description based on room name
                strncpy(response, "You are in: ", sizeof(response) - 1);
                strncat(response, roomName, sizeof(response) - strlen(response) - 1);
                strncat(response, "\n\n", sizeof(response) - strlen(response) - 1);
                
                // Add room-specific static description
                if (strcmp(m_player.currentRoom, "Start Room") == 0) {
                    strncat(response, "This is the starting area, a simple room with stone walls and a wooden floor. "
                                    "There's a door leading north and a small window on the east wall.", 
                                    sizeof(response) - strlen(response) - 1);
                }
                else if (strcmp(m_player.currentRoom, "North Room") == 0) {
                    strncat(response, "This is a larger chamber with a high ceiling. Dusty tapestries hang on the walls, "
                                    "and there's an old desk in the corner. The exit to the south leads back to the starting room.", 
                                    sizeof(response) - strlen(response) - 1);
                }
                else {
                    strncat(response, "This area has not been fully explored yet. There are exits in various directions.", 
                                    sizeof(response) - strlen(response) - 1);
                }
                
                return std::string(response);
            } catch (...) {
                return "Critical error processing look command.";
            }
        }
    };
    
    // Register the 'get' command
    m_commands["get"] = {
        .name = "get",
        .help = "get <item>",
        .description = "Pick up an item from the current room.",
        .handler = [this](const std::string& args) -> std::string {
            try {
                char response[256] = {0};
                strncpy(response, "You pick up the '", sizeof(response) - 1);
                size_t len = strlen(response);
                strncat(response, args.c_str(), sizeof(response) - len - 1);
                len = strlen(response);
                strncat(response, "'.", sizeof(response) - len - 1);
                
                return std::string(response);
            } catch (...) {
                return "Error processing get command.";
            }
        }
    };
    
    // Register the 'north' command
    m_commands["north"] = {
        .name = "north",
        .help = "north",
        .description = "Move to the north if possible.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            try {
                if (m_hookSystem.beforeMove(m_player.name, "north")) {
                    return "You feel a mysterious force preventing you from moving north.";
                } else {
                    // Check if movement is valid from current room
                    if (strcmp(m_player.currentRoom, "Start Room") == 0) {
                        // Copy "North Room" to player's current room
                        memset(m_player.currentRoom, 0, Player::MAX_ROOM_LENGTH);
                        strncpy(m_player.currentRoom, "North Room", Player::MAX_ROOM_LENGTH - 1);
                        
                        char response[256] = {0};
                        strncpy(response, "You move north into ", sizeof(response) - 1);
                        size_t len = strlen(response);
                        strncat(response, m_player.currentRoom, sizeof(response) - len - 1);
                        len = strlen(response);
                        strncat(response, ".", sizeof(response) - len - 1);
                        
                        return std::string(response);
                    } else {
                        return "You can't go that way.";
                    }
                }
            } catch (...) {
                return "Error processing north command.";
            }
        }
    };
    
    // Register the 'south' command
    m_commands["south"] = {
        .name = "south",
        .help = "south",
        .description = "Move to the south if possible.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            try {
                if (m_hookSystem.beforeMove(m_player.name, "south")) {
                    return "You feel a mysterious force preventing you from moving south.";
                } else {
                    // Check if movement is valid from current room
                    if (strcmp(m_player.currentRoom, "North Room") == 0) {
                        // Copy "Start Room" to player's current room
                        memset(m_player.currentRoom, 0, Player::MAX_ROOM_LENGTH);
                        strncpy(m_player.currentRoom, "Start Room", Player::MAX_ROOM_LENGTH - 1);
                        
                        char response[256] = {0};
                        strncpy(response, "You move south into ", sizeof(response) - 1);
                        size_t len = strlen(response);
                        strncat(response, m_player.currentRoom, sizeof(response) - len - 1);
                        len = strlen(response);
                        strncat(response, ".", sizeof(response) - len - 1);
                        
                        return std::string(response);
                    } else {
                        return "You can't go that way.";
                    }
                }
            } catch (...) {
                return "Error processing south command.";
            }
        }
    };
    
    // Register the 'east' command
    m_commands["east"] = {
        .name = "east",
        .help = "east",
        .description = "Move to the east if possible.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            try {
                if (m_hookSystem.beforeMove(m_player.name, "east")) {
                    return "You feel a mysterious force preventing you from moving east.";
                } else {
                    // No valid east paths in our simple demo
                    return "You can't go that way.";
                }
            } catch (...) {
                return "Error processing east command.";
            }
        }
    };
    
    // Register the 'west' command
    m_commands["west"] = {
        .name = "west",
        .help = "west",
        .description = "Move to the west if possible.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            try {
                if (m_hookSystem.beforeMove(m_player.name, "west")) {
                    return "You feel a mysterious force preventing you from moving west.";
                } else {
                    // No valid west paths in our simple demo
                    return "You can't go that way.";
                }
            } catch (...) {
                return "Error processing west command.";
            }
        }
    };
    
    // Register the 'exit' command
    m_commands["exit"] = {
        .name = "exit",
        .help = "exit",
        .description = "Exit the game.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            return "Exiting game...";
        }
    };
    
    // Register the 'quit' command (alias for exit)
    m_commands["quit"] = {
        .name = "quit",
        .help = "quit",
        .description = "Exit the game.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            return "Exiting game...";
        }
    };
    
    // Register the 'help' command
    m_commands["help"] = {
        .name = "help",
        .help = "help [command]",
        .description = "Display help for all commands or a specific command.",
        .handler = [this](const std::string& args) -> std::string {
            return handleHelpCommand(args);
        }
    };
}

std::string GameEngine::handleHelpCommand(const std::string& args) {
    try {
        // Use a large fixed buffer for the help text
        static char helpBuffer[2048] = {0};
        
        // Clear the buffer
        memset(helpBuffer, 0, sizeof(helpBuffer));
        
        if (args.empty()) {
            // List all commands with their help strings
            strncpy(helpBuffer, "Available commands:\n", sizeof(helpBuffer) - 1);
            
            // Add each command
            for (const auto& [cmdName, entry] : m_commands) {
                // Skip certain commands if needed
                // if (cmdName == "someinternalcommand") continue;
                
                // Add command name
                strncat(helpBuffer, "  ", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                strncat(helpBuffer, cmdName.c_str(), sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                
                // Add separator
                strncat(helpBuffer, " - ", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                
                // Add description (truncate if too long)
                size_t remainingSpace = sizeof(helpBuffer) - strlen(helpBuffer) - 5;
                if (entry.description.length() < remainingSpace) {
                    strncat(helpBuffer, entry.description.c_str(), remainingSpace);
                } else {
                    strncat(helpBuffer, entry.description.c_str(), remainingSpace - 4);
                    strncat(helpBuffer, "...", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                }
                
                // Add newline
                strncat(helpBuffer, "\n", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
            }
            
            return std::string(helpBuffer);
        } else {
            // Show detailed help for a specific command
            auto cmdIt = m_commands.find(args);
            if (cmdIt != m_commands.end()) {
                const auto& entry = cmdIt->second;
                
                // Clear buffer for safety
                memset(helpBuffer, 0, sizeof(helpBuffer));
                
                // Format with simple concatenation to avoid memory issues
                char smallBuffer[32] = {0};
                
                // Add command name
                strncpy(helpBuffer, entry.name.c_str(), sizeof(helpBuffer) - 1);
                helpBuffer[sizeof(helpBuffer) - 1] = '\0';
                
                // Add separator
                strncat(helpBuffer, " - ", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                
                // Add help text (truncate if too long)
                if (entry.help.length() < sizeof(smallBuffer) - 1) {
                    strncat(helpBuffer, entry.help.c_str(), sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                } else {
                    strncat(helpBuffer, entry.help.c_str(), sizeof(smallBuffer) - 4);
                    strncat(helpBuffer, "...", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                }
                
                // Add usage line
                strncat(helpBuffer, "\nUsage: ", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                
                // Add help text again (truncate if too long)
                if (entry.help.length() < sizeof(smallBuffer) - 1) {
                    strncat(helpBuffer, entry.help.c_str(), sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                } else {
                    strncat(helpBuffer, entry.help.c_str(), sizeof(smallBuffer) - 4);
                    strncat(helpBuffer, "...", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                }
                
                // Add newline
                strncat(helpBuffer, "\n", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                
                // Add description (truncate if too long)
                if (entry.description.length() < sizeof(helpBuffer) - strlen(helpBuffer) - 1) {
                    strncat(helpBuffer, entry.description.c_str(), sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                } else {
                    strncat(helpBuffer, entry.description.c_str(), sizeof(helpBuffer) - strlen(helpBuffer) - 20);
                    strncat(helpBuffer, "...", sizeof(helpBuffer) - strlen(helpBuffer) - 1);
                }
                
                return std::string(helpBuffer);
            } else {
                char errorBuffer[256] = {0};
                strncpy(errorBuffer, "Unknown command: '", sizeof(errorBuffer) - 1);
                
                size_t position = strlen(errorBuffer);
                strncat(errorBuffer, args.c_str(), sizeof(errorBuffer) - position - 1);
                
                position = strlen(errorBuffer);
                strncat(errorBuffer, "'. Type 'help' for a list of commands.", sizeof(errorBuffer) - position - 1);
                
                std::string errorMsg(errorBuffer);
                return errorMsg;
            }
        }
    } catch (...) {
        return "Error processing help command";
    }
}

std::string GameEngine::handleCommand(const std::string& cmd, const std::string& args) {
    try {
        // Look up the command in the registry
        auto cmdIt = m_commands.find(cmd);
        
        if (cmdIt != m_commands.end()) {
            // Execute the command handler with try/catch for safety
            try {
                std::string result = cmdIt->second.handler(args);
                return result;
            } catch (...) {
                // Use a fixed buffer for the error message
                char errorBuffer[256] = {0};
                strncpy(errorBuffer, "Error executing command '", sizeof(errorBuffer) - 1);
                
                size_t position = strlen(errorBuffer);
                strncat(errorBuffer, cmd.c_str(), sizeof(errorBuffer) - position - 1);
                
                position = strlen(errorBuffer);
                strncat(errorBuffer, "'", sizeof(errorBuffer) - position - 1);
                
                return std::string(errorBuffer);
            }
        } else {
            char errorBuffer[256] = {0};
            strncpy(errorBuffer, "Unknown command: '", sizeof(errorBuffer) - 1);
            
            size_t position = strlen(errorBuffer);
            strncat(errorBuffer, cmd.c_str(), sizeof(errorBuffer) - position - 1);
            
            position = strlen(errorBuffer);
            strncat(errorBuffer, "'. Type 'help' for a list of commands.", sizeof(errorBuffer) - position - 1);
            
            return std::string(errorBuffer);
        }
    } catch (...) {
        return "Error processing command";
    }
}

bool GameEngine::shouldQuit(const std::string& cmd, const std::string& args) {
    // Check for exit/quit commands directly
    if (cmd == "exit" || cmd == "quit") {
        return true;
    }
    
    // Also check the hook system for any other quit conditions
    return m_hookSystem.beforeCommand(cmd, args);
} 