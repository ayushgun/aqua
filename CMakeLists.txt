cmake_minimum_required(VERSION 3.5)
project(aqua)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEV_WARNINGS "-Wall -Wextra -Werror -pedantic")
    set(SANITIZERS "-fsanitize=address -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEV_WARNINGS} ${SANITIZERS}")
endif()

add_library(aqua SHARED
    src/pool.cpp
)

target_include_directories(aqua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
