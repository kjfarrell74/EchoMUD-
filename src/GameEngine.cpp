#define _CRT_SECURE_NO_WARNINGS
#include "../include/GameEngine.h"
#include <algorithm>
#include <sstream>  // For stringstream
#include <iostream> // For debugging
#include <fstream>  // For file logging
#include <format>   // For std::format
#include <stdexcept>

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

GameEngine::GameEngine(std::string playerName)
    : m_player(std::move(playerName))
{
    // Don't call registerCommands() here as it uses shared_from_this()
    // It will be called from initialize() after the shared_ptr is fully constructed
}

// Initialize the game engine after construction
void GameEngine::initialize() {
    // Register all available commands
    registerCommands();
}

// Move constructor implementation
GameEngine::GameEngine(GameEngine&& other) noexcept
    : m_player(std::move(other.m_player)),
      m_hookSystem(std::move(other.m_hookSystem)),
      m_commands() // Initialize empty map
{
    // Command registration will be handled by initialize()
}

// Move assignment operator implementation
GameEngine& GameEngine::operator=(GameEngine&& other) noexcept {
    if (this != &other) {
        m_player = std::move(other.m_player);
        m_hookSystem = std::move(other.m_hookSystem);
        // Clear existing commands
        m_commands.clear();
        // Command registration will be handled by initialize()
    }
    return *this;
}

void GameEngine::registerCommands() {
    // Get a shared pointer to this instance for safe capturing
    auto self = shared_from_this();

    // Register the 'say' command directly
    m_commands["say"] = {
        .name = "say",
        .help = "say <message>",
        .description = "Speak aloud in the room for others to hear.",
        .handler = [](GameEnginePtr engine, std::string_view args) -> CommandResult {
            try {
                return CommandResult::success(std::format("You say: '{}'", args));
            } catch (const std::exception& e) {
                return CommandResult::error(std::format("Error processing say command: {}", e.what()));
            }
        }
    };
    
    // Register the 'look' command
    m_commands["look"] = {
        .name = "look",
        .help = "look",
        .description = "Look around and examine your surroundings.",
        .handler = [](GameEnginePtr engine, std::string_view /*args*/) -> CommandResult {
            try {
                // Get current room name
                const std::string& roomName = engine->m_player.currentRoom.empty() ? 
                    "an unknown location" : engine->m_player.currentRoom;
                
                // Build response using string formatting
                std::string response = std::format("You are in: {}\n\n", roomName);
                
                // Add room-specific description
                if (engine->m_player.currentRoom == "Start Room") {
                    response += "This is the starting area, a simple room with stone walls and a wooden floor. "
                               "There's a door leading north and a small window on the east wall.";
                }
                else if (engine->m_player.currentRoom == "North Room") {
                    response += "This is a larger chamber with a high ceiling. Dusty tapestries hang on the walls, "
                               "and there's an old desk in the corner. The exit to the south leads back to the starting room.";
                }
                else {
                    response += "This area has not been fully explored yet. There are exits in various directions.";
                }
                
                return CommandResult::success(response);
            } catch (...) {
                return CommandResult::error("Critical error processing look command.");
            }
        }
    };
    
    // Register the 'get' command
    m_commands["get"] = {
        .name = "get",
        .help = "get <item>",
        .description = "Pick up an item from the current room.",
        .handler = [](GameEnginePtr engine, std::string_view args) -> CommandResult {
            try {
                return CommandResult::success(std::format("You pick up the '{}'.", args));
            } catch (...) {
                return CommandResult::error("Error processing get command.");
            }
        }
    };
    
    // Register the 'north' command
    m_commands["north"] = {
        .name = "north",
        .help = "north",
        .description = "Move to the north if possible.",
        .handler = [](GameEnginePtr engine, std::string_view /*args*/) -> CommandResult {
            try {
                if (engine->m_hookSystem.beforeMove(engine->m_player.name, "north")) {
                    return CommandResult::success("You feel a mysterious force preventing you from moving north.");
                } else {
                    // Check if movement is valid from current room
                    if (engine->m_player.currentRoom == "Start Room") {
                        // Update player's current room
                        engine->m_player.currentRoom = "North Room";
                        
                        return CommandResult::success(std::format("You move north into {}.", engine->m_player.currentRoom));
                    } else {
                        return CommandResult::success("You can't go that way.");
                    }
                }
            } catch (...) {
                return CommandResult::error("Error processing north command.");
            }
        }
    };
    
    // Register the 'south' command
    m_commands["south"] = {
        .name = "south",
        .help = "south",
        .description = "Move to the south if possible.",
        .handler = [](GameEnginePtr engine, std::string_view /*args*/) -> CommandResult {
            try {
                if (engine->m_hookSystem.beforeMove(engine->m_player.name, "south")) {
                    return CommandResult::success("You feel a mysterious force preventing you from moving south.");
                } else {
                    // Check if movement is valid from current room
                    if (engine->m_player.currentRoom == "North Room") {
                        // Update player's current room
                        engine->m_player.currentRoom = "Start Room";
                        
                        return CommandResult::success(std::format("You move south into {}.", engine->m_player.currentRoom));
                    } else {
                        return CommandResult::success("You can't go that way.");
                    }
                }
            } catch (...) {
                return CommandResult::error("Error processing south command.");
            }
        }
    };
    
    // Register the 'east' command
    m_commands["east"] = {
        .name = "east",
        .help = "east",
        .description = "Move to the east if possible.",
        .handler = [](GameEnginePtr engine, std::string_view /*args*/) -> CommandResult {
            try {
                if (engine->m_hookSystem.beforeMove(engine->m_player.name, "east")) {
                    return CommandResult::success("You feel a mysterious force preventing you from moving east.");
                } else {
                    // No valid east paths in our simple demo
                    return CommandResult::success("You can't go that way.");
                }
            } catch (...) {
                return CommandResult::error("Error processing east command.");
            }
        }
    };
    
    // Register the 'west' command
    m_commands["west"] = {
        .name = "west",
        .help = "west",
        .description = "Move to the west if possible.",
        .handler = [](GameEnginePtr engine, std::string_view /*args*/) -> CommandResult {
            try {
                if (engine->m_hookSystem.beforeMove(engine->m_player.name, "west")) {
                    return CommandResult::success("You feel a mysterious force preventing you from moving west.");
                } else {
                    // No valid west paths in our simple demo
                    return CommandResult::success("You can't go that way.");
                }
            } catch (...) {
                return CommandResult::error("Error processing west command.");
            }
        }
    };
    
    // Register the 'exit' command
    m_commands["exit"] = {
        .name = "exit",
        .help = "exit",
        .description = "Exit the game.",
        .handler = [](GameEnginePtr /*engine*/, std::string_view /*args*/) -> CommandResult {
            return CommandResult::success("Exiting game...");
        }
    };
    
    // Register the 'quit' command (alias for exit)
    m_commands["quit"] = {
        .name = "quit",
        .help = "quit",
        .description = "Exit the game.",
        .handler = [](GameEnginePtr /*engine*/, std::string_view /*args*/) -> CommandResult {
            return CommandResult::success("Exiting game...");
        }
    };
    
    // Register the 'help' command
    m_commands["help"] = {
        .name = "help",
        .help = "help [command]",
        .description = "Display help for all commands or a specific command.",
        .handler = [](GameEnginePtr engine, std::string_view args) -> CommandResult {
            return engine->handleHelpCommand(args);
        }
    };
}

