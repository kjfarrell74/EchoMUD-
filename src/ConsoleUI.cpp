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
        localtime_r(&timestamp, &timeInfo);
        
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
      m_game(playerName),                   // Initialize game engine with player name
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
    SignalHandler::registerHandler(SIGINT, m_interruptCallback);   // Ctrl+C
    SignalHandler::registerHandler(SIGTERM, m_terminateCallback);  // Termination request
}

// Clean up signal handlers
void ConsoleUI::cleanupSignalHandlers() {
    // Unregister the handlers to ensure clean shutdown
    SignalHandler::unregisterHandler(SIGINT);
    SignalHandler::unregisterHandler(SIGTERM);
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
      m_game(std::move(other.m_game)),
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
        m_game = std::move(other.m_game);
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

    // Set up the line editor
    setupLineEditor();

    // Prepare the screen
    werase(stdscr);
    wnoutrefresh(stdscr);
    drawLayout();  // Draw initial layout
    doupdate();  // Update physical screen
    
    return true;
}

// Draw all UI elements
void ConsoleUI::drawLayout() {
    // Check if we have valid windows
    if (!m_outputWin || !m_inputWin || !m_outputBorderWin || !m_inputBorderWin) {
        // Show simple fallback message
        clear();
        attron(COLOR_PAIR(1));
        mvprintw(0, 0, "Console");
        attroff(COLOR_PAIR(1));
        refresh();
        return;
    }
    
    // Draw output window border with title if there's enough space
    if (m_outputBorderWin) {
        box(m_outputBorderWin.get(), 0, 0);
        if (m_termWidth > 8) {
            mvwprintw(m_outputBorderWin.get(), 0, 2, " Out ");
        }
        wnoutrefresh(m_outputBorderWin.get());
    }
    
    // Draw input window border with title if there's enough space
    if (m_inputBorderWin) {
        box(m_inputBorderWin.get(), 0, 0);
        if (m_termWidth > 7) {
            mvwprintw(m_inputBorderWin.get(), 0, 2, " In ");
        }
        wnoutrefresh(m_inputBorderWin.get());
    }
    
    // Draw content in windows
    drawOutputWindow();
    drawInputWindow();
    
    // Stage changes for update
    if (m_outputWin) wnoutrefresh(m_outputWin.get());
    if (m_inputWin) wnoutrefresh(m_inputWin.get());
    
    // doupdate() is handled in run loop for efficiency
}

// Draw the output window content
void ConsoleUI::drawOutputWindow() {
    // Skip if window doesn't exist
    if (!m_outputWin) return;
    
    // Clear window
    werase(m_outputWin.get());
    
    // Get window dimensions
    int winHeight, winWidth;
    getmaxyx(m_outputWin.get(), winHeight, winWidth);
    
    // Skip if window is too small
    if (winHeight <= 0 || winWidth <= 0) return;
    
    // Lock output buffer while accessing it
    std::lock_guard<std::mutex> lock(m_outputMutex);
    
    // Calculate which lines to display based on scroll position
    int bufferSize = static_cast<int>(m_outputBuffer.size());
    int firstLineIdx = std::max(0, bufferSize - winHeight - m_scrollOffset);
    int lastLineIdx = std::max(0, bufferSize - m_scrollOffset);
    
    // Set text color
    wattron(m_outputWin.get(), COLOR_PAIR(1));
    
    // Draw each visible line
    for (int i = firstLineIdx; i < lastLineIdx; ++i) {
        int screenY = i - firstLineIdx;
        if (screenY >= winHeight) break;
        mvwaddnstr(m_outputWin.get(), screenY, 0, m_outputBuffer[i].c_str(), winWidth);
    }
    
    // Reset text attributes
    wattroff(m_outputWin.get(), COLOR_PAIR(1));
}

// Draw the input window content
void ConsoleUI::drawInputWindow() {
    // Skip if window or line editor doesn't exist
    if (!m_inputWin || !m_lineEditor) return;
    
    // Let the line editor draw itself
    m_lineEditor->draw();
    
    // Stage changes for update
    wnoutrefresh(m_inputWin.get());
}

