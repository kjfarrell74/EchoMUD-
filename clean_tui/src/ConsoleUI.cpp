#define _CRT_SECURE_NO_WARNINGS
#include "../include/ConsoleUI.h"
#include <clocale>
#include <stdexcept>
#include <format>
#include <iostream>
#include <csignal>
#include <mutex>
#include <chrono>
#include <thread>
#include <algorithm>
#include <vector>
#include <string>
#include <utility> // Needed for std::move
#include <limits>
#include <filesystem> // For creating debug log directory
#include <optional>
#include <sstream>  // For std::stringstream used in logMemoryStats
#include <ctime>    // For std::time_t, std::tm, std::localtime_r, std::strftime used in logDebug
#include <cstdio>   // For std::FILE, fprintf etc. if used as fallback (check if needed)

// Initialize static debug log file
std::ofstream ConsoleUI::debugLogFile;

// Initialize debug log file
void ConsoleUI::initDebugLog() {
#if DEBUG_MODE
    try {
        // Create logs directory if it doesn't exist
        std::filesystem::path logDir = "logs";
        if (!std::filesystem::exists(logDir)) {
            std::filesystem::create_directory(logDir);
        }
        
        // Create a timestamped log file
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        std::stringstream filename;
        filename << "logs/console_debug_" << timestamp << ".log";
        
        // Open the log file
        debugLogFile.open(filename.str(), std::ios::out | std::ios::trunc);
        
        if (debugLogFile.is_open()) {
            debugLogFile << "=== Debug Log Started at " << std::ctime(&timestamp) << " ===" << std::endl;
            debugLogFile.flush();
        } else {
            std::cerr << "Failed to open debug log file" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing debug log: " << e.what() << std::endl;
    }
#endif
}

// Log a debug message
void ConsoleUI::logDebug(const std::string& message) {
#if DEBUG_MODE
    if (debugLogFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // Get local time
        std::tm timeInfo;
#ifdef _WIN32
        localtime_s(&timeInfo, &timestamp);
#else
        localtime_r(&timestamp, &timeInfo);
#endif
        
        // Format timestamp
        char timeBuffer[20];
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeInfo);
        
        // Write to log file
        debugLogFile << "[" << timeBuffer << "] " << message << std::endl;
        debugLogFile.flush();
    }
#endif
}

// Helper method to create a ConsoleUI instance
std::optional<ConsoleUI> ConsoleUI::create() {
    try {
        // Initialize debug logging
        initDebugLog();
        DEBUG_LOG("ConsoleUI::create() - Starting initialization");

        // Set the locale for proper character handling (important for UTF-8)
        if (!std::setlocale(LC_ALL, "")) {
            std::cerr << "Warning: Failed to set locale.\n";
        }

        // Initialize ncurses - this is the first step and must succeed
        if (!initscr()) {
            return std::nullopt;
        }

        // Configure ncurses for non-blocking input and raw key processing
        nodelay(stdscr, TRUE);  // Non-blocking getch()
        raw();                  // Raw input mode (no line buffering)
        noecho();               // Don't echo input characters
        keypad(stdscr, TRUE);   // Enable function keys, arrow keys, etc.
        curs_set(1);            // Show cursor

        // Check color support
        if (!has_colors()) {
            endwin();
            return std::nullopt;
        }

        // Initialize color support
        if (start_color() != OK) {
            endwin();
            return std::nullopt;
        }

        // Set up color pairs if the terminal supports them
        if (can_change_color() && COLORS >= 8 && COLOR_PAIRS >= 4) {
            // Define color pairs for different UI elements
            init_pair(1, COLOR_WHITE, COLOR_BLACK);  // Normal text
            init_pair(2, COLOR_CYAN, COLOR_BLACK);   // Borders
            init_pair(3, COLOR_YELLOW, COLOR_BLACK); // Highlighted text
        } // Else use default colors

        // Get terminal dimensions for initial setup
        int height, width;
        getmaxyx(stdscr, height, width);

        // Make sure we have a minimum usable size
        if (height < 2) height = 2;
        if (width < 5) width = 5;

        // Create the UI object
        ConsoleUI ui(height, width, "Kieran");
        
        // Initialize ncurses-specific settings
        if (!ui.initializeNcurses()) {
            // Ensure cleanup if initialization fails
            if(!isendwin()) endwin();
            return std::nullopt;
        }

        // Initial screen clear
        clear();
        refresh();
        
        // Create the windows with current terminal dimensions
        auto resizeResult = ui.createWindows(height, width);
        
        // Always continue even if window setup has issues
        ui.m_resizeStatus = resizeResult;

        // Set up signal handlers for clean termination
        ui.setupSignalHandlers();

        // Return the initialized UI object
        return ui;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        if (!isendwin()) endwin();
        return std::nullopt;
    }
    catch (...) {
        std::cerr << "Unknown exception during initialization" << std::endl;
        if (!isendwin()) endwin();
        return std::nullopt;
    }
}

