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
TEST_DEPS_RUNNER := $(BUILD_DIR)/test-deps
TEST_CORE_RUNNER := $(BUILD_DIR)/test-core

.PHONY: all run test clean dirs

all: $(APP)

dirs:
	mkdir -p $(BUILD_DIR)

SRC_FILES := $(wildcard src/*.c)
TEST_SRC := $(filter-out src/main.c src/app.c, $(SRC_FILES))

$(APP): $(SRC_FILES) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_FILES) -o $(APP) $(LIBS) $(RPATH)

$(TEST_DEPS_RUNNER): tests/test_deps.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_deps.c -o $(TEST_DEPS_RUNNER) -L$(VENDOR_DIR)/lib64 -lcmocka -lSDL3 -lSDL3_mixer -lenet -lm $(RPATH)

$(TEST_CORE_RUNNER): tests/test_core.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_core.c $(TEST_SRC) -o $(TEST_CORE_RUNNER) -L$(VENDOR_DIR)/lib64 -lcmocka -lSDL3 -lSDL3_mixer -lenet -lm $(RPATH)

run: $(APP)
	./$(APP) --mode raycast

run-normal: $(APP)
	./$(APP) --mode normal

run-stress: $(APP)
	./$(APP) --mode stress

test: $(TEST_DEPS_RUNNER) $(TEST_CORE_RUNNER)
	./$(TEST_DEPS_RUNNER)
	./$(TEST_CORE_RUNNER)
	@echo "Note: benchmark and stability require a video environment to fully run."

benchmark: $(APP)
	./$(APP) --benchmark-stress 5

stability: $(APP)
	./$(APP) --stability-test 30

clean:
	rm -rf $(BUILD_DIR)
