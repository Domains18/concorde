cmake_minimum_required(VERSION 3.14)
project(Concorde VERSION 1.0 LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)



find_package(Threads REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/include)



add_executable(concorde
    src/main.cpp
    src/discovery.cpp
    src/http_server.cpp
)

target_link_libraries(concorde PRIVATE Threads::Threads)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()


if(MSCV)
    target_compile_options(concorde PRIVATE /W4 /WX)
else()
    target_compile_options(concorde PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()