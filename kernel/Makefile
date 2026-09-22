# kernel/Makefile
#
# Builds libjarvis_kernel (jarvis_kernel.dll) from the C kernel sources.
#
# The kernel must be built as a 64-bit DLL to match the 64-bit Python
# bridge (ctypes). This machine ships two gcc builds:
#   - C:\MinGW\bin\gcc.exe           32-bit MinGW.org GCC 6.3.0  (wrong)
#   - scoop gcc (nuwen, 64-bit)      15.2.0 x86_64-w64-mingw32   (right)
# So CC defaults to the scoop 64-bit gcc unless overridden. NASM is
# reachable through the same .toolchain junction layout.
#
# Usage:
#   mingw32-make            # build jarvis_kernel.dll
#   mingw32-make clean      # remove build outputs
#   mingw32-make test       # build + run the C CPU smoke test

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Linux)
  CC      ?= gcc
  NASM    ?= nasm
  NASM_FMT = elf64
  TARGET   = libjarvis_kernel.so
  IMPLIB   =
  LDFLAGS ?= 
  TESTBINS = test_cpu test_process test_memory test_interrupts
else
  CC      := ../.toolchain/gcc/bin/gcc.exe
  NASM    := ../.toolchain/nasm/nasm.exe
  NASM_FMT = win64
  TARGET   = jarvis_kernel.dll
  IMPLIB   = libjarvis_kernel.a
  LDFLAGS ?= -static-libgcc
  TESTBINS = test_cpu.exe test_process.exe test_memory.exe test_interrupts.exe
endif
CFLAGS  ?= -std=c17 -Wall -Wextra -O2
ifeq ($(UNAME_S),Linux)
  CFLAGS += -fPIC
endif

SRCDIR   = core
CPUDIR   = cpu
SCHEDDIR = scheduler
PROCDIR  = process
MEMDIR   = memory
INTDIR   = interrupts
ASMDIR   = asm
DEPSDIR  = deps/cJSON

SRCS = $(SRCDIR)/kernel.c $(SRCDIR)/kernel_memory.c \
       $(CPUDIR)/alu.c $(CPUDIR)/clock.c $(CPUDIR)/cpu.c \
       $(SCHEDDIR)/scheduler.c \
       $(PROCDIR)/context_switch.c $(PROCDIR)/pid_gen.c \
       $(PROCDIR)/process_manager.c $(PROCDIR)/queues.c \
       $(MEMDIR)/alloc_strategy.c $(MEMDIR)/frame_table.c \
       $(MEMDIR)/memory_manager.c $(MEMDIR)/mm_types.c \
       $(MEMDIR)/page_table.c $(MEMDIR)/replace_policy.c $(MEMDIR)/swap.c \
       $(INTDIR)/ic.c $(INTDIR)/isr.c $(INTDIR)/error_manager.c \
       $(DEPSDIR)/cJSON.c

ASM_SRC = $(ASMDIR)/context_switch.S
ASM_OBJ = $(ASMDIR)/context_switch.o

OBJS = $(SRCS:.c=.o) $(ASM_OBJ)

.PHONY: all clean test

all: $(TARGET)

ifeq ($(UNAME_S),Linux)
$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LDFLAGS)
else
$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LDFLAGS) -Wl,--out-implib,$(IMPLIB)
endif

%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(DEPSDIR)/cJSON.o: $(DEPSDIR)/cJSON.c
	$(CC) $(CFLAGS) -w -I$(DEPSDIR) -c $< -o $@

ifeq ($(UNAME_S),Linux)
$(ASM_OBJ): $(ASM_SRC)
	$(NASM) -f $(NASM_FMT) -D__linux__ -o $@ $<
else
$(ASM_OBJ): $(ASM_SRC)
	$(NASM) -f $(NASM_FMT) -o $@ $<
endif

test: $(TARGET) $(TESTBINS)
	@for t in $(TESTBINS); do ./$$t; done

test_cpu test_cpu.exe: tests/test_cpu.c $(OBJS)
	$(CC) $(CFLAGS) -I. tests/test_cpu.c $(OBJS) $(LDFLAGS) -o $@

test_process test_process.exe: tests/test_process.c $(OBJS)
	$(CC) $(CFLAGS) -I. tests/test_process.c $(OBJS) $(LDFLAGS) -o $@

test_memory test_memory.exe: tests/test_memory.c $(OBJS)
	$(CC) $(CFLAGS) -I. tests/test_memory.c $(OBJS) $(LDFLAGS) -o $@

test_interrupts test_interrupts.exe: tests/test_interrupts.c $(OBJS)
	$(CC) $(CFLAGS) -I. tests/test_interrupts.c $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(IMPLIB) $(TESTBINS)