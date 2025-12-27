cmake_minimum_required(VERSION 3.16)
project(game-of-life)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CACHE{EXT_SDL} HELP "Enable C-Core SDL extension" VALUE ON)
add_subdirectory(c-core)

# Create your game executable target as usual
add_executable(game-of-life MACOSX_BUNDLE main.c)

# Link to the actual SDL3 library.

target_link_libraries(game-of-life PRIVATE CCORE::std CCORE::sdl)
