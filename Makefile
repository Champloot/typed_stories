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

# ---- Windows: game.exe, cross-compiled with mingw-w64 (no Windows needed) ----
#   make raylib-win     once: downloads raylib's prebuilt Windows release
#   make windows        builds game.exe
#   make dist-windows   game.exe + packs + assets in typed_stories-windows.zip
WIN_CC     ?= x86_64-w64-mingw32-gcc
RAYLIB_VER  = 5.5
RAYLIB_WIN ?= third_party/raylib-$(RAYLIB_VER)_win64_mingw-w64
WIN_O       = $(patsubst src/%.c,build/win/%.o,$(CORE) $(GL))

windows: game.exe

game.exe: $(WIN_O)
	$(WIN_CC) -O2 -o $@ $^ $(RAYLIB_WIN)/lib/libraylib.a -lopengl32 -lgdi32 -lwinmm -static -mwindows

build/win/%.o: src/%.c | build/win
	$(WIN_CC) $(CFLAGS) -I$(RAYLIB_WIN)/include -c -o $@ $<

build/win:
	mkdir -p build/win

raylib-win:
	mkdir -p third_party
	curl -fL -o third_party/raylib-win.zip https://github.com/raysan5/raylib/releases/download/$(RAYLIB_VER)/raylib-$(RAYLIB_VER)_win64_mingw-w64.zip
	cd third_party && unzip -oq raylib-win.zip && rm raylib-win.zip

dist-windows: game.exe
	rm -rf dist/typed_stories && mkdir -p dist/typed_stories
	cp -r game.exe packs assets README.md dist/typed_stories/
	cd dist && rm -f ../typed_stories-windows.zip && zip -qr ../typed_stories-windows.zip typed_stories

.PHONY: windows raylib-win dist-windows

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
	rm -rf build game game-tty game.exe dist typed_stories-windows.zip

-include $(wildcard build/*.d build/win/*.d)

.PHONY: all run run-tty test test-quick clean
