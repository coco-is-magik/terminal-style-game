CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
BUILD_DIR := build

# SMC integration toggle.  Set USE_SMC=1 to build with generated dispatch code.
USE_SMC ?= 0

# Lighting cache toggle.  Set USE_LIGHTING_CACHE=1 to enable lighting shadow ray cache.
USE_LIGHTING_CACHE ?= 0

# Glyph cache toggle.  Set USE_GLYPH_CACHE=1 to enable glyph block caching.
USE_GLYPH_CACHE ?= 0

# Dirty cell tracking toggle.  Set USE_DIRTY_CELLS=1 for renderer-specific custom dirty-cell tracking (reference path).
USE_DIRTY_CELLS ?= 0

# SMC state tracker toggle.  Set USE_SMC_STATE_TRACKER=1 for SMC's general-purpose hash-based API (diagnostic/general-purpose use).
USE_SMC_STATE_TRACKER ?= 0

# SMC indexed state tracker toggle.  Set USE_SMC_INDEXED_STATE_TRACKER=1 for SMC per-cell indexed API (diagnostic mode).
USE_SMC_INDEXED_STATE_TRACKER ?= 0

# SMC batch state tracker toggle.  Set USE_SMC_BATCH_STATE_TRACKER=1 for SMC batch indexed mode (fallback/comparator).
USE_SMC_BATCH_STATE_TRACKER ?= 0

# SMC stream state tracker toggle.  Set USE_SMC_STREAM_STATE_TRACKER=1 for preferred SMC renderer stream mode.
USE_SMC_STREAM_STATE_TRACKER ?= 0

# Frame profiling toggle.  Set PROFILE_FRAME=1 to compile in per-frame phase timing.
PROFILE_FRAME ?= 0

# SMC-specific CFLAGS for disabling optimizations in vendored SMC sources
SMC_CFLAGS ?=

# Mutual exclusivity check - only one dirty/state tracking mode allowed
ifneq ($(shell expr $(USE_DIRTY_CELLS) + $(USE_SMC_STATE_TRACKER) + $(USE_SMC_INDEXED_STATE_TRACKER) + $(USE_SMC_BATCH_STATE_TRACKER) + $(USE_SMC_STREAM_STATE_TRACKER)),0)
  ifneq ($(shell expr $(USE_DIRTY_CELLS) + $(USE_SMC_STATE_TRACKER) + $(USE_SMC_INDEXED_STATE_TRACKER) + $(USE_SMC_BATCH_STATE_TRACKER) + $(USE_SMC_STREAM_STATE_TRACKER)),1)
    $(error Only one of USE_DIRTY_CELLS, USE_SMC_STATE_TRACKER, USE_SMC_INDEXED_STATE_TRACKER, USE_SMC_BATCH_STATE_TRACKER, USE_SMC_STREAM_STATE_TRACKER may be set)
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

ifeq ($(USE_SMC_STREAM_STATE_TRACKER),1)
  SMC_DIR := vendor/src/smc
  SMC_INCLUDES := -I"$(SMC_DIR)/include" -I"$(SMC_DIR)/src/c"
  SMC_STREAM_DEFS := -DUSE_SMC_STREAM_STATE_TRACKER=1
  SMC_STREAM_LIBS := -lm
  SMC_STREAM_FILES := $(SMC_DIR)/src/c/smc_runtime_stub.c $(SMC_DIR)/src/c/smc_artifact.c $(SMC_DIR)/src/c/smc_state.c
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
TEST_UI_ELE_RUNNER           := $(BUILD_DIR)/test-ui-ele
TEST_SCENE_DOCUMENT_RUNNER   := $(BUILD_DIR)/test-scene-document
TEST_COMMAND_SYSTEM_RUNNER   := $(BUILD_DIR)/test-command-system
TEST_EDITOR_SELECTION_RUNNER := $(BUILD_DIR)/test-editor-selection
TEST_UNIFIED_EDITOR_RUNNER   := $(BUILD_DIR)/test-unified-editor




