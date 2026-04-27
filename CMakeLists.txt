cmake_minimum_required(VERSION 3.16)
project(ChessEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Shared engine library (used by both CLI and GUI)
add_library(chess_core STATIC
    Board.cpp
    MoveGenerator.cpp
    Evaluator.cpp
    Engine.cpp
)
target_compile_features(chess_core PUBLIC cxx_std_17)
if(NOT MSVC)
    target_compile_options(chess_core PRIVATE -mpopcnt -mbmi)
endif()

# CLI executable
add_executable(chess_engine main.cpp)
target_link_libraries(chess_engine PRIVATE chess_core)
if(NOT MSVC)
    target_compile_options(chess_engine PRIVATE -mpopcnt -mbmi)
endif()

# GUI executable (requires SFML 3)
find_package(SFML 3 COMPONENTS Graphics QUIET)
if(SFML_FOUND)
    message(STATUS "SFML ${SFML_VERSION} found — building chess_gui")
    add_executable(chess_gui main_gui.cpp ChessApp.cpp)
    target_link_libraries(chess_gui PRIVATE chess_core SFML::Graphics)
    if(NOT MSVC)
        target_compile_options(chess_gui PRIVATE -mpopcnt -mbmi)
        # Suppress console window on Windows
        target_link_options(chess_gui PRIVATE -mwindows)
    endif()
else()
    message(WARNING "SFML not found — GUI skipped. Install: pacman -S mingw-w64-x86_64-sfml")
endif()