// Constructor initializes members but doesn't set up ncurses or windows
ConsoleUI::ConsoleUI(int termHeight, int termWidth, const std::string& playerName)
    : m_outputWin(nullptr, delwin),         // Initialize window pointers with null
      m_outputBorderWin(nullptr, delwin),   // and custom deleter for RAII
      m_inputWin(nullptr, delwin),
      m_inputBorderWin(nullptr, delwin),
      m_termHeight(termHeight),             // Store terminal dimensions
      m_termWidth(termWidth),
      m_outputHeight(20),                   // Default window sizes
      m_inputHeight(3),
      m_game(GameEngine::create(playerName)), // Initialize game engine with player name using shared_ptr
      m_lineEditor(nullptr),                // Line editor will be set up later
      m_outputBuffer(),                     // Empty output buffer
      m_scrollOffset(0),                    // Start with no scroll
      m_isRunning(false),                   // UI starts in stopped state
      m_resizeStatus(),                     // No resize status yet
      // Define signal handler callbacks as lambdas that call stop()
      m_interruptCallback([this]() { this->stop(); }),
      m_terminateCallback([this]() { this->stop(); }),
      m_outputMutex()
{
    // Line editor will be initialized when we have a valid input window
}

// Set up signal handlers for clean termination
void ConsoleUI::setupSignalHandlers() {
    // Register the callbacks with the SignalHandler
    auto result1 = SignalHandler::registerHandler(SIGINT, m_interruptCallback);   // Ctrl+C
    if (!result1) {
        DEBUG_LOG("Failed to register SIGINT handler: " +
                  std::to_string(static_cast<int>(result1.error())));
    }
    
    auto result2 = SignalHandler::registerHandler(SIGTERM, m_terminateCallback);  // Termination request
    if (!result2) {
        DEBUG_LOG("Failed to register SIGTERM handler: " +
                  std::to_string(static_cast<int>(result2.error())));
    }
}

// Clean up signal handlers
void ConsoleUI::cleanupSignalHandlers() {
    // Unregister the handlers to ensure clean shutdown
    auto result1 = SignalHandler::unregisterHandler(SIGINT);
    if (!result1) {
        DEBUG_LOG("Failed to unregister SIGINT handler: " +
                  std::to_string(static_cast<int>(result1.error())));
    }
    
    auto result2 = SignalHandler::unregisterHandler(SIGTERM);
    if (!result2) {
        DEBUG_LOG("Failed to unregister SIGTERM handler: " +
                  std::to_string(static_cast<int>(result2.error())));
    }
}

// Move constructor transfers ownership of all resources
ConsoleUI::ConsoleUI(ConsoleUI&& other) noexcept
    : m_outputWin(std::move(other.m_outputWin)),
      m_outputBorderWin(std::move(other.m_outputBorderWin)),
      m_inputWin(std::move(other.m_inputWin)),
      m_inputBorderWin(std::move(other.m_inputBorderWin)),
      m_termHeight(other.m_termHeight),
      m_termWidth(other.m_termWidth),
      m_outputHeight(other.m_outputHeight),
      m_inputHeight(other.m_inputHeight),
      m_game(std::move(other.m_game)), // Move the shared_ptr
      m_lineEditor(std::move(other.m_lineEditor)),
      m_outputBuffer(std::move(other.m_outputBuffer)),
      m_scrollOffset(other.m_scrollOffset),
      m_isRunning(other.m_isRunning.load()),
      m_resizeStatus(std::move(other.m_resizeStatus)),
      m_outputMutex(),  // Create a new mutex, can't move mutexes
      // Create new callbacks that reference this object, not the moved-from object
      m_interruptCallback([this]() { this->stop(); }),
      m_terminateCallback([this]() { this->stop(); })
{
    // Reset moved-from object's state to prevent issues
    other.m_termHeight = 0;
    other.m_termWidth = 0;
    other.m_outputHeight = 0;
    other.m_inputHeight = 0;
    other.m_scrollOffset = 0;
    other.m_isRunning.store(false);
    // Explicitly reset pointers in moved-from object
    other.m_outputWin.reset();
    other.m_outputBorderWin.reset();
    other.m_inputWin.reset();
    other.m_inputBorderWin.reset();
    other.m_lineEditor.reset();
    other.m_game.reset(); // Reset the shared_ptr in the moved-from object

    // Update line editor's window reference if we have a valid input window
    if (m_lineEditor && m_inputWin) {
        m_lineEditor->setWindow(m_inputWin.get());
    }

    // Re-register signal handlers with this instance
    setupSignalHandlers();
}

