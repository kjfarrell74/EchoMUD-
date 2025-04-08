#include "ConsoleUI.h"
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

// Global pointer for signal handling
static std::atomic<ConsoleUI*> g_consoleUIInstance = nullptr;
static std::mutex g_outputMutex; // Mutex for thread-safe output buffer access

// Signal handler function
void handleSignal(int signum) {
    ConsoleUI* instance = g_consoleUIInstance.load();
    if (instance) {
        if (signum == SIGINT || signum == SIGTERM) {
            instance->stop();
        }
    }
}

std::expected<ConsoleUI, InitError> ConsoleUI::create() {
    if (!std::setlocale(LC_ALL, "")) {
        // Log locale setting failure? Continue?
    }

    if (!initscr()) {
        return std::unexpected(InitError::NCURSES_INIT_FAILED);
    }

    nodelay(stdscr, TRUE);
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    if (!has_colors()) {
        endwin();
        return std::unexpected(InitError::COLOR_SUPPORT_MISSING);
    }

    if (start_color() != OK) {
        endwin();
        return std::unexpected(InitError::CANNOT_CHANGE_COLOR);
    }

    if (can_change_color() && COLORS >= 8 && COLOR_PAIRS >= 4) {
        init_pair(1, COLOR_WHITE, COLOR_BLACK);
        init_pair(2, COLOR_CYAN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    } // Else use default colors

    int height, width;
    getmaxyx(stdscr, height, width);

    ConsoleUI ui(height, width); // Create instance on stack
    // Note: initializeNcurses might be redundant if all setup is here
    if (!ui.initializeNcurses()) { 
         // Ensure cleanup if init fails after ncurses screen active
         if(!isendwin()) endwin();
         return std::unexpected(InitError::NCURSES_INIT_FAILED); 
    }

    ui.m_resizeStatus = ui.setupWindows(height, width);

    // Do NOT set g_consoleUIInstance here; ui is local and about to be moved.
    signal(SIGINT, handleSignal); signal(SIGTERM, handleSignal);

    // MUST return std::move(ui) to correctly construct the expected<ConsoleUI, ...>
    return std::move(ui);
}

ConsoleUI::ConsoleUI(int termHeight, int termWidth)
    : m_termHeight(termHeight), m_termWidth(termWidth) {}

// Correct Move Constructor Implementation
ConsoleUI::ConsoleUI(ConsoleUI&& other) noexcept
    : m_outputWin(std::move(other.m_outputWin)),
      m_outputBorderWin(std::move(other.m_outputBorderWin)),
      m_inputWin(std::move(other.m_inputWin)),
      m_inputBorderWin(std::move(other.m_inputBorderWin)),
      m_termHeight(other.m_termHeight),
      m_termWidth(other.m_termWidth),
      m_outputHeight(other.m_outputHeight),
      m_inputHeight(other.m_inputHeight),
      m_minHeight(other.m_minHeight),
      m_minWidth(other.m_minWidth),
      m_outputBuffer(std::move(other.m_outputBuffer)),
      m_scrollOffset(other.m_scrollOffset),
      m_inputBuffer(std::move(other.m_inputBuffer)),
      m_cursorPos(other.m_cursorPos),
      m_commandHistory(std::move(other.m_commandHistory)),
      m_historyIndex(other.m_historyIndex),
      m_isRunning(other.m_isRunning.load()),
      m_resizeStatus(std::move(other.m_resizeStatus))
{
    // If the moved-from object was the global instance, update the global instance to point to this new object.
    ConsoleUI* expected = &other;
    if (g_consoleUIInstance.compare_exchange_strong(expected, this)) {
      // Global pointer updated successfully.
    }
    // Reset moved-from object's primitive members
    other.m_termHeight = 0; other.m_termWidth = 0; other.m_outputHeight = 0;
    other.m_scrollOffset = 0; other.m_cursorPos = 0; other.m_historyIndex = -1;
}

// Correct Move Assignment Operator Implementation
ConsoleUI& ConsoleUI::operator=(ConsoleUI&& other) noexcept {
    if (this != &other) {
        // Move all members
        m_outputWin = std::move(other.m_outputWin);
        m_outputBorderWin = std::move(other.m_outputBorderWin);
        m_inputWin = std::move(other.m_inputWin);
        m_inputBorderWin = std::move(other.m_inputBorderWin);
        m_termHeight = other.m_termHeight;
        m_termWidth = other.m_termWidth;
        m_outputHeight = other.m_outputHeight;
        m_inputHeight = other.m_inputHeight;
        m_minHeight = other.m_minHeight;
        m_minWidth = other.m_minWidth;
        m_outputBuffer = std::move(other.m_outputBuffer);
        m_scrollOffset = other.m_scrollOffset;
        m_inputBuffer = std::move(other.m_inputBuffer);
        m_cursorPos = other.m_cursorPos;
        m_commandHistory = std::move(other.m_commandHistory);
        m_historyIndex = other.m_historyIndex;
        m_isRunning.store(other.m_isRunning.load());
        m_resizeStatus = std::move(other.m_resizeStatus);

        // Handle global pointer update
        ConsoleUI* expected_other = &other;
        if (g_consoleUIInstance.compare_exchange_strong(expected_other, this)) {
            // Other was global, now this is global.
        } else {
            ConsoleUI* expected_this = this;
            g_consoleUIInstance.compare_exchange_strong(expected_this, nullptr);
        }
        // Reset moved-from object's primitive members
        other.m_termHeight = 0; /* etc. */
    }
    return *this;
}

ConsoleUI::~ConsoleUI() {
    ConsoleUI* expected = this;
    // Only call cleanup if this instance is the one registered globally
    if (g_consoleUIInstance.compare_exchange_strong(expected, nullptr)) {
       cleanupNcurses();
    }
}

void ConsoleUI::cleanupNcurses() {
    if (isendwin()) return;
    curs_set(1);
    noraw(); // Match raw() in create()
    echo();
    keypad(stdscr, FALSE);
    nodelay(stdscr, FALSE);
    endwin();
}

bool ConsoleUI::initializeNcurses() {
    // Can be empty if all setup is in create()
    return true;
}

std::expected<void, ResizeError> ConsoleUI::setupWindows(int height, int width) {
    m_termHeight = height;
    m_termWidth = width;
    clear();
    refresh();

    if (m_termHeight < m_minHeight || m_termWidth < m_minWidth) {
        m_outputBorderWin.reset(); m_outputWin.reset();
        m_inputBorderWin.reset(); m_inputWin.reset();
        return std::unexpected(ResizeError::TERMINAL_TOO_SMALL);
    }

    m_outputHeight = m_termHeight - m_inputHeight;
    // Reset windows before creating new ones
    m_outputBorderWin.reset(); m_outputWin.reset();
    m_inputBorderWin.reset(); m_inputWin.reset();

    m_outputBorderWin.reset(newwin(m_outputHeight, m_termWidth, 0, 0));
    m_inputBorderWin.reset(newwin(m_inputHeight, m_termWidth, m_outputHeight, 0));
    if (!m_outputBorderWin || !m_inputBorderWin) return std::unexpected(ResizeError::TERMINAL_TOO_SMALL);

    wbkgd(m_outputBorderWin.get(), COLOR_PAIR(2));
    wbkgd(m_inputBorderWin.get(), COLOR_PAIR(2));

    int outputInnerH = m_outputHeight > 2 ? m_outputHeight - 2 : 0;
    int outputInnerW = m_termWidth > 2 ? m_termWidth - 2 : 0;
    int inputInnerH = m_inputHeight > 2 ? m_inputHeight - 2 : 0;
    int inputInnerW = m_termWidth > 2 ? m_termWidth - 2 : 0;

    m_outputWin.reset(newwin(outputInnerH, outputInnerW, 1, 1));
    m_inputWin.reset(newwin(inputInnerH, inputInnerW, m_outputHeight + 1, 1));
    if (!m_outputWin || !m_inputWin) return std::unexpected(ResizeError::TERMINAL_TOO_SMALL);

    scrollok(m_outputWin.get(), TRUE);
    wbkgd(m_outputWin.get(), COLOR_PAIR(1));
    wbkgd(m_inputWin.get(), COLOR_PAIR(1));
    keypad(m_inputWin.get(), TRUE); // Essential for input window

    werase(stdscr);
    wnoutrefresh(stdscr);
    drawLayout(); // Redraw immediately after resize
    doupdate();
    return {};
}

void ConsoleUI::drawLayout() {
    if (!m_resizeStatus) {
        clear();
        attron(COLOR_PAIR(1));
        mvprintw(0, 0, "Terminal too small!");
        mvprintw(1, 0, "Required: %d x %d, Current: %d x %d",
                 m_minWidth, m_minHeight, m_termWidth, m_termHeight);
        attroff(COLOR_PAIR(1));
        refresh();
        return;
    }
    if (m_outputBorderWin) { box(m_outputBorderWin.get(), 0, 0); mvwprintw(m_outputBorderWin.get(), 0, 2, " Output "); wnoutrefresh(m_outputBorderWin.get()); }
    if (m_inputBorderWin) { box(m_inputBorderWin.get(), 0, 0); mvwprintw(m_inputBorderWin.get(), 0, 2, " Input "); wnoutrefresh(m_inputBorderWin.get()); }
    drawOutputWindow();
    drawInputWindow();
    if (m_outputWin) wnoutrefresh(m_outputWin.get());
    if (m_inputWin) wnoutrefresh(m_inputWin.get());
    // doupdate() is handled in run loop
}

void ConsoleUI::drawOutputWindow() {
    if (!m_outputWin) return;
    werase(m_outputWin.get());
    int winHeight, winWidth; getmaxyx(m_outputWin.get(), winHeight, winWidth);
    if (winHeight <= 0 || winWidth <= 0) return;
    std::lock_guard<std::mutex> lock(g_outputMutex);
    int bufferSize = static_cast<int>(m_outputBuffer.size());
    int firstLineIdx = std::max(0, bufferSize - winHeight - m_scrollOffset);
    int lastLineIdx = std::max(0, bufferSize - m_scrollOffset);
    wattron(m_outputWin.get(), COLOR_PAIR(1));
    for (int i = firstLineIdx; i < lastLineIdx; ++i) {
        int screenY = i - firstLineIdx;
        if (screenY >= winHeight) break;
        mvwaddnstr(m_outputWin.get(), screenY, 0, m_outputBuffer[i].c_str(), winWidth);
    }
    wattroff(m_outputWin.get(), COLOR_PAIR(1));
}

void ConsoleUI::drawInputWindow() {
    if (!m_inputWin) return;
    werase(m_inputWin.get());
    int winHeight, winWidth; getmaxyx(m_inputWin.get(), winHeight, winWidth);
    if (winHeight <= 0 || winWidth <= 0) return;
    wattron(m_inputWin.get(), COLOR_PAIR(3));
    mvwaddnstr(m_inputWin.get(), 0, 0, m_inputBuffer.c_str(), winWidth);
    wattroff(m_inputWin.get(), COLOR_PAIR(3));
    // Cursor position handled in run loop
}

void ConsoleUI::handleInput() {
    WINDOW* inputSource = m_inputWin ? m_inputWin.get() : stdscr;
    int ch = wgetch(inputSource);
    if (!m_resizeStatus && ch != KEY_RESIZE && ch != ERR) { beep(); return; }
    if (ch == ERR) return;

    switch (ch) {
        case KEY_RESIZE: handleResize(); break;
        case KEY_BACKSPACE: case 127: case 8:
            if (m_cursorPos > 0) { m_inputBuffer.erase(m_cursorPos - 1, 1); m_cursorPos--; }
            break;
        case KEY_DC:
            if (m_cursorPos < static_cast<int>(m_inputBuffer.length())) { m_inputBuffer.erase(m_cursorPos, 1); }
            break;
        case KEY_LEFT: if (m_cursorPos > 0) m_cursorPos--; break;
        case KEY_RIGHT: 
            if (m_cursorPos < static_cast<int>(m_inputBuffer.length())) m_cursorPos++; 
            break;
        case KEY_HOME: m_cursorPos = 0; break;
        case KEY_END: m_cursorPos = m_inputBuffer.length(); break; // length() is valid end pos for cursor
        case KEY_UP:
            if (!m_commandHistory.empty()) {
                if (m_historyIndex == -1) { 
                    m_historyIndex = static_cast<int>(m_commandHistory.size()) - 1; 
                }
                else if (m_historyIndex > 0) { m_historyIndex--; }
                
                if(m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_commandHistory.size())) {
                   m_inputBuffer = m_commandHistory[m_historyIndex];
                   m_cursorPos = m_inputBuffer.length();
                }
            }
            break;
        case KEY_DOWN:
            if (m_historyIndex != -1) {
                if (m_historyIndex < static_cast<int>(m_commandHistory.size()) - 1) {
                    m_historyIndex++; 
                    m_inputBuffer = m_commandHistory[m_historyIndex]; 
                    m_cursorPos = m_inputBuffer.length();
                } else { m_historyIndex = -1; m_inputBuffer.clear(); m_cursorPos = 0; }
            }
            break;
        case KEY_ENTER: case 10: case 13: // Use KEY_ENTER and/or ASCII values
            if (m_resizeStatus && !m_inputBuffer.empty()) {
                addOutputMessage(std::format("> {}", m_inputBuffer));
                processCommand(m_inputBuffer);
                if (m_inputBuffer != "exit" && (m_commandHistory.empty() || m_commandHistory.back() != m_inputBuffer)) {
                    m_commandHistory.push_back(m_inputBuffer);
                     // Optional: Limit history size
                     const size_t MAX_HISTORY_SIZE = 100;
                     if (m_commandHistory.size() > MAX_HISTORY_SIZE) {
                        m_commandHistory.erase(m_commandHistory.begin());
                     }
                }
                m_inputBuffer.clear(); m_cursorPos = 0; m_historyIndex = -1; m_scrollOffset = 0;
            } else if (!m_resizeStatus) { beep(); }
            break;
        case KEY_PPAGE:
            if (m_outputWin && m_resizeStatus) {
                int wh, ww; getmaxyx(m_outputWin.get(), wh, ww);
                std::lock_guard<std::mutex> lock(g_outputMutex);
                int bs = static_cast<int>(m_outputBuffer.size());
                int ms = std::max(0, bs - wh);
                m_scrollOffset = std::min(ms, m_scrollOffset + wh);
            }
            break;
        case KEY_NPAGE:
             if (m_outputWin && m_resizeStatus) {
                int wh, ww; getmaxyx(m_outputWin.get(), wh, ww);
                 m_scrollOffset -= wh; if (m_scrollOffset < 0) m_scrollOffset = 0;
             }
            break;
        default:
            // Basic printable character handling (Needs UTF-8 for full support)
            if (m_resizeStatus && ch >= 32 && ch <= 126) { 
                 m_inputBuffer.insert(m_cursorPos, 1, static_cast<char>(ch)); m_cursorPos++;
            } else if (m_resizeStatus && ch != ERR) { /* Optionally beep for unhandled keys */ }
            break;
    }
}