.PHONY: all run test clean dirs benchmark-raycast


all: $(APP)

benchmark-raycast: $(APP)
	./$(APP) --benchmark-raycast 5

dirs:
	mkdir -p $(BUILD_DIR)

SRC_FILES := $(wildcard src/*.c)

# ---------------------------------------------------------------------------
# Per-test production source groups
# Each test links only the modules it needs so new editor units are not
# pulled into every existing runner automatically.
# ---------------------------------------------------------------------------

SRC_CONFIG        := src/config.c
SRC_GRID          := src/grid.c
SRC_MATH          := src/math.c
SRC_MAP           := src/map.c
SRC_ASSETS        := src/assets.c
SRC_WORLD         := src/world.c
SRC_INPUT         := src/input.c
SRC_CAMERA        := src/camera.c
SRC_MAP_LOADER    := src/map_loader.c
SRC_ASSET_LOADER  := src/asset_loader.c
SRC_DECAL_IO      := src/decal_io.c
SRC_UI_ELE        := src/ui_ele.c
SRC_MENU_STATE    := src/menu_state.c
SRC_SCALE         := src/scale.c
SRC_TIMING        := src/timing.c
SRC_RAYCAST       := src/raycast.c src/smc_render_opt.c
SRC_LIGHTING      := src/lighting.c
SRC_RENDERER      := src/renderer.c src/glyph_atlas.c
SRC_SCENE_DOCUMENT := src/scene_document.c
SRC_COMMAND_SYSTEM := src/command_system.c
SRC_EDITOR_SELECTION := src/editor_selection.c
SRC_UNIFIED_EDITOR := src/unified_editor.c




# Optional modules required only when the matching feature flag is enabled.

ifeq ($(USE_LIGHTING_CACHE),1)
  SRC_LIGHTING += src/lighting_cache.c
endif

ifeq ($(USE_GLYPH_CACHE),1)
  SRC_RENDERER += src/glyph_block_cache.c
endif

ifeq ($(USE_SMC_STATE_TRACKER),1)
  SRC_RENDERER += src/smc_state_tracker.c
endif

ifneq ($(filter 1,$(USE_SMC_INDEXED_STATE_TRACKER) $(USE_SMC_BATCH_STATE_TRACKER) $(USE_SMC_STREAM_STATE_TRACKER)),)
  SRC_RENDERER += src/smc_indexed_state_tracker.c
endif

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
SMC_ALL_STATE_FILES := $(SMC_STATE_FILES) $(SMC_INDEXED_FILES) $(SMC_BATCH_FILES) $(SMC_STREAM_FILES)
SMC_ALL_STATE_FILES := $(SMC_ALL_STATE_FILES)  # Remove duplicates
SMC_STATE_FILES_ACTIVE :=
SMC_INCLUDES_ACTIVE :=
SMC_DEFS_ACTIVE :=
SMC_CFLAGS_ACTIVE :=

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

ifeq ($(USE_SMC_STREAM_STATE_TRACKER),1)
    SMC_STATE_FILES_ACTIVE := $(SMC_STREAM_FILES)
    SMC_INCLUDES_ACTIVE := $(SMC_INCLUDES)
    SMC_DEFS_ACTIVE := $(SMC_STREAM_DEFS)
    SMC_LIBS_ACTIVE := $(SMC_STREAM_LIBS)
    SMC_CFLAGS_ACTIVE := $(SMC_CFLAGS)
endif

# Shared flag/include/lib bundles for tests that compile renderer/lighting/raycast.
# Defined after feature-flag variables so expansion sees final values.
TEST_FEATURE_DEFS := $(SMC_DEFS) $(SMC_DEFS_ACTIVE) $(LIGHTING_DEFS) $(GLYPH_DEFS) $(DIRTY_DEFS) $(PROFILE_DEFS)
TEST_FEATURE_INCLUDES := $(INCLUDES) $(SMC_INCLUDES) $(SMC_INCLUDES_ACTIVE)
TEST_FEATURE_LIBS := $(TEST_LIBS) $(SMC_LIBS) $(SMC_LIBS_ACTIVE)
TEST_FEATURE_CFLAGS := $(SMC_CFLAGS_ACTIVE)
TEST_FEATURE_EXTRA_SRC := $(SMC_STATE_FILES_ACTIVE)
ifeq ($(USE_SMC),1)
  TEST_FEATURE_EXTRA_SRC += $(SMC_FILES)
endif

TEST_MENU_STATE_SRC := \
	$(SRC_MENU_STATE) \
	$(SRC_CONFIG)

TEST_UI_ELE_SRC := \
	$(SRC_UI_ELE) \
	$(SRC_GRID)

TEST_DECAL_IO_SRC := \
	$(SRC_DECAL_IO) \
	$(SRC_ASSETS) \
	$(SRC_WORLD) \
	$(SRC_MAP) \
	$(SRC_ASSET_LOADER) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG)

TEST_CORE_SRC := \
	$(SRC_GRID) \
	$(SRC_SCALE) \
	$(SRC_TIMING) \
	$(SRC_RENDERER) \
	$(SRC_MATH) \
	$(SRC_MAP) \
	$(SRC_CAMERA) \
	$(SRC_RAYCAST) \
	$(SRC_LIGHTING) \
	$(SRC_CONFIG) \
	$(SRC_ASSET_LOADER) \
	$(SRC_MAP_LOADER) \
	$(SRC_ASSETS) \
	$(SRC_WORLD) \
	$(SRC_INPUT)

TEST_DECALS_SRC := \
	$(SRC_GRID) \
	$(SRC_MAP) \
	$(SRC_CAMERA) \
	$(SRC_RAYCAST) \
	$(SRC_ASSETS) \
	$(SRC_WORLD) \
	$(SRC_LIGHTING) \
	$(SRC_CONFIG) \
	$(SRC_ASSET_LOADER) \
	$(SRC_MAP_LOADER) \
	$(SRC_MATH) \
	$(SRC_INPUT)

TEST_SCENE_DOCUMENT_SRC := \
	$(SRC_SCENE_DOCUMENT) \
	$(SRC_MAP) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG)

TEST_COMMAND_SYSTEM_SRC := \
	$(SRC_COMMAND_SYSTEM) \
	$(SRC_SCENE_DOCUMENT) \
	$(SRC_MAP) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG)

TEST_EDITOR_SELECTION_SRC := \
	$(SRC_EDITOR_SELECTION) \
	$(SRC_RAYCAST) \
	$(SRC_CAMERA) \
	$(SRC_MAP) \
	$(SRC_CONFIG) \
	$(SRC_MATH) \
	$(SRC_ASSETS) \
	$(SRC_WORLD) \
	$(SRC_GRID) \
	$(SRC_INPUT)

TEST_UNIFIED_EDITOR_SRC := \
	$(SRC_UNIFIED_EDITOR) \
	$(SRC_COMMAND_SYSTEM) \
	$(SRC_SCENE_DOCUMENT) \
	$(SRC_EDITOR_SELECTION) \
	$(SRC_RAYCAST) \
	$(SRC_CAMERA) \
	$(SRC_MAP) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG) \
	$(SRC_MATH) \
	$(SRC_ASSETS) \
	$(SRC_WORLD) \
	$(SRC_GRID) \
	$(SRC_INPUT)



$(APP): $(SRC_FILES) $(SMC_STATE_FILES_ACTIVE) $(SMC_SRC) | dirs


	$(CC) $(CFLAGS) $(SMC_DEFS_ACTIVE) $(LIGHTING_DEFS) $(GLYPH_DEFS) $(DIRTY_DEFS) $(PROFILE_DEFS) $(SMC_CFLAGS_ACTIVE) $(INCLUDES) $(SMC_INCLUDES_ACTIVE) $(SRC_FILES) $(SMC_STATE_FILES_ACTIVE) -o $(APP) $(LIBS) $(SMC_LIBS_ACTIVE) $(RPATH)


$(TEST_DEPS_RUNNER): tests/test_deps.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_deps.c -o $(TEST_DEPS_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_CORE_RUNNER): tests/test_core.c $(TEST_CORE_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_core.c $(TEST_CORE_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_CORE_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_DECALS_RUNNER): tests/test_decals.c $(TEST_DECALS_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_decals.c $(TEST_DECALS_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_DECALS_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_MENU_STATE_RUNNER): tests/test_menu_state.c $(TEST_MENU_STATE_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_menu_state.c $(TEST_MENU_STATE_SRC) \
		-o $(TEST_MENU_STATE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_DECAL_IO_RUNNER): tests/test_decal_io.c $(TEST_DECAL_IO_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_decal_io.c $(TEST_DECAL_IO_SRC) \
		-o $(TEST_DECAL_IO_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_ELE_RUNNER): tests/test_ui_ele.c $(TEST_UI_ELE_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_ele.c $(TEST_UI_ELE_SRC) \
		-o $(TEST_UI_ELE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SCENE_DOCUMENT_RUNNER): tests/test_scene_document.c $(TEST_SCENE_DOCUMENT_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_scene_document.c $(TEST_SCENE_DOCUMENT_SRC) \
		-o $(TEST_SCENE_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_COMMAND_SYSTEM_RUNNER): tests/test_command_system.c $(TEST_COMMAND_SYSTEM_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_command_system.c $(TEST_COMMAND_SYSTEM_SRC) \
		-o $(TEST_COMMAND_SYSTEM_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_EDITOR_SELECTION_RUNNER): tests/test_editor_selection.c $(TEST_EDITOR_SELECTION_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_editor_selection.c $(TEST_EDITOR_SELECTION_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_EDITOR_SELECTION_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_UNIFIED_EDITOR_RUNNER): tests/test_unified_editor.c $(TEST_UNIFIED_EDITOR_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_unified_editor.c $(TEST_UNIFIED_EDITOR_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_UNIFIED_EDITOR_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)



run: $(APP)



	./$(APP) --mode raycast

run-normal: $(APP)
	./$(APP) --mode normal

run-stress: $(APP)
	./$(APP) --mode stress

test: $(TEST_DEPS_RUNNER) $(TEST_CORE_RUNNER) $(TEST_DECALS_RUNNER) $(TEST_MENU_STATE_RUNNER) $(TEST_DECAL_IO_RUNNER) $(TEST_UI_ELE_RUNNER) $(TEST_SCENE_DOCUMENT_RUNNER) $(TEST_COMMAND_SYSTEM_RUNNER) $(TEST_EDITOR_SELECTION_RUNNER) $(TEST_UNIFIED_EDITOR_RUNNER)
	./$(TEST_DEPS_RUNNER)
	./$(TEST_CORE_RUNNER)
	./$(TEST_DECALS_RUNNER)
	./$(TEST_MENU_STATE_RUNNER)
	./$(TEST_DECAL_IO_RUNNER)
	./$(TEST_UI_ELE_RUNNER)
	./$(TEST_SCENE_DOCUMENT_RUNNER)
	./$(TEST_COMMAND_SYSTEM_RUNNER)
	./$(TEST_EDITOR_SELECTION_RUNNER)
	./$(TEST_UNIFIED_EDITOR_RUNNER)
	@echo "Note: benchmark and stability require a video environment to fully run."





benchmark: $(APP)
	./$(APP) --benchmark-stress 5

stability: $(APP)
	./$(APP) --stability-test 30

clean:
	rm -rf $(BUILD_DIR)
