cmake_minimum_required(VERSION 3.20)
project(ConsoleUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable debug build by default
set(CMAKE_BUILD_TYPE Debug)

# Add debug symbols
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -O0")
endif()

# Set up sol2 - header-only library
set(SOL2_DIR "${PROJECT_SOURCE_DIR}/deps/sol2")
if(NOT EXISTS "${SOL2_DIR}/include/sol/abort.hpp")
    message(WARNING "sol2 not found at ${SOL2_DIR}. Please make sure it's installed or update the path.")
endif()

# Set up Lua - required for sol2
# Set Lua paths explicitly
set(LUA_INCLUDE_DIR "C:/Program Files (x86)/Lua/5.1/include")
set(LUA_LIBRARIES "C:/Program Files (x86)/Lua/5.1/lib/lua51.lib")
set(LUA_FOUND TRUE)
set(LUA_VERSION_STRING "5.1")

message(STATUS "Using Lua from: C:/Program Files (x86)/Lua/5.1")
message(STATUS "Lua include dir: ${LUA_INCLUDE_DIR}")
message(STATUS "Lua libraries: ${LUA_LIBRARIES}")

# Set up PDCurses
set(PDCURSES_DIR "${PROJECT_SOURCE_DIR}/deps/pdcurses")
set(PDCURSES_INCLUDE_DIR "${PDCURSES_DIR}/include")
set(PDCURSES_LIB "${PDCURSES_DIR}/pdcurses.lib")

# Check if PDCurses files exist
if(EXISTS "${PDCURSES_INCLUDE_DIR}/curses.h" AND EXISTS "${PDCURSES_LIB}")
    message(STATUS "Found PDCurses in ${PDCURSES_DIR}")
    set(CURSES_INCLUDE_DIRS ${PDCURSES_INCLUDE_DIR})
    set(CURSES_LIBRARIES ${PDCURSES_LIB})
else()
    message(FATAL_ERROR "PDCurses not found in ${PDCURSES_DIR}. Please check the paths.")
endif()

# Create script directory and copy Lua scripts
set(SCRIPTS_DIR "${PROJECT_SOURCE_DIR}/scripts")
file(GLOB LUA_SCRIPTS "${SCRIPTS_DIR}/*.lua")
file(COPY ${LUA_SCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/scripts)

# Define common source files
set(COMMON_SOURCES
    src/ConsoleUI.cpp 
    src/GameEngine.cpp 
    src/CommandLineEditor.cpp
    src/SignalHandler.cpp
)

# Main console app (without scripting)
add_executable(console_app 
    src/main.cpp 
    ${COMMON_SOURCES}
)

# Scripted console app
add_executable(scripted_app 
    src/main_with_scripts.cpp 
    ${COMMON_SOURCES}
    src/ScriptRunner.cpp
)

# Link libraries for main app
target_link_libraries(console_app PRIVATE 
    ${CURSES_LIBRARIES}
)

# Link libraries for scripted app
target_link_libraries(scripted_app PRIVATE 
    ${CURSES_LIBRARIES}
    ${LUA_LIBRARIES}
)

# Set include directories for main app
target_include_directories(console_app PRIVATE 
    ${CURSES_INCLUDE_DIRS} 
    ${PROJECT_SOURCE_DIR}/include
)

# Set include directories for scripted app
target_include_directories(scripted_app PRIVATE 
    ${CURSES_INCLUDE_DIRS} 
    ${LUA_INCLUDE_DIR}
    ${SOL2_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

# Enable C++23 features for main app
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(console_app PRIVATE -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(console_app PRIVATE /W4 /EHsc /FS)
endif()

# Enable C++23 features for scripted app
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(scripted_app PRIVATE -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(scripted_app PRIVATE /W4 /EHsc /FS)
endif()

# Define feature for scripted app
target_compile_definitions(scripted_app PRIVATE ENABLE_LUA_SCRIPTING=1)

# Install targets
install(TARGETS console_app scripted_app DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION bin/scripts)