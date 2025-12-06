# CECS 326 Lab 2 - Simple Makefile

all:
	gcc -Wall -Wextra -std=c11 barbarian.c -o barbarian -pthread -lm -lrt
	gcc -Wall -Wextra -std=c11 wizard.c -o wizard -pthread -lm -lrt
	gcc -Wall -Wextra -std=c11 rogue.c -o rogue -pthread -lm -lrt
	gcc -Wall -Wextra -std=c11 game.c dungeon_ARM64.o -o game -pthread -lm -lrt

clean:
	rm -f barbarian wizard rogue game

