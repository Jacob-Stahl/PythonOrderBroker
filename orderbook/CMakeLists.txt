cmake_minimum_required(VERSION 3.10)
project(orderbook)

set(CMAKE_CXX_STANDARD 17)

add_executable(orderbook main.cpp)

### UNIT TESTS WITH GOOGLE TEST###
enable_testing()
add_executable(orderbook_tests test_orderbook.cpp)
add_test(NAME OrderbookTests COMMAND orderbook_tests)
