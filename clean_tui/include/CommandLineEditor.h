#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <expected>
#include <chrono>
#include "../deps/pdcurses/include/curses.h"

// Forward declaration to avoid circular dependencies
class ConsoleUI;

// Define error types for the command line editor
enum class CommandError {
    InvalidInput,
    WindowError,
    HistoryError
};

// Result returned from processing a key in the command line editor
struct KeyProcessResult {
    bool needsRedraw = false;
    bool commandSubmitted = false;
    std::string submittedCommand;
};

// History entry with timestamp for better management
struct HistoryEntry {
    std::string command;
    std::chrono::time_point<std::chrono::system_clock> timestamp;

    // Constructor with current time as default
    explicit HistoryEntry(std::string cmd) 
        : command(std::move(cmd)), 
          timestamp(std::chrono::system_clock::now()) 
    {}
};

class CommandLineEditor {
public:
    // Constructor takes dimensions and a reference to the parent UI
    CommandLineEditor(int width, WINDOW* inputWindow);
    
    // Process a keypress, return true if command was submitted
    KeyProcessResult processKey(int key);
    
    // Drawing-related methods
    void draw();
    [[nodiscard]] int getCursorPosition() const noexcept { return m_cursorPos; }
    
    // Content accessors
    [[nodiscard]] const std::string& getCurrentInput() const noexcept { return m_inputBuffer; }
    [[nodiscard]] std::string takeCurrentInput();
    
    // History methods
    std::expected<void, CommandError> addToHistory(std::string_view command);
    void clearHistory() noexcept;
    [[nodiscard]] const std::vector<HistoryEntry>& getHistory() const noexcept { return m_commandHistory; }
    
    // Window management
    void setWindow(WINDOW* window) noexcept;
    void resize(int width) noexcept;
    
private:
    // Input state
    std::string m_inputBuffer;
    int m_cursorPos = 0;
    
    // History management
    std::vector<HistoryEntry> m_commandHistory;
    int m_historyIndex = -1;
    static constexpr size_t MAX_HISTORY_SIZE = 100;
    
    // Window reference (not owned)
    WINDOW* m_window = nullptr;
    int m_width = 0;
    
    // Handle specific key types
    void handleBackspace() noexcept;
    void handleDelete() noexcept;
    void handleLeftArrow() noexcept;
    void handleRightArrow() noexcept;
    void handleUpArrow() noexcept;
    void handleDownArrow() noexcept;
    void handleHome() noexcept;
    void handleEnd() noexcept;
    void handleCharacter(int ch) noexcept;
    
    // Safety check for cursor position
    void ensureCursorInBounds() noexcept;
};