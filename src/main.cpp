#include "../include/ConsoleUI.h"
#include <iostream>
#include <string>
#include <utility> // Include for std::move
#include <memory> // Include for std::make_unique
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
        case InitError::WINDOW_SETUP_FAILED: return "Failed to set up console windows.";
        case InitError::TERMINAL_TOO_SMALL: return "Terminal is too small.";
        default: return "Unknown initialization error.";
    }
}

int main() {
    auto consoleUIResult = ConsoleUI::create();

    if (!consoleUIResult.has_value()) {
        std::println(stderr, "Error initializing Console UI");
        if (!isendwin()) { endwin(); }
        return 1;
    }

    try {
        // Use a unique_ptr to ensure proper destruction even in case of exceptions
        auto consoleUI = std::make_unique<ConsoleUI>(std::move(consoleUIResult.value()));
        
        // The run method now handles setting the global pointer
        consoleUI->run();
        
        // Explicitly release resources before exiting
        consoleUI.reset();
    }
    catch (const std::exception& e) {
        std::println(stderr, "Exception: {}", e.what());
        if (!isendwin()) { endwin(); }
        return 1;
    }

    // RAII handles cleanup via destructor
    return 0;
} 