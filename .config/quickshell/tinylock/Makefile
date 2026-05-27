CC      ?= clang
CFLAGS  := $(shell pkg-config --cflags wlroots-0.19 wayland-server)
LDFLAGS := $(shell pkg-config --libs wlroots-0.19 wayland-server)

BUILD   := build
TARGET  := $(BUILD)/tinylock
SRC     := tinylock.c

.PHONY: all clean

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(BUILD)
