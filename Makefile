CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -MMD -MP

# raylib: from pkg-config (the Nix dev shell provides it), or pass both on the
# command line: make RAYLIB_CFLAGS=-I/path/to/raylib/src RAYLIB_LIBS="/path/libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11"
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   ?= $(shell pkg-config --libs raylib 2>/dev/null || echo -lraylib -lGL -lm -lpthread -ldl -lrt -lX11)

# everything except the screen backends: the terminal one, and the window one
# (ui_gl.c and its helpers, every src/*_gl.c)
GL      = $(wildcard src/*_gl.c)
CORE    = $(filter-out src/ui_tty.c $(GL), $(wildcard src/*.c))
CORE_O  = $(CORE:src/%.c=build/%.o)
GL_O    = $(GL:src/%.c=build/%.o)

all: game game-tty

# the game in a window (raylib)
game: $(CORE_O) $(GL_O)
	$(CC) $(CFLAGS) -o $@ $^ $(RAYLIB_LIBS) -lm

# the same game in a terminal (ncurses): for development and the tests
game-tty: $(CORE_O) build/ui_tty.o
	$(CC) $(CFLAGS) -o $@ $^ -lncurses -lm

build/%_gl.o: src/%_gl.c | build
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -c -o $@ $<

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

run: game
	./game

run-tty: game-tty
	./game-tty

# quick: broken pack files must be rejected with a clear message
test-quick: game-tty
	python3 tests/loader_test.py

# full: plays whole days through a terminal emulator (needs python3 + pyte)
test: game-tty
	python3 tests/loader_test.py
	python3 tests/play_test.py

clean:
	rm -rf build game game-tty

-include $(wildcard build/*.d)

.PHONY: all run run-tty test test-quick clean
