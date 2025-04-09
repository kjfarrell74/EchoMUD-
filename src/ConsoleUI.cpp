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

// Thread-safe mutex for output buffer access - still needed for thread safety
static std::mutex g_outputMutex;

// Helper method to create a ConsoleUI instance
std::expected<ConsoleUI, InitError> ConsoleUI::create() {
    try {
        // Set the locale for proper character handling (important for UTF-8)
        if (!std::setlocale(LC_ALL, "")) {
            std::cerr << "Warning: Failed to set locale.\n";
        }

        // Initialize ncurses - this is the first step and must succeed
        if (!initscr()) {
            return std::unexpected(InitError::NCURSES_INIT_FAILED);
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
            return std::unexpected(InitError::COLOR_SUPPORT_MISSING);
        }

        // Initialize color support
        if (start_color() != OK) {
            endwin();
            return std::unexpected(InitError::CANNOT_CHANGE_COLOR);
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

        // Create the UI object
        ConsoleUI ui(height, width, "Kieran");
        
        // Initialize ncurses-specific settings
        if (!ui.initializeNcurses()) { 
            // Ensure cleanup if initialization fails
            if(!isendwin()) endwin();
            return std::unexpected(InitError::NCURSES_INIT_FAILED); 
        }

        // Set up the windows with current terminal dimensions
        auto resizeResult = ui.setupWindows(height, width);
        if (!resizeResult) {
            if(!isendwin()) endwin();
            
            // Map ResizeError to appropriate InitError
            if (resizeResult.error() == ResizeError::TERMINAL_TOO_SMALL) {
                return std::unexpected(InitError::TERMINAL_TOO_SMALL);
            }
            
            return std::unexpected(InitError::WINDOW_SETUP_FAILED);
        }
        ui.m_resizeStatus = resizeResult;

        // Set up signal handlers for clean termination
        ui.setupSignalHandlers();

        // Return the initialized UI object
        return ui;
    } 
    catch (const std::exception& e) {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        if (!isendwin()) endwin();
        return std::unexpected(InitError::NCURSES_INIT_FAILED);
    }
    catch (...) {
        std::cerr << "Unknown exception during initialization" << std::endl;
        if (!isendwin()) endwin();
        return std::unexpected(InitError::NCURSES_INIT_FAILED);
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
      m_terminateCallback([this]() { this->stop(); })
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
        cleanupSignalHandlers();
        
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
        
        // Re-register signal handlers with this instance
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

// Set up the windows with the given dimensions
std::expected<void, ResizeError> ConsoleUI::setupWindows(int height, int width) {
    // Store terminal dimensions
    m_termHeight = height;
    m_termWidth = width;
    
    // Clear the screen
    clear();
    refresh();

    // Log terminal size for debugging
    std::cerr << "Setting up windows. Terminal size: " << width << "x" << height << "\n";

    // Check if terminal is too small
    if (m_termHeight < m_minHeight || m_termWidth < m_minWidth) {
        std::cerr << "Terminal too small! Min: " << m_minWidth << "x" << m_minHeight << "\n";
        
        // Clean up any existing windows
        m_outputBorderWin.reset();
        m_outputWin.reset();
        m_inputBorderWin.reset();
        m_inputWin.reset();
        m_lineEditor.reset();
        
        // Return error
        return std::unexpected(ResizeError::TERMINAL_TOO_SMALL);
    }

    // Calculate window sizes
    m_outputHeight = m_termHeight - m_inputHeight;
    
    // Reset windows before creating new ones to avoid memory leaks
    m_outputBorderWin.reset();
    m_outputWin.reset();
    m_inputBorderWin.reset();
    m_inputWin.reset();

    // Create border windows
    m_outputBorderWin.reset(newwin(m_outputHeight, m_termWidth, 0, 0));
    m_inputBorderWin.reset(newwin(m_inputHeight, m_termWidth, m_outputHeight, 0));
    
    // Check if window creation failed
    if (!m_outputBorderWin || !m_inputBorderWin) {
        return std::unexpected(ResizeError::TERMINAL_TOO_SMALL);
    }

    // Set window backgrounds
    wbkgd(m_outputBorderWin.get(), COLOR_PAIR(2));
    wbkgd(m_inputBorderWin.get(), COLOR_PAIR(2));

    // Calculate inner window sizes (inside borders)
    int outputInnerH = m_outputHeight > 2 ? m_outputHeight - 2 : 0;
    int outputInnerW = m_termWidth > 2 ? m_termWidth - 2 : 0;
    int inputInnerH = m_inputHeight > 2 ? m_inputHeight - 2 : 0;
    int inputInnerW = m_termWidth > 2 ? m_termWidth - 2 : 0;

    // Create inner windows
    m_outputWin.reset(newwin(outputInnerH, outputInnerW, 1, 1));
    m_inputWin.reset(newwin(inputInnerH, inputInnerW, m_outputHeight + 1, 1));
    
    // Check if inner window creation failed
    if (!m_outputWin || !m_inputWin) {
        return std::unexpected(ResizeError::TERMINAL_TOO_SMALL);
    }

    // Initialize or update the line editor with the new input window
    if (!m_lineEditor) {
        // First time initialization
        m_lineEditor = std::make_unique<CommandLineEditor>(inputInnerW, m_inputWin.get());
    } else {
        // Update existing line editor with new window
        m_lineEditor->setWindow(m_inputWin.get());
        m_lineEditor->resize(inputInnerW);
    }

    // Configure windows
    scrollok(m_outputWin.get(), TRUE);  // Allow scrolling in output window
    wbkgd(m_outputWin.get(), COLOR_PAIR(1));  // Set background color
    wbkgd(m_inputWin.get(), COLOR_PAIR(1));
    keypad(m_inputWin.get(), TRUE);  // Enable special keys in input window

    // Clear screen and update
    werase(stdscr);
    wnoutrefresh(stdscr);
    drawLayout();  // Draw initial layout
    doupdate();  // Update physical screen
    
    std::cerr << "Window setup completed successfully" << std::endl;
    return {};
}

// Draw all UI elements
void ConsoleUI::drawLayout() {
    // Handle case when terminal is too small
    if (!m_resizeStatus) {
        // Show error message
        clear();
        attron(COLOR_PAIR(1));
        mvprintw(0, 0, "Terminal too small!");
        mvprintw(1, 0, "Required: %d x %d, Current: %d x %d",
                 m_minWidth, m_minHeight, m_termWidth, m_termHeight);
        attroff(COLOR_PAIR(1));
        refresh();
        return;
    }
    
    // Draw output window border with title
    if (m_outputBorderWin) {
        box(m_outputBorderWin.get(), 0, 0);
        mvwprintw(m_outputBorderWin.get(), 0, 2, " Output ");
        wnoutrefresh(m_outputBorderWin.get());
    }
    
    // Draw input window border with title
    if (m_inputBorderWin) {
        box(m_inputBorderWin.get(), 0, 0);
        mvwprintw(m_inputBorderWin.get(), 0, 2, " Input ");
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
    std::lock_guard<std::mutex> lock(g_outputMutex);
    
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
        
        // Handle error cases
        if (!m_resizeStatus && ch != KEY_RESIZE && ch != ERR) {
            beep();  // Alert user if terminal is too small
            return;
        }
        
        // Skip if no input
        if (ch == ERR) return;

        // Handle window resize event
        if (ch == KEY_RESIZE) {
            handleResize();
            return;
        }
        
        // Process input with the line editor
        if (m_lineEditor && m_resizeStatus) {
            try {
                // Let the line editor process the key
                auto result = m_lineEditor->processKey(ch);
                
                // Handle command submission
                if (result.commandSubmitted) {
                    // Echo command to output
                    addOutputMessage(std::format("> {}", result.submittedCommand));
                    
                    // Process the command
                    try {
                        processCommand(result.submittedCommand);
                    } catch (const std::exception& e) {
                        addOutputMessage(std::format("ERROR: Exception in processCommand: {}", e.what()));
                    } catch (...) {
                        addOutputMessage("ERROR: Unknown exception in processCommand");
                    }
                }
                
                // Redraw if needed
                if (result.needsRedraw) {
                    drawInputWindow();
                }
            } catch (const std::exception& e) {
                addOutputMessage(std::format("ERROR: Exception in line editor: {}", e.what()));
            } catch (...) {
                addOutputMessage("ERROR: Unknown exception in line editor");
            }
        }
    } catch (const std::exception& e) {
        addOutputMessage(std::format("ERROR: Exception in handleInput: {}", e.what()));
    } catch (...) {
        addOutputMessage("ERROR: Unknown exception in handleInput");
    }
}

// Handle terminal resize events
void ConsoleUI::handleResize() {
    // Get new terminal dimensions
    int h, w;
    getmaxyx(stdscr, h, w);
    
    // Check if terminal was previously too small
    bool wasTooSmall = !m_resizeStatus.has_value();
    
    // Attempt to set up windows with new dimensions
    m_resizeStatus = setupWindows(h, w);
    
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
        // Add debug output
        addOutputMessage(std::format("DEBUG: Handling command: '{}' with args: '{}'", cmd, args));
        
        // Check if command should quit the application
        if (m_game.shouldQuit(cmd, args)) {
            addOutputMessage("Exiting game...");
            stop();  // End the run loop
            return;
        }
        
        // Get response from game engine and display it
        std::string response = m_game.handleCommand(cmd, args);
        addOutputMessage(response);
    } catch (const std::exception& e) {
        addOutputMessage(std::format("ERROR: Exception in handleGameCommand: {}", e.what()));
    } catch (...) {
        addOutputMessage("ERROR: Unknown exception in handleGameCommand");
    }
}

// Parse and process a user command
void ConsoleUI::processCommand(const std::string& command) {
    try {
        // Trim leading and trailing spaces
        std::string trimmedCommand = command;
        
        // Debug each character in the command as hex
        std::string hexDump = "Command hex dump: ";
        for (unsigned char c : trimmedCommand) {
            hexDump += std::format("{:02X} ", static_cast<int>(c));
        }
        addOutputMessage(hexDump);
        
        // Trim leading spaces
        size_t start = trimmedCommand.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos) {
            // Command is only whitespace, ignore
            addOutputMessage("Empty command.");
            return;
        }
        
        trimmedCommand = trimmedCommand.substr(start);
        
        // Trim trailing spaces
        size_t end = trimmedCommand.find_last_not_of(" \t\n\r\f\v");
        if (end != std::string::npos) {
            trimmedCommand = trimmedCommand.substr(0, end + 1);
        }
        
        // Add debug output
        addOutputMessage(std::format("DEBUG: Processing command: '{}'", trimmedCommand));
        
        // Parse command and arguments (split at first space)
        size_t spacePos = trimmedCommand.find(' ');
        if (spacePos != std::string::npos) {
            // Command has arguments
            std::string cmd = trimmedCommand.substr(0, spacePos);
            std::string args = trimmedCommand.substr(spacePos + 1);
            
            // Trim leading spaces from args
            args.erase(0, args.find_first_not_of(" \t\n\r\f\v"));
            
            addOutputMessage(std::format("DEBUG: Split - cmd: '{}', args: '{}'", cmd, args));
            handleGameCommand(cmd, args);
        } else {
            // Command with no arguments
            addOutputMessage(std::format("DEBUG: No args - cmd: '{}'", trimmedCommand));
            handleGameCommand(trimmedCommand, "");
        }
    } catch (const std::exception& e) {
        addOutputMessage(std::format("ERROR: Exception while processing command: {}", e.what()));
    } catch (...) {
        addOutputMessage("ERROR: Unknown exception while processing command");
    }
}

// Add a message to the output buffer
void ConsoleUI::addOutputMessage(const std::string& message) {
    // Lock the mutex to ensure thread safety
    std::lock_guard<std::mutex> lock(g_outputMutex);
    
    // Add message to buffer
    m_outputBuffer.push_back(message);
    
    // Limit buffer size to prevent memory issues
    const size_t MAX_BUFFER_SIZE = 1000;
    if (m_outputBuffer.size() > MAX_BUFFER_SIZE) {
        m_outputBuffer.erase(m_outputBuffer.begin(), 
                           m_outputBuffer.begin() + (m_outputBuffer.size() - MAX_BUFFER_SIZE));
    }
}

// Main UI loop
void ConsoleUI::run() {
    // Set running flag
    m_isRunning = true;
    
    // Show welcome message if terminal is large enough
    if (m_resizeStatus) {
        addOutputMessage("Console UI Ready. Type 'help' or 'exit'.");
    }

    // Main loop
    while (m_isRunning.load(std::memory_order_relaxed)) {
        try {
            // Process input
            handleInput();
            
            // Update display
            drawLayout();
            
            // Position cursor at input position
            if (m_inputWin && m_lineEditor && m_resizeStatus) {
                // Get window dimensions
                int wh, ww;
                getmaxyx(m_inputWin.get(), wh, ww);
                
                // Get cursor position from line editor and ensure it's valid
                int adjPos = std::min(m_lineEditor->getCursorPosition(), std::max(0, ww - 1));
                
                // Position cursor
                wmove(m_inputWin.get(), 0, adjPos);
                curs_set(1);  // Make cursor visible
                wnoutrefresh(m_inputWin.get());  // Stage window for update
            } else if (!m_resizeStatus) { 
                curs_set(0);  // Hide cursor if terminal is too small
                wnoutrefresh(stdscr);  // Update stdscr if showing error message
            }
            
            // Update physical screen
            doupdate();
        } catch (const std::exception& e) {
            // Log the error but don't crash
            addOutputMessage(std::format("ERROR: Exception in main loop: {}", e.what()));
            
            // Small delay to prevent error message flood
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } catch (...) {
            addOutputMessage("ERROR: Unknown exception in main loop");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Limit refresh rate to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

// Stop the UI loop
void ConsoleUI::stop() {
    m_isRunning.store(false, std::memory_order_relaxed);
} 