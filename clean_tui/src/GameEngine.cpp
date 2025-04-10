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
#ifdef ENABLE_LUA_SCRIPTING
    , m_scriptRunner(std::make_unique<ScriptRunner>())
    , m_scriptDir("scripts")
#endif
{
    // Don't call registerCommands() here as it uses shared_from_this()
    // It will be called from initialize() after the shared_ptr is fully constructed
}

// Initialize the game engine after construction
void GameEngine::initialize() {
    // Register all available commands
    registerCommands();
    
#ifdef ENABLE_LUA_SCRIPTING
    // Register script commands
    registerScripts();
#endif
}

// Move constructor implementation
GameEngine::GameEngine(GameEngine&& other) noexcept
    : m_player(std::move(other.m_player)),
      m_hookSystem(std::move(other.m_hookSystem)),
      m_commands() // Initialize empty map
#ifdef ENABLE_LUA_SCRIPTING
    , m_scriptRunner(std::move(other.m_scriptRunner))
    , m_scriptDir(std::move(other.m_scriptDir))
#endif
{
    // Command registration will be handled by initialize()
}

// Move assignment operator implementation
GameEngine& GameEngine::operator=(GameEngine&& other) noexcept {
    if (this != &other) {
        m_player = std::move(other.m_player);
        m_hookSystem = std::move(other.m_hookSystem);
#ifdef ENABLE_LUA_SCRIPTING
        m_scriptRunner = std::move(other.m_scriptRunner);
        m_scriptDir = std::move(other.m_scriptDir);
#endif
        // Clear existing commands
        m_commands.clear();
        // Command registration will be handled by initialize()
    }
    return *this;
}

#ifdef ENABLE_LUA_SCRIPTING
void GameEngine::registerScripts() {
    // Get current directory
    std::error_code ec;
    auto currentPath = std::filesystem::current_path(ec);
    if (ec) {
        DEBUG_LOG(std::format("Error getting current path: {}", ec.message()));
    }
    
    // Multiple possible script paths to try
    std::vector<std::filesystem::path> scriptPaths = {
        m_scriptDir,                            // Current specified dir
        "scripts",                              // Relative to working dir
        currentPath / "scripts",                // Absolute path in working dir
        std::filesystem::path("..") / "scripts" // Parent directory
    };

    // Scripts to load - add more scripts here
    const std::vector<std::pair<std::string, std::string>> scriptsToLoad = {
        {"say", "say.lua"},
        {"test", "test.lua"}
    };
    
    // Try loading each script
    for (const auto& [cmdName, scriptFile] : scriptsToLoad) {
        bool scriptFound = false;
        std::filesystem::path scriptPath;
        
        // Try each possible script path
        for (const auto& basePath : scriptPaths) {
            scriptPath = basePath / scriptFile;
            
            if (std::filesystem::exists(scriptPath)) {
                scriptFound = true;
                DEBUG_LOG(std::format("Found script '{}' at: {}", cmdName, scriptPath.string()));
                break;
            }
        }
        
        if (!scriptFound) {
            DEBUG_LOG(std::format("Failed to find {} in any of the search paths", scriptFile));
            continue;
        }
        
        // Load the found script
        if (!loadScriptCommand(cmdName, scriptPath)) {
            DEBUG_LOG(std::format("Failed to load script command '{}' from {}", cmdName, scriptPath.string()));
        }
    }
}

bool GameEngine::loadScriptCommand(const std::string& name, const std::filesystem::path& scriptPath) {
    // Try to load the script
    auto result = m_scriptRunner->loadScript(name, scriptPath);
    if (!result) {
        DEBUG_LOG(std::format("Failed to load script command '{}' from {}", 
                              name, scriptPath.string()));
        return false;
    }
    
    // Get help and description from the script
    auto helpResult = m_scriptRunner->getHelp(name);
    auto descResult = m_scriptRunner->getDescription(name);
    
    std::string help = helpResult ? helpResult.value() : name;
    std::string desc = descResult ? descResult.value() : "Script command";
    
    // Register the command
    m_commands[name] = {
        .name = name,
        .help = help,
        .description = desc,
        .handler = [this, name](GameEnginePtr engine, std::string_view args) -> CommandResult {
            return handleScriptCommand(name, args);
        }
    };
    
    DEBUG_LOG(std::format("Successfully registered script command '{}'", name));
    return true;
}

CommandResult GameEngine::handleScriptCommand(const std::string& cmdName, std::string_view args) {
    // Convert string_view to string for the script
    std::string argsStr(args);
    
    // Run the script command
    auto result = m_scriptRunner->runCommand(cmdName, argsStr);
    if (!result) {
        // Script execution failed
        return CommandResult::error(std::format("Script error: {}", 
            static_cast<int>(result.error())));
    }
    
    // Return the script output
    return CommandResult::success(result.value());
}
#endif

void GameEngine::registerCommands() {
    // Get a shared pointer to this instance for safe capturing
    auto self = shared_from_this();

#ifndef ENABLE_LUA_SCRIPTING
    // Only register the 'say' command directly if scripting is disabled
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
#else
    // Note: The 'say' command is loaded from a Lua script in registerScripts()
#endif
    
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