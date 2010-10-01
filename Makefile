SOURCES := $(wildcard *.c *.h)
TARGETS := ybinlogp

CFLAGS=-Wall -ggdb -Wextra
#CFLAGS=-Wall -ggdb -Wextra -DDEBUG

all: $(TARGETS)

ybinlogp: ybinlogp.o

force:: clean all

clean::
	rm -f $(TARGETS) *.o

ybinlogp.o: ybinlogp.c ybinlogp.h
