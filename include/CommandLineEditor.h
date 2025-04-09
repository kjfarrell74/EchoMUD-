#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include "../pdcurses/include/curses.h"

// Forward declaration to avoid circular dependencies
class ConsoleUI;

// Result returned from processing a key in the command line editor
struct KeyProcessResult {
    bool needsRedraw = false;
    bool commandSubmitted = false;
    std::string submittedCommand;
};

class CommandLineEditor {
public:
    // Constructor takes dimensions and a reference to the parent UI
    CommandLineEditor(int width, WINDOW* inputWindow);
    
    // Process a keypress, return true if command was submitted
    KeyProcessResult processKey(int key);
    
    // Drawing-related methods
    void draw();
    int getCursorPosition() const { return m_cursorPos; }
    
    // Content accessors
    const std::string& getCurrentInput() const { return m_inputBuffer; }
    std::string takeCurrentInput();
    
    // History methods
    void addToHistory(const std::string& command);
    void clearHistory();
    const std::vector<std::string>& getHistory() const { return m_commandHistory; }
    
    // Window management
    void setWindow(WINDOW* window);
    void resize(int width);
    
private:
    // Input state
    std::string m_inputBuffer;
    int m_cursorPos = 0;
    
    // History management
    std::vector<std::string> m_commandHistory;
    int m_historyIndex = -1;
    
    // Window reference (not owned)
    WINDOW* m_window = nullptr;
    int m_width = 0;
    
    // Handle specific key types
    void handleBackspace();
    void handleDelete();
    void handleLeftArrow();
    void handleRightArrow();
    void handleUpArrow();
    void handleDownArrow();
    void handleHome();
    void handleEnd();
    void handleCharacter(int ch);
    
    // Safety check for cursor position
    void ensureCursorInBounds();
}; 