CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
BUILD_DIR := build

# SMC integration toggle.  Set USE_SMC=1 to build with generated dispatch code.
USE_SMC ?= 0

# Lighting cache toggle.  Set USE_LIGHTING_CACHE=1 to enable lighting shadow ray cache.
USE_LIGHTING_CACHE ?= 0

# Glyph cache toggle.  Set USE_GLYPH_CACHE=1 to enable glyph block caching.
USE_GLYPH_CACHE ?= 0

# Dirty cell tracking toggle.  Set USE_DIRTY_CELLS=1 to skip unchanged cells.
USE_DIRTY_CELLS ?= 0

# SMC state tracker toggle.  Set USE_SMC_STATE_TRACKER=1 to use SMC's generic dirty-state API.
USE_SMC_STATE_TRACKER ?= 0

# SMC indexed state tracker toggle.  Set USE_SMC_INDEXED_STATE_TRACKER=1 for per-cell indexed state tracking.
USE_SMC_INDEXED_STATE_TRACKER ?= 0

# SMC batch state tracker toggle.  Set USE_SMC_BATCH_STATE_TRACKER=1 for batch indexed state tracking.
USE_SMC_BATCH_STATE_TRACKER ?= 0

# Frame profiling toggle.  Set PROFILE_FRAME=1 to compile in per-frame phase timing.
PROFILE_FRAME ?= 0

# Mutual exclusivity check - only one dirty/state tracking mode allowed
ifneq ($(shell expr $(USE_DIRTY_CELLS) + $(USE_SMC_STATE_TRACKER) + $(USE_SMC_INDEXED_STATE_TRACKER) + $(USE_SMC_BATCH_STATE_TRACKER)),0)
  ifneq ($(shell expr $(USE_DIRTY_CELLS) + $(USE_SMC_STATE_TRACKER) + $(USE_SMC_INDEXED_STATE_TRACKER) + $(USE_SMC_BATCH_STATE_TRACKER)),1)
    $(error Only one of USE_DIRTY_CELLS, USE_SMC_STATE_TRACKER, USE_SMC_INDEXED_STATE_TRACKER, USE_SMC_BATCH_STATE_TRACKER may be set)
  endif
endif

#VENDOR_DIR := $(pwd)/vendor
# Note: $(pwd) might not work in some makes, better use $(shell pwd)
VENDOR_DIR := $(shell pwd)/vendor/dist

INCLUDES := -I"$(VENDOR_DIR)/include" -I"$(shell pwd)/vendor/src/SDL/include"
LIBS := -L"$(VENDOR_DIR)/lib64" -lSDL3 -lSDL3_mixer -lenet -lm
TEST_LIBS := -L"$(VENDOR_DIR)/lib64" -lcmocka -lSDL3 -lSDL3_mixer -lenet -lm
RPATH := -Wl,-rpath,'$$ORIGIN/../vendor/dist/lib64'

ifeq ($(USE_SMC_STATE_TRACKER),1)
  SMC_DIR := vendor/src/smc
  SMC_INCLUDES := -I"$(SMC_DIR)/include" -I"$(SMC_DIR)/src/c"
  SMC_STATE_DEFS := -DUSE_SMC_STATE_TRACKER=1
  SMC_STATE_LIBS := -lm
  SMC_STATE_FILES := $(SMC_DIR)/src/c/smc_runtime_stub.c $(SMC_DIR)/src/c/smc_artifact.c $(SMC_DIR)/src/c/smc_state.c
endif

ifeq ($(USE_SMC_INDEXED_STATE_TRACKER),1)
  SMC_DIR := vendor/src/smc
  SMC_INCLUDES := -I"$(SMC_DIR)/include" -I"$(SMC_DIR)/src/c"
  SMC_INDEXED_DEFS := -DUSE_SMC_INDEXED_STATE_TRACKER=1
  SMC_INDEXED_LIBS := -lm
  SMC_INDEXED_FILES := $(SMC_DIR)/src/c/smc_runtime_stub.c $(SMC_DIR)/src/c/smc_artifact.c $(SMC_DIR)/src/c/smc_state.c
endif