// Process user input
void ConsoleUI::handleInput() {
    try {
        // Get input window or use stdscr if none exists
        WINDOW* inputSource = m_inputWin ? m_inputWin.get() : stdscr;
        
        // Get a character (non-blocking)
        int ch = wgetch(inputSource);
        
        // Skip if no input
        if (ch == ERR) return;

        // Handle window resize event
        if (ch == KEY_RESIZE) {
            handleResize();
            return;
        }
        
        // Process input with the line editor
        if (m_lineEditor) {
            try {
                // Let the line editor process the key
                auto result = m_lineEditor->processKey(ch);
                
                // Handle command submission
                if (result.commandSubmitted) {
                    // Echo command to output
                    addOutputMessage("> " + result.submittedCommand);
                    
                    // Process the command
                    try {
                        processCommand(result.submittedCommand);
                    } catch (const std::exception& e) {
                        addOutputMessage("ERROR: Exception in processCommand: " + std::string(e.what()));
                    } catch (...) {
                        addOutputMessage("ERROR: Unknown exception in processCommand");
                    }
                }
                
                // Redraw if needed
                if (result.needsRedraw) {
                    drawInputWindow();
                }
            } catch (const std::exception& e) {
                addOutputMessage("ERROR: Exception in line editor: " + std::string(e.what()));
            } catch (...) {
                addOutputMessage("ERROR: Unknown exception in line editor");
            }
        }
    } catch (const std::exception& e) {
        addOutputMessage("ERROR: Exception in handleInput: " + std::string(e.what()));
    } catch (...) {
        addOutputMessage("ERROR: Unknown exception in handleInput");
    }
}

// Handle terminal resize events
void ConsoleUI::handleResize() {
    // Get new terminal dimensions
    int h, w;
    getmaxyx(stdscr, h, w);
    
    // Update stored dimensions
    m_termHeight = h;
    m_termWidth = w;
    
    // Check if terminal was previously too small
    bool wasTooSmall = !m_resizeStatus.has_value();
    
    // Attempt to create windows with new dimensions
    m_resizeStatus = createWindows(h, w);
    
    // Notify user if terminal is now usable
    if (wasTooSmall && m_resizeStatus.has_value()) {
        addOutputMessage("Terminal resized to usable dimensions.");
    }
    
    // Force refresh to ensure clean redraw
    refresh();
}

// Process a game command
void ConsoleUI::handleGameCommand(const std::string& cmd, const std::string& args) {
    try {
        DEBUG_LOG("handleGameCommand: cmd='" + cmd + "', args='" + args + "'");
        
        // Check if command should quit the application
        if (m_game.shouldQuit(cmd, args)) {
            DEBUG_LOG("Command triggers application exit");
            addOutputMessage("Exiting game...");
            stop();  // End the run loop
            return;
        }
        
        // Get response from game engine and display it
        DEBUG_LOG("Calling game engine handler");
        try {
            std::string response = m_game.handleCommand(cmd, args);
            DEBUG_LOG("Game engine response: '" + response + "'");
            addOutputMessage(response);
        } catch (const std::bad_alloc& e) {
            // Handle memory allocation errors specifically
            std::string errorMsg = "Memory error in game engine (bad_alloc): " + std::string(e.what());
            DEBUG_LOG("ERROR: " + errorMsg);
            addOutputMessage("ERROR: Out of memory. Please try a simpler command.");
        } catch (const std::exception& e) {
            std::string errorMsg = "Exception in handleGameCommand: " + std::string(e.what());
            DEBUG_LOG("ERROR: " + errorMsg);
            addOutputMessage("ERROR: " + errorMsg);
        }
    } catch (const std::bad_alloc& e) {
        // Handle memory allocation errors specifically
        DEBUG_LOG("ERROR: Memory allocation failure in handleGameCommand: " + std::string(e.what()));
        addOutputMessage("ERROR: Out of memory. Please try a simpler command.");
    } catch (const std::exception& e) {
        std::string errorMsg = "Exception in handleGameCommand: " + std::string(e.what());
        DEBUG_LOG("ERROR: " + errorMsg);
        addOutputMessage("ERROR: " + errorMsg);
    } catch (...) {
        std::string errorMsg = "Unknown exception in handleGameCommand";
        DEBUG_LOG("ERROR: " + errorMsg);
        addOutputMessage("ERROR: " + errorMsg);
    }
}

