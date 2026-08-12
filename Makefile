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

# Explicit baseline toggle. Set USE_NO_STATE_TRACKER=1 to disable all
# dirty/state tracking; this is diagnostic and never selected by default.
USE_NO_STATE_TRACKER ?= 0

# SMC stream state tracker toggle. This preferred mode is selected by default
# when no other dirty/state tracker was requested explicitly.
ifeq ($(origin USE_SMC_STREAM_STATE_TRACKER),undefined)
  ifeq ($(filter 1,$(USE_NO_STATE_TRACKER) $(USE_DIRTY_CELLS) $(USE_SMC_STATE_TRACKER) $(USE_SMC_INDEXED_STATE_TRACKER) $(USE_SMC_BATCH_STATE_TRACKER)),)
    USE_SMC_STREAM_STATE_TRACKER := 1
  else
    USE_SMC_STREAM_STATE_TRACKER := 0
  endif
endif

# Frame profiling toggle.  Set PROFILE_FRAME=1 to compile in per-frame phase timing.
PROFILE_FRAME ?= 0

# SMC-specific CFLAGS for disabling optimizations in vendored SMC sources
SMC_CFLAGS ?=

# Mutual exclusivity check - select exactly one explicit/default tracker mode.
ifneq ($(shell expr $(USE_NO_STATE_TRACKER) + $(USE_DIRTY_CELLS) + $(USE_SMC_STATE_TRACKER) + $(USE_SMC_INDEXED_STATE_TRACKER) + $(USE_SMC_BATCH_STATE_TRACKER) + $(USE_SMC_STREAM_STATE_TRACKER)),0)
  ifneq ($(shell expr $(USE_NO_STATE_TRACKER) + $(USE_DIRTY_CELLS) + $(USE_SMC_STATE_TRACKER) + $(USE_SMC_INDEXED_STATE_TRACKER) + $(USE_SMC_BATCH_STATE_TRACKER) + $(USE_SMC_STREAM_STATE_TRACKER)),1)
    $(error Only one of USE_NO_STATE_TRACKER, USE_DIRTY_CELLS, USE_SMC_STATE_TRACKER, USE_SMC_INDEXED_STATE_TRACKER, USE_SMC_BATCH_STATE_TRACKER, USE_SMC_STREAM_STATE_TRACKER may be set)
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
TEST_DECAL_PAINTER_RUNNER    := $(BUILD_DIR)/test-decal-painter
TEST_UI_ELE_RUNNER           := $(BUILD_DIR)/test-ui-ele
TEST_SCENE_DOCUMENT_RUNNER   := $(BUILD_DIR)/test-scene-document
TEST_SCENE_FORMAT_RUNNER     := $(BUILD_DIR)/test-scene-format
TEST_COMMAND_SYSTEM_RUNNER   := $(BUILD_DIR)/test-command-system
TEST_EDITOR_SELECTION_RUNNER := $(BUILD_DIR)/test-editor-selection
TEST_EDITOR_HIGHLIGHT_RUNNER := $(BUILD_DIR)/test-editor-highlight
BENCH_EDITOR_HIGHLIGHT_RUNNER := $(BUILD_DIR)/benchmark-editor-highlight
BENCH_SURFACE_RENDER_RUNNER := $(BUILD_DIR)/benchmark-surface-render
TEST_EDITOR_DOMAIN_RUNNER    := $(BUILD_DIR)/test-editor-domain
TEST_UNIFIED_EDITOR_RUNNER   := $(BUILD_DIR)/test-unified-editor
TEST_INPUT_RUNNER            := $(BUILD_DIR)/test-input
TEST_GLYPH_CACHE_RUNNER      := $(BUILD_DIR)/test-glyph-block-cache
TEST_LIGHTING_CACHE_RUNNER   := $(BUILD_DIR)/test-lighting-cache
TEST_LIGHTING_RUNNER         := $(BUILD_DIR)/test-lighting
TEST_APP_OPTIONS_RUNNER      := $(BUILD_DIR)/test-app-options
TEST_SMC_STATE_RUNNER        := $(BUILD_DIR)/test-smc-state-tracker
TEST_SMC_INDEXED_RUNNER      := $(BUILD_DIR)/test-smc-indexed-state-tracker
TEST_BENCHMARK_RUNNER        := $(BUILD_DIR)/test-benchmark-session
TEST_APP_MODULES_RUNNER      := $(BUILD_DIR)/test-app-modules
TEST_DECAL_PROJECTION_RUNNER := $(BUILD_DIR)/test-decal-projection
TEST_MAP_CATALOG_RUNNER      := $(BUILD_DIR)/test-map-catalog
TEST_UI_PREFERENCES_RUNNER   := $(BUILD_DIR)/test-ui-preferences
TEST_UI_COMPOSITOR_RUNNER    := $(BUILD_DIR)/test-ui-compositor
TEST_CAMERA_RUNNER           := $(BUILD_DIR)/test-camera
TEST_MATERIAL_DOCUMENT_RUNNER := $(BUILD_DIR)/test-material-document
TEST_DECAL_DOCUMENT_RUNNER := $(BUILD_DIR)/test-decal-document
TEST_ASSET_REFRESH_RUNNER := $(BUILD_DIR)/test-asset-refresh