// Move assignment operator transfers ownership of all resources
ConsoleUI& ConsoleUI::operator=(ConsoleUI&& other) noexcept {
    if (this != &other) {
        // Clean up our own resources first
        cleanupSignalHandlers(); // Unregister handlers for this object

        // Move all members
        m_outputWin = std::move(other.m_outputWin);
        m_outputBorderWin = std::move(other.m_outputBorderWin);
        m_inputWin = std::move(other.m_inputWin);
        m_inputBorderWin = std::move(other.m_inputBorderWin);
        m_termHeight = other.m_termHeight;
        m_termWidth = other.m_termWidth;
        m_outputHeight = other.m_outputHeight;
        m_inputHeight = other.m_inputHeight;
        m_game = std::move(other.m_game); // Move the shared_ptr
        m_lineEditor = std::move(other.m_lineEditor);
        m_outputBuffer = std::move(other.m_outputBuffer);
        m_scrollOffset = other.m_scrollOffset;
        m_isRunning.store(other.m_isRunning.load());
        m_resizeStatus = std::move(other.m_resizeStatus);
        // Note: we don't move m_outputMutex - mutexes can't be moved or copied

        // Re-create the callbacks to point to this instance
        m_interruptCallback = [this]() { this->stop(); };
        m_terminateCallback = [this]() { this->stop(); };

        // Reset moved-from object's state to prevent issues
        other.m_termHeight = 0;
        other.m_termWidth = 0;
        other.m_outputHeight = 0;
        other.m_inputHeight = 0;
        other.m_scrollOffset = 0;
        other.m_isRunning.store(false);
        // Explicitly reset pointers in moved-from object
        other.m_outputWin.reset();
        other.m_outputBorderWin.reset();
        other.m_inputWin.reset();
        other.m_inputBorderWin.reset();
        other.m_lineEditor.reset();
        other.m_game.reset(); // Reset the shared_ptr in the moved-from object

        // Re-register signal handlers with this instance
        // (Crucially, this must happen *after* moving members and *after*
        // setting up the new callbacks for 'this' instance)
        setupSignalHandlers();

        // Update line editor's window reference if needed
        if (m_lineEditor && m_inputWin) {
            m_lineEditor->setWindow(m_inputWin.get());
        }
    }
    return *this;
}

// Destructor ensures proper cleanup
ConsoleUI::~ConsoleUI() {
    // Clean up signal handlers
    cleanupSignalHandlers();
    
    // Clean up ncurses
    cleanupNcurses();
}

// Clean up ncurses resources
void ConsoleUI::cleanupNcurses() {
    // Skip if already ended
    if (isendwin()) return;

    // Ensure window resources are released first
    m_lineEditor.reset();
    m_inputWin.reset();
    m_outputWin.reset();
    m_inputBorderWin.reset();
    m_outputBorderWin.reset();

    // Reset terminal state
    curs_set(1);         // Show cursor
    noraw();             // Disable raw mode
    echo();              // Re-enable echo
    keypad(stdscr, FALSE); // Disable keypad
    nodelay(stdscr, FALSE); // Disable non-blocking mode

    // End ncurses
    endwin();
}

// Initialize ncurses-specific settings
bool ConsoleUI::initializeNcurses() {
    // Can be empty if all setup is in create()
    return true;
}

