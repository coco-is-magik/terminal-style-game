CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
BUILD_DIR := build

VENDOR_DIR := $(pwd)/vendor/dist
# Note: $(pwd) might not work in some makes, better use $(shell pwd)
VENDOR_DIR := $(shell pwd)/vendor/dist

INCLUDES := -I$(VENDOR_DIR)/include
LIBS := -L$(VENDOR_DIR)/lib64 -lSDL3 -lSDL3_mixer -lenet -lm
RPATH := -Wl,-rpath,'$$ORIGIN/../vendor/dist/lib64'

APP := $(BUILD_DIR)/ascii-fps
TEST_RUNNER := $(BUILD_DIR)/test-runner

.PHONY: all run test clean dirs

all: $(APP)

dirs:
	mkdir -p $(BUILD_DIR)

SRC_FILES := $(wildcard src/*.c)

$(APP): $(SRC_FILES) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_FILES) -o $(APP) $(LIBS) $(RPATH)

$(TEST_RUNNER): tests/test_deps.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_deps.c -o $(TEST_RUNNER) -L$(VENDOR_DIR)/lib64 -lcmocka -lSDL3 -lSDL3_mixer -lenet -lm $(RPATH)

run: $(APP)
	./$(APP)

test: $(TEST_RUNNER)
	./$(TEST_RUNNER)

clean:
	rm -rf $(BUILD_DIR)
