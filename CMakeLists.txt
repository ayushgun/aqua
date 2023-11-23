cmake_minimum_required(VERSION 3.5)
project(aqua)

set(CMAKE_CXX_STANDARD 20)

# Define the library name and specify that it is SHARED.
add_library(aqua SHARED
    src/pool.cpp
)

# Specify include directories
target_include_directories(aqua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
