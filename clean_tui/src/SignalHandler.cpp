#include "../include/SignalHandler.h"
#include <csignal>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <iostream>
#include <format>
#include <expected>

// Initialize static members
std::unordered_map<int, SignalHandler::SignalCallback> SignalHandler::s_signalCallbacks;
std::mutex SignalHandler::s_signalMutex;

// Global signal handler that routes signals to our handler class
void signalRouter(int signal) {
    SignalHandler::handleSignal(signal);
}

std::expected<void, SignalError> SignalHandler::registerHandler(int signal, SignalCallback callback) {
    if (signal <= 0) {
        return std::unexpected(SignalError::InvalidSignal);
    }
    
    if (!callback) {
        return std::unexpected(SignalError::CallbackError);
    }
    
    try {
        // Lock during modification
        std::lock_guard<std::mutex> lock(s_signalMutex);
        
        // Register the callback
        s_signalCallbacks[signal] = std::move(callback);
        
        // Set up the signal handler to point to our router
        if (std::signal(signal, signalRouter) == SIG_ERR) {
            // Revert our registration on failure
            s_signalCallbacks.erase(signal);
            return std::unexpected(SignalError::RegisterFailed);
        }
        
        return {};  // Success
    } catch (...) {
        return std::unexpected(SignalError::RegisterFailed);
    }
}

std::expected<void, SignalError> SignalHandler::unregisterHandler(int signal) {
    if (signal <= 0) {
        return std::unexpected(SignalError::InvalidSignal);
    }
    
    try {
        // Lock during modification
        std::lock_guard<std::mutex> lock(s_signalMutex);
        
        // Check if we have this signal registered
        if (s_signalCallbacks.find(signal) == s_signalCallbacks.end()) {
            // Nothing to unregister
            return {};
        }
        
        // Remove the callback
        s_signalCallbacks.erase(signal);
        
        // Reset the signal handler to default
        if (std::signal(signal, SIG_DFL) == SIG_ERR) {
            return std::unexpected(SignalError::UnregisterFailed);
        }
        
        return {};  // Success
    } catch (...) {
        return std::unexpected(SignalError::UnregisterFailed);
    }
}

void SignalHandler::handleSignal(int signal) {
    try {
        // Lock during access - use try_lock to avoid deadlocks in signal handlers
        std::unique_lock<std::mutex> lock(s_signalMutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            // If we can't get the lock, log and return
            std::cerr << std::format("Warning: Signal {} received but mutex is locked", signal) << std::endl;
            return;
        }
        
        // Find and call the callback if it exists
        auto it = s_signalCallbacks.find(signal);
        if (it != s_signalCallbacks.end() && it->second) {
            try {
                // Call the registered callback
                it->second();
            } 
            catch (const std::exception& e) {
                // Log exception from signal handler, but don't rethrow
                // as that would be unsafe in a signal handler
                std::cerr << std::format("Exception in signal handler {}: {}", signal, e.what()) << std::endl;
            }
            catch (...) {
                std::cerr << std::format("Unknown exception in signal handler {}", signal) << std::endl;
            }
        }
    } catch (...) {
        // Log error but continue - we're in a signal handler so we must be careful
        std::cerr << std::format("Critical error in signal handler for signal {}", signal) << std::endl;
    }
}