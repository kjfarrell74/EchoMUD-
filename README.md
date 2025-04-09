# Modern C++ Console UI Application

This is a sample console application demonstrating a split-screen UI using ncurses (or pdcurses) and modern C++23 features.

## Build Instructions

1.  Ensure you have a C++23 compliant compiler (GCC 13+, Clang 16+, MSVC 19.34+).
2.  Install CMake (version 3.20 or higher).
3.  Install ncurses development libraries (e.g., `libncurses-dev` on Debian/Ubuntu, `ncurses-devel` on Fedora) or pdcurses for Windows.
4.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
5.  Configure the project:
    ```bash
    cmake ..
    ```
6.  Build the application:
    ```bash
    cmake --build .
    ```
7.  Run the executable:
    ```bash
    ./console_app 
    ```
    (Or `.\Debug\console_app.exe` on Windows with MSVC)

## Features

*   Split-screen layout (output and input areas).
*   Basic input handling (cursor movement, history, backspace/delete).
*   Terminal resize handling.
*   Graceful shutdown (Ctrl+C, "exit" command).
*   Modern C++23 features (<expected>, <format>, ranges, smart pointers).
*   RAII for ncurses window management.
*   UTF-8 support (basic). 