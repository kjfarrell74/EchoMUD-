#include "../include/CommandLineEditor.h"
#include <algorithm>
#include <limits>
#include <format>
#include <chrono>

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

std::expected<void, CommandError> CommandLineEditor::addToHistory(std::string_view command) {
    try {
        // Don't add empty commands or duplicates of the last command
        if (command.empty()) {
            return std::unexpected(CommandError::InvalidInput);
        }
        
        // Check for duplicates of the last command
        if (!m_commandHistory.empty() && m_commandHistory.back().command == command) {
            return {}; // Success but nothing added
        }
        
        // Create new history entry with current timestamp
        m_commandHistory.emplace_back(std::string{command});
        
        // Limit history size to MAX_HISTORY_SIZE entries
        if (m_commandHistory.size() > MAX_HISTORY_SIZE) {
            m_commandHistory.erase(m_commandHistory.begin());
        }
        
        return {};  // Success
    } catch (...) {
        return std::unexpected(CommandError::HistoryError);
    }
}

void CommandLineEditor::clearHistory() noexcept {
    try {
        m_commandHistory.clear();
        m_historyIndex = -1;
    } catch (...) {
        // If clear throws (shouldn't happen), silently maintain exception neutrality
    }
}

void CommandLineEditor::setWindow(WINDOW* window) noexcept {
    m_window = window;
}

void CommandLineEditor::resize(int width) noexcept {
    m_width = width;
    // Make sure cursor position is still valid
    ensureCursorInBounds();
}

// Private methods for handling specific key presses

void CommandLineEditor::handleBackspace() noexcept {
    if (m_cursorPos > 0) {
        try {
            m_inputBuffer.erase(m_cursorPos - 1, 1);
            m_cursorPos--;
        } catch (...) {
            // Maintain exception neutrality
        }
    }
}

void CommandLineEditor::handleDelete() noexcept {
    if (m_cursorPos < static_cast<int>(m_inputBuffer.length())) {
        try {
            m_inputBuffer.erase(m_cursorPos, 1);
        } catch (...) {
            // Maintain exception neutrality
        }
    }
}

void CommandLineEditor::handleLeftArrow() noexcept {
    if (m_cursorPos > 0) {
        m_cursorPos--;
    }
}

void CommandLineEditor::handleRightArrow() noexcept {
    if (m_cursorPos < static_cast<int>(m_inputBuffer.length())) {
        m_cursorPos++;
    }
}

void CommandLineEditor::handleHome() noexcept {
    m_cursorPos = 0;
}

void CommandLineEditor::handleEnd() noexcept {
    m_cursorPos = static_cast<int>(std::min(m_inputBuffer.length(), 
        static_cast<size_t>(std::numeric_limits<int>::max())));
}

void CommandLineEditor::handleUpArrow() noexcept {
    if (m_commandHistory.empty()) return;
    
    try {
        if (m_historyIndex == -1) {
            m_historyIndex = static_cast<int>(m_commandHistory.size()) - 1;
        } else if (m_historyIndex > 0) {
            m_historyIndex--;
        }
        
        if (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_commandHistory.size())) {
            m_inputBuffer = m_commandHistory[m_historyIndex].command;
            m_cursorPos = static_cast<int>(std::min(m_inputBuffer.length(), 
                static_cast<size_t>(std::numeric_limits<int>::max())));
        }
    } catch (...) {
        // Maintain exception neutrality
    }
}

void CommandLineEditor::handleDownArrow() noexcept {
    if (m_historyIndex == -1) return;
    
    try {
        if (m_historyIndex < static_cast<int>(m_commandHistory.size()) - 1) {
            m_historyIndex++;
            m_inputBuffer = m_commandHistory[m_historyIndex].command;
            m_cursorPos = static_cast<int>(std::min(m_inputBuffer.length(), 
                static_cast<size_t>(std::numeric_limits<int>::max())));
        } else {
            m_historyIndex = -1;
            m_inputBuffer.clear();
            m_cursorPos = 0;
        }
    } catch (...) {
        // Maintain exception neutrality
    }
}

void CommandLineEditor::handleCharacter(int ch) noexcept {
    try {
        m_inputBuffer.insert(m_cursorPos, 1, static_cast<char>(ch));
        m_cursorPos++;
    } catch (...) {
        // Maintain exception neutrality
    }
}

void CommandLineEditor::ensureCursorInBounds() noexcept {
    try {
        int maxPos = static_cast<int>(std::min(m_inputBuffer.length(), 
            static_cast<size_t>(std::numeric_limits<int>::max())));
        m_cursorPos = std::min(std::max(0, m_cursorPos), maxPos);
    } catch (...) {
        // Maintain exception neutrality
        m_cursorPos = 0;
    }
}