.PHONY: all run test check clean dirs benchmark-raycast benchmark-editor-highlight stability-editor-highlight benchmark-surface-render stability-surface-render asan ubsan sanitize leak coverage style matrix matrix-one smoke


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
SRC_CHECKED_SIZE  := src/checked_size.c
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
SRC_DECAL_PAINTER := src/decal_painter.c
SRC_UI_ELE        := src/ui_ele.c
SRC_MENU_STATE    := src/menu_state.c
SRC_SCALE         := src/scale.c
SRC_TIMING        := src/timing.c
SRC_RAYCAST       := src/raycast.c src/decal_projection.c src/smc_render_opt.c
SRC_LIGHTING      := src/lighting.c
SRC_RENDERER      := src/renderer.c src/glyph_atlas.c
SRC_SCENE_DOCUMENT := src/scene_document.c
SRC_SCENE_DIAGNOSTIC := src/scene_diagnostic.c
SRC_SCENE_FORMAT := src/scene_format.c
SRC_SCENE_BLOCK_CODEC := src/scene_block_codec.c
SRC_COMMAND_SYSTEM := src/command_system.c
SRC_EDITOR_SELECTION := src/editor_selection.c
SRC_EDITOR_HIGHLIGHT := src/editor_highlight.c
SRC_EDITOR_DOMAIN    := src/editor_domain.c
SRC_UNIFIED_EDITOR := src/unified_editor.c
SRC_MAP_CATALOG    := src/map_catalog.c
SRC_UI_PREFERENCES := src/ui_preferences.c
SRC_UI_CANVAS      := src/ui_canvas.c
SRC_UI_COMPOSITOR  := src/ui_compositor.c
SRC_ASSET_DOCUMENT := src/asset_document.c
SRC_MATERIAL_DOCUMENT := src/material_document.c
SRC_DECAL_DOCUMENT := src/decal_document.c
SRC_ASSET_REFRESH := src/asset_refresh.c




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
	$(SRC_CHECKED_SIZE) \
	$(SRC_UI_ELE) \
	$(SRC_GRID)

TEST_DECAL_IO_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_DECAL_IO) \
	$(SRC_ASSETS) \
	$(SRC_WORLD) \
	$(SRC_MAP) \
	$(SRC_ASSET_LOADER) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG)

TEST_DECAL_PAINTER_SRC := \
	$(SRC_DECAL_PAINTER)

TEST_CORE_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_DECAL_IO) \
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
	$(SRC_INPUT) \
	$(SRC_UI_CANVAS) \
	$(SRC_UI_COMPOSITOR) \
	$(SRC_UI_PREFERENCES)

TEST_DECALS_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_DECAL_IO) \
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
	$(SRC_CHECKED_SIZE) \
	$(SRC_ASSETS) \
	$(SRC_SCENE_DIAGNOSTIC) \
	$(SRC_SCENE_FORMAT) \
	$(SRC_SCENE_BLOCK_CODEC) \
	$(SRC_SCENE_DOCUMENT) \
	$(SRC_LIGHTING) \
	$(SRC_RAYCAST) \
	$(SRC_CAMERA) \
	$(SRC_WORLD) \
	$(SRC_MAP) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG) \
	$(SRC_MATH) \
	$(SRC_GRID)

