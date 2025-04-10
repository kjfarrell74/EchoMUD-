#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <memory>
#include <variant>
#include <optional>
#include <filesystem>
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

// Forward declaration for shared_ptr usage
class GameEngine;
using GameEnginePtr = std::shared_ptr<GameEngine>;

// Command result can be a success with a message or an error
struct CommandResult {
    enum class Status { Success, Error };
    Status status;
    std::string message;
    
    // Static factory methods for cleaner code
    static CommandResult success(std::string msg) {
        return CommandResult{Status::Success, std::move(msg)};
    }
    
    static CommandResult error(std::string msg) {
        return CommandResult{Status::Error, std::move(msg)};
    }
};

// Define the structure for command entries
struct CommandEntry {
    std::string name;
    std::string help;
    std::string description;
    std::function<CommandResult(GameEnginePtr, std::string_view)> handler;
};

class GameEngine : public std::enable_shared_from_this<GameEngine> {
private:
    Player m_player;
    FakeHookSystem m_hookSystem;
    std::unordered_map<std::string, CommandEntry> m_commands;
    
    // Internal methods
    void registerCommands();
    CommandResult handleHelpCommand(std::string_view args);

public:
    // Constructor with player name
    explicit GameEngine(std::string playerName);
    
    // Initialize the game engine after construction
    void initialize();
    
    // Prevent copying - these objects should be managed via shared_ptr
    GameEngine(const GameEngine&) = delete;
    GameEngine& operator=(const GameEngine&) = delete;
    
    // Allow moving
    GameEngine(GameEngine&& other) noexcept;
    GameEngine& operator=(GameEngine&& other) noexcept;
    
    // Create a shared_ptr instance
    static GameEnginePtr create(std::string playerName) {
        auto engine = std::make_shared<GameEngine>(std::move(playerName));
        engine->initialize();
        return engine;
    }
    
    // Get a shared_ptr to this
    GameEnginePtr getPtr() {
        return shared_from_this();
    }
    
    // Command handlers
    CommandResult handleCommand(std::string_view cmd, std::string_view args);
    
    // Quit check
    bool shouldQuit(std::string_view cmd, std::string_view args);
    
    // Game state access (for save/load etc.)
    const Player& getPlayer() const { return m_player; }
};