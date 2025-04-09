#pragma once

#include <string>
#include <cstring> // For strcpy
#include <array>   // For fixed-size arrays

class FakeHookSystem {
public:
    // Simulate a hook: return true if command should be blocked
    bool beforeCommand(const std::string& command, const std::string& /*args*/) {
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
    // Use fixed-size char arrays instead of std::string
    static constexpr size_t MAX_NAME_LENGTH = 32;
    static constexpr size_t MAX_ROOM_LENGTH = 32;
    
    char name[MAX_NAME_LENGTH];
    char currentRoom[MAX_ROOM_LENGTH];

    // Default constructor
    Player() {
        safeCopy(name, "Unknown", MAX_NAME_LENGTH);
        safeCopy(currentRoom, "Start Room", MAX_ROOM_LENGTH);
    }
    
    // Constructor with player name
    Player(const std::string& playerName) {
        // Clear both arrays completely with zeros
        memset(name, 0, MAX_NAME_LENGTH);
        memset(currentRoom, 0, MAX_ROOM_LENGTH);
        
        // Manually copy into name with bound checking
        const char* src = playerName.c_str();
        size_t i = 0;
        while (i < MAX_NAME_LENGTH - 1 && src[i] != '\0') {
            name[i] = src[i];
            i++;
        }
        name[i] = '\0';
        
        // Hard-code "Start Room" directly
        const char* roomName = "Start Room";
        i = 0;
        while (i < MAX_ROOM_LENGTH - 1 && roomName[i] != '\0') {
            currentRoom[i] = roomName[i];
            i++;
        }
        currentRoom[i] = '\0';
    }
    
    // Safe string copy helper
    static void safeCopy(char* dest, const char* src, size_t maxLen) {
        if (src && dest) {
            // Start with a clean slate
            memset(dest, 0, maxLen);
            
            // Copy the string
            size_t i = 0;
            for (; i < maxLen - 1 && src[i] != '\0'; ++i) {
                dest[i] = src[i];
            }
            // Double-ensure null termination
            dest[i] = '\0';
        } else if (dest) {
            // Ensure the destination is null-terminated
            dest[0] = '\0';
        }
    }
}; 