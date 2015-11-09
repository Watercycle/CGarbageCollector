cmake_minimum_required(VERSION 3.3)
project(C_GarbageCollected)

set(COMPILER "-std=c11")
set(GCC_INCLUDE "-pedantic -g -Wall -Wextra -Winit-self -Wshadow -Wpointer-arith -Wcast-align -Winit-self")
set(GCC_EXCLUDE "-Wno-unused-function -Wno-unused-parameter")
set(GCC_OPTIMIZE "-O0 -pipe")

set(CMAKE_C_FLAGS "${COMPILER} ${GCC_OPTIMIZE} ${GCC_EXCLUDE} ")

set(SOURCE_FILES src/main.c)

include_directories(src)
add_executable(C_GarbageCollected ${SOURCE_FILES})