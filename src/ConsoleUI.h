#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

#include <curses.h>
#include <string>
#include <vector>
#include <memory>
#include <expected>
#include <functional>
#include <atomic>

// Custom deleter for ncurses windows
struct WindowDeleter {
    void operator()(WINDOW* win) const {
        if (win) {
            delwin(win);
        }
    }
};

using WindowPtr = std::unique_ptr<WINDOW, WindowDeleter>;

enum class InitError {
    NCURSES_INIT_FAILED,
    COLOR_SUPPORT_MISSING,
    CANNOT_CHANGE_COLOR
};

enum class ResizeError {
    TERMINAL_TOO_SMALL
};

class ConsoleUI {
public:
    // Using std::expected for fallible initialization
    static std::expected<ConsoleUI, InitError> create();

    ConsoleUI(const ConsoleUI&) = delete;
    ConsoleUI& operator=(const ConsoleUI&) = delete;
    ConsoleUI(ConsoleUI&&) = default;
    ConsoleUI& operator=(ConsoleUI&&) = default;
    ~ConsoleUI();

    void run();
    void stop();

    // Add a message to the output window (thread-safe)
    void addOutputMessage(const std::string& message);

private:
    // Private constructor for use by the static create method
    ConsoleUI(int termHeight, int termWidth);

    bool initializeNcurses();
    bool initializeColors();
    void cleanupNcurses();

    std::expected<void, ResizeError> setupWindows(int height, int width);
    void drawLayout();
    void drawOutputWindow();
    void drawInputWindow();

    void handleInput();
    void handleResize();
    void processCommand(const std::string& command);

    // Window management with RAII
    WindowPtr m_outputWin;       // Window for output messages
    WindowPtr m_outputBorderWin; // Border around output
    WindowPtr m_inputWin;        // Window for input
    WindowPtr m_inputBorderWin;  // Border around input

    int m_termHeight = 0;
    int m_termWidth = 0;
    int m_outputHeight = 0;
    int m_inputHeight = 3; // Fixed height for input area
    int m_minHeight = 10;  // Minimum required terminal height
    int m_minWidth = 30;   // Minimum required terminal width

    std::vector<std::string> m_outputBuffer;
    int m_scrollOffset = 0; // For scrolling the output buffer

    std::string m_inputBuffer;
    int m_cursorPos = 0;
    std::vector<std::string> m_commandHistory;
    int m_historyIndex = -1;

    std::atomic<bool> m_isRunning{false};

    // Store the result of setupWindows to handle initial small terminal
    std::expected<void, ResizeError> m_resizeStatus = {};
};

#endif // CONSOLE_UI_H 