void ConsoleUI::handleResize() {
    int h, w; getmaxyx(stdscr, h, w);
    bool wasTooSmall = !m_resizeStatus.has_value();
    // Attempt to set up windows with new dimensions
    m_resizeStatus = setupWindows(h, w);
    if (wasTooSmall && m_resizeStatus.has_value()) {
        addOutputMessage("Terminal resized to usable dimensions.");
    }
    // setupWindows calls drawLayout and doupdate, but an extra refresh can help
    refresh(); 
}

void ConsoleUI::processCommand(const std::string& command) {
    if (command == "exit") { stop(); }
    else if (command == "clear") { std::lock_guard<std::mutex> lock(g_outputMutex); m_outputBuffer.clear(); m_scrollOffset = 0; }
    else if (command == "help") { addOutputMessage("Commands: exit, clear, help. Scroll: PgUp/PgDn"); }
    else { addOutputMessage(std::format("Unknown: '{}'", command)); }
}

void ConsoleUI::addOutputMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_outputMutex);
    // TODO: Proper line wrapping for long messages
    m_outputBuffer.push_back(message);
    const size_t MAX_BUFFER_SIZE = 1000;
    if (m_outputBuffer.size() > MAX_BUFFER_SIZE) {
        m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + (m_outputBuffer.size() - MAX_BUFFER_SIZE));
    }
    // Scrolling is handled by drawOutputWindow based on m_scrollOffset
}