// Calculate optimal window dimensions based on terminal size
void ConsoleUI::calculateWindowSizes(int termHeight, int termWidth) {
    // Adapt window sizes based on available terminal height
    // Make sure input window is at least 1 line tall
    m_inputHeight = std::min(3, termHeight / 5);
    m_inputHeight = std::max(1, m_inputHeight);
    
    // Output window takes the rest of the space
    m_outputHeight = termHeight - m_inputHeight;
    
    // Calculate inner window sizes (inside borders)
    // Ensure we have at least some space for inner windows
    m_outputInnerHeight = m_outputHeight > 2 ? m_outputHeight - 2 : m_outputHeight > 0 ? 1 : 0;
    m_outputInnerWidth = termWidth > 2 ? termWidth - 2 : termWidth > 0 ? 1 : 0;
    m_inputInnerHeight = m_inputHeight > 2 ? m_inputHeight - 2 : m_inputHeight > 0 ? 1 : 0;
    m_inputInnerWidth = termWidth > 2 ? termWidth - 2 : termWidth > 0 ? 1 : 0;
}

// Create the border windows
bool ConsoleUI::createBorderWindows() {
    // Create border windows
    m_outputBorderWin.reset(newwin(m_outputHeight, m_termWidth, 0, 0));
    // Check immediately after creation
    if (!m_outputBorderWin) {
        return false; // Failed to create the first border window
    }

    m_inputBorderWin.reset(newwin(m_inputHeight, m_termWidth, m_outputHeight, 0));
    // Check if the second window creation failed
    if (!m_inputBorderWin) {
        m_outputBorderWin.reset(); // Clean up the successfully created window
        return false;
    }

    // Configure the windows
    wbkgd(m_outputBorderWin.get(), COLOR_PAIR(2));
    wbkgd(m_inputBorderWin.get(), COLOR_PAIR(2));

    return true;
}

// Create the inner content windows
bool ConsoleUI::createInnerWindows() {
    // Create inner windows
    m_outputWin.reset(newwin(m_outputInnerHeight, m_outputInnerWidth, 1, 1));
    // Check immediately
    if (!m_outputWin) {
        // If inner window fails, border windows might exist, reset them too (optional, depends on desired state)
        // m_outputBorderWin.reset(); // Assuming border windows should also be cleaned up if inner fails
        // m_inputBorderWin.reset();
        return false;
    }

    m_inputWin.reset(newwin(m_inputInnerHeight, m_inputInnerWidth, m_outputHeight + 1, 1));
    // Check second inner window
    if (!m_inputWin) {
        m_outputWin.reset(); // Clean up the first inner window
        // m_outputBorderWin.reset(); // Clean up borders as well
        // m_inputBorderWin.reset();
        return false;
    }

    // Configure the windows
    scrollok(m_outputWin.get(), TRUE);     // Allow scrolling in output window
    wbkgd(m_outputWin.get(), COLOR_PAIR(1)); // Set background color
    wbkgd(m_inputWin.get(), COLOR_PAIR(1));
    keypad(m_inputWin.get(), TRUE);        // Enable special keys in input window

    return true;
}

// Set up or update the line editor
void ConsoleUI::setupLineEditor() {
    if (!m_lineEditor) {
        // First time initialization
        m_lineEditor = std::make_unique<CommandLineEditor>(m_inputInnerWidth, m_inputWin.get());
    } else {
        // Update existing line editor
        m_lineEditor->setWindow(m_inputWin.get());
        m_lineEditor->resize(m_inputInnerWidth);
    }
}

// Destroy all windows and reset pointers
void ConsoleUI::destroyWindows() {
    m_outputBorderWin.reset();
    m_outputWin.reset();
    m_inputBorderWin.reset();
    m_inputWin.reset();
}

// Set up the windows with the given dimensions
std::optional<bool> ConsoleUI::setupWindows(int height, int width) {
    // Store terminal dimensions
    m_termHeight = height;
    m_termWidth = width;
    
    // Clear the screen
    clear();
    refresh();
    
    // Create windows
    return createWindows(height, width);
}

// Create windows with the given dimensions
std::optional<bool> ConsoleUI::createWindows(int height, int width) {
    // Don't fail on small terminal, just adapt the window sizes
    if (height < m_minHeight || width < m_minWidth) {
        // Adapt to available size
        // Use at least 1 line for each window
        if (height < 2) height = 2;
        if (width < 5) width = 5;
    }

    // Store terminal dimensions
    m_termHeight = height;
    m_termWidth = width;
    
    // Calculate optimal window sizes
    calculateWindowSizes(height, width);
    
    // Clean up existing windows
    destroyWindows();

    // Create border windows
    if (!createBorderWindows()) {
        return false;
    }

    // Create inner content windows
    if (!createInnerWindows()) {
        return false;
    }