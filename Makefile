
CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = myShell
SRC = myShell.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
