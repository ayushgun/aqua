cmake_minimum_required(VERSION 3.27)
project(aqua LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    "$<$<CONFIG:DEBUG>:-Wall;-Wextra;-Werror;-pedantic;-fsanitize=undefined;-fsanitize=thread>"
)

find_package(Boost 1.83.0 REQUIRED COMPONENTS system thread)
find_package(GTest REQUIRED)

add_library(aqua SHARED src/pool.cpp)
target_include_directories(aqua PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(aqua PRIVATE ${Boost_LIBRARIES})

enable_testing()
add_executable(aqua_tests test/test_pool.cpp)
target_link_libraries(aqua_tests PRIVATE GTest::GTest GTest::Main aqua)

include(GoogleTest)
gtest_discover_tests(aqua_tests)
