#include "../include/CommandLineEditor.h"
#include <algorithm>
#include <limits>

CommandLineEditor::CommandLineEditor(int width, WINDOW* inputWindow)
    : m_window(inputWindow), m_width(width)
{
    // Initialize empty state
}

KeyProcessResult CommandLineEditor::processKey(int key) {
    KeyProcessResult result;
    result.needsRedraw = true;
    
    switch (key) {
        case KEY_BACKSPACE: case 127: case 8:
            handleBackspace();
            break;
        case KEY_DC: // Delete key
            handleDelete();
            break;
        case KEY_LEFT:
            handleLeftArrow();
            break;
        case KEY_RIGHT: 
            handleRightArrow();
            break;
        case KEY_HOME:
            handleHome();
            break;
        case KEY_END:
            handleEnd();
            break;
        case KEY_UP:
            handleUpArrow();
            break;
        case KEY_DOWN:
            handleDownArrow();
            break;
        case KEY_ENTER: case 10: case 13: // Enter key (different representations)
            if (!m_inputBuffer.empty()) {
                result.commandSubmitted = true;
                result.submittedCommand = m_inputBuffer;
                addToHistory(m_inputBuffer);
                m_inputBuffer.clear();
                m_cursorPos = 0;
                m_historyIndex = -1;
            }
            break;
        default:
            // Handle printable characters
            if (key >= 32 && key <= 126) {
                handleCharacter(key);
            }
            break;
    }
    
    return result;
}

void CommandLineEditor::draw() {
    if (!m_window) return;
    
    werase(m_window);
    mvwaddnstr(m_window, 0, 0, m_inputBuffer.c_str(), m_width - 1);
}

std::string CommandLineEditor::takeCurrentInput() {
    std::string temp = m_inputBuffer;
    m_inputBuffer.clear();
    m_cursorPos = 0;
    return temp;
}

void CommandLineEditor::addToHistory(const std::string& command) {
    // Don't add empty commands or duplicates of the last command
    if (command.empty() || (!m_commandHistory.empty() && m_commandHistory.back() == command)) {
        return;
    }
    
    m_commandHistory.push_back(command);
    
    // Limit history size to 100 entries
    const size_t MAX_HISTORY = 100;
    if (m_commandHistory.size() > MAX_HISTORY) {
        m_commandHistory.erase(m_commandHistory.begin());
    }
}

void CommandLineEditor::clearHistory() {
    m_commandHistory.clear();
    m_historyIndex = -1;
}

void CommandLineEditor::setWindow(WINDOW* window) {
    m_window = window;
}

void CommandLineEditor::resize(int width) {
    m_width = width;
    // Make sure cursor position is still valid
    ensureCursorInBounds();
}

// Private methods for handling specific key presses

void CommandLineEditor::handleBackspace() {
    if (m_cursorPos > 0) {
        m_inputBuffer.erase(m_cursorPos - 1, 1);
        m_cursorPos--;
    }
}

void CommandLineEditor::handleDelete() {
    if (m_cursorPos < static_cast<int>(m_inputBuffer.length())) {
        m_inputBuffer.erase(m_cursorPos, 1);
    }
}

void CommandLineEditor::handleLeftArrow() {
    if (m_cursorPos > 0) {
        m_cursorPos--;
    }
}

void CommandLineEditor::handleRightArrow() {
    if (m_cursorPos < static_cast<int>(m_inputBuffer.length())) {
        m_cursorPos++;
    }
}

void CommandLineEditor::handleHome() {
    m_cursorPos = 0;
}

void CommandLineEditor::handleEnd() {
    m_cursorPos = static_cast<int>(std::min(m_inputBuffer.length(), 
        static_cast<size_t>(std::numeric_limits<int>::max())));
}

void CommandLineEditor::handleUpArrow() {
    if (m_commandHistory.empty()) return;
    
    if (m_historyIndex == -1) {
        m_historyIndex = static_cast<int>(m_commandHistory.size()) - 1;
    } else if (m_historyIndex > 0) {
        m_historyIndex--;
    }
    
    if (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_commandHistory.size())) {
        m_inputBuffer = m_commandHistory[m_historyIndex];
        m_cursorPos = static_cast<int>(std::min(m_inputBuffer.length(), 
            static_cast<size_t>(std::numeric_limits<int>::max())));
    }
}

void CommandLineEditor::handleDownArrow() {
    if (m_historyIndex == -1) return;
    
    if (m_historyIndex < static_cast<int>(m_commandHistory.size()) - 1) {
        m_historyIndex++;
        m_inputBuffer = m_commandHistory[m_historyIndex];
        m_cursorPos = static_cast<int>(std::min(m_inputBuffer.length(), 
            static_cast<size_t>(std::numeric_limits<int>::max())));
    } else {
        m_historyIndex = -1;
        m_inputBuffer.clear();
        m_cursorPos = 0;
    }
}

void CommandLineEditor::handleCharacter(int ch) {
    m_inputBuffer.insert(m_cursorPos, 1, static_cast<char>(ch));
    m_cursorPos++;
}

void CommandLineEditor::ensureCursorInBounds() {
    int maxPos = static_cast<int>(std::min(m_inputBuffer.length(), 
        static_cast<size_t>(std::numeric_limits<int>::max())));
    m_cursorPos = std::min(std::max(0, m_cursorPos), maxPos);
} 