# Disclaimer: Tephra is mainly developed, built and used with Visual Studio on Windows. For that purpose
# see build/Tephra.sln. Cmake in this project exists as a secondary portable build system. It is checked
# by CI and should work, but I apologize if something here isn't quite right.
# You are invited to file an issue or PR.

cmake_minimum_required (VERSION 3.15)
project("Tephra" LANGUAGES CXX)

find_package(Vulkan REQUIRED)

set(SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(HEADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# Add main Tephra library
add_subdirectory(src)

# Optionally add example program
option(TEPHRA_COMPILE_EXAMPLES "Compile examples" ON)

if(TEPHRA_COMPILE_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests are currently only set up in the Visual Studio solution
