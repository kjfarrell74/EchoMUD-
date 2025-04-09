cmake_minimum_required(VERSION 3.20)
project(ConsoleUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PDCURSES_FOUND_LOCALLY FALSE) # Initialize flag

# Find ncurses (or pdcurses on Windows)
if(WIN32)
    # Attempt to find PDCurses locally first
    set(PDCURSES_LOCAL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/pdcurses/include)
    set(PDCURSES_LOCAL_LIB ${PROJECT_SOURCE_DIR}/pdcurses/pdcurses.lib)

    # Check using full paths for reliability
    if(EXISTS "${PDCURSES_LOCAL_INCLUDE_DIR}/curses.h" AND EXISTS "${PDCURSES_LOCAL_LIB}")
        message(STATUS "Found local PDCurses in ${PROJECT_SOURCE_DIR}/pdcurses. Using local paths.")
        # Set the standard variables directly
        set(CURSES_INCLUDE_DIRS ${PDCURSES_LOCAL_INCLUDE_DIR})
        set(CURSES_LIBRARIES ${PDCURSES_LOCAL_LIB})
        set(PDCURSES_FOUND_LOCALLY TRUE) # Set flag indicating we found it
    else()
        message(STATUS "Local PDCurses not found in ${PROJECT_SOURCE_DIR}/pdcurses (checked ${PDCURSES_LOCAL_INCLUDE_DIR}/curses.h and ${PDCURSES_LOCAL_LIB}). Will try find_package.")
    endif()
endif()

# Only call find_package if we didn't find the local PDCurses on Windows
if(NOT PDCURSES_FOUND_LOCALLY)
    message(STATUS "Calling find_package(Curses REQUIRED)")
    find_package(Curses REQUIRED)
endif()

# Check if Curses was found either locally or via find_package
if(NOT CURSES_LIBRARIES OR NOT CURSES_INCLUDE_DIRS)
    message(FATAL_ERROR "Curses library not found. Please install ncurses/pdcurses or check paths. Include: ${CURSES_INCLUDE_DIRS} Lib: ${CURSES_LIBRARIES}")
endif()

add_executable(console_app 
    src/main.cpp 
    src/ConsoleUI.cpp 
    src/GameEngine.cpp 
    src/CommandLineEditor.cpp
    src/SignalHandler.cpp
)

# Link ncurses library - use the variables found/set above
target_link_libraries(console_app PRIVATE ${CURSES_LIBRARIES})

# Set include directories - use the variables found/set above
target_include_directories(console_app PRIVATE 
    ${CURSES_INCLUDE_DIRS} 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

# Enable C++23 features (may vary slightly by compiler)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(console_app PRIVATE -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(console_app PRIVATE /W4 /EHsc)
endif()

# Add source group for IDEs
source_group(TREE ${PROJECT_SOURCE_DIR} FILES 
    src/main.cpp 
    src/ConsoleUI.cpp 
    src/GameEngine.cpp
    src/CommandLineEditor.cpp
    src/SignalHandler.cpp
    include/ConsoleUI.h
    include/GameWorld.h
    include/GameEngine.h
    include/CommandLineEditor.h
)

install(TARGETS console_app DESTINATION bin) 