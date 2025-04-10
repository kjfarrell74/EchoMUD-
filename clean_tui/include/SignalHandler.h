#pragma once

#include <functional>
#include <unordered_map>
#include <mutex>
#include <expected>

// Forward declarations
enum class SignalError {
    RegisterFailed,
    UnregisterFailed,
    InvalidSignal,
    CallbackError
};

/**
 * @class SignalHandler
 * @brief Manages signal handling with thread-safe registration and callback execution
 */
class SignalHandler {
public:
    using SignalCallback = std::function<void()>;
    
    /**
     * @brief Registers a callback function for a specific signal
     * @param signal The signal number to handle
     * @param callback The function to call when the signal is received
     * @return Success or an error code
     */
    static std::expected<void, SignalError> registerHandler(int signal, SignalCallback callback);
    
    /**
     * @brief Unregisters a previously registered signal handler
     * @param signal The signal number to unregister
     * @return Success or an error code
     */
    static std::expected<void, SignalError> unregisterHandler(int signal);
    
    /**
     * @brief Processes a received signal by calling the appropriate callback
     * @param signal The signal that was received
     */
    static void handleSignal(int signal);
    
private:
    // Map of signal numbers to callback functions
    static std::unordered_map<int, SignalCallback> s_signalCallbacks;
    
    // Mutex for thread safety when accessing the callbacks map
    static std::mutex s_signalMutex;
};