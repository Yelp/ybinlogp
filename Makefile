SOURCES := $(wildcard *.c *.h)
TARGETS := ybinlogp

prefix := /usr

CFLAGS += -Wall -ggdb -Wextra --std=c99
#CFLAGS=-Wall -ggdb -Wextra -DDEBUG

all: $(TARGETS)

py: $(SOURCES) setup.py
	python setup.py build

.PHONY : pytest
pytest: py test.py
	PYTHONPATH=build/lib.linux-x86_64-2.6/ python test.py mysql-bin.000001

testsh: py
	PYTHONPATH=build/lib.linux-x86_64-2.6/ /bin/zsh

ybinlogp: ybinlogp.o

#install: all
#	install -m 755 -o root -g root ybinlogp $(prefix)/sbin/ybinlogp

clean::
	rm -f $(TARGETS) *.o
	rm -rf build

ybinlogp.o: ybinlogp.c ybinlogp.h
