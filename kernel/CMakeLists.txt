cmake_minimum_required(VERSION 3.20)
project(jarvis_kernel LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

# Output library
add_library(jarvis_kernel SHARED
    src/kernel.c
)

target_include_directories(jarvis_kernel PUBLIC
    $<BUILD_INTERFACE:include>
)

# Google Test (if building tests)
option(BUILD_TESTS "Build Google Test suites" OFF)
if(BUILD_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(test)