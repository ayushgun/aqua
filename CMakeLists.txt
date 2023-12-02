cmake_minimum_required(VERSION 3.5)
project(aqua)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEV_WARNINGS "-Wall -Wextra -Werror -pedantic")
    set(SANITIZERS "-fsanitize=undefined -fsanitize=thread")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEV_WARNINGS} ${SANITIZERS}")
endif()

add_library(aqua SHARED src/pool.cpp)
target_include_directories(aqua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(GTest REQUIRED)
if(GTEST_FOUND)
    enable_testing()
    add_executable(aqua_tests test/test_pool.cpp test/test_queue.cpp)
    target_link_libraries(aqua_tests GTest::GTest GTest::Main aqua)
    target_include_directories(aqua_tests PRIVATE ${GTEST_INCLUDE_DIRS})

    include(GoogleTest)
    gtest_discover_tests(aqua_tests)
endif()
