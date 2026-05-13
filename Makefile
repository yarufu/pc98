CC = ia16-elf-gcc
CFLAGS = -O2 -mcmodel=small -march=v30

TARGET = build/test.exe
SRCS = src/graph98.c src/fm86.c src/pmd.c src/main.c

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
	
