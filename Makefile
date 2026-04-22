# Makefile — Custom Memory Allocator
CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11
TARGET  = allocator
SRCS    = main.c allocator.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run with valgrind to check for leaks (Linux)
valgrind: $(TARGET)
	valgrind --leak-check=full --track-origins=yes ./$(TARGET)

# Run with GDB for debugging
debug: $(TARGET)
	gdb ./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean valgrind debug
