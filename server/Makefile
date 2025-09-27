CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC = src
BIN = bin
TARGET = server

.PHONY: all clean

all: $(BIN)/$(TARGET)

$(BIN)/$(TARGET): $(SRC)/main.c $(SRC)/networking.c $(SRC)/clients.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRC)/main.c $(SRC)/networking.c $(SRC)/clients.c

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)

