#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <expected>
#include <sol/sol.hpp>

/**
 * @class ScriptRunner
 * @brief Manages loading and execution of Lua script commands
 * 
 * The ScriptRunner class handles loading Lua scripts from files, registering them
 * as commands, and executing them with provided arguments. It provides methods for
 * retrieving script metadata such as help text and descriptions.
 */
class ScriptRunner {
public:
    // Error types for script operations
    enum class ScriptError {
        LoadFailed,
        ExecutionFailed,
        CommandNotFound,
        InvalidScript
    };

    // Constructor initializes the Lua state
    ScriptRunner();

    /**
     * @brief Loads a Lua script from file and registers it as a command
     * @param name The command name to register
     * @param scriptPath Path to the Lua script file
     * @return Success or an error code
     */
    std::expected<void, ScriptError> loadScript(const std::string& name, const std::filesystem::path& scriptPath);

    /**
     * @brief Executes a loaded script command with arguments
     * @param name The command name to execute
     * @param args Arguments to pass to the script
     * @return The script's output or an error message
     */
    std::expected<std::string, ScriptError> runCommand(const std::string& name, const std::string& args);

    /**
     * @brief Gets the help string for a command
     * @param name The command name
     * @return The help string or an error message
     */
    std::expected<std::string, ScriptError> getHelp(const std::string& name);

    /**
     * @brief Gets the description for a command
     * @param name The command name
     * @return The description or an error message
     */
    std::expected<std::string, ScriptError> getDescription(const std::string& name);

    /**
     * @brief Checks if a command script is loaded
     * @param name The command name
     * @return True if the command exists
     */
    bool hasCommand(const std::string& name) const;

private:
    // The Lua state
    sol::state m_lua;

    // Map of command names to loaded script objects
    std::unordered_map<std::string, sol::table> m_scripts;

    // Map of script paths to track file modifications
    std::unordered_map<std::string, std::filesystem::file_time_type> m_scriptTimes;

    // Get the script table for a command
    std::expected<sol::table, ScriptError> getScript(const std::string& name);

    // Helper to validate that script has required elements
    bool validateScript(const sol::table& script) const;
};