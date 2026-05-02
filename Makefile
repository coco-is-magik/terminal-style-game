CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
BUILD_DIR := build

APP := $(BUILD_DIR)/ascii-fps
TEST := $(BUILD_DIR)/test-runner

.PHONY: all run test clean dirs

all: $(APP)

dirs:
	mkdir -p $(BUILD_DIR)

$(APP): src/main.c | dirs
	$(CC) $(CFLAGS) src/main.c -o $(APP)

$(TEST): tests/test.c $(APP) | dirs
	$(CC) $(CFLAGS) tests/test.c -o $(TEST)

run: $(APP)
	./$(APP)

test: $(TEST)
	./$(TEST)

clean:
	rm -rf $(BUILD_DIR)