ifeq ($(USE_SMC_BATCH_STATE_TRACKER),1)
  SMC_DIR := vendor/src/smc
  SMC_INCLUDES := -I"$(SMC_DIR)/include" -I"$(SMC_DIR)/src/c"
  SMC_BATCH_DEFS := -DUSE_SMC_BATCH_STATE_TRACKER=1
  SMC_BATCH_LIBS := -lm
  SMC_BATCH_FILES := $(SMC_DIR)/src/c/smc_runtime_stub.c $(SMC_DIR)/src/c/smc_artifact.c $(SMC_DIR)/src/c/smc_state.c
endif

ifeq ($(USE_SMC),1)
  SMC_DIR := vendor/src/smc
  SMC_SRC := $(BUILD_DIR)/smc_generated.c
  SMC_INCLUDES := -I"$(SMC_DIR)/include" -I"$(SMC_DIR)/src/c" -I"$(BUILD_DIR)"
  SMC_DEFS := -DUSE_SMC=1
  SMC_LIBS := -lm
  SMC_FILES := $(SMC_DIR)/src/c/smc_runtime_stub.c $(SMC_DIR)/src/c/smc_artifact.c $(SMC_DIR)/src/c/smc_state.c $(SMC_DIR)/src/c/smc_generated_runtime.c
endif

APP := $(BUILD_DIR)/ascii-fps
TEST_DEPS_RUNNER := $(BUILD_DIR)/test-deps
TEST_CORE_RUNNER := $(BUILD_DIR)/test-core
TEST_DECALS_RUNNER := $(BUILD_DIR)/test-decals
TEST_MENU_STATE_RUNNER := $(BUILD_DIR)/test-menu-state
TEST_DECAL_IO_RUNNER         := $(BUILD_DIR)/test-decal-io
TEST_ASSET_DESIGNER_RUNNER   := $(BUILD_DIR)/test-asset-designer
TEST_LIVE_EDITOR_RUNNER      := $(BUILD_DIR)/test-live-editor
TEST_UI_ELE_RUNNER           := $(BUILD_DIR)/test-ui-ele

.PHONY: all run test clean dirs benchmark-raycast

all: $(APP)

benchmark-raycast: $(APP)
	./$(APP) --benchmark-raycast 5

dirs:
	mkdir -p $(BUILD_DIR)

