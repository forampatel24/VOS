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

CC      := ../.toolchain/gcc/bin/gcc.exe
NASM    := ../.toolchain/nasm/nasm.exe
CFLAGS  ?= -std=c17 -Wall -Wextra -O2
LDFLAGS ?= -static-libgcc

TARGET   = jarvis_kernel.dll
IMPLIB   = libjarvis_kernel.a
TESTBIN  = test_cpu.exe

SRCDIR   = core
CPUDIR   = cpu
SCHEDDIR = scheduler
ASMDIR   = asm
DEPSDIR  = deps/cJSON

SRCS = $(SRCDIR)/kernel.c \
       $(CPUDIR)/alu.c $(CPUDIR)/clock.c $(CPUDIR)/cpu.c \
       $(SCHEDDIR)/scheduler.c \
       $(DEPSDIR)/cJSON.c

ASM_SRC = $(ASMDIR)/context_switch.S
ASM_OBJ = $(ASMDIR)/context_switch.o

OBJS = $(SRCS:.c=.o) $(ASM_OBJ)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LDFLAGS) -Wl,--out-implib,$(IMPLIB)

%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(DEPSDIR)/cJSON.o: $(DEPSDIR)/cJSON.c
	$(CC) $(CFLAGS) -w -I$(DEPSDIR) -c $< -o $@

$(ASM_OBJ): $(ASM_SRC)
	$(NASM) -f win64 -o $@ $<

test: $(TARGET) $(TESTBIN)
	$(TESTBIN)

$(TESTBIN): tests/test_cpu.c $(OBJS)
	$(CC) $(CFLAGS) -I. tests/test_cpu.c $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(IMPLIB) $(TESTBIN)