#pragma once

#include <string>
#include <string_view>

class FakeHookSystem {
public:
    // Simulate a hook: return true if command should be blocked
    bool beforeCommand(std::string_view command, std::string_view /*args*/) {
        // Example rule: handle exit command
        if (command == "exit" || command == "quit") {
            return true; // exit the game
        }
        return false; // continue processing
    }

    // Simulate a hook: return true if movement should be blocked
    bool beforeMove(std::string_view playerName, std::string_view direction) {
        // Example rule: block Kieran from going north
        if (playerName == "Kieran" && direction == "north") {
            return true; // block
        }
        return false; // allow
    }
};

class Player {
public:
    // Modern C++ with std::string instead of char arrays
    std::string name;
    std::string currentRoom;

    // Default constructor
    Player() : 
        name("Unknown"),
        currentRoom("Start Room")
    {}
    
    // Constructor with player name
    explicit Player(std::string playerName) : 
        name(std::move(playerName)),
        currentRoom("Start Room")
    {}
}; 