SRC_FILES := $(wildcard src/*.c)
TEST_SRC := $(filter-out src/main.c src/app.c, $(SRC_FILES))

ifeq ($(USE_SMC),1)
$(SMC_SRC): scripts/generate-smc-renderer.lisp | dirs
	sbcl --script scripts/generate-smc-renderer.lisp $(SMC_SRC)
endif

# Lighting cache definitions
LIGHTING_DEFS :=
ifeq ($(USE_LIGHTING_CACHE),1)
    LIGHTING_DEFS := -DUSE_LIGHTING_CACHE=1
endif

# Glyph block cache definitions
GLYPH_DEFS :=
ifeq ($(USE_GLYPH_CACHE),1)
    GLYPH_DEFS := -DUSE_GLYPH_CACHE=1
endif

# Dirty cell tracking definitions
DIRTY_DEFS :=
ifeq ($(USE_DIRTY_CELLS),1)
    DIRTY_DEFS := -DUSE_DIRTY_CELLS=1
endif

# Frame profiling definitions
PROFILE_DEFS :=
ifeq ($(PROFILE_FRAME),1)
    PROFILE_DEFS := -DPROFILE_FRAME=1
endif

# SMC state tracker files - combine all possible modes
SMC_ALL_STATE_FILES := $(SMC_STATE_FILES) $(SMC_INDEXED_FILES) $(SMC_BATCH_FILES)
SMC_ALL_STATE_FILES := $(SMC_ALL_STATE_FILES)  # Remove duplicates
SMC_STATE_FILES_ACTIVE :=
SMC_INCLUDES_ACTIVE :=
SMC_DEFS_ACTIVE :=

ifeq ($(USE_SMC_STATE_TRACKER),1)
    SMC_STATE_FILES_ACTIVE := $(SMC_STATE_FILES)
    SMC_INCLUDES_ACTIVE := $(SMC_INCLUDES)
    SMC_DEFS_ACTIVE := $(SMC_STATE_DEFS)
    SMC_LIBS_ACTIVE := $(SMC_STATE_LIBS)
endif

ifeq ($(USE_SMC_INDEXED_STATE_TRACKER),1)
    SMC_STATE_FILES_ACTIVE := $(SMC_INDEXED_FILES)
    SMC_INCLUDES_ACTIVE := $(SMC_INCLUDES)
    SMC_DEFS_ACTIVE := $(SMC_INDEXED_DEFS)
    SMC_LIBS_ACTIVE := $(SMC_INDEXED_LIBS)
endif

ifeq ($(USE_SMC_BATCH_STATE_TRACKER),1)
    SMC_STATE_FILES_ACTIVE := $(SMC_BATCH_FILES)
    SMC_INCLUDES_ACTIVE := $(SMC_INCLUDES)
    SMC_DEFS_ACTIVE := $(SMC_BATCH_DEFS)
    SMC_LIBS_ACTIVE := $(SMC_BATCH_LIBS)
endif

$(APP): $(SRC_FILES) $(SMC_STATE_FILES_ACTIVE) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(SMC_DEFS_ACTIVE) $(LIGHTING_DEFS) $(GLYPH_DEFS) $(DIRTY_DEFS) $(PROFILE_DEFS) $(INCLUDES) $(SMC_INCLUDES_ACTIVE) $(SRC_FILES) $(SMC_STATE_FILES_ACTIVE) -o $(APP) $(LIBS) $(SMC_LIBS_ACTIVE) $(RPATH)

$(TEST_DEPS_RUNNER): tests/test_deps.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_deps.c -o $(TEST_DEPS_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_CORE_RUNNER): tests/test_core.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_core.c $(TEST_SRC) -o $(TEST_CORE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_DECALS_RUNNER): tests/test_decals.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_decals.c $(TEST_SRC) -o $(TEST_DECALS_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_MENU_STATE_RUNNER): tests/test_menu_state.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_menu_state.c $(TEST_SRC) -o $(TEST_MENU_STATE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_DECAL_IO_RUNNER): tests/test_decal_io.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_decal_io.c $(TEST_SRC) -o $(TEST_DECAL_IO_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_ASSET_DESIGNER_RUNNER): tests/test_asset_designer.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_asset_designer.c $(TEST_SRC) -o $(TEST_ASSET_DESIGNER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_LIVE_EDITOR_RUNNER): tests/test_live_editor.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_live_editor.c $(TEST_SRC) -o $(TEST_LIVE_EDITOR_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_ELE_RUNNER): tests/test_ui_ele.c $(TEST_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_ele.c $(TEST_SRC) -o $(TEST_UI_ELE_RUNNER) $(TEST_LIBS) $(RPATH)

run: $(APP)
	./$(APP) --mode raycast

run-normal: $(APP)
	./$(APP) --mode normal

run-stress: $(APP)
	./$(APP) --mode stress

test: $(TEST_DEPS_RUNNER) $(TEST_CORE_RUNNER) $(TEST_DECALS_RUNNER) $(TEST_MENU_STATE_RUNNER) $(TEST_DECAL_IO_RUNNER) $(TEST_ASSET_DESIGNER_RUNNER) $(TEST_LIVE_EDITOR_RUNNER) $(TEST_UI_ELE_RUNNER)
	./$(TEST_DEPS_RUNNER)
	./$(TEST_CORE_RUNNER)
	./$(TEST_DECALS_RUNNER)
	./$(TEST_MENU_STATE_RUNNER)
	./$(TEST_DECAL_IO_RUNNER)
	./$(TEST_ASSET_DESIGNER_RUNNER)
	./$(TEST_LIVE_EDITOR_RUNNER)
	./$(TEST_UI_ELE_RUNNER)
	@echo "Note: benchmark and stability require a video environment to fully run."

benchmark: $(APP)
	./$(APP) --benchmark-stress 5

stability: $(APP)
	./$(APP) --stability-test 30

clean:
	rm -rf $(BUILD_DIR)
