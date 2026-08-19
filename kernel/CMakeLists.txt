cmake_minimum_required(VERSION 3.20)
project(jarvis_kernel LANGUAGES C CXX ASM_NASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

# NASM is installed via scoop and reachable through the project toolchain
# junction (the Makefile build uses the same path).
set(CMAKE_ASM_NASM_COMPILER
    "${CMAKE_CURRENT_SOURCE_DIR}/../.toolchain/nasm/nasm.exe"
    CACHE FILEPATH "NASM assembler")

# Output library (matches the kernel Makefile target)
add_library(jarvis_kernel SHARED
    core/kernel.c
    cpu/alu.c
    cpu/clock.c
    cpu/cpu.c
    scheduler/scheduler.c
    process/context_switch.c
    process/pid_gen.c
    process/process_manager.c
    process/queues.c
    deps/cJSON/cJSON.c
    asm/context_switch.S
)

target_include_directories(jarvis_kernel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Google Test (if building tests)
option(BUILD_TESTS "Build Google Test suites" OFF)
if(BUILD_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(test)
endif()