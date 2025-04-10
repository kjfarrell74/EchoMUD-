# Console UI Project

A text-based user interface (TUI) application with Lua scripting support.

## Project Overview

This project provides two applications:

1. **console_app**: A basic console application without scripting
2. **scripted_app**: An enhanced version with Lua scripting support

## Project Structure

```
TUI/
├── CMakeLists.txt           # Main build configuration
├── README.md                # Project documentation
├── build.bat                # Build script for Windows
├── src/                     # Source files
│   ├── CommandLineEditor.cpp
│   ├── ConsoleUI.cpp
│   ├── GameEngine.cpp
│   ├── ScriptRunner.cpp
│   ├── SignalHandler.cpp
│   ├── main.cpp             # Entry point for console_app
│   └── main_with_scripts.cpp # Entry point for scripted_app
├── include/                 # Header files
│   ├── CommandLineEditor.h
│   ├── ConsoleUI.h
│   ├── GameEngine.h
│   ├── GameWorld.h
│   ├── ScriptRunner.h
│   └── SignalHandler.h
├── scripts/                 # Lua scripts
│   ├── say.lua
│   └── test.lua
└── deps/                    # Dependencies
    ├── pdcurses/            # PDCurses library
    └── sol2/                # Sol2 library
```

## Dependencies

- **PDCurses**: A public domain curses library for Windows
- **Lua 5.1**: Required for scripting support
- **Sol2**: A C++ binding for Lua

## Building the Project

### Prerequisites

- CMake 3.20 or higher
- C++23 compatible compiler
- Lua 5.1 installed at `C:/Program Files (x86)/Lua/5.1/`

### Build Steps

1. Create a build directory:
   ```
   mkdir build
   cd build
   ```

2. Generate build files:
   ```
   cmake ..
   ```

3. Build the project:
   ```
   cmake --build .
   ```

4. Run the applications:
   ```
   ./console_app     # Basic console app
   ./scripted_app    # App with Lua scripting
   ```

## Lua Scripting

The scripted version supports Lua commands defined in the `scripts/` directory. Each script should follow this format:

```lua
-- Command metadata
local script = {
    help = "command_name - Brief help text",
    description = "Detailed description of what the command does",
    
    -- Main command function
    run = function(args)
        -- Command implementation
        return "Result message"
    end
}

-- Return the script table
return script
```

## License

This project is available under the MIT License.