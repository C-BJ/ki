
CC=gcc
LCC=gcc

CFLAGS=-g -pthread
LLVMCF=`llvm-config --cflags`
LDFLAGS=-lcurl -lm `llvm-config --ldflags --system-libs --libs all-targets analysis bitreader bitwriter core codegen executionengine instrumentation interpreter ipo irreader linker mc mcjit objcarcopts option profiledata scalaropts support target object transformutils debuginfodwarf` -lm -lz -lstdc++

SRC=$(wildcard src/*.c) $(wildcard src/libs/*.c) $(wildcard src/build/*.c) $(wildcard src/build/LLVM/*.c)
SRC := $(filter-out src/build/stage-8-link.c, $(SRC))
OBJECTS=$(patsubst %.c, debug/build/%.o, $(SRC))
TARGET=ki

ki: ./lib/libs/linux-x64/libki_os.a $(OBJECTS) debug/build/link.o
	$(LCC) $(CFLAGS) -o $@ $(OBJECTS) debug/build/link.o $(LDFLAGS)

debug/build/link.o: src/build/stage-8-link.c
	$(CC) $(CFLAGS) $(LLVMCF) -o debug/build/link.o -c src/build/stage-8-link.c

$(OBJECTS): debug/build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<

./lib/libs/linux-x64/libki_os.a: ./src/os/linux.c
	gcc -g -O3 -c "./src/os/linux.c" -o /tmp/libki_os.o
	ar rcs "./lib/libs/linux-x64/libki_os.a" /tmp/libki_os.o

.PHONY: clean all

clean:
	rm -f ki $(OBJECTS) core

all: ki os_linux

