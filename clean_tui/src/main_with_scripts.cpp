#include <iostream>
#include <string>
#include <format>
#include <filesystem>
#include "../include/GameEngine.h"

// Simple example demonstrating script integration
int main() {
    try {
        std::cout << "Initializing MUD engine with Lua scripting..." << std::endl;
        
        // Create the game engine
        auto gameEngine = GameEngine::create("Player");
        
        // Print available commands
        std::cout << "Available commands:" << std::endl;
        std::cout << "-------------------" << std::endl;
        
        // Test some commands including the scripted commands
        const std::vector<std::pair<std::string, std::string>> testCommands = {
            {"help", ""},
            {"look", ""},
            {"say", "Hello, world!"},
            {"say", ""},  // This will test the script's error handling
            {"test", ""},  // Test the test.lua script
            {"test", "With some arguments"},  // Test with arguments
            {"north", ""},
            {"look", ""},
            {"south", ""}
        };
        
        // Run the test commands
        for (const auto& [cmd, args] : testCommands) {
            std::cout << std::format("\n> {} {}", cmd, args) << std::endl;
            auto result = gameEngine->handleCommand(cmd, args);
            
            if (result.status == CommandResult::Status::Success) {
                std::cout << result.message << std::endl;
            } else {
                std::cerr << "Error: " << result.message << std::endl;
            }
        }
        
        std::cout << "\nTest completed successfully." << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown error occurred." << std::endl;
        return 2;
    }
}