

.PHONY: flakes tests clean docs build


all: build

build:
	make -C build all

debug:
	make -C build debug

flakes:
	find -name "*.py" -print0 | xargs -0 pyflakes

test: tests

tests: all
	tox

clean:
	make -C build clean
	find . -iname '*.pyc' -delete

