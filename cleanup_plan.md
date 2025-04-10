# TUI Project Cleanup Plan

## Current Project Analysis

The current project structure is cluttered with:
- Git-related files (.git directory)
- External libraries (external/sol2-3.3.0)
- Build artifacts (build directory)
- IDE configuration files (.vscode)
- Duplicated header files (src/ConsoleUI.h and include/ConsoleUI.h)

## Cleanup and Reorganization Plan

### 1. Clean Project Structure

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
└── deps/                    # Dependencies (optional)
    ├── pdcurses/            # PDCurses library
    └── sol2/                # Sol2 library
```

### 2. Implementation Steps

1. **Create a new clean directory structure**
   - Create the directory structure as outlined above

2. **Copy essential source files**
   - Copy all .cpp files from src/ to the new src/ directory
   - Ensure there are no duplicates

3. **Copy essential header files**
   - Copy all .h files from include/ to the new include/ directory
   - Remove any duplicates (like ConsoleUI.h that exists in both src/ and include/)

4. **Copy script files**
   - Copy all .lua files from scripts/ to the new scripts/ directory

5. **Copy build configuration**
   - Copy and update CMakeLists.txt
   - Copy build.bat

6. **Copy documentation**
   - Copy README.md

7. **Handle dependencies**
   - Include minimal required files from pdcurses and sol2

### 3. Benefits

1. **Cleaner Structure**: Organized directories with clear separation of concerns
2. **Reduced Size**: Removal of unnecessary files and build artifacts
3. **Easier Maintenance**: Simplified project structure makes it easier to understand and modify
4. **Preserved Functionality**: Both applications (console_app and scripted_app) remain functional