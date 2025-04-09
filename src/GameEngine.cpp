#include "../include/GameEngine.h"
#include <format>
#include <algorithm>

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
            return std::format("You say: '{}'", args);
        }
    };
    
    // Register the 'look' command
    m_commands["look"] = {
        .name = "look",
        .help = "look",
        .description = "Look around and examine your surroundings.",
        .handler = [this](const std::string& args) -> std::string {
            return "You are in: " + m_player.currentRoom;
        }
    };
    
    // Register the 'get' command
    m_commands["get"] = {
        .name = "get",
        .help = "get <item>",
        .description = "Pick up an item from the current room.",
        .handler = [this](const std::string& args) -> std::string {
            return std::format("You pick up the '{}'.", args);
        }
    };
    
    // Register the 'north' command
    m_commands["north"] = {
        .name = "north",
        .help = "north",
        .description = "Move to the north if possible.",
        .handler = [this](const std::string& /*args*/) -> std::string {
            if (m_hookSystem.beforeMove(m_player.name, "north")) {
                return "You feel a mysterious force preventing you from moving north.";
            } else {
                m_player.currentRoom = "North Room";
                return "You move north into " + m_player.currentRoom + ".";
            }
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
    if (args.empty()) {
        // List all commands with their help strings
        std::string helpText = "Available commands:\n";
        
        for (const auto& [cmdName, entry] : m_commands) {
            helpText += std::format("  {:<10} - {}\n", cmdName, entry.help);
        }
        
        return helpText;
    } else {
        // Show detailed help for a specific command
        auto cmdIt = m_commands.find(args);
        if (cmdIt != m_commands.end()) {
            const auto& entry = cmdIt->second;
            return std::format("{} - {}\nUsage: {}\n{}", 
                entry.name, entry.help, entry.help, entry.description);
        } else {
            return std::format("Unknown command: '{}'. Type 'help' for a list of commands.", args);
        }
    }
}

std::string GameEngine::handleCommand(const std::string& cmd, const std::string& args) {
    // Look up the command in the registry
    auto cmdIt = m_commands.find(cmd);
    
    if (cmdIt != m_commands.end()) {
        // Execute the command handler
        return cmdIt->second.handler(args);
    } else {
        return std::format("Unknown command: '{}'. Type 'help' for a list of commands.", cmd);
    }
}

bool GameEngine::shouldQuit(const std::string& cmd, const std::string& args) {
    return m_hookSystem.beforeCommand(cmd, args);
} 