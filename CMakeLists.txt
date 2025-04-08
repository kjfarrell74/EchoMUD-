cmake_minimum_required(VERSION 3.20)
project(ConsoleUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find ncurses (or pdcurses on Windows)
find_package(Curses REQUIRED)

add_executable(console_app src/main.cpp src/ConsoleUI.cpp)

# Link ncurses library
target_link_libraries(console_app PRIVATE ${CURSES_LIBRARIES})

# Set include directories
target_include_directories(console_app PRIVATE ${CURSES_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Enable C++23 features (may vary slightly by compiler)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(console_app PRIVATE -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(console_app PRIVATE /W4 /EHsc)
endif()

# Add source group for IDEs
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES src/main.cpp src/ConsoleUI.cpp src/ConsoleUI.h)

install(TARGETS console_app DESTINATION bin) 