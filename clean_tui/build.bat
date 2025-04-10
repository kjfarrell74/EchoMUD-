@echo off
setlocal enabledelayedexpansion

echo MUD Engine Build Script
echo ======================

:: Check prerequisites
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: CMake is not installed or not in your PATH.
    echo Please install CMake from https://cmake.org/download/
    exit /b 1
)

:: Check if external/sol2 exists
if not exist external\sol2\include\sol\sol.hpp (
    echo WARNING: sol2 library not found in external/sol2
    echo Creating external directory...
    mkdir external 2>nul
    
    echo Would you like to download sol2? [Y/N]
    set /p download=
    if /i "!download!"=="Y" (
        echo Downloading sol2...
        powershell -Command "Invoke-WebRequest -Uri 'https://github.com/ThePhD/sol2/archive/refs/tags/v3.3.0.zip' -OutFile 'sol2.zip'"
        
        echo Extracting...
        powershell -Command "Expand-Archive -Path 'sol2.zip' -DestinationPath 'external' -Force"
        ren external\sol2-3.3.0 external\sol2
        del sol2.zip
        
        echo sol2 has been downloaded and extracted.
    ) else (
        echo Please manually install sol2 to external/sol2
        echo See https://github.com/ThePhD/sol2 for more information
    )
)

:: Check if scripts directory exists
if not exist scripts (
    echo Creating scripts directory...
    mkdir scripts 2>nul
)

:: Check for say.lua
if not exist scripts\say.lua (
    echo Creating sample Lua script...
    (
        echo -- say.lua - Script for the 'say' command
        echo -- This command sends a message to everyone in the room
        echo.
        echo -- Command metadata
        echo local script = {
        echo     help = "say ^<message^> - Speak a message to everyone in the room",
        echo     description = "Broadcasts a message to all players in your current location",
        echo     
        echo     -- Main command function
        echo     run = function^(args^)
        echo         if not args or args == "" then
        echo             return "Say what?"
        echo         end
        echo         
        echo         -- Return a formatted message
        echo         -- In a real implementation, this would broadcast to all players in the room
        echo         return string.format^("You say: \"%%s\"", args^)
        echo     end
        echo }
        echo.
        echo -- Return the script table
        echo return script
    ) > scripts\say.lua
)

:: Check for test.lua
if not exist scripts\test.lua (
    echo Creating test Lua script...
    (
        echo -- test.lua - A simple test script to verify Lua integration
        echo.
        echo -- Command metadata
        echo local script = {
        echo     help = "test - A test command to verify script loading",
        echo     description = "Tests the Lua script integration system",
        echo     
        echo     -- Main command function
        echo     run = function^(args^)
        echo         -- This is a very simple script that just prints information
        echo         -- about the Lua environment and any arguments passed
        echo         
        echo         local info = "Lua script system is working!\n"
        echo         info = info .. string.format^("Lua version: %%s\n", _VERSION^)
        echo         
        echo         if args and args ~= "" then
        echo             info = info .. string.format^("Arguments received: \"%%s\"", args^)
        echo         else
        echo             info = info .. "No arguments provided"
        echo         end
        echo         
        echo         return info
        echo     end
        echo }
        echo.
        echo -- Return the script table
        echo return script
    ) > scripts\test.lua
)

:: Create build directory if it doesn't exist
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

echo Configuring project...
cmake .. -A x64

if %ERRORLEVEL% neq 0 (
    echo Error: CMake configuration failed.
    cd ..
    exit /b 1
)

echo Building main console app...
cmake --build . --config Release --target console_app

echo Building scripted version...
cmake --build . --config Release --target scripted_app

cd ..

echo.
echo Build completed. You can find:
echo - Regular version: build\Release\console_app.exe
echo - Scripted version: build\Release\scripted_app.exe
echo.
echo Run 'build\Release\scripted_app.exe' to test the Lua scripting system.

endlocal 