void ConsoleUI::run() {
    m_isRunning = true;
    if (m_resizeStatus) { addOutputMessage("Console UI Ready. Type 'help' or 'exit'."); }
    
    // Set the global pointer *here* safely, pointing to the valid object `run` is called on.
    g_consoleUIInstance = this;

    while (m_isRunning.load(std::memory_order_relaxed)) {
        handleInput();
        drawLayout();
        if (m_inputWin && m_resizeStatus) {
            int wh, ww; getmaxyx(m_inputWin.get(), wh, ww);
            // Ensure cursor position is valid (0 to width-1)
            int adjPos = std::min(m_cursorPos, std::max(0, ww - 1)); 
            wmove(m_inputWin.get(), 0, adjPos);
            curs_set(1); // Make cursor visible
            wnoutrefresh(m_inputWin.get()); // Stage input window changes
        } else if (!m_resizeStatus) { 
             curs_set(0); // Hide cursor if too small
             wnoutrefresh(stdscr); // Need to refresh stdscr if showing error msg
        }
        doupdate(); // Update physical screen
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // Ensure global pointer is cleared if this instance was the one running
    ConsoleUI* expected = this;
    g_consoleUIInstance.compare_exchange_strong(expected, nullptr);
}

void ConsoleUI::stop() {
    m_isRunning.store(false, std::memory_order_relaxed);
} 