TEST_SCENE_FORMAT_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_SCENE_DIAGNOSTIC) \
	$(SRC_SCENE_FORMAT) \
	$(SRC_SCENE_BLOCK_CODEC)

TEST_COMMAND_SYSTEM_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_ASSETS) \
	$(SRC_SCENE_DIAGNOSTIC) \
	$(SRC_SCENE_FORMAT) \
	$(SRC_SCENE_BLOCK_CODEC) \
	$(SRC_COMMAND_SYSTEM) \
	$(SRC_SCENE_DOCUMENT) \
	$(SRC_WORLD) \
	$(SRC_MAP) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG)

TEST_EDITOR_SELECTION_SRC := \
	$(SRC_CHECKED_SIZE) \
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

TEST_EDITOR_HIGHLIGHT_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_EDITOR_HIGHLIGHT) \
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

TEST_EDITOR_DOMAIN_SRC := \
	$(SRC_EDITOR_DOMAIN) \
	$(SRC_COMMAND_SYSTEM) \
	$(SRC_SCENE_DOCUMENT) \
	$(SRC_SCENE_FORMAT) \
	$(SRC_SCENE_BLOCK_CODEC) \
	$(SRC_SCENE_DIAGNOSTIC) \
	$(SRC_CHECKED_SIZE) \
	$(SRC_ASSETS) \
	$(SRC_WORLD) \
	$(SRC_MAP) \
	$(SRC_MAP_LOADER) \
	$(SRC_CONFIG)

TEST_UNIFIED_EDITOR_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_ASSET_DOCUMENT) \
	$(SRC_MATERIAL_DOCUMENT) \
	$(SRC_DECAL_DOCUMENT) \
	$(SRC_DECAL_PAINTER) \
	$(SRC_DECAL_IO) \
	$(SRC_ASSET_LOADER) \
	$(SRC_ASSET_REFRESH) \
	$(SRC_SCENE_DIAGNOSTIC) \
	$(SRC_SCENE_FORMAT) \
	$(SRC_SCENE_BLOCK_CODEC) \
	$(SRC_UNIFIED_EDITOR) \
	$(SRC_MAP_CATALOG) \
	$(SRC_EDITOR_HIGHLIGHT) \
	$(SRC_EDITOR_DOMAIN) \
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

