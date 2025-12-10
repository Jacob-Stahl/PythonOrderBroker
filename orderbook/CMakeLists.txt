### Setup ###
cmake_minimum_required(VERSION 3.14)
# suppress annoyiing warnings from google test
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "No dev warnings")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

### Compile main application ###
add_executable(orderbook main.cpp)

### UNIT TESTS WITH GOOGLE TEST###

# https://google.github.io/googletest/quickstart-cmake.html
enable_testing()
add_executable(unittests tests/test_orderbook.cpp)

target_link_libraries(
  unittests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unittests)