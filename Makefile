CC = ia16-elf-gcc
CFLAGS = -O2 -mcmodel=small -march=v30

TARGET = build/ADV98.EXE
SRCS = src/graph98.c src/fm86.c src/pmd.c src/mouse98.c src/text98.c src/title.c src/save.c src/game_flags.c src/script.c src/debug.c src/menu.c src/input.c src/main.c

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
	
