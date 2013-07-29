VPATH := src
SOURCES := $(wildcard *.c *.h)
TARGETS := build/libybinlogp.so.1 build/libybinlogp.so build/ybinlogp

prefix := /usr

CFLAGS += -Wall -ggdb -Wextra --std=c99 -pedantic
LDFLAGS += -L.
#CFLAGS += -DDEBUG

all: $(TARGETS)

build/ybinlogp: build/ybinlogp.o build/libybinlogp.so
	gcc $(CFLAGS) $(LDFLAGS) -o $@ -lybinlogp $<

build/libybinlogp.so: build/libybinlogp.so.1
	ln -fs $< $@

build/libybinlogp.so.1: build/libybinlogp.o
	gcc $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^

build/libybinlogp.o: libybinlogp.c ybinlogp-private.h
	gcc $(CFLAGS) $(LDFLAGS) -c -fPIC -o $@ $<

clean:
	rm -f build/*
	find . -iname '*.pyc' -delete

build/ybinlogp.o: ybinlogp.c ybinlogp.h
	cc $(CFLAGS) $(LDFLAGS) -c -o $@ $<
