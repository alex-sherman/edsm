cmake_minimum_required(VERSION 2.6)

# Add -DNDEBUG for release versions to ignore assert()s
set(CMAKE_C_FLAGS "-g -rdynamic -Wall -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -fPIC")

# Include the common header files
include_directories(uthash)

add_subdirectory(test)
add_subdirectory(src)
