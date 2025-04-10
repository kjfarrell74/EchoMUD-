# MUD Engine with Lua Scripting

A modern C++23 MUD (Multi-User Dungeon) engine featuring a robust console interface and dynamic Lua scripting integration. Built with thread safety and cross-platform compatibility in mind.

## Features

- **Modern C++ Design**
  - C++23 compliant codebase
  - RAII principles throughout
  - Thread-safe message handling
  - Smart pointer memory management

- **Console Interface**
  - ncurses/pdcurses-based UI
  - Command history navigation
  - Dynamic window resizing
  - Scrollable output buffer
  - Command-line editing with cursor support

- **Scripting System**
  - Lua 5.3+ integration via sol2
  - Hot-reloadable commands
  - Script error handling
  - Sandboxed script execution
  - Easy command registration

- **Core Architecture**
  - Event-driven design
  - Signal handling for graceful shutdown
  - Modular component system
  - Cross-platform compatibility

## Requirements

- **Compiler Support**
  - Windows: MSVC 2022 or newer
  - Linux: GCC 12+ or Clang 15+
  - C++23 features required

- **Build System**
  - CMake 3.20 or higher
  - Build scripts for Windows/Linux

- **Dependencies**
  - Lua 5.3+ development libraries
  - PDCurses (Windows) or ncurses (Linux/Mac)
  - sol2 library (automatically downloaded)

## Architecture

### Core Components

1. **ConsoleUI (`ConsoleUI.h/cpp`)**
   - Manages the ncurses/pdcurses interface
   - Handles window management and resizing
   - Processes user input
   - Maintains output message buffer

2. **GameEngine (`GameEngine.h/cpp`)**
   - Core game logic coordinator
   - Script management and execution
   - Command processing
   - Game state management

3. **CommandLineEditor (`CommandLineEditor.h/cpp`)**
   - Input line editing capabilities
   - Command history management
   - Cursor movement and text manipulation

4. **SignalHandler (`SignalHandler.h/cpp`)**
   - System signal management
   - Graceful shutdown coordination
   - Cross-platform signal handling

5. **ScriptRunner (`ScriptRunner.h/cpp`)**
   - Lua script integration
   - Script loading and execution
   - Error handling and reporting

## Building the Project

### Windows Quick Start

```batch
# Clone the repository
git clone https://your-repository-url.git
cd mud-engine

# Run the build script
build.bat

# Run the scripted version
build\Release\scripted_app.exe
```

### Linux Build Process

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake libncurses5-dev liblua5.3-dev

# Clone and build
git clone https://your-repository-url.git
cd mud-engine
mkdir build && cd build
cmake ..
make

# Run the application
./scripted_app
```

## Scripting Guide

### Script Structure

Scripts are Lua files that must return a table with the following structure:

```lua
local script = {
    -- Command help text
    help = "command - Brief description",
    
    -- Detailed command description
    description = "Detailed explanation of what the command does",
    
    -- Main command function
    run = function(args)
        -- Command implementation
        return "Command output"
    end,
    
    -- Optional initialization
    init = function()
        -- Called when script is loaded
    end
}

return script
```

### Adding New Commands

1. Create a new Lua script in `scripts/`:

```lua
-- scripts/wave.lua
local script = {
    help = "wave [target] - Wave at someone or everyone",
    description = "Wave friendly at a specific target or to everyone in the room",
    
    run = function(args)
        if #args == 0 then
            return "You wave to everyone."
        end
        return string.format("You wave at %s.", args[1])
    end
}

return script
```

2. Register the script in `GameEngine.cpp`:

```cpp
const std::vector<std::pair<std::string, std::string>> scriptsToLoad = {
    {"say", "say.lua"},
    {"wave", "wave.lua"}  // Add your new script
};
```

### Script API Reference

Scripts have access to the following game engine functions:

```lua
-- Send a message to a player
game.sendMessage(player, message)

-- Get player information
game.getPlayerName()
game.getPlayerLocation()

-- Room management
game.getCurrentRoom()
game.getPlayersInRoom()
```

## Command Line Interface

### Basic Commands

- `/help` - Display available commands
- `/quit` - Exit the application
- `/clear` - Clear the screen
- `/history` - Show command history

### Key Bindings

- `Up/Down` - Navigate command history
- `Left/Right` - Move cursor
- `Ctrl+A` - Move to start of line
- `Ctrl+E` - Move to end of line
- `Ctrl+K` - Clear to end of line
- `Ctrl+U` - Clear entire line

## Development

### Build Targets

- `console_app` - Basic version without scripting
- `scripted_app` - Full version with Lua support

### Build Configurations

```bash
# Debug build
cmake --build . --config Debug

# Release build
cmake --build . --config Release

# Build specific target
cmake --build . --target scripted_app
```

### Testing

Run the test suite:
```bash
ctest -C Release
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## License

MIT License - See LICENSE file for details.

## Contact

For issues and feature requests, please use the GitHub issue tracker. 