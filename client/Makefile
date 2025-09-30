CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC = src
BIN = bin
TARGET = client
SRCS = $(wildcard $(SRC)/*.c)

.PHONY: all clean

all: $(BIN)/$(TARGET)

$(BIN)/$(TARGET): $(SRCS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)

