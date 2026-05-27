CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -pedantic
BUILD   := build
TARGET  := $(BUILD)/greeter-launcher
SRC     := greeter-launcher.c

.PHONY: all clean

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD)
