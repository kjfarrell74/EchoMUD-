#include "../include/ConsoleUI.h"
#include <csignal>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <iostream>

// Initialize static members
std::unordered_map<int, SignalHandler::SignalCallback> SignalHandler::s_signalCallbacks;
std::mutex SignalHandler::s_signalMutex;

// Global signal handler that routes signals to our handler class
void signalRouter(int signal) {
    SignalHandler::handleSignal(signal);
}

void SignalHandler::registerHandler(int signal, SignalCallback callback) {
    // Lock during modification
    std::lock_guard<std::mutex> lock(s_signalMutex);
    
    // Register the callback
    s_signalCallbacks[signal] = callback;
    
    // Set up the signal handler to point to our router
    std::signal(signal, signalRouter);
}

void SignalHandler::unregisterHandler(int signal) {
    // Lock during modification
    std::lock_guard<std::mutex> lock(s_signalMutex);
    
    // Remove the callback
    s_signalCallbacks.erase(signal);
    
    // Reset the signal handler to default
    std::signal(signal, SIG_DFL);
}

void SignalHandler::handleSignal(int signal) {
    // Lock during access
    std::lock_guard<std::mutex> lock(s_signalMutex);
    
    // Find and call the callback if it exists
    auto it = s_signalCallbacks.find(signal);
    if (it != s_signalCallbacks.end()) {
        try {
            // Call the registered callback
            it->second();
        } 
        catch (const std::exception& e) {
            // Log exception from signal handler, but don't rethrow
            // as that would be unsafe in a signal handler
            std::cerr << "Exception in signal handler: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown exception in signal handler" << std::endl;
        }
    }
} 