CommandResult GameEngine::handleHelpCommand(std::string_view args) {
    try {
        // Use std::string instead of fixed buffer
        std::string helpText;
        
        if (args.empty()) {
            // List all commands with their help strings
            helpText = "Available commands:\n";
            
            // Add each command
            for (const auto& [cmdName, entry] : m_commands) {
                helpText += std::format("  {} - {}\n", cmdName, entry.description);
            }
            
            return CommandResult::success(helpText);
        } else {
            // Convert string_view to string for map lookup
            std::string argStr{args};
            
            // Show detailed help for a specific command
            auto cmdIt = m_commands.find(argStr);
            if (cmdIt != m_commands.end()) {
                const auto& entry = cmdIt->second;
                
                // Build help text using string formatting
                std::string detailedHelp = std::format(
                    "{} - {}\nUsage: {}\n{}", 
                    entry.name, entry.help, entry.help, entry.description
                );
                
                return CommandResult::success(detailedHelp);
            } else {
                return CommandResult::error(std::format("Unknown command: '{}'. Type 'help' for a list of commands.", args));
            }
        }
    } catch (...) {
        return CommandResult::error("Error processing help command");
    }
}

CommandResult GameEngine::handleCommand(std::string_view cmd, std::string_view args) {
    try {
        // Convert string_view to string for map lookup
        std::string cmdStr{cmd};
        
        // Look up the command in the registry
        auto cmdIt = m_commands.find(cmdStr);
        
        if (cmdIt != m_commands.end()) {
            // Execute the command handler with try/catch for safety
            try {
                return cmdIt->second.handler(shared_from_this(), args);
            } catch (...) {
                return CommandResult::error(std::format("Error executing command '{}'", cmd));
            }
        } else {
            return CommandResult::error(std::format("Unknown command: '{}'. Type 'help' for a list of commands.", cmd));
        }
    } catch (...) {
        return CommandResult::error("Error processing command");
    }
}

bool GameEngine::shouldQuit(std::string_view cmd, std::string_view args) {
    // Check for exit/quit commands directly
    if (cmd == "exit" || cmd == "quit") {
        return true;
    }
    
    // Convert string_view to string for hook system
    std::string cmdStr{cmd};
    std::string argsStr{args};
    
    // Also check the hook system for any other quit conditions
    return m_hookSystem.beforeCommand(cmdStr, argsStr);
}