$(TEST_DECAL_PAINTER_RUNNER): tests/test_decal_painter.c $(TEST_DECAL_PAINTER_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_decal_painter.c $(TEST_DECAL_PAINTER_SRC) \
		-o $(TEST_DECAL_PAINTER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_ELE_RUNNER): tests/test_ui_ele.c $(TEST_UI_ELE_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_ele.c $(TEST_UI_ELE_SRC) \
		-o $(TEST_UI_ELE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SCENE_DOCUMENT_RUNNER): tests/test_scene_document.c $(TEST_SCENE_DOCUMENT_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_scene_document.c $(TEST_SCENE_DOCUMENT_SRC) \
		-o $(TEST_SCENE_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SCENE_FORMAT_RUNNER): tests/test_scene_format.c $(TEST_SCENE_FORMAT_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_scene_format.c $(TEST_SCENE_FORMAT_SRC) \
		-o $(TEST_SCENE_FORMAT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_COMMAND_SYSTEM_RUNNER): tests/test_command_system.c $(TEST_COMMAND_SYSTEM_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_command_system.c $(TEST_COMMAND_SYSTEM_SRC) \
		-o $(TEST_COMMAND_SYSTEM_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_EDITOR_SELECTION_RUNNER): tests/test_editor_selection.c $(TEST_EDITOR_SELECTION_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_editor_selection.c $(TEST_EDITOR_SELECTION_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_EDITOR_SELECTION_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_EDITOR_HIGHLIGHT_RUNNER): tests/test_editor_highlight.c $(TEST_EDITOR_HIGHLIGHT_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_editor_highlight.c $(TEST_EDITOR_HIGHLIGHT_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_EDITOR_HIGHLIGHT_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(BENCH_EDITOR_HIGHLIGHT_RUNNER): tests/benchmark_editor_highlight.c $(TEST_EDITOR_HIGHLIGHT_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/benchmark_editor_highlight.c $(TEST_EDITOR_HIGHLIGHT_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(BENCH_EDITOR_HIGHLIGHT_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(BENCH_SURFACE_RENDER_RUNNER): tests/benchmark_surface_render.c $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_RAYCAST) $(SRC_CONFIG) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/benchmark_surface_render.c $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) \
		$(SRC_CAMERA) $(SRC_RAYCAST) $(SRC_CONFIG) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) \
		$(TEST_FEATURE_EXTRA_SRC) -o $(BENCH_SURFACE_RENDER_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_EDITOR_DOMAIN_RUNNER): tests/test_editor_domain.c $(TEST_EDITOR_DOMAIN_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_editor_domain.c $(TEST_EDITOR_DOMAIN_SRC) \
		-o $(TEST_EDITOR_DOMAIN_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UNIFIED_EDITOR_RUNNER): tests/test_unified_editor.c $(TEST_UNIFIED_EDITOR_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_unified_editor.c $(TEST_UNIFIED_EDITOR_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_UNIFIED_EDITOR_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_INPUT_RUNNER): tests/test_input.c $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_input.c $(SRC_INPUT) \
		-o $(TEST_INPUT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_CAMERA_RUNNER): tests/test_camera.c $(SRC_CAMERA) $(SRC_MAP) $(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_camera.c $(SRC_CAMERA) $(SRC_MAP) \
		$(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) -o $(TEST_CAMERA_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_MAP_CATALOG_RUNNER): tests/test_map_catalog.c $(SRC_MAP_CATALOG) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_map_catalog.c $(SRC_MAP_CATALOG) \
		-o $(TEST_MAP_CATALOG_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_GLYPH_CACHE_RUNNER): tests/test_glyph_block_cache.c src/glyph_block_cache.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_glyph_block_cache.c src/glyph_block_cache.c \
		-o $(TEST_GLYPH_CACHE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_LIGHTING_CACHE_RUNNER): tests/test_lighting_cache.c src/lighting_cache.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_lighting_cache.c src/lighting_cache.c \
		-o $(TEST_LIGHTING_CACHE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_LIGHTING_RUNNER): tests/test_lighting.c $(SRC_CHECKED_SIZE) $(SRC_LIGHTING) $(SRC_RAYCAST) $(SRC_CAMERA) $(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CONFIG) $(SRC_MATH) $(SRC_GRID) $(SRC_INPUT) $(TEST_FEATURE_EXTRA_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_lighting.c $(SRC_CHECKED_SIZE) $(SRC_LIGHTING) $(SRC_RAYCAST) $(SRC_CAMERA) \
		$(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CONFIG) $(SRC_MATH) $(SRC_GRID) $(SRC_INPUT) \
		$(TEST_FEATURE_EXTRA_SRC) -o $(TEST_LIGHTING_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_APP_OPTIONS_RUNNER): tests/test_app_options.c src/app_options.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_app_options.c src/app_options.c \
		-o $(TEST_APP_OPTIONS_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SMC_STATE_RUNNER): tests/test_smc_state_tracker.c src/smc_state_tracker.c | dirs
	$(CC) $(CFLAGS) -DUSE_SMC_STATE_TRACKER=1 $(INCLUDES) \
		-I"vendor/src/smc/include" -I"vendor/src/smc/src/c" \
		tests/test_smc_state_tracker.c src/smc_state_tracker.c \
		vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c \
		vendor/src/smc/src/c/smc_state.c -o $(TEST_SMC_STATE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SMC_INDEXED_RUNNER): tests/test_smc_indexed_state_tracker.c src/smc_indexed_state_tracker.c | dirs
	$(CC) $(CFLAGS) -DUSE_SMC_INDEXED_STATE_TRACKER=1 $(INCLUDES) \
		-I"vendor/src/smc/include" -I"vendor/src/smc/src/c" \
		tests/test_smc_indexed_state_tracker.c src/smc_indexed_state_tracker.c \
		vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c \
		vendor/src/smc/src/c/smc_state.c -o $(TEST_SMC_INDEXED_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_BENCHMARK_RUNNER): tests/test_benchmark_session.c src/benchmark_session.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_benchmark_session.c src/benchmark_session.c \
		-o $(TEST_BENCHMARK_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_APP_MODULES_RUNNER): tests/test_app_modules.c src/menu_controller.c src/frame_dispatch.c src/grid.c src/camera.c src/math.c src/map.c src/checked_size.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_app_modules.c src/menu_controller.c \
		src/frame_dispatch.c src/grid.c src/camera.c src/math.c src/map.c src/checked_size.c \
		-o $(TEST_APP_MODULES_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_DECAL_PROJECTION_RUNNER): tests/test_decal_projection.c src/decal_projection.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_decal_projection.c src/decal_projection.c \
		-o $(TEST_DECAL_PROJECTION_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_PREFERENCES_RUNNER): tests/test_ui_preferences.c $(SRC_UI_PREFERENCES) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_preferences.c $(SRC_UI_PREFERENCES) \
		-o $(TEST_UI_PREFERENCES_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_COMPOSITOR_RUNNER): tests/test_ui_compositor.c tests/font8x8_test_data.c $(SRC_UI_CANVAS) $(SRC_UI_COMPOSITOR) $(SRC_UI_PREFERENCES) $(SRC_CHECKED_SIZE) $(SRC_GRID) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_compositor.c $(SRC_UI_CANVAS) $(SRC_GRID) \
		tests/font8x8_test_data.c $(SRC_UI_COMPOSITOR) $(SRC_UI_PREFERENCES) $(SRC_CHECKED_SIZE) \
		-o $(TEST_UI_COMPOSITOR_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_MATERIAL_DOCUMENT_RUNNER): tests/test_material_document.c $(SRC_ASSET_DOCUMENT) $(SRC_MATERIAL_DOCUMENT) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_material_document.c $(SRC_ASSET_DOCUMENT) \
		$(SRC_MATERIAL_DOCUMENT) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_MATERIAL_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_DECAL_DOCUMENT_RUNNER): tests/test_decal_document.c $(SRC_ASSET_DOCUMENT) $(SRC_DECAL_DOCUMENT) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_decal_document.c $(SRC_ASSET_DOCUMENT) \
		$(SRC_DECAL_DOCUMENT) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) $(SRC_ASSETS) \
		$(SRC_CHECKED_SIZE) -o $(TEST_DECAL_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_ASSET_REFRESH_RUNNER): tests/test_asset_refresh.c $(SRC_ASSET_REFRESH) $(SRC_ASSET_DOCUMENT) $(SRC_MATERIAL_DOCUMENT) $(SRC_DECAL_DOCUMENT) $(SRC_ASSET_LOADER) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) $(SRC_SCENE_DOCUMENT) $(SRC_SCENE_FORMAT) $(SRC_SCENE_BLOCK_CODEC) $(SRC_SCENE_DIAGNOSTIC) $(SRC_CONFIG) $(SRC_MAP_LOADER) $(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_asset_refresh.c $(SRC_ASSET_REFRESH) \
		$(SRC_ASSET_DOCUMENT) $(SRC_MATERIAL_DOCUMENT) $(SRC_DECAL_DOCUMENT) \
		$(SRC_ASSET_LOADER) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) \
		$(SRC_SCENE_DOCUMENT) $(SRC_SCENE_FORMAT) $(SRC_SCENE_BLOCK_CODEC) \
		$(SRC_SCENE_DIAGNOSTIC) $(SRC_CONFIG) $(SRC_MAP_LOADER) $(SRC_MAP) \
		$(SRC_WORLD) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_ASSET_REFRESH_RUNNER) $(TEST_LIBS) $(RPATH)



run: $(APP)



	./$(APP) --mode raycast

run-normal: $(APP)
	./$(APP) --mode normal

run-stress: $(APP)
	./$(APP) --mode stress

test: $(TEST_DEPS_RUNNER) $(TEST_CORE_RUNNER) $(TEST_DECALS_RUNNER) $(TEST_MENU_STATE_RUNNER) $(TEST_DECAL_IO_RUNNER) $(TEST_DECAL_PAINTER_RUNNER) $(TEST_UI_ELE_RUNNER) $(TEST_SCENE_DOCUMENT_RUNNER) $(TEST_SCENE_FORMAT_RUNNER) $(TEST_COMMAND_SYSTEM_RUNNER) $(TEST_EDITOR_SELECTION_RUNNER) $(TEST_EDITOR_HIGHLIGHT_RUNNER) $(TEST_EDITOR_DOMAIN_RUNNER) $(TEST_UNIFIED_EDITOR_RUNNER) $(TEST_INPUT_RUNNER) $(TEST_CAMERA_RUNNER) $(TEST_MAP_CATALOG_RUNNER) $(TEST_GLYPH_CACHE_RUNNER) $(TEST_LIGHTING_CACHE_RUNNER) $(TEST_LIGHTING_RUNNER) $(TEST_APP_OPTIONS_RUNNER) $(TEST_SMC_STATE_RUNNER) $(TEST_SMC_INDEXED_RUNNER) $(TEST_BENCHMARK_RUNNER) $(TEST_APP_MODULES_RUNNER) $(TEST_DECAL_PROJECTION_RUNNER) $(TEST_UI_PREFERENCES_RUNNER) $(TEST_UI_COMPOSITOR_RUNNER) $(TEST_MATERIAL_DOCUMENT_RUNNER) $(TEST_DECAL_DOCUMENT_RUNNER) $(TEST_ASSET_REFRESH_RUNNER)
	./$(TEST_DEPS_RUNNER)
	./$(TEST_CORE_RUNNER)
	./$(TEST_DECALS_RUNNER)
	./$(TEST_MENU_STATE_RUNNER)
	./$(TEST_DECAL_IO_RUNNER)
	./$(TEST_DECAL_PAINTER_RUNNER)
	./$(TEST_UI_ELE_RUNNER)
	./$(TEST_SCENE_DOCUMENT_RUNNER)
	./$(TEST_SCENE_FORMAT_RUNNER)
	./$(TEST_COMMAND_SYSTEM_RUNNER)
	./$(TEST_EDITOR_SELECTION_RUNNER)
	./$(TEST_EDITOR_HIGHLIGHT_RUNNER)
	./$(TEST_EDITOR_DOMAIN_RUNNER)
	./$(TEST_UNIFIED_EDITOR_RUNNER)
	./$(TEST_INPUT_RUNNER)
	./$(TEST_CAMERA_RUNNER)
	./$(TEST_MAP_CATALOG_RUNNER)
	./$(TEST_GLYPH_CACHE_RUNNER)
	./$(TEST_LIGHTING_CACHE_RUNNER)
	./$(TEST_LIGHTING_RUNNER)
	./$(TEST_APP_OPTIONS_RUNNER)
	./$(TEST_SMC_STATE_RUNNER)
	./$(TEST_SMC_INDEXED_RUNNER)
	./$(TEST_BENCHMARK_RUNNER)
	./$(TEST_APP_MODULES_RUNNER)
	./$(TEST_DECAL_PROJECTION_RUNNER)
	./$(TEST_UI_PREFERENCES_RUNNER)
	./$(TEST_UI_COMPOSITOR_RUNNER)
	./$(TEST_MATERIAL_DOCUMENT_RUNNER)
	./$(TEST_DECAL_DOCUMENT_RUNNER)
	./$(TEST_ASSET_REFRESH_RUNNER)
	@echo "Note: benchmark and stability require a video environment to fully run."

check: all test

asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O1 -g -fsanitize=address -fno-omit-frame-pointer" test

ubsan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O1 -g -fsanitize=undefined -fno-omit-frame-pointer" test

sanitize: asan ubsan

leak:
	@if command -v valgrind >/dev/null 2>&1; then \
		$(MAKE) $(TEST_DECAL_IO_RUNNER) $(TEST_CORE_RUNNER) && \
		valgrind --quiet --error-exitcode=1 --leak-check=full ./$(TEST_DECAL_IO_RUNNER) && \
		valgrind --quiet --error-exitcode=1 --leak-check=full ./$(TEST_CORE_RUNNER); \
	else echo "SKIP: valgrind is not available"; fi

coverage:
	@if command -v gcov >/dev/null 2>&1; then \
		$(MAKE) clean && $(MAKE) CFLAGS="$(CFLAGS) -O0 -g --coverage" test && \
		mkdir -p $(BUILD_DIR)/coverage && \
		gcov -o $(BUILD_DIR) $(BUILD_DIR)/*.gcno; \
		status=$$?; \
		for report in ./*.gcov; do \
			if test -f "$$report"; then mv "$$report" $(BUILD_DIR)/coverage/; fi; \
		done; \
		exit $$status; \
	else echo "SKIP: gcov is not available"; fi

style:
	@if command -v cppcheck >/dev/null 2>&1; then cppcheck --quiet --error-exitcode=1 --std=c11 src; \
	else echo "SKIP: cppcheck is not available"; fi

check-legacy-unused:
	@echo "Checking that deprecated legacy load/save symbols are not called from new production code..."
	@# These symbols are retained for the second removal pass; any new production
	@# caller outside the expected deprecated sites is a regression of Item 9.
	@# Patterns require a real call site (trailing semicolon) so deprecation
	@# comments like "symbol()" are not counted as callers.
	@! grep -Rsn "scene_document_load[(][^)]*[)];" src/*.c | grep -v "src/scene_document.c:" | grep -v "src/unified_editor.c:" >/dev/null
	@! grep -Rsn "write_map_digits[(][^)]*[)];" src/*.c | grep -v "src/scene_document.c:" | grep -v "src/unified_editor.c:" >/dev/null
	@! grep -Rsn "scene_document_save[(][^)]*[)];" src/*.c | grep -v "src/scene_document.c:" | grep -v "src/unified_editor.c:" >/dev/null
	@! grep -Rsn "unified_editor_load_scene[(][^)]*[)];" src/*.c | grep -v "src/scene_document.c:" | grep -v "src/unified_editor.c:" >/dev/null
	@echo "OK: deprecated legacy symbols have no unexpected production callers"

matrix:
	@set -e; for mode in \
		"USE_NO_STATE_TRACKER=1" "USE_DIRTY_CELLS=1" "USE_SMC_STATE_TRACKER=1" \
		"USE_SMC_INDEXED_STATE_TRACKER=1" "USE_SMC_BATCH_STATE_TRACKER=1" \
		"USE_SMC_STREAM_STATE_TRACKER=1" "USE_LIGHTING_CACHE=1" "USE_GLYPH_CACHE=1"; do \
		$(MAKE) matrix-one MATRIX_MODE="$$mode"; \
	done

matrix-one:
	@test -n "$(MATRIX_MODE)" || { echo "MATRIX_MODE is required"; exit 2; }
	@$(MAKE) clean >/dev/null
	@$(MAKE) $(MATRIX_MODE) build/test-core >/dev/null
	@./build/test-core >/dev/null
	@echo "PASS: $(MATRIX_MODE)"

smoke: all
	./$(APP) --smoke-test





benchmark: $(APP)
	./$(APP) --benchmark-raycast 5

stability: $(APP)
	./$(APP) --stability-test 30

benchmark-editor-highlight: $(BENCH_EDITOR_HIGHLIGHT_RUNNER)
	./$(BENCH_EDITOR_HIGHLIGHT_RUNNER)

stability-editor-highlight: $(BENCH_EDITOR_HIGHLIGHT_RUNNER)
	./$(BENCH_EDITOR_HIGHLIGHT_RUNNER) --stability

benchmark-surface-render: $(BENCH_SURFACE_RENDER_RUNNER)
	./$(BENCH_SURFACE_RENDER_RUNNER)

stability-surface-render: $(BENCH_SURFACE_RENDER_RUNNER)
	./$(BENCH_SURFACE_RENDER_RUNNER) --stability

clean:
	rm -rf $(BUILD_DIR)
	rm -f -- *.gcov
