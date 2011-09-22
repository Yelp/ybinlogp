SOURCES := $(wildcard *.c *.h)
TARGETS := ybinlogp ybinlogp.so.1 ybinlogp.so ybinlogp_test

prefix := /usr

CFLAGS += -Wall -ggdb -Wextra --std=c99 -pedantic
LDFLAGS += -L.
#CFLAGS=-Wall -ggdb -Wextra -DDEBUG

all: $(TARGETS)

ybinlogp_test: ybinlogp_test.o libybinlogp.so.1
	gcc $(CFLAGS) $(LDFLAGS) -o $@ -lybinlogp $<

libybinlogp.so: libybinlogp.so.1
	ln -s $< $@

libybinlogp.so.1: libybinlogp.o
	gcc $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^

libybinlogp.o: libybinlogp.c
	gcc $(CFLAGS) $(LDFLAGS) -c -fPIC -o $@ $^

#install: all
#	install -m 755 -o root -g root ybinlogp $(prefix)/sbin/ybinlogp

clean::
	rm -f $(TARGETS) *.o

ybinlogp.o: ybinlogp.c ybinlogp.h
