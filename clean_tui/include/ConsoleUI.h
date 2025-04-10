#pragma once

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <optional>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <fstream>  // For debug logging
#include <expected>  // For std::expected
#include "../deps/pdcurses/include/curses.h"
#include "GameEngine.h"
#include "CommandLineEditor.h"
#include "SignalHandler.h"  // For SignalHandler and SignalError

// Enable debug mode
#define DEBUG_MODE 1

// Rename DEBUG_LOG to CONSOLE_DEBUG_LOG
#undef DEBUG_LOG
#define CONSOLE_DEBUG_LOG(msg) ConsoleUI::logDebug(msg)

// Debug macros
#if DEBUG_MODE
    #define DEBUG_LOG(msg) ConsoleUI::logDebug(msg)
#else
    #define DEBUG_LOG(msg)
#endif

// Define error types for window resizing
enum class ResizeError {
    TERMINAL_TOO_SMALL
};

// Define error types for initialization
enum class InitError {
    NCURSES_INIT_FAILED,
    COLOR_SUPPORT_MISSING,
    CANNOT_CHANGE_COLOR,
    WINDOW_SETUP_FAILED,
    TERMINAL_TOO_SMALL
};

/**
 * ConsoleUI class manages the user interface of the application using ncurses.
 * It handles input/output, window management, and coordinates with the game engine.
 */
class ConsoleUI {
private:
    // Define the signal callback type
    using SignalCallback = SignalHandler::SignalCallback;
    
    // Window management with RAII unique pointers
    std::unique_ptr<WINDOW, decltype(&delwin)> m_outputWin{nullptr, delwin};
    std::unique_ptr<WINDOW, decltype(&delwin)> m_outputBorderWin{nullptr, delwin};
    std::unique_ptr<WINDOW, decltype(&delwin)> m_inputWin{nullptr, delwin};
    std::unique_ptr<WINDOW, decltype(&delwin)> m_inputBorderWin{nullptr, delwin};

    // Terminal dimensions
    int m_termHeight;
    int m_termWidth;
    int m_outputHeight = 20;
    int m_inputHeight = 3;
    const int m_minHeight = 3;
    const int m_minWidth = 10;
    
    // Inner window dimensions
    int m_outputInnerHeight = 0;
    int m_outputInnerWidth = 0;
    int m_inputInnerHeight = 0;
    int m_inputInnerWidth = 0;

    // Game engine instance (now a shared_ptr)
    GameEnginePtr m_game;
    
    // Command line editor that handles input and history
    std::unique_ptr<CommandLineEditor> m_lineEditor;

    // Output buffer for game messages
    std::vector<std::string> m_outputBuffer;
    int m_scrollOffset = 0;

    // Thread-safe flag indicating if the UI is running
    std::atomic<bool> m_isRunning{false};
    
    // Result of window setup - may contain error if the terminal is too small
    std::optional<bool> m_resizeStatus;
    
    // Mutex for thread-safe access to the output buffer
    std::mutex m_outputMutex;
    
    // Signal handler callbacks
    SignalCallback m_interruptCallback;
    SignalCallback m_terminateCallback;

    // Debug logging
    static std::ofstream debugLogFile;

    // UI methods
    void handleInput();
    void handleResize();
    void drawLayout();
    void drawOutputWindow();
    void drawInputWindow();
    void addOutputMessage(const std::string& message);
    void processCommand(const std::string& command);
    void cleanupNcurses();
    bool initializeNcurses();
    std::optional<bool> setupWindows(int height, int width);
    std::optional<bool> createWindows(int height, int width);
    
    // Window management methods
    void calculateWindowSizes(int termHeight, int termWidth);
    bool createBorderWindows();
    bool createInnerWindows();
    void setupLineEditor();
    void destroyWindows();

    // Initialize signal handlers
    void setupSignalHandlers();
    
    // Clean up signal handlers
    void cleanupSignalHandlers();

public:
    // Constructor with terminal dimensions and player name
    ConsoleUI(int termHeight, int termWidth, const std::string& playerName = "Kieran");
    
    // Destructor ensures ncurses is properly cleaned up
    ~ConsoleUI();

    // Prevent copying (ncurses windows can't be copied)
    ConsoleUI(const ConsoleUI&) = delete;
    ConsoleUI& operator=(const ConsoleUI&) = delete;

    // Allow moving to transfer ownership
    ConsoleUI(ConsoleUI&& other) noexcept;
    ConsoleUI& operator=(ConsoleUI&& other) noexcept;

    // Factory method to create and initialize the UI
    static std::optional<ConsoleUI> create();
    
    // Main UI loop
    void run();
    
    // Stop the UI loop
    void stop();
    
    // Process a game command
    void handleGameCommand(const std::string& cmd, const std::string& args);

    // Debug methods
    static void initDebugLog();
    static void logDebug(const std::string& message);
    void logMemoryStats() const;
};