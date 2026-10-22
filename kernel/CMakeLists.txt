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
    core/kernel_memory.c
    core/kernel_fs.c
    core/kernel_fs_cmds.c
    core/kernel_devices.c
    core/kernel_ipc.c
    core/kernel_ipc_cmds.c
    core/kernel_ipc_sync_cmds.c
    core/kernel_ipc_snapshot.c
    core/kernel_shell.c
    machine/machine.c
    ipc/ipc_types.c
    ipc/ipc_manager.c
    ipc/ipc_queue.c
    ipc/ipc_shm.c
    ipc/ipc_pipe.c
    ipc/ipc_sync.c
    ipc/ipc_deadlock.c
    shell/shell_parse.c
    shell/shell_format.c
    shell/shell_cmds.c
    shell/shell_proc_cmds.c
    shell/shell_fs_cmds.c
    cpu/alu.c
    cpu/clock.c
    cpu/cpu.c
    scheduler/scheduler.c
    process/context_switch.c
    process/pid_gen.c
    process/process_manager.c
    process/queues.c
    memory/alloc_strategy.c
    memory/frame_table.c
    memory/memory_manager.c
    memory/mm_types.c
    memory/page_table.c
    memory/replace_policy.c
    memory/swap.c
    filesystem/disk.c
    filesystem/fs_types.c
    filesystem/open_table.c
    filesystem/vfs.c
    filesystem/vfs_ops.c
    device/device_types.c
    device/device_manager.c
    device/drivers/clock_driver.c
    device/drivers/disk_driver.c
    device/drivers/display.c
    device/drivers/keyboard.c
    device/drivers/mouse.c
    device/drivers/printer.c
    interrupts/ic.c
    interrupts/isr.c
    interrupts/error_manager.c
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