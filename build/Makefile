VPATH := ../src
SOURCES := $(wildcard *.c *.h)
TARGETS := libybinlogp.so.1 libybinlogp.so ybinlogp

prefix := /usr

CC := gcc
CFLAGS += -Wall -ggdb -Wextra --std=c99 -pedantic
LDFLAGS += -L.

# Enable for debugging
debug: CFLAGS += -DDEBUG
debug: all

all: $(TARGETS)

ybinlogp: ybinlogp.o libybinlogp.so
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< -lybinlogp

libybinlogp.so: libybinlogp.so.1
	ln -fs $< $@

libybinlogp.so.1: libybinlogp.o
	gcc $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^

libybinlogp.o: libybinlogp.c ybinlogp-private.h
	gcc $(CFLAGS) $(LDFLAGS) -c -fPIC -o $@ $<

clean:
	rm -f $(TARGETS) *.o

ybinlogp.o: ybinlogp.c ybinlogp.h
