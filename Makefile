CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDLIBS  = -lncurses

game: src/main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

run: game
	./game

clean:
	rm -f game

.PHONY: run clean
