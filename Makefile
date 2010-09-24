SOURCES := $(wildcard *.c *.h)
TARGETS := binlogp

CFLAGS=-Wall -ggdb -Wextra

all: $(TARGETS)

binlogp: binlogp.o


clean::
	rm -f $(TARGETS) *.o

binlogp.o: binlogp.c
