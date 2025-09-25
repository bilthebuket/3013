CC = gcc
CFLAGS = -Wall -Wextra -pthread
OBJ = mailbox.o

all: addem life

addem: addem.o $(OBJ)
	$(CC) $^ -o $@

life: life.o $(OBJ)
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o addem life
