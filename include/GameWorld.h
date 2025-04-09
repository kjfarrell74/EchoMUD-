#pragma once

#include <string>

class FakeHookSystem {
public:
    // Simulate a hook: return true if command should be blocked
    bool beforeCommand(const std::string& command, const std::string& args) {
        // Example rule: handle exit command
        if (command == "exit" || command == "quit") {
            return true; // exit the game
        }
        return false; // continue processing
    }

    // Simulate a hook: return true if movement should be blocked
    bool beforeMove(const std::string& playerName, const std::string& direction) {
        // Example rule: block Kieran from going north
        if (playerName == "Kieran" && direction == "north") {
            return true; // block
        }
        return false; // allow
    }
};

class Player {
public:
    std::string name;
    std::string currentRoom = "Start Room";

    Player(const std::string& playerName) : name(playerName) {}
}; 