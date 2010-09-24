SOURCES := $(wildcard *.c *.h)
TARGETS := binlogp

CFLAGS=-Wall -ggdb -Wextra

all: $(TARGETS)

binlogp: binlogp.o

force:: clean all

clean::
	rm -f $(TARGETS) *.o

binlogp.o: binlogp.c binlogp.h
