#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include "GameWorld.h"

// Forward declaration of ConsoleUI for DEBUG_LOG
class ConsoleUI;

// Define DEBUG_LOG for GameEngine.cpp
// This makes sure it's defined regardless of inclusion order
#ifndef DEBUG_LOG
    // Forward declare the logging function
    namespace internal {
        void logDebug(const std::string& msg);
    }
    #define DEBUG_LOG(msg) internal::logDebug(msg)
#endif

// Define the structure for command entries
struct CommandEntry {
    std::string name;
    std::string help;
    std::string description;
    std::function<std::string(const std::string&)> handler;
};

class GameEngine {
private:
    Player m_player;
    FakeHookSystem m_hookSystem;
    std::unordered_map<std::string, CommandEntry> m_commands;
    
    // Internal methods
    void registerCommands();
    std::string handleHelpCommand(const std::string& args);

public:
    // Constructor with player name
    explicit GameEngine(const std::string& playerName);
    
    // Command handlers
    std::string handleCommand(const std::string& cmd, const std::string& args);
    
    // Quit check
    bool shouldQuit(const std::string& cmd, const std::string& args);
    
    // Game state access (for save/load etc.)
    const Player& getPlayer() const { return m_player; }
}; 