// Parse and process a user command
void ConsoleUI::processCommand(const std::string& command) {
    try {
        DEBUG_LOG("Processing command: '" + command + "'");
        
        if (command.empty()) {
            DEBUG_LOG("Empty command, ignoring");
            return;
        }
        
        // Make a copy of the command before processing
        std::string trimmedCommand;
        try {
            trimmedCommand = command;
            
            // Trim leading spaces safely
            size_t start = trimmedCommand.find_first_not_of(" \t\n\r\f\v");
            if (start == std::string::npos) {
                // Command is only whitespace, ignore
                DEBUG_LOG("Empty command (whitespace only), ignoring");
                return;
            }
            
            if (start > 0) {
                try {
                    trimmedCommand = trimmedCommand.substr(start);
                } catch (const std::exception& e) {
                    DEBUG_LOG("Error trimming leading spaces: " + std::string(e.what()));
                    // Continue with original command
                    trimmedCommand = command;
                }
            }
            
            // Trim trailing spaces safely
            size_t end = trimmedCommand.find_last_not_of(" \t\n\r\f\v");
            if (end != std::string::npos && end + 1 < trimmedCommand.length()) {
                try {
                    trimmedCommand = trimmedCommand.substr(0, end + 1);
                } catch (const std::exception& e) {
                    DEBUG_LOG("Error trimming trailing spaces: " + std::string(e.what()));
                    // Proceed with what we have
                }
            }
            
            DEBUG_LOG("Trimmed command: '" + trimmedCommand + "'");
        } catch (const std::exception& e) {
            DEBUG_LOG("Error in command preprocessing: " + std::string(e.what()));
            // Fall back to the original command
            trimmedCommand = command;
        }
        
        // Parse command and arguments safely
        try {
            // Parse command and arguments (split at first space)
            size_t spacePos = trimmedCommand.find(' ');
            if (spacePos != std::string::npos && spacePos > 0) {
                // Command has arguments
                std::string cmd;
                std::string args;
                
                try {
                    cmd = trimmedCommand.substr(0, spacePos);
                    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower); // Convert to lowercase
                } catch (const std::exception& e) {
                    DEBUG_LOG("Error extracting command: " + std::string(e.what()));
                    return;
                }
                
                try {
                    args = trimmedCommand.substr(spacePos + 1);
                    // Trim leading spaces from args
                    size_t argsStart = args.find_first_not_of(" \t\n\r\f\v");
                    if (argsStart != std::string::npos) {
                        args = args.substr(argsStart);
                    }
                } catch (const std::exception& e) {
                    DEBUG_LOG("Error extracting arguments: " + std::string(e.what()));
                    args = "";
                }
                
                DEBUG_LOG("Executing command: '" + cmd + "' with args: '" + args + "'");
                handleGameCommand(cmd, args);
            } else {
                // Command with no arguments
                std::string cmd = trimmedCommand;
                std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
                DEBUG_LOG("Executing command: '" + cmd + "' with no args");
                handleGameCommand(cmd, "");
            }
        } catch (const std::exception& e) {
            DEBUG_LOG("Error parsing command and args: " + std::string(e.what()));
            addOutputMessage("ERROR: Could not process command: " + std::string(e.what()));
        }
    } catch (const std::bad_alloc& e) {
        // Handle memory allocation errors specifically
        DEBUG_LOG("ERROR: Memory allocation failure in processCommand: " + std::string(e.what()));
        addOutputMessage("ERROR: Out of memory. Please try a simpler command.");
    } catch (const std::exception& e) {
        std::string errorMsg = "Exception while processing command: " + std::string(e.what());
        DEBUG_LOG("ERROR: " + errorMsg);
        addOutputMessage("ERROR: " + errorMsg);
    } catch (...) {
        std::string errorMsg = "Unknown exception while processing command";
        DEBUG_LOG("ERROR: " + errorMsg);
        addOutputMessage("ERROR: " + errorMsg);
    }
}

// Add a message to the output buffer
void ConsoleUI::addOutputMessage(const std::string& message) {
    try {
        // Lock the mutex to ensure thread safety
        std::lock_guard<std::mutex> lock(m_outputMutex); // Using member mutex

        // Add message to buffer
        try {
             m_outputBuffer.push_back(message);
        } catch (const std::bad_alloc& e) {
            // Emergency cleanup on allocation failure during push_back
            DEBUG_LOG("ERROR: bad_alloc adding message. Clearing buffer.");
            m_outputBuffer.clear();
            m_outputBuffer.shrink_to_fit(); // Try to release memory
             try {
                 // Try adding an error message and the original message (truncated)
                 m_outputBuffer.push_back("WARNING: Memory limits reached. Buffer was cleared.");
                 m_outputBuffer.push_back("Last message: " + message.substr(0, 50) + (message.length() > 50 ? "..." : ""));
             } catch (...) {
                  // If even that fails, we can't do much more.
             }
             // Skip buffer trimming since we just cleared it.
             return; // Exit after handling allocation failure
        }


        // Limit buffer size to prevent memory issues
        const size_t MAX_BUFFER_SIZE = 1000;
        if (m_outputBuffer.size() > MAX_BUFFER_SIZE) {
            try {
                // Use erase for trimming
                size_t elementsToRemove = m_outputBuffer.size() - MAX_BUFFER_SIZE;
                m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + elementsToRemove);

            } catch (const std::exception& e) {
                // If error during pruning, log and potentially clear buffer as fallback
                DEBUG_LOG("Error during buffer pruning: " + std::string(e.what()) + ". Clearing buffer.");
                m_outputBuffer.clear();
                m_outputBuffer.shrink_to_fit(); // Attempt to release memory
                 try {
                     m_outputBuffer.push_back("WARNING: Buffer pruning failed. Buffer cleared.");
                 } catch (...) { /* Ignore if this fails too */ }
            }
        }
    } catch (const std::exception& e) {
        // Last resort if we can't even lock the mutex or other general exception
         // Use CONSOLE_DEBUG_LOG if available, otherwise cerr
        #ifdef CONSOLE_DEBUG_LOG
            CONSOLE_DEBUG_LOG("ERROR in addOutputMessage (outer catch): " + std::string(e.what()));
        #else
             std::cerr << "ERROR in addOutputMessage (outer catch): " << e.what() << std::endl;
        #endif
    } catch (...) {
        #ifdef CONSOLE_DEBUG_LOG
            CONSOLE_DEBUG_LOG("Unknown ERROR in addOutputMessage (outer catch)");
        #else
            std::cerr << "Unknown ERROR in addOutputMessage (outer catch)" << std::endl;
        #endif
    }
}

