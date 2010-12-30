SOURCES := $(wildcard *.c *.h)
TARGETS := ybinlogp

prefix := /usr

CFLAGS += -Wall -ggdb -Wextra --std=c99
#CFLAGS=-Wall -ggdb -Wextra -DDEBUG

all: $(TARGETS)

ybinlogp: ybinlogp.o

#install: all
#	install -m 755 -o root -g root ybinlogp $(prefix)/sbin/ybinlogp

clean::
	rm -f $(TARGETS) *.o

ybinlogp.o: ybinlogp.c ybinlogp.h
