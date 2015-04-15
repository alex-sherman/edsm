cmake_minimum_required(VERSION 2.6)

# Add -DNDEBUG for release versions to ignore assert()s
set(CMAKE_C_FLAGS "-g -rdynamic -Wall -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -fPIC")

# Include the common header files
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/uthash)

add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/test)
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/src)
