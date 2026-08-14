# kernel/Makefile
#
# Builds libjarvis_kernel (jarvis_kernel.dll) from the C kernel sources.
#
# The kernel must be built as a 64-bit DLL to match the 64-bit Python
# bridge (ctypes). This machine ships two gcc builds:
#   - C:\MinGW\bin\gcc.exe           32-bit MinGW.org GCC 6.3.0  (wrong)
#   - scoop gcc (nuwen, 64-bit)      15.2.0 x86_64-w64-mingw32   (right)
# So CC defaults to the scoop 64-bit gcc unless overridden.
#
# Usage:
#   mingw32-make            # build jarvis_kernel.dll
#   mingw32-make clean      # remove build outputs
#   mingw32-make test       # build + run the C smoke test (if any)

CC      := ../.toolchain/gcc/bin/gcc.exe
CFLAGS  ?= -std=c17 -Wall -Wextra -O2
LDFLAGS ?= -static-libgcc

TARGET   = jarvis_kernel.dll
IMPLIB   = libjarvis_kernel.a

SRCDIR   = core
INCDIR   = .

SRCS = $(SRCDIR)/kernel.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LDFLAGS) -Wl,--out-implib,$(IMPLIB)

$(SRCDIR)/kernel.o: $(SRCDIR)/kernel.c abi.h
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(IMPLIB)