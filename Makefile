CC = ia16-elf-gcc
CFLAGS = -O2 -mcmodel=small -march=v30
USE_EGC ?= 0

TARGET = build/ADV98.EXE
SRCS = src/graph98.c src/fm86.c src/pmd.c src/mouse98.c src/text98.c src/title.c src/save.c src/game_flags.c src/script.c src/debug.c src/menu.c src/input.c src/main.c

ifeq ($(USE_EGC),1)
CPPFLAGS += -DUSE_EGC=1
SRCS += src/egc98.c
endif

EGC_TEST_TARGET = build/EGCTEST.EXE
EGC_TEST_SRCS = src/egc98.c tests/egc_test.c
EGC_TEST_ASM = build/egc98.s build/egc_test.s

EGC_BENCH_TARGET = build/EGCBENCH.EXE
EGC_BENCH_SRCS = src/egc98.c tests/egc_bench.c

.PHONY: all egc-test egc-test-asm egc-bench clean

all:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRCS) -o $(TARGET)

egc-test:
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc $(EGC_TEST_SRCS) -o $(EGC_TEST_TARGET)

egc-test-asm:
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -S src/egc98.c -o build/egc98.s
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -S tests/egc_test.c -o build/egc_test.s

egc-bench:
	$(CC) $(CFLAGS) -Isrc $(EGC_BENCH_SRCS) -o $(EGC_BENCH_TARGET)

clean:
	rm -f $(TARGET) build/ADV98.EXE build/ADV98D.EXE build/ADV98E.EXE
	rm -f $(EGC_TEST_TARGET) $(EGC_TEST_ASM) $(EGC_BENCH_TARGET)
