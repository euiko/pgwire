cmake_minimum_required(VERSION 2.17)
project(duckpg LANGUAGES CXX)

# some options
option(PGWIRE_BUILD_SHARED "Build shared library" OFF)
option(PGWIRE_BUILD_DEMO "Build demo" ON)
option(PGWIRE_BUILD_TEST "Build test" ON)

# dependencies
option(PROMISE_BUILD_EXAMPLES "Build examples" OFF)
include(third_party/asio.cmake)
add_subdirectory(third_party/catch2)
add_subdirectory(third_party/endian)
add_subdirectory(third_party/function2)
add_subdirectory(third_party/promise-cpp)

# options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# main project
include_directories(include)
add_subdirectory(src/pgwire)

# demo project
if(PGWIRE_BUILD_DEMO)
    add_subdirectory(src/demo)
endif()
# tests
if(PGWIRE_BUILD_TEST)
    add_subdirectory(test)
endif()
