CC = ia16-elf-gcc
CFLAGS = -O2 -mcmodel=small -march=v30

TARGET = build/ADV98.EXE
SRCS = src/graph98.c src/fm86.c src/pmd.c src/mouse98.c src/text98.c src/title.c src/save.c src/game_flags.c src/script.c src/debug.c src/menu.c src/input.c src/main.c

EGC_TEST_TARGET = build/EGCTEST.EXE
EGC_TEST_SRCS = src/egc98.c tests/egc_test.c
EGC_TEST_ASM = build/egc98.s build/egc_test.s

.PHONY: all egc-test egc-test-asm clean

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

egc-test:
	$(CC) $(CFLAGS) -Isrc $(EGC_TEST_SRCS) -o $(EGC_TEST_TARGET)

egc-test-asm:
	$(CC) $(CFLAGS) -Isrc -S src/egc98.c -o build/egc98.s
	$(CC) $(CFLAGS) -Isrc -S tests/egc_test.c -o build/egc_test.s

clean:
	rm -f $(TARGET) $(EGC_TEST_TARGET) $(EGC_TEST_ASM)
