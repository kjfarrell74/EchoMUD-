#include "ConsoleUI.h"
#include <iostream>
#include <string>
#include <utility> // Include for std::move
// Use <print> if available (C++23), otherwise fallback
#if __has_include(<print>)
#include <print>
#else
#include <format>
#include <cstdio> // Include for fprintf
#include <utility> // Include for std::forward
// Simple fallback if std::println isn't available
namespace std {
    template<typename... Args>
    void println(FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
        fprintf(stream, "%s\n", std::format(fmt, std::forward<Args>(args)...).c_str());
    }
     template<typename... Args>
    void println(std::format_string<Args...> fmt, Args&&... args) {
        println(stdout, fmt, std::forward<Args>(args)...);
    }
}
#endif

// Function to map InitError enum to string
std::string initErrorToString(InitError err) {
    switch (err) {
        case InitError::NCURSES_INIT_FAILED: return "Failed to initialize ncurses.";
        case InitError::COLOR_SUPPORT_MISSING: return "Terminal does not support colors.";
        case InitError::CANNOT_CHANGE_COLOR: return "Unable to initialize color support.";
        default: return "Unknown initialization error.";
    }
}

int main() {
    auto consoleUIResult = ConsoleUI::create();

    if (!consoleUIResult) {
        std::println(stderr, "Error initializing Console UI: {}", initErrorToString(consoleUIResult.error()));
        if (!isendwin()) { endwin(); } // Ensure ncurses cleanup on init failure
        return 1;
    }

    // Correctly move the ConsoleUI object from the std::expected
    ConsoleUI consoleUI = std::move(consoleUIResult.value());

    // The run method now handles setting the global pointer
    consoleUI.run();

    // RAII handles cleanup via destructor
    std::println("Application exited cleanly.");
    return 0;
} 