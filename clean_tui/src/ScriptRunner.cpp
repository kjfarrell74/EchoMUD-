#include "../include/ScriptRunner.h"
#include <fstream>
#include <format>
#include <iostream>

ScriptRunner::ScriptRunner() {
    try {
        // Initialize Lua state
        m_lua.open_libraries(
            sol::lib::base,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::io
        );
        
        // Add custom error handler for better debugging
        m_lua.set_exception_handler([](lua_State*, sol::optional<const std::exception&> maybe_exception, sol::string_view description) {
            std::cerr << "Lua error: ";
            if (maybe_exception) {
                std::cerr << maybe_exception->what();
            }
            else {
                std::cerr << description;
            }
            std::cerr << std::endl;
            
            return sol::stack::push(maybe_exception ? maybe_exception->what() : description);
        });
    }
    catch (const std::exception& e) {
        std::cerr << "Error initializing Lua: " << e.what() << std::endl;
    }
}

std::expected<void, ScriptRunner::ScriptError> ScriptRunner::loadScript(
    const std::string& name, 
    const std::filesystem::path& scriptPath
) {
    try {
        // Check if file exists
        if (!std::filesystem::exists(scriptPath)) {
            std::cerr << std::format("Script file not found: {}", scriptPath.string()) << std::endl;
            return std::unexpected(ScriptError::LoadFailed);
        }

        // Load the script file
        auto result = m_lua.safe_script_file(scriptPath.string());
        if (!result.valid()) {
            sol::error err = result;
            std::cerr << std::format("Failed to load script {}: {}", 
                scriptPath.string(), err.what()) << std::endl;
            return std::unexpected(ScriptError::LoadFailed);
        }

        // Get the script table
        sol::table scriptTable = result;
        
        // Validate the script has the required elements
        if (!validateScript(scriptTable)) {
            std::cerr << std::format("Invalid script format in {}: missing required elements", 
                scriptPath.string()) << std::endl;
            return std::unexpected(ScriptError::InvalidScript);
        }

        // Store the script and its modification time
        m_scripts[name] = scriptTable;
        m_scriptTimes[name] = std::filesystem::last_write_time(scriptPath);
        
        std::cout << std::format("Successfully loaded script: {}", name) << std::endl;
        return {};  // Success
    }
    catch (const std::exception& e) {
        std::cerr << std::format("Exception loading script {}: {}", 
            scriptPath.string(), e.what()) << std::endl;
        return std::unexpected(ScriptError::LoadFailed);
    }
}

std::expected<std::string, ScriptRunner::ScriptError> ScriptRunner::runCommand(
    const std::string& name, 
    const std::string& args
) {
    // Get the script
    auto scriptResult = getScript(name);
    if (!scriptResult) {
        return std::unexpected(scriptResult.error());
    }
    
    try {
        // Get the run function
        sol::table script = scriptResult.value();
        sol::protected_function runFunc = script["run"];
        
        // Execute the function with args
        auto result = runFunc(args);
        if (!result.valid()) {
            sol::error err = result;
            std::cerr << std::format("Error executing script {}: {}", 
                name, err.what()) << std::endl;
            return std::unexpected(ScriptError::ExecutionFailed);
        }
        
        // Convert the result to string
        std::string output = result;
        return output;
    }
    catch (const std::exception& e) {
        std::cerr << std::format("Exception running script {}: {}", 
            name, e.what()) << std::endl;
        return std::unexpected(ScriptError::ExecutionFailed);
    }
}

std::expected<std::string, ScriptRunner::ScriptError> ScriptRunner::getHelp(const std::string& name) {
    // Get the script
    auto scriptResult = getScript(name);
    if (!scriptResult) {
        return std::unexpected(scriptResult.error());
    }
    
    try {
        // Get the help string
        sol::table script = scriptResult.value();
        std::string help = script["help"];
        return help;
    }
    catch (const std::exception& e) {
        std::cerr << std::format("Error getting help for {}: {}", 
            name, e.what()) << std::endl;
        return std::unexpected(ScriptError::ExecutionFailed);
    }
}

std::expected<std::string, ScriptRunner::ScriptError> ScriptRunner::getDescription(const std::string& name) {
    // Get the script
    auto scriptResult = getScript(name);
    if (!scriptResult) {
        return std::unexpected(scriptResult.error());
    }
    
    try {
        // Get the description string
        sol::table script = scriptResult.value();
        std::string description = script["description"];
        return description;
    }
    catch (const std::exception& e) {
        std::cerr << std::format("Error getting description for {}: {}", 
            name, e.what()) << std::endl;
        return std::unexpected(ScriptError::ExecutionFailed);
    }
}

bool ScriptRunner::hasCommand(const std::string& name) const {
    return m_scripts.contains(name);
}

std::expected<sol::table, ScriptRunner::ScriptError> ScriptRunner::getScript(const std::string& name) {
    // Check if the command exists
    if (!hasCommand(name)) {
        std::cerr << std::format("Script command not found: {}", name) << std::endl;
        return std::unexpected(ScriptError::CommandNotFound);
    }
    
    return m_scripts[name];
}

bool ScriptRunner::validateScript(const sol::table& script) const {
    // Check for required elements
    bool valid = script["help"].valid() && 
                 script["description"].valid() && 
                 script["run"].valid() &&
                 script["run"].is<sol::protected_function>();
    
    // Print detailed validation info
    if (!valid) {
        if (!script["help"].valid())
            std::cerr << "Script missing 'help' string" << std::endl;
        if (!script["description"].valid())
            std::cerr << "Script missing 'description' string" << std::endl;
        if (!script["run"].valid())
            std::cerr << "Script missing 'run' function" << std::endl;
        else if (!script["run"].is<sol::protected_function>())
            std::cerr << "Script 'run' is not a function" << std::endl;
    }
    
    return valid;
}