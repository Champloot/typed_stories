CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -MMD -MP
LDLIBS  = -lncurses

SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)

game: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

run: game
	./game

# quick: broken pack files must be rejected with a clear message
test-quick: game
	python3 tests/loader_test.py

# full: plays whole days through a terminal emulator (needs python3 + pyte)
test: game
	python3 tests/loader_test.py
	python3 tests/play_test.py

clean:
	rm -rf build game

-include $(OBJ:.o=.d)

.PHONY: run test test-quick clean
