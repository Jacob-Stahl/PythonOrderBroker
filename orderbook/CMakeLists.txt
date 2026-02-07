cmake_minimum_required(VERSION 3.14)
project(orderbook VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources for the orderbook library
set(ORDERBOOK_SOURCES
		order.cpp
		matcher.cpp
		notifier.cpp
		abm.cpp
		agent.cpp
)

add_library(orderbook STATIC ${ORDERBOOK_SOURCES})
target_include_directories(orderbook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(orderbook_app main.cpp)
target_link_libraries(orderbook_app PRIVATE orderbook)

enable_testing()

include(FetchContent)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG main
)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(test_orderbook ${TEST_SOURCES})
target_link_libraries(test_orderbook PRIVATE orderbook gtest_main)
add_test(NAME orderbook_tests COMMAND test_orderbook)

