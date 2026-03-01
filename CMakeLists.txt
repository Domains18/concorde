cmake_minimum_required(VERSION 3.14)
project(Concorde VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

# Try to find libsodium
find_library(SODIUM_LIBRARY NAMES sodium libsodium)
find_path(SODIUM_INCLUDE_DIR NAMES sodium.h)

if(NOT SODIUM_LIBRARY OR NOT SODIUM_INCLUDE_DIR)
    message(FATAL_ERROR "libsodium not found! Install with: brew install libsodium (Mac) or apt install libsodium-dev (Linux)")
endif()

message(STATUS "Found libsodium: ${SODIUM_LIBRARY}")

# Check for nlohmann/json (header-only)
find_path(JSON_INCLUDE_DIR NAMES nlohmann/json.hpp)

if(NOT JSON_INCLUDE_DIR)
    message(STATUS "nlohmann/json not found, downloading header...")
    file(DOWNLOAD 
        https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann/json.hpp
        SHOW_PROGRESS
    )
    set(JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

message(STATUS "Found nlohmann/json: ${JSON_INCLUDE_DIR}")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${SODIUM_INCLUDE_DIR})
include_directories(${JSON_INCLUDE_DIR})

add_executable(concorde
    src/main.cpp
    src/discovery.cpp
    src/server.cpp
    src/crypto.cpp
    src/trust.cpp
    src/hostname.cpp
)

target_link_libraries(concorde PRIVATE 
    Threads::Threads
    ${SODIUM_LIBRARY}
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    target_compile_options(concorde PRIVATE /W4)
else()
    target_compile_options(concorde PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Test executable
add_executable(test_crypto
    src/test_crypto.cpp
    src/crypto.cpp
    src/trust.cpp
    src/hostname.cpp
)

target_link_libraries(test_crypto PRIVATE
    Threads::Threads
    ${SODIUM_LIBRARY}
)
