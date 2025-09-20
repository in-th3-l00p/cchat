CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC = src
BIN = bin
TARGET = server

.PHONY: all clean

all: $(BIN)/$(TARGET)

$(BIN)/$(TARGET): $(SRC)/main.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)