// Main UI loop
void ConsoleUI::run() {
    // Set running flag
    m_isRunning = true;
    
    // Show welcome message
    addOutputMessage("Console UI Ready. Type 'help' or 'exit'.");
    
    // Draw the UI immediately on startup
    drawLayout();
    
    // Position cursor at input position
    if (m_inputWin && m_lineEditor) {
        // Get window dimensions
        int winWidth;
        getmaxyx(m_inputWin.get(), std::ignore, winWidth);
        
        // Get cursor position from line editor and ensure it's valid
        int adjPos = std::min(m_lineEditor->getCursorPosition(), std::max(0, winWidth - 1));
        
        // Position cursor
        wmove(m_inputWin.get(), 0, adjPos);
        curs_set(1);  // Make cursor visible
        wnoutrefresh(m_inputWin.get());  // Stage window for update
    }
    
    // Apply all pending changes to make UI visible immediately
    doupdate();

    // Main loop
    while (m_isRunning.load(std::memory_order_relaxed)) {
        try {
            // Process input
            handleInput();
            
            // Update display
            drawLayout();
            
            // Position cursor at input position
            if (m_inputWin && m_lineEditor) {
                // Get window dimensions
                int winWidth;
                getmaxyx(m_inputWin.get(), std::ignore, winWidth);
                
                // Get cursor position from line editor and ensure it's valid
                int adjPos = std::min(m_lineEditor->getCursorPosition(), std::max(0, winWidth - 1));
                
                // Position cursor
                wmove(m_inputWin.get(), 0, adjPos);
                curs_set(1);  // Make cursor visible
                wnoutrefresh(m_inputWin.get());  // Stage window for update
            } else {
                curs_set(0);  // Hide cursor if no input window
                wnoutrefresh(stdscr);  // Update stdscr if showing error message
            }
            
            // Apply all pending changes
            doupdate();  // Apply all refresh calls at once
            
            // Prevent CPU spinning in main loop
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (const std::exception& e) {
            addOutputMessage("ERROR: Exception in main loop: " + std::string(e.what()));
            // Continue running despite error
        } catch (...) {
            addOutputMessage("ERROR: Unknown exception in main loop");
            // Continue running despite error
        }
    }
    
    // Clean up resources
    cleanupSignalHandlers();
    cleanupNcurses();
}

// Stop the UI loop
void ConsoleUI::stop() {
    m_isRunning.store(false, std::memory_order_relaxed);
}

// Debug method implementation (Optional)
void ConsoleUI::logMemoryStats() const {
    // Use const_cast if mutex needs to be locked in a const method,
    // or make the mutex mutable if stats logging is common.
    // Or avoid locking if brief inconsistency is acceptable for debug stats.
    // std::lock_guard<std::mutex> lock(m_outputMutex); // May require mutable mutex or const_cast
    try {
        std::stringstream ss;
        ss << "Memory stats - Output buffer: size=" << m_outputBuffer.size()
           << ", capacity=" << m_outputBuffer.capacity();
        #ifdef CONSOLE_DEBUG_LOG
            CONSOLE_DEBUG_LOG(ss.str());
        #else
            std::cerr << ss.str() << std::endl;
        #endif
    } catch (const std::exception& e) {
         #ifdef CONSOLE_DEBUG_LOG
             CONSOLE_DEBUG_LOG("Exception in logMemoryStats: " + std::string(e.what()));
         #else
              std::cerr << "Exception in logMemoryStats: " << e.what() << std::endl;
         #endif
    }
} 