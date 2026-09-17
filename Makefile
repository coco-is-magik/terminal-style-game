CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
BUILD_DIR := build
VALGRIND ?= valgrind
CPPCHECK ?= cppcheck
CPPCHECK_FLAGS := --quiet --error-exitcode=100 --std=c11 \
	--suppress=normalCheckLevelMaxBranches --suppress=toomanyconfigs
GCOV ?= gcov
DOCKER ?= docker
VALGRIND_IMAGE ?= terminal-style-game-valgrind:ubuntu24.04-amd64

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
PLATFORM_LIBS :=
ifeq ($(OS),Windows_NT)
  PLATFORM_LIBS := -lwinmm -lws2_32
endif
LIBS := -L"$(VENDOR_DIR)/lib64" -lSDL3 -lSDL3_mixer -lenet $(PLATFORM_LIBS) -lm
TEST_LIBS := -L"$(VENDOR_DIR)/lib64" -lcmocka -lSDL3 -lSDL3_mixer -lenet $(PLATFORM_LIBS) -lm
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
TEST_RGBA_PARSE_RUNNER       := $(BUILD_DIR)/test-rgba-parse
TEST_NUMBER_PARSE_RUNNER     := $(BUILD_DIR)/test-number-parse
TEST_SCENE_DOCUMENT_RUNNER   := $(BUILD_DIR)/test-scene-document
TEST_SCENE_FORMAT_RUNNER     := $(BUILD_DIR)/test-scene-format
TEST_COMMAND_SYSTEM_RUNNER   := $(BUILD_DIR)/test-command-system
TEST_EDITOR_SELECTION_RUNNER := $(BUILD_DIR)/test-editor-selection
TEST_EDITOR_HIGHLIGHT_RUNNER := $(BUILD_DIR)/test-editor-highlight
BENCH_EDITOR_HIGHLIGHT_RUNNER := $(BUILD_DIR)/benchmark-editor-highlight
BENCH_SURFACE_RENDER_RUNNER := $(BUILD_DIR)/benchmark-surface-render
BENCH_COLORED_LIGHTING_RUNNER := $(BUILD_DIR)/benchmark-colored-lighting
TEST_EDITOR_DOMAIN_RUNNER    := $(BUILD_DIR)/test-editor-domain
TEST_UNIFIED_EDITOR_RUNNER   := $(BUILD_DIR)/test-unified-editor
TEST_INPUT_RUNNER            := $(BUILD_DIR)/test-input
TEST_GLYPH_CACHE_RUNNER      := $(BUILD_DIR)/test-glyph-block-cache
TEST_LIGHTING_CACHE_RUNNER   := $(BUILD_DIR)/test-lighting-cache
TEST_LIGHTING_RUNNER         := $(BUILD_DIR)/test-lighting
TEST_APP_OPTIONS_RUNNER      := $(BUILD_DIR)/test-app-options
TEST_PLATFORM_CAPABILITIES_RUNNER := $(BUILD_DIR)/test-platform-capabilities
TEST_PLATFORM_NUMBER_RUNNER := $(BUILD_DIR)/test-platform-number
TEST_SMC_STATE_RUNNER        := $(BUILD_DIR)/test-smc-state-tracker
TEST_SMC_INDEXED_RUNNER      := $(BUILD_DIR)/test-smc-indexed-state-tracker
TEST_BENCHMARK_RUNNER        := $(BUILD_DIR)/test-benchmark-session
TEST_APP_MODULES_RUNNER      := $(BUILD_DIR)/test-app-modules
TEST_CONFIG_RUNNER           := $(BUILD_DIR)/test-config
TEST_ASSET_LOADER_RUNNER     := $(BUILD_DIR)/test-asset-loader
TEST_DECAL_PROJECTION_RUNNER := $(BUILD_DIR)/test-decal-projection
TEST_MAP_CATALOG_RUNNER      := $(BUILD_DIR)/test-map-catalog
TEST_UI_PREFERENCES_RUNNER   := $(BUILD_DIR)/test-ui-preferences
TEST_UI_COMPOSITOR_RUNNER    := $(BUILD_DIR)/test-ui-compositor
TEST_UI_THEME_RUNNER         := $(BUILD_DIR)/test-ui-theme
TEST_UI_MOTION_RUNNER        := $(BUILD_DIR)/test-ui-motion
TEST_UI_PAUSE_MOTION_RUNNER  := $(BUILD_DIR)/test-ui-pause-motion
TEST_UI_WORKBENCH_STORE_RUNNER := $(BUILD_DIR)/test-ui-workbench-store
TEST_UI_WORKBENCH_RUNNER     := $(BUILD_DIR)/test-ui-workbench
TEST_UI_APP_THEME_ADAPTER_RUNNER := $(BUILD_DIR)/test-ui-app-theme-adapter
TEST_UI_THEME_DEMO_RUNNER    := $(BUILD_DIR)/test-ui-theme-demo
TEST_UI_MOTION_DEMO_RUNNER   := $(BUILD_DIR)/test-ui-motion-demo
TEST_CAMERA_RUNNER           := $(BUILD_DIR)/test-camera
TEST_VERTICAL_PHYSICS_RUNNER := $(BUILD_DIR)/test-vertical-physics
TEST_MATERIAL_DOCUMENT_RUNNER := $(BUILD_DIR)/test-material-document
TEST_DECAL_DOCUMENT_RUNNER := $(BUILD_DIR)/test-decal-document
TEST_SPRITE_DOCUMENT_RUNNER := $(BUILD_DIR)/test-sprite-document
TEST_OBJECT_DOCUMENT_RUNNER := $(BUILD_DIR)/test-object-document
TEST_FLOW_DOCUMENT_RUNNER := $(BUILD_DIR)/test-flow-document
TEST_FLOW_REFERENCE_RUNNER := $(BUILD_DIR)/test-flow-reference
TEST_FLOW_RUNTIME_RUNNER := $(BUILD_DIR)/test-flow-runtime
TEST_FLOW_BINDING_RUNNER := $(BUILD_DIR)/test-flow-binding
TEST_FLOW_WORKSPACE_RUNNER := $(BUILD_DIR)/test-flow-workspace
TEST_FLOW_PROJECT_CATALOG_RUNNER := $(BUILD_DIR)/test-flow-project-catalog
TEST_UI_DOCUMENT_RUNNER := $(BUILD_DIR)/test-ui-document
TEST_UI_LAYOUT_RESOLVER_RUNNER := $(BUILD_DIR)/test-ui-layout-resolver
TEST_UI_RENDER_ADAPTER_RUNNER := $(BUILD_DIR)/test-ui-render-adapter
TEST_UI_INTERACTION_RUNNER := $(BUILD_DIR)/test-ui-interaction
TEST_UI_MENU_RUNTIME_RUNNER := $(BUILD_DIR)/test-ui-menu-runtime
TEST_UI_MENU_WORKSPACE_RUNNER := $(BUILD_DIR)/test-ui-menu-workspace
TEST_SCENE_FLOW_ADAPTER_RUNNER := $(BUILD_DIR)/test-scene-flow-adapter
TEST_ENTITY_TRIGGER_SESSION_RUNNER := $(BUILD_DIR)/test-entity-trigger-session
TEST_ASSET_REFRESH_RUNNER := $(BUILD_DIR)/test-asset-refresh
TEST_R9_OPTICAL_SEMANTICS_RUNNER := $(BUILD_DIR)/test-r9-optical-semantics
TEST_R9_MULTIHIT_TRACE_RUNNER := $(BUILD_DIR)/test-r9-multihit-trace
BENCH_R9_MULTIHIT_TRACE_RUNNER := $(BUILD_DIR)/benchmark-r9-multihit-trace
TEST_R9_OPTICAL_COMPOSITOR_RUNNER := $(BUILD_DIR)/test-r9-optical-compositor
BENCH_R9_OPTICAL_COMPOSITOR_RUNNER := $(BUILD_DIR)/benchmark-r9-optical-compositor
TEST_R9_MIRROR_TRACE_RUNNER := $(BUILD_DIR)/test-r9-mirror-trace
BENCH_R9_MIRROR_TRACE_RUNNER := $(BUILD_DIR)/benchmark-r9-mirror-trace
TEST_OPTICAL_RUNTIME_VIEW_RUNNER := $(BUILD_DIR)/test-optical-runtime-view
BENCH_OPTICAL_RUNTIME_VIEW_RUNNER := $(BUILD_DIR)/benchmark-optical-runtime-view
TEST_HEIGHTFIELD_SELECTIVE_RUNNER := $(BUILD_DIR)/test-heightfield-selective
BENCH_HEIGHTFIELD_SELECTIVE_RUNNER := $(BUILD_DIR)/benchmark-heightfield-selective
TEST_MIRROR_TRACE_RUNNER := $(BUILD_DIR)/test-mirror-trace
TEST_OPTICAL_RENDER_RUNNER := $(BUILD_DIR)/test-optical-render
BENCH_OPTICAL_RENDER_RUNNER := $(BUILD_DIR)/benchmark-optical-render
TEST_SPRITE_RENDER_RUNNER := $(BUILD_DIR)/test-sprite-render
TEST_SPRITE_ANIMATION_PLAYER_RUNNER := $(BUILD_DIR)/test-sprite-animation-player
BENCH_SPRITE_RENDER_RUNNER := $(BUILD_DIR)/benchmark-sprite-render




.PHONY: all run ui-theme-demo test test-build test-ui-standards check standards standards-core clean dirs verification-environment benchmark benchmark-headless stability stability-fast stability-headless benchmark-raycast benchmark-editor-highlight stability-editor-highlight stability-surface-render benchmark-surface-render stability-optical-render benchmark-colored-lighting benchmark-sprite-render r9-p1-memory-report benchmark-r9-multihit-trace benchmark-r9-optical-compositor benchmark-r9-mirror-trace benchmark-optical-runtime-view benchmark-heightfield-selective benchmark-optical-render asan ubsan sanitize leak leak-native leak-image leak-image-self-test test-leak-classifier test-platform-harness display-acceptance-linux platform-image-ubuntu-gcc platform-image-ubuntu-clang platform-image-fedora-gcc platform-image-alpine-gcc platform-images platform-test-ubuntu-gcc platform-test-ubuntu-clang platform-test-fedora-gcc platform-test-alpine-gcc platform-test-windows platform-bootstrap-windows-dependencies platform-survey platform-check coverage style check-static-analysis-policy check-unsafe-calls check-project-structure check-test-inventory check-legacy-unused check-current-renderer matrix matrix-one smoke
.PHONY: ui-motion-demo ui-workbench


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
SRC_VERTICAL_PHYSICS := src/vertical_physics.c
SRC_MAP_LOADER    := src/map_loader.c
SRC_ASSET_LOADER  := src/asset_loader.c
SRC_DECAL_IO      := src/decal_io.c src/number_parse.c
SRC_DECAL_PAINTER := src/decal_painter.c
SRC_UI_ELE        := src/ui_ele.c src/number_parse.c
SRC_RGBA_PARSE    := src/rgba_parse.c
SRC_UI_THEME      := src/ui_theme.c
SRC_UI_MOTION     := src/ui_motion.c
SRC_UI_PAUSE_MOTION := src/ui_pause_motion.c
SRC_UI_WORKBENCH_STORE := src/ui_workbench_store.c
SRC_UI_WORKBENCH := src/ui_workbench.c
SRC_UI_APP_THEME_ADAPTER := src/ui_app_theme_adapter.c
SRC_UI_THEME_DEMO := src/ui_theme_demo.c
SRC_UI_MOTION_DEMO := src/ui_motion_demo.c
SRC_MENU_STATE    := src/menu_state.c
SRC_SCALE         := src/scale.c
SRC_TIMING        := src/timing.c
SRC_RAYCAST       := src/raycast.c src/decal_projection.c src/sprite_render.c src/heightfield_trace.c src/smc_render_opt.c
SRC_RAYCAST_OPTICAL := $(SRC_RAYCAST) src/raycast_optical.c \
	src/heightfield_trace_selective.c src/optical_runtime_view.c \
	src/optical_compositor.c src/mirror_trace.c
SRC_LIGHTING      := src/lighting.c
SRC_RENDERER      := src/renderer.c src/glyph_atlas.c
SRC_PLATFORM_PATH := src/platform_path.c
SRC_PLATFORM_FS := src/platform_fs.c
SRC_PLATFORM_CATALOG := src/platform_catalog.c
SRC_PLATFORM_NUMBER := src/platform_number.c
SRC_SCENE_DOCUMENT := src/scene_document.c src/optical_runtime_view.c \
	$(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS)
SRC_SCENE_DIAGNOSTIC := src/scene_diagnostic.c
SRC_SCENE_FORMAT := src/scene_format.c $(SRC_PLATFORM_NUMBER)
SRC_SCENE_BLOCK_CODEC := src/scene_block_codec.c
SRC_COMMAND_SYSTEM := src/command_system.c
SRC_EDITOR_SELECTION := src/editor_selection.c
SRC_EDITOR_HIGHLIGHT := src/editor_highlight.c
SRC_EDITOR_DOMAIN    := src/editor_domain.c
SRC_UNIFIED_EDITOR := src/unified_editor.c
SRC_MAP_CATALOG    := src/map_catalog.c $(SRC_PLATFORM_CATALOG)
SRC_UI_PREFERENCES := src/ui_preferences.c $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS)
SRC_UI_CANVAS      := src/ui_canvas.c
SRC_UI_COMPOSITOR  := src/ui_compositor.c
SRC_ASSET_DOCUMENT := src/asset_document.c
SRC_MATERIAL_DOCUMENT := src/material_document.c
SRC_DECAL_DOCUMENT := src/decal_document.c
SRC_SPRITE_DOCUMENT := src/sprite_document.c
SRC_SPRITE_ANIMATION_PLAYER := src/sprite_animation_player.c
SRC_OBJECT_DOCUMENT := src/object_document.c
SRC_FLOW_DOCUMENT := src/flow_document.c
SRC_FLOW_DOCUMENT_PLATFORM := $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS)
SRC_FLOW_REFERENCE := src/flow_reference.c
SRC_FLOW_RUNTIME := src/flow_runtime.c
SRC_FLOW_BINDING := src/flow_binding.c
SRC_FLOW_WORKSPACE := src/flow_workspace.c
SRC_FLOW_PROJECT_CATALOG := src/flow_project_catalog.c
SRC_UI_DOCUMENT := src/ui_document.c
SRC_UI_LAYOUT_RESOLVER := src/ui_layout_resolver.c
SRC_UI_RENDER_ADAPTER := src/ui_render_adapter.c
SRC_UI_INTERACTION := src/ui_interaction.c
SRC_UI_MENU_RUNTIME := src/ui_menu_runtime.c
SRC_UI_MENU_WORKSPACE := src/ui_menu_workspace.c
SRC_UI_NESTED_INSPECTOR := src/ui_nested_inspector.c
SRC_SCENE_FLOW_ADAPTER := src/scene_flow_adapter.c
SRC_ENTITY_TRIGGER_SESSION := src/entity_trigger_session.c
SRC_ASSET_REFRESH := src/asset_refresh.c
SRC_OPTICAL_RUNTIME_VIEW := src/optical_runtime_view.c




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
	$(SRC_RGBA_PARSE) \
	$(SRC_UI_ELE) \
	$(SRC_GRID) \
	$(SRC_UI_THEME)

TEST_DECAL_IO_SRC := \
	$(SRC_CHECKED_SIZE) \
	$(SRC_RGBA_PARSE) \
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
	$(SRC_RGBA_PARSE) \
	$(SRC_DECAL_IO) \
	$(SRC_GRID) \
	$(SRC_SCALE) \
	$(SRC_TIMING) \
	$(SRC_RENDERER) \
	$(SRC_MATH) \
	$(SRC_MAP) \
	$(SRC_CAMERA) \
	$(SRC_RAYCAST_OPTICAL) \
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
	$(SRC_RGBA_PARSE) \
	$(SRC_DECAL_IO) \
	$(SRC_GRID) \
	$(SRC_MAP) \
	$(SRC_CAMERA) \
	$(SRC_OPTICAL_RUNTIME_VIEW) \
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
	$(SRC_SCENE_BLOCK_CODEC) \
	$(SRC_OPTICAL_RUNTIME_VIEW)

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
	$(SRC_OPTICAL_RUNTIME_VIEW) \
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
	$(SRC_OPTICAL_RUNTIME_VIEW) \
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
	$(SRC_RGBA_PARSE) \
	$(SRC_ASSET_DOCUMENT) \
	$(SRC_MATERIAL_DOCUMENT) \
	$(SRC_DECAL_DOCUMENT) \
	$(SRC_SPRITE_DOCUMENT) \
	$(SRC_SPRITE_ANIMATION_PLAYER) \
	$(SRC_OBJECT_DOCUMENT) \
    $(SRC_FLOW_DOCUMENT) \
    $(SRC_FLOW_REFERENCE) \
    $(SRC_FLOW_RUNTIME) \
    $(SRC_FLOW_BINDING) \
    $(SRC_FLOW_WORKSPACE) \
    $(SRC_FLOW_PROJECT_CATALOG) \
    $(SRC_SCENE_FLOW_ADAPTER) \
    $(SRC_UI_DOCUMENT) \
    $(SRC_UI_MENU_WORKSPACE) \
    $(SRC_UI_NESTED_INSPECTOR) \
    $(SRC_UI_LAYOUT_RESOLVER) \
    $(SRC_UI_RENDER_ADAPTER) \
    $(SRC_UI_INTERACTION) \
    $(SRC_UI_MENU_RUNTIME) \
    $(SRC_UI_CANVAS) \
    $(SRC_ENTITY_TRIGGER_SESSION) \
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
	$(SRC_VERTICAL_PHYSICS) \
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

$(TEST_RGBA_PARSE_RUNNER): tests/test_rgba_parse.c $(SRC_RGBA_PARSE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_rgba_parse.c $(SRC_RGBA_PARSE) \
		-o $(TEST_RGBA_PARSE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_NUMBER_PARSE_RUNNER): tests/test_number_parse.c src/number_parse.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_number_parse.c src/number_parse.c \
		-o $(TEST_NUMBER_PARSE_RUNNER) $(TEST_LIBS) $(RPATH)

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

$(BENCH_SURFACE_RENDER_RUNNER): tests/benchmark_surface_render.c $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_RAYCAST_OPTICAL) $(SRC_CONFIG) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/benchmark_surface_render.c $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) \
		$(SRC_CAMERA) $(SRC_RAYCAST_OPTICAL) $(SRC_CONFIG) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) \
		$(TEST_FEATURE_EXTRA_SRC) -o $(BENCH_SURFACE_RENDER_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_EDITOR_DOMAIN_RUNNER): tests/test_editor_domain.c $(TEST_EDITOR_DOMAIN_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_editor_domain.c $(TEST_EDITOR_DOMAIN_SRC) \
		-o $(TEST_EDITOR_DOMAIN_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UNIFIED_EDITOR_RUNNER): tests/test_unified_editor.c $(TEST_UNIFIED_EDITOR_SRC) $(TEST_FEATURE_EXTRA_SRC) $(SMC_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_unified_editor.c $(TEST_UNIFIED_EDITOR_SRC) $(TEST_FEATURE_EXTRA_SRC) \
		-o $(TEST_UNIFIED_EDITOR_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_ENTITY_TRIGGER_SESSION_RUNNER): tests/test_entity_trigger_session.c $(SRC_ENTITY_TRIGGER_SESSION) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_entity_trigger_session.c $(SRC_ENTITY_TRIGGER_SESSION) \
		-o $(TEST_ENTITY_TRIGGER_SESSION_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SCENE_FLOW_ADAPTER_RUNNER): tests/test_scene_flow_adapter.c $(SRC_SCENE_FLOW_ADAPTER) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_scene_flow_adapter.c \
		$(SRC_SCENE_FLOW_ADAPTER) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) \
		$(SRC_FLOW_DOCUMENT_PLATFORM) \
		$(SRC_ASSET_DOCUMENT) -o $(TEST_SCENE_FLOW_ADAPTER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_INPUT_RUNNER): tests/test_input.c $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_input.c $(SRC_INPUT) \
		-o $(TEST_INPUT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_CAMERA_RUNNER): tests/test_camera.c $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_camera.c $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) \
		$(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) -o $(TEST_CAMERA_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_MAP_CATALOG_RUNNER): tests/test_map_catalog.c $(SRC_MAP_CATALOG) $(SRC_PLATFORM_PATH) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_map_catalog.c $(SRC_MAP_CATALOG) \
		$(SRC_PLATFORM_PATH) \
		-o $(TEST_MAP_CATALOG_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_VERTICAL_PHYSICS_RUNNER): tests/test_vertical_physics.c $(SRC_VERTICAL_PHYSICS) $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_vertical_physics.c $(SRC_VERTICAL_PHYSICS) \
		$(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) \
		-o $(TEST_VERTICAL_PHYSICS_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_GLYPH_CACHE_RUNNER): tests/test_glyph_block_cache.c src/glyph_block_cache.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_glyph_block_cache.c src/glyph_block_cache.c \
		-o $(TEST_GLYPH_CACHE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_LIGHTING_CACHE_RUNNER): tests/test_lighting_cache.c src/lighting_cache.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_lighting_cache.c src/lighting_cache.c \
		-o $(TEST_LIGHTING_CACHE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_LIGHTING_RUNNER): tests/test_lighting.c $(SRC_CHECKED_SIZE) $(SRC_LIGHTING) $(SRC_RAYCAST) $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CONFIG) $(SRC_MATH) $(SRC_GRID) $(SRC_INPUT) $(TEST_FEATURE_EXTRA_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/test_lighting.c $(SRC_CHECKED_SIZE) $(SRC_LIGHTING) $(SRC_RAYCAST) $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) \
		$(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CONFIG) $(SRC_MATH) $(SRC_GRID) $(SRC_INPUT) \
		$(TEST_FEATURE_EXTRA_SRC) -o $(TEST_LIGHTING_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(BENCH_COLORED_LIGHTING_RUNNER): tests/benchmark_colored_lighting.c $(SRC_CHECKED_SIZE) $(SRC_LIGHTING) $(SRC_RAYCAST) $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CONFIG) $(SRC_MATH) $(SRC_GRID) $(SRC_INPUT) $(TEST_FEATURE_EXTRA_SRC) | dirs
	$(CC) $(CFLAGS) $(TEST_FEATURE_CFLAGS) $(TEST_FEATURE_DEFS) $(TEST_FEATURE_INCLUDES) \
		tests/benchmark_colored_lighting.c $(SRC_CHECKED_SIZE) $(SRC_LIGHTING) $(SRC_RAYCAST) $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) \
		$(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CONFIG) $(SRC_MATH) $(SRC_GRID) $(SRC_INPUT) \
		$(TEST_FEATURE_EXTRA_SRC) -o $(BENCH_COLORED_LIGHTING_RUNNER) $(TEST_FEATURE_LIBS) $(RPATH)

$(TEST_APP_OPTIONS_RUNNER): tests/test_app_options.c src/app_options.c src/display_acceptance.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_app_options.c src/app_options.c src/display_acceptance.c \
		-o $(TEST_APP_OPTIONS_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_PLATFORM_CAPABILITIES_RUNNER): tests/test_platform_capabilities.c $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_PLATFORM_CATALOG) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_platform_capabilities.c \
		$(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_PLATFORM_CATALOG) \
		-o $(TEST_PLATFORM_CAPABILITIES_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_PLATFORM_NUMBER_RUNNER): tests/test_platform_number.c $(SRC_PLATFORM_NUMBER) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_platform_number.c $(SRC_PLATFORM_NUMBER) \
		-o $(TEST_PLATFORM_NUMBER_RUNNER) $(TEST_LIBS) $(RPATH)

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

$(TEST_APP_MODULES_RUNNER): tests/test_app_modules.c src/menu_controller.c src/frame_dispatch.c src/grid.c src/camera.c $(SRC_OPTICAL_RUNTIME_VIEW) src/math.c src/map.c src/checked_size.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_app_modules.c src/menu_controller.c \
		src/frame_dispatch.c src/grid.c src/camera.c $(SRC_OPTICAL_RUNTIME_VIEW) src/math.c src/map.c src/checked_size.c \
		-o $(TEST_APP_MODULES_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_CONFIG_RUNNER): tests/test_config.c $(SRC_CONFIG) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_config.c $(SRC_CONFIG) \
		-o $(TEST_CONFIG_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_ASSET_LOADER_RUNNER): tests/test_asset_loader.c $(TEST_DECAL_IO_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_asset_loader.c $(TEST_DECAL_IO_SRC) \
		-o $(TEST_ASSET_LOADER_RUNNER) $(TEST_LIBS) $(RPATH)

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

$(TEST_UI_THEME_RUNNER): tests/test_ui_theme.c $(SRC_UI_THEME) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_theme.c $(SRC_UI_THEME) \
		-o $(TEST_UI_THEME_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_MOTION_RUNNER): tests/test_ui_motion.c $(SRC_UI_MOTION) $(SRC_UI_THEME) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_motion.c $(SRC_UI_MOTION) $(SRC_UI_THEME) \
		-o $(TEST_UI_MOTION_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_PAUSE_MOTION_RUNNER): tests/test_ui_pause_motion.c $(SRC_UI_PAUSE_MOTION) $(SRC_UI_MOTION) $(SRC_UI_THEME) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_pause_motion.c $(SRC_UI_PAUSE_MOTION) \
		$(SRC_UI_MOTION) $(SRC_UI_THEME) \
		-o $(TEST_UI_PAUSE_MOTION_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_WORKBENCH_STORE_RUNNER): tests/test_ui_workbench_store.c $(SRC_UI_WORKBENCH_STORE) $(TEST_UI_ELE_SRC) $(SRC_PLATFORM_FS) $(SRC_PLATFORM_PATH) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_workbench_store.c \
		$(SRC_UI_WORKBENCH_STORE) $(TEST_UI_ELE_SRC) \
		$(SRC_PLATFORM_FS) $(SRC_PLATFORM_PATH) \
		-o $(TEST_UI_WORKBENCH_STORE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_WORKBENCH_RUNNER): tests/test_ui_workbench.c $(SRC_UI_WORKBENCH) $(SRC_UI_WORKBENCH_STORE) $(TEST_UI_ELE_SRC) $(SRC_MENU_STATE) $(SRC_CONFIG) $(SRC_PLATFORM_FS) $(SRC_PLATFORM_PATH) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_workbench.c $(SRC_UI_WORKBENCH) \
		$(SRC_UI_WORKBENCH_STORE) $(TEST_UI_ELE_SRC) $(SRC_MENU_STATE) $(SRC_CONFIG) \
		$(SRC_PLATFORM_FS) $(SRC_PLATFORM_PATH) \
		-o $(TEST_UI_WORKBENCH_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_APP_THEME_ADAPTER_RUNNER): tests/test_ui_app_theme_adapter.c $(SRC_UI_APP_THEME_ADAPTER) $(SRC_UI_THEME) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_app_theme_adapter.c \
		$(SRC_UI_APP_THEME_ADAPTER) $(SRC_UI_THEME) \
		-o $(TEST_UI_APP_THEME_ADAPTER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_THEME_DEMO_RUNNER): tests/test_ui_theme_demo.c $(SRC_UI_THEME_DEMO) src/ui_theme_demo_runtime.c $(SRC_UI_THEME) $(SRC_UI_CANVAS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_theme_demo.c $(SRC_UI_THEME_DEMO) \
		src/ui_theme_demo_runtime.c $(SRC_UI_THEME) $(SRC_UI_CANVAS) $(SRC_CHECKED_SIZE) \
		$(SRC_INPUT) $(SRC_TIMING) $(SRC_UI_COMPOSITOR) \
		$(SRC_UI_PREFERENCES) $(SRC_GRID) src/renderer.c src/glyph_atlas.c \
		-o $(TEST_UI_THEME_DEMO_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_MOTION_DEMO_RUNNER): tests/test_ui_motion_demo.c $(SRC_UI_MOTION_DEMO) src/ui_motion_demo_runtime.c $(SRC_UI_MOTION) $(SRC_UI_THEME) $(SRC_UI_CANVAS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_motion_demo.c $(SRC_UI_MOTION_DEMO) \
		src/ui_motion_demo_runtime.c $(SRC_UI_MOTION) $(SRC_UI_THEME) $(SRC_UI_CANVAS) \
		$(SRC_CHECKED_SIZE) $(SRC_INPUT) $(SRC_TIMING) $(SRC_UI_COMPOSITOR) \
		$(SRC_UI_PREFERENCES) $(SRC_GRID) src/renderer.c src/glyph_atlas.c \
		-o $(TEST_UI_MOTION_DEMO_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_MATERIAL_DOCUMENT_RUNNER): tests/test_material_document.c $(SRC_ASSET_DOCUMENT) $(SRC_MATERIAL_DOCUMENT) $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_material_document.c $(SRC_ASSET_DOCUMENT) \
		$(SRC_MATERIAL_DOCUMENT) $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) \
		$(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_MATERIAL_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_DECAL_DOCUMENT_RUNNER): tests/test_decal_document.c $(SRC_ASSET_DOCUMENT) $(SRC_DECAL_DOCUMENT) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_decal_document.c $(SRC_ASSET_DOCUMENT) \
		$(SRC_DECAL_DOCUMENT) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) $(SRC_ASSETS) \
		$(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_DECAL_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SPRITE_DOCUMENT_RUNNER): tests/test_sprite_document.c $(SRC_SPRITE_DOCUMENT) $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_sprite_document.c $(SRC_SPRITE_DOCUMENT) \
		$(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_SPRITE_DOCUMENT_RUNNER) \
		$(TEST_LIBS) $(RPATH)

$(TEST_SPRITE_ANIMATION_PLAYER_RUNNER): tests/test_sprite_animation_player.c $(SRC_SPRITE_ANIMATION_PLAYER) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_sprite_animation_player.c \
		$(SRC_SPRITE_ANIMATION_PLAYER) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_SPRITE_ANIMATION_PLAYER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_OBJECT_DOCUMENT_RUNNER): tests/test_object_document.c $(SRC_OBJECT_DOCUMENT) $(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_object_document.c $(SRC_OBJECT_DOCUMENT) \
		$(SRC_PLATFORM_PATH) $(SRC_PLATFORM_FS) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_OBJECT_DOCUMENT_RUNNER) \
		$(TEST_LIBS) $(RPATH)

$(TEST_FLOW_DOCUMENT_RUNNER): tests/test_flow_document.c $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_flow_document.c $(SRC_FLOW_DOCUMENT) \
		$(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) \
		-o $(TEST_FLOW_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_FLOW_REFERENCE_RUNNER): tests/test_flow_reference.c $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_flow_reference.c $(SRC_FLOW_REFERENCE) \
		$(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) \
		-o $(TEST_FLOW_REFERENCE_RUNNER) \
		$(TEST_LIBS) $(RPATH)

$(TEST_FLOW_RUNTIME_RUNNER): tests/test_flow_runtime.c $(SRC_FLOW_RUNTIME) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_flow_runtime.c $(SRC_FLOW_RUNTIME) \
		$(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) \
		$(SRC_ASSET_DOCUMENT) \
		-o $(TEST_FLOW_RUNTIME_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_FLOW_BINDING_RUNNER): tests/test_flow_binding.c $(SRC_FLOW_BINDING) $(SRC_FLOW_RUNTIME) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_flow_binding.c $(SRC_FLOW_BINDING) \
		$(SRC_FLOW_RUNTIME) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) \
		$(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) \
		-o $(TEST_FLOW_BINDING_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_FLOW_WORKSPACE_RUNNER): tests/test_flow_workspace.c $(SRC_FLOW_WORKSPACE) $(SRC_UI_NESTED_INSPECTOR) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_flow_workspace.c $(SRC_FLOW_WORKSPACE) $(SRC_UI_NESTED_INSPECTOR) \
		$(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) \
		$(SRC_ASSET_DOCUMENT) \
		-o $(TEST_FLOW_WORKSPACE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_FLOW_PROJECT_CATALOG_RUNNER): tests/test_flow_project_catalog.c $(SRC_FLOW_PROJECT_CATALOG) $(SRC_SCENE_FLOW_ADAPTER) $(SRC_UI_DOCUMENT) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_ASSET_DOCUMENT) $(SRC_MAP_CATALOG) $(SRC_ASSET_LOADER) $(SRC_RGBA_PARSE) $(SRC_DECAL_IO) $(SRC_OBJECT_DOCUMENT) $(TEST_SCENE_DOCUMENT_SRC) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_flow_project_catalog.c \
		$(SRC_FLOW_PROJECT_CATALOG) $(SRC_SCENE_FLOW_ADAPTER) $(SRC_UI_DOCUMENT) \
		$(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_ASSET_DOCUMENT) \
		$(SRC_MAP_CATALOG) $(SRC_ASSET_LOADER) $(SRC_RGBA_PARSE) $(SRC_DECAL_IO) \
		$(SRC_OBJECT_DOCUMENT) $(TEST_SCENE_DOCUMENT_SRC) \
		-o $(TEST_FLOW_PROJECT_CATALOG_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_DOCUMENT_RUNNER): tests/test_ui_document.c $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_document.c $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) \
		$(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) \
		$(SRC_ASSET_DOCUMENT) \
		-o $(TEST_UI_DOCUMENT_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_LAYOUT_RESOLVER_RUNNER): tests/test_ui_layout_resolver.c $(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_layout_resolver.c \
		$(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_FLOW_REFERENCE) \
		$(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) \
		-o $(TEST_UI_LAYOUT_RESOLVER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_RENDER_ADAPTER_RUNNER): tests/test_ui_render_adapter.c $(SRC_UI_RENDER_ADAPTER) $(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_UI_CANVAS) $(SRC_GRID) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_render_adapter.c \
		$(SRC_UI_RENDER_ADAPTER) $(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) \
		$(SRC_UI_CANVAS) $(SRC_GRID) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		$(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) \
		$(SRC_ASSET_DOCUMENT) \
		-o $(TEST_UI_RENDER_ADAPTER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_INTERACTION_RUNNER): tests/test_ui_interaction.c $(SRC_UI_INTERACTION) $(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_interaction.c $(SRC_UI_INTERACTION) \
		$(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_FLOW_REFERENCE) \
		$(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) \
		-o $(TEST_UI_INTERACTION_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_MENU_RUNTIME_RUNNER): tests/test_ui_menu_runtime.c $(SRC_UI_MENU_RUNTIME) $(SRC_UI_RENDER_ADAPTER) $(SRC_UI_INTERACTION) $(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_UI_CANVAS) $(SRC_GRID) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) $(SRC_FLOW_BINDING) $(SRC_FLOW_RUNTIME) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_menu_runtime.c $(SRC_UI_MENU_RUNTIME) \
		$(SRC_UI_RENDER_ADAPTER) $(SRC_UI_INTERACTION) $(SRC_UI_LAYOUT_RESOLVER) \
		$(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_UI_CANVAS) $(SRC_GRID) $(SRC_ASSETS) \
		$(SRC_CHECKED_SIZE) $(SRC_FLOW_BINDING) $(SRC_FLOW_RUNTIME) \
		$(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) \
		$(SRC_ASSET_DOCUMENT) \
		-o $(TEST_UI_MENU_RUNTIME_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_UI_MENU_WORKSPACE_RUNNER): tests/test_ui_menu_workspace.c $(SRC_UI_MENU_WORKSPACE) $(SRC_UI_NESTED_INSPECTOR) $(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_MAP_CATALOG) $(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) $(SRC_ASSET_DOCUMENT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_ui_menu_workspace.c \
		$(SRC_UI_MENU_WORKSPACE) $(SRC_UI_NESTED_INSPECTOR) $(SRC_UI_LAYOUT_RESOLVER) $(SRC_UI_DOCUMENT) $(SRC_RGBA_PARSE) $(SRC_MAP_CATALOG) \
		$(SRC_FLOW_REFERENCE) $(SRC_FLOW_DOCUMENT) $(SRC_FLOW_DOCUMENT_PLATFORM) \
		$(SRC_ASSET_DOCUMENT) \
		-o $(TEST_UI_MENU_WORKSPACE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_ASSET_REFRESH_RUNNER): tests/test_asset_refresh.c $(SRC_ASSET_REFRESH) $(SRC_ASSET_DOCUMENT) $(SRC_MATERIAL_DOCUMENT) $(SRC_DECAL_DOCUMENT) $(SRC_ASSET_LOADER) $(SRC_RGBA_PARSE) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) $(SRC_SCENE_DOCUMENT) $(SRC_SCENE_FORMAT) $(SRC_SCENE_BLOCK_CODEC) $(SRC_SCENE_DIAGNOSTIC) $(SRC_CONFIG) $(SRC_MAP_LOADER) $(SRC_MAP) $(SRC_WORLD) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_asset_refresh.c $(SRC_ASSET_REFRESH) \
		$(SRC_ASSET_DOCUMENT) $(SRC_MATERIAL_DOCUMENT) $(SRC_DECAL_DOCUMENT) \
		$(SRC_ASSET_LOADER) $(SRC_RGBA_PARSE) $(SRC_DECAL_PAINTER) $(SRC_DECAL_IO) \
		$(SRC_SCENE_DOCUMENT) $(SRC_SCENE_FORMAT) $(SRC_SCENE_BLOCK_CODEC) \
		$(SRC_SCENE_DIAGNOSTIC) $(SRC_CONFIG) $(SRC_MAP_LOADER) $(SRC_MAP) \
		$(SRC_WORLD) $(SRC_ASSETS) $(SRC_CHECKED_SIZE) \
		-o $(TEST_ASSET_REFRESH_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_R9_OPTICAL_SEMANTICS_RUNNER): tests/test_r9_optical_semantics.c src/r9_optical_semantics.c $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) -DR9_OPTICAL_RESEARCH=1 $(INCLUDES) \
		tests/test_r9_optical_semantics.c src/r9_optical_semantics.c $(SRC_CHECKED_SIZE) \
		-o $(TEST_R9_OPTICAL_SEMANTICS_RUNNER) $(TEST_LIBS) $(RPATH)

r9-p1-memory-report: $(TEST_R9_OPTICAL_SEMANTICS_RUNNER)
	./$(TEST_R9_OPTICAL_SEMANTICS_RUNNER) --memory-report

$(TEST_R9_MULTIHIT_TRACE_RUNNER): tests/test_r9_multihit_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) -DR9_OPTICAL_RESEARCH=1 $(INCLUDES) \
		tests/test_r9_multihit_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) \
		-o $(TEST_R9_MULTIHIT_TRACE_RUNNER) $(TEST_LIBS) $(RPATH)

$(BENCH_R9_MULTIHIT_TRACE_RUNNER): tests/benchmark_r9_multihit_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_CAMERA) $(SRC_MAP) $(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) -DR9_OPTICAL_RESEARCH=1 $(INCLUDES) \
		tests/benchmark_r9_multihit_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_CAMERA) \
		$(SRC_MAP) $(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) \
		-o $(BENCH_R9_MULTIHIT_TRACE_RUNNER) $(TEST_LIBS) $(RPATH)

benchmark-r9-multihit-trace: $(BENCH_R9_MULTIHIT_TRACE_RUNNER)
	./$(BENCH_R9_MULTIHIT_TRACE_RUNNER)

$(TEST_R9_OPTICAL_COMPOSITOR_RUNNER): tests/test_r9_optical_compositor.c src/r9_optical_compositor.c | dirs
	$(CC) $(CFLAGS) -DR9_OPTICAL_RESEARCH=1 $(INCLUDES) \
		tests/test_r9_optical_compositor.c src/r9_optical_compositor.c \
		-o $(TEST_R9_OPTICAL_COMPOSITOR_RUNNER) $(TEST_LIBS) $(RPATH)

$(BENCH_R9_OPTICAL_COMPOSITOR_RUNNER): tests/benchmark_r9_optical_compositor.c src/r9_optical_compositor.c | dirs
	$(CC) $(CFLAGS) -DR9_OPTICAL_RESEARCH=1 $(INCLUDES) \
		tests/benchmark_r9_optical_compositor.c src/r9_optical_compositor.c \
		-o $(BENCH_R9_OPTICAL_COMPOSITOR_RUNNER) $(TEST_LIBS) $(RPATH)

benchmark-r9-optical-compositor: $(BENCH_R9_OPTICAL_COMPOSITOR_RUNNER)
	./$(BENCH_R9_OPTICAL_COMPOSITOR_RUNNER)

$(TEST_R9_MIRROR_TRACE_RUNNER): tests/test_r9_mirror_trace.c src/r9_mirror_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) -DR9_OPTICAL_RESEARCH=1 $(INCLUDES) \
		tests/test_r9_mirror_trace.c src/r9_mirror_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c \
		$(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) -o $(TEST_R9_MIRROR_TRACE_RUNNER) \
		$(TEST_LIBS) $(RPATH)

$(BENCH_R9_MIRROR_TRACE_RUNNER): tests/benchmark_r9_mirror_trace.c src/r9_mirror_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) -DR9_OPTICAL_RESEARCH=1 $(INCLUDES) \
		tests/benchmark_r9_mirror_trace.c src/r9_mirror_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c \
		$(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) -o $(BENCH_R9_MIRROR_TRACE_RUNNER) \
		$(TEST_LIBS) $(RPATH)

benchmark-r9-mirror-trace: $(BENCH_R9_MIRROR_TRACE_RUNNER)
	./$(BENCH_R9_MIRROR_TRACE_RUNNER)

$(TEST_OPTICAL_RUNTIME_VIEW_RUNNER): tests/test_optical_runtime_view.c $(SRC_OPTICAL_RUNTIME_VIEW) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_optical_runtime_view.c $(SRC_OPTICAL_RUNTIME_VIEW) \
		-o $(TEST_OPTICAL_RUNTIME_VIEW_RUNNER) $(TEST_LIBS) $(RPATH)

$(BENCH_OPTICAL_RUNTIME_VIEW_RUNNER): tests/benchmark_optical_runtime_view.c $(SRC_OPTICAL_RUNTIME_VIEW) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/benchmark_optical_runtime_view.c \
		$(SRC_OPTICAL_RUNTIME_VIEW) -o $(BENCH_OPTICAL_RUNTIME_VIEW_RUNNER) $(LIBS) $(RPATH)

benchmark-optical-runtime-view: $(BENCH_OPTICAL_RUNTIME_VIEW_RUNNER)
	./$(BENCH_OPTICAL_RUNTIME_VIEW_RUNNER)

$(TEST_HEIGHTFIELD_SELECTIVE_RUNNER): tests/test_heightfield_selective.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_heightfield_selective.c src/heightfield_trace.c src/heightfield_trace_selective.c \
		$(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) \
		-o $(TEST_HEIGHTFIELD_SELECTIVE_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_MIRROR_TRACE_RUNNER): tests/test_mirror_trace.c src/mirror_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_mirror_trace.c src/mirror_trace.c src/heightfield_trace.c src/heightfield_trace_selective.c \
		$(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MAP) $(SRC_CHECKED_SIZE) \
		-o $(TEST_MIRROR_TRACE_RUNNER) $(TEST_LIBS) $(RPATH)

$(BENCH_HEIGHTFIELD_SELECTIVE_RUNNER): tests/benchmark_heightfield_selective.c src/heightfield_trace.c src/heightfield_trace_selective.c $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_CAMERA) $(SRC_MAP) $(SRC_CHECKED_SIZE) $(SRC_MATH) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/benchmark_heightfield_selective.c src/heightfield_trace.c src/heightfield_trace_selective.c \
		$(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_CAMERA) $(SRC_MAP) $(SRC_CHECKED_SIZE) \
		$(SRC_MATH) $(SRC_INPUT) -o $(BENCH_HEIGHTFIELD_SELECTIVE_RUNNER) $(TEST_LIBS) $(RPATH)

benchmark-heightfield-selective: $(BENCH_HEIGHTFIELD_SELECTIVE_RUNNER)
	./$(BENCH_HEIGHTFIELD_SELECTIVE_RUNNER)

$(TEST_OPTICAL_RENDER_RUNNER): tests/test_optical_render.c $(SRC_RAYCAST_OPTICAL) $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_CONFIG) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_optical_render.c $(SRC_RAYCAST_OPTICAL) \
		$(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_CONFIG) \
		$(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) \
		-o $(TEST_OPTICAL_RENDER_RUNNER) $(TEST_LIBS) $(RPATH)

$(TEST_SPRITE_RENDER_RUNNER): tests/test_sprite_render.c src/sprite_render.c $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_sprite_render.c src/sprite_render.c \
		$(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_OPTICAL_RUNTIME_VIEW) $(SRC_MATH) \
		$(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) \
		-o $(TEST_SPRITE_RENDER_RUNNER) $(TEST_LIBS) $(RPATH)

$(BENCH_SPRITE_RENDER_RUNNER): tests/benchmark_sprite_render.c $(SRC_RAYCAST_OPTICAL) $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_CONFIG) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/benchmark_sprite_render.c $(SRC_RAYCAST_OPTICAL) \
		$(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_CONFIG) \
		$(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) \
		-o $(BENCH_SPRITE_RENDER_RUNNER) $(TEST_LIBS) $(RPATH)

benchmark-sprite-render: $(BENCH_SPRITE_RENDER_RUNNER)
	./$(BENCH_SPRITE_RENDER_RUNNER)

$(BENCH_OPTICAL_RENDER_RUNNER): tests/benchmark_optical_render.c $(SRC_RAYCAST_OPTICAL) $(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_CONFIG) $(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) tests/benchmark_optical_render.c $(SRC_RAYCAST_OPTICAL) \
		$(SRC_CHECKED_SIZE) $(SRC_GRID) $(SRC_MAP) $(SRC_CAMERA) $(SRC_CONFIG) \
		$(SRC_MATH) $(SRC_ASSETS) $(SRC_WORLD) $(SRC_INPUT) \
		-o $(BENCH_OPTICAL_RENDER_RUNNER) $(TEST_LIBS) $(RPATH)

benchmark-optical-render: $(BENCH_OPTICAL_RENDER_RUNNER)
	./$(BENCH_OPTICAL_RENDER_RUNNER)



run: $(APP)



	./$(APP) --mode raycast

run-normal: $(APP)
	./$(APP) --mode normal

ui-theme-demo: $(APP)
	./$(APP) --ui-theme-demo

ui-motion-demo: $(APP)
	./$(APP) --ui-motion-demo

ui-workbench: $(APP)
	./$(APP) --ui-workbench

run-stress: $(APP)
	./$(APP) --mode stress

TEST_RUNNERS := $(TEST_DEPS_RUNNER) $(TEST_CORE_RUNNER) $(TEST_DECALS_RUNNER) \
	$(TEST_MENU_STATE_RUNNER) $(TEST_DECAL_IO_RUNNER) $(TEST_DECAL_PAINTER_RUNNER) \
	$(TEST_UI_ELE_RUNNER) $(TEST_RGBA_PARSE_RUNNER) $(TEST_NUMBER_PARSE_RUNNER) \
	$(TEST_PLATFORM_CAPABILITIES_RUNNER) $(TEST_PLATFORM_NUMBER_RUNNER) \
	$(TEST_SCENE_DOCUMENT_RUNNER) $(TEST_SCENE_FORMAT_RUNNER) \
	$(TEST_COMMAND_SYSTEM_RUNNER) $(TEST_EDITOR_SELECTION_RUNNER) \
	$(TEST_EDITOR_HIGHLIGHT_RUNNER) $(TEST_EDITOR_DOMAIN_RUNNER) \
	$(TEST_UNIFIED_EDITOR_RUNNER) $(TEST_ENTITY_TRIGGER_SESSION_RUNNER) \
	$(TEST_SCENE_FLOW_ADAPTER_RUNNER) $(TEST_INPUT_RUNNER) $(TEST_CAMERA_RUNNER) \
	$(TEST_VERTICAL_PHYSICS_RUNNER) $(TEST_MAP_CATALOG_RUNNER) \
	$(TEST_GLYPH_CACHE_RUNNER) $(TEST_LIGHTING_CACHE_RUNNER) \
	$(TEST_LIGHTING_RUNNER) $(TEST_APP_OPTIONS_RUNNER) $(TEST_SMC_STATE_RUNNER) \
	$(TEST_SMC_INDEXED_RUNNER) $(TEST_BENCHMARK_RUNNER) $(TEST_APP_MODULES_RUNNER) \
	$(TEST_CONFIG_RUNNER) $(TEST_ASSET_LOADER_RUNNER) \
	$(TEST_DECAL_PROJECTION_RUNNER) $(TEST_UI_PREFERENCES_RUNNER) \
	$(TEST_UI_COMPOSITOR_RUNNER) $(TEST_UI_THEME_RUNNER) \
	$(TEST_UI_MOTION_RUNNER) \
	$(TEST_UI_PAUSE_MOTION_RUNNER) \
	$(TEST_UI_WORKBENCH_STORE_RUNNER) \
	$(TEST_UI_WORKBENCH_RUNNER) \
	$(TEST_UI_APP_THEME_ADAPTER_RUNNER) $(TEST_UI_THEME_DEMO_RUNNER) \
	$(TEST_UI_MOTION_DEMO_RUNNER) \
	$(TEST_MATERIAL_DOCUMENT_RUNNER) \
	$(TEST_DECAL_DOCUMENT_RUNNER) $(TEST_SPRITE_DOCUMENT_RUNNER) \
	$(TEST_SPRITE_ANIMATION_PLAYER_RUNNER) $(TEST_OBJECT_DOCUMENT_RUNNER) \
	$(TEST_FLOW_DOCUMENT_RUNNER) $(TEST_FLOW_REFERENCE_RUNNER) \
	$(TEST_FLOW_RUNTIME_RUNNER) $(TEST_FLOW_BINDING_RUNNER) \
	$(TEST_FLOW_WORKSPACE_RUNNER) $(TEST_FLOW_PROJECT_CATALOG_RUNNER) \
	$(TEST_UI_DOCUMENT_RUNNER) $(TEST_UI_LAYOUT_RESOLVER_RUNNER) \
	$(TEST_UI_RENDER_ADAPTER_RUNNER) $(TEST_UI_INTERACTION_RUNNER) \
	$(TEST_UI_MENU_RUNTIME_RUNNER) $(TEST_UI_MENU_WORKSPACE_RUNNER) \
	$(TEST_ASSET_REFRESH_RUNNER) $(TEST_R9_OPTICAL_SEMANTICS_RUNNER) \
	$(TEST_R9_MULTIHIT_TRACE_RUNNER) $(TEST_R9_OPTICAL_COMPOSITOR_RUNNER) \
	$(TEST_R9_MIRROR_TRACE_RUNNER) $(TEST_OPTICAL_RUNTIME_VIEW_RUNNER) \
	$(TEST_HEIGHTFIELD_SELECTIVE_RUNNER) $(TEST_MIRROR_TRACE_RUNNER) \
	$(TEST_OPTICAL_RENDER_RUNNER) $(TEST_SPRITE_RENDER_RUNNER)

test-build: $(TEST_RUNNERS)
	@echo "PASS: complete test runner inventory built"

test: $(TEST_RUNNERS)
	./$(TEST_DEPS_RUNNER)
	./$(TEST_CORE_RUNNER)
	./$(TEST_DECALS_RUNNER)
	./$(TEST_MENU_STATE_RUNNER)
	./$(TEST_DECAL_IO_RUNNER)
	./$(TEST_DECAL_PAINTER_RUNNER)
	./$(TEST_UI_ELE_RUNNER)
	./$(TEST_RGBA_PARSE_RUNNER)
	./$(TEST_NUMBER_PARSE_RUNNER)
	./$(TEST_PLATFORM_CAPABILITIES_RUNNER)
	./$(TEST_PLATFORM_NUMBER_RUNNER)
	./$(TEST_SCENE_DOCUMENT_RUNNER)
	./$(TEST_SCENE_FORMAT_RUNNER)
	./$(TEST_COMMAND_SYSTEM_RUNNER)
	./$(TEST_EDITOR_SELECTION_RUNNER)
	./$(TEST_EDITOR_HIGHLIGHT_RUNNER)
	./$(TEST_EDITOR_DOMAIN_RUNNER)
	./$(TEST_UNIFIED_EDITOR_RUNNER)
	./$(TEST_ENTITY_TRIGGER_SESSION_RUNNER)
	./$(TEST_SCENE_FLOW_ADAPTER_RUNNER)
	./$(TEST_INPUT_RUNNER)
	./$(TEST_CAMERA_RUNNER)
	./$(TEST_VERTICAL_PHYSICS_RUNNER)
	./$(TEST_MAP_CATALOG_RUNNER)
	./$(TEST_GLYPH_CACHE_RUNNER)
	./$(TEST_LIGHTING_CACHE_RUNNER)
	./$(TEST_LIGHTING_RUNNER)
	./$(TEST_APP_OPTIONS_RUNNER)
	./$(TEST_SMC_STATE_RUNNER)
	./$(TEST_SMC_INDEXED_RUNNER)
	./$(TEST_BENCHMARK_RUNNER)
	./$(TEST_APP_MODULES_RUNNER)
	./$(TEST_CONFIG_RUNNER)
	./$(TEST_ASSET_LOADER_RUNNER)
	./$(TEST_DECAL_PROJECTION_RUNNER)
	./$(TEST_UI_PREFERENCES_RUNNER)
	./$(TEST_UI_COMPOSITOR_RUNNER)
	./$(TEST_UI_THEME_RUNNER)
	./$(TEST_UI_MOTION_RUNNER)
	./$(TEST_UI_PAUSE_MOTION_RUNNER)
	./$(TEST_UI_WORKBENCH_STORE_RUNNER)
	./$(TEST_UI_WORKBENCH_RUNNER)
	./$(TEST_UI_APP_THEME_ADAPTER_RUNNER)
	./$(TEST_UI_THEME_DEMO_RUNNER)
	./$(TEST_UI_MOTION_DEMO_RUNNER)
	./$(TEST_MATERIAL_DOCUMENT_RUNNER)
	./$(TEST_DECAL_DOCUMENT_RUNNER)
	./$(TEST_SPRITE_DOCUMENT_RUNNER)
	./$(TEST_SPRITE_ANIMATION_PLAYER_RUNNER)
	./$(TEST_OBJECT_DOCUMENT_RUNNER)
	./$(TEST_FLOW_DOCUMENT_RUNNER)
	./$(TEST_FLOW_REFERENCE_RUNNER)
	./$(TEST_FLOW_RUNTIME_RUNNER)
	./$(TEST_FLOW_BINDING_RUNNER)
	./$(TEST_FLOW_WORKSPACE_RUNNER)
	./$(TEST_FLOW_PROJECT_CATALOG_RUNNER)
	./$(TEST_UI_DOCUMENT_RUNNER)
	./$(TEST_UI_LAYOUT_RESOLVER_RUNNER)
	./$(TEST_UI_RENDER_ADAPTER_RUNNER)
	./$(TEST_UI_INTERACTION_RUNNER)
	./$(TEST_UI_MENU_RUNTIME_RUNNER)
	./$(TEST_UI_MENU_WORKSPACE_RUNNER)
	./$(TEST_ASSET_REFRESH_RUNNER)
	./$(TEST_R9_OPTICAL_SEMANTICS_RUNNER)
	./$(TEST_R9_MULTIHIT_TRACE_RUNNER)
	./$(TEST_R9_OPTICAL_COMPOSITOR_RUNNER)
	./$(TEST_R9_MIRROR_TRACE_RUNNER)
	./$(TEST_OPTICAL_RUNTIME_VIEW_RUNNER)
	./$(TEST_HEIGHTFIELD_SELECTIVE_RUNNER)
	./$(TEST_MIRROR_TRACE_RUNNER)
	./$(TEST_OPTICAL_RENDER_RUNNER)
	./$(TEST_SPRITE_RENDER_RUNNER)
	@echo "Note: benchmark and stability require a video environment to fully run."

test-ui-standards: $(TEST_UI_ELE_RUNNER) $(TEST_UI_PREFERENCES_RUNNER) \
	$(TEST_UI_COMPOSITOR_RUNNER) $(TEST_UI_THEME_RUNNER) \
	$(TEST_UI_MOTION_RUNNER) \
	$(TEST_UI_PAUSE_MOTION_RUNNER) \
	$(TEST_UI_WORKBENCH_STORE_RUNNER) \
	$(TEST_UI_WORKBENCH_RUNNER) \
	$(TEST_UI_APP_THEME_ADAPTER_RUNNER) $(TEST_UI_THEME_DEMO_RUNNER) \
	$(TEST_UI_MOTION_DEMO_RUNNER) \
	$(TEST_UI_DOCUMENT_RUNNER) \
	$(TEST_UI_LAYOUT_RESOLVER_RUNNER) $(TEST_UI_RENDER_ADAPTER_RUNNER) \
	$(TEST_UI_INTERACTION_RUNNER) $(TEST_UI_MENU_RUNTIME_RUNNER) \
	$(TEST_UI_MENU_WORKSPACE_RUNNER)
	./$(TEST_UI_ELE_RUNNER)
	./$(TEST_UI_PREFERENCES_RUNNER)
	./$(TEST_UI_COMPOSITOR_RUNNER)
	./$(TEST_UI_THEME_RUNNER)
	./$(TEST_UI_MOTION_RUNNER)
	./$(TEST_UI_PAUSE_MOTION_RUNNER)
	./$(TEST_UI_WORKBENCH_STORE_RUNNER)
	./$(TEST_UI_WORKBENCH_RUNNER)
	./$(TEST_UI_APP_THEME_ADAPTER_RUNNER)
	./$(TEST_UI_THEME_DEMO_RUNNER)
	./$(TEST_UI_MOTION_DEMO_RUNNER)
	./$(TEST_UI_DOCUMENT_RUNNER)
	./$(TEST_UI_LAYOUT_RESOLVER_RUNNER)
	./$(TEST_UI_RENDER_ADAPTER_RUNNER)
	./$(TEST_UI_INTERACTION_RUNNER)
	./$(TEST_UI_MENU_RUNTIME_RUNNER)
	./$(TEST_UI_MENU_WORKSPACE_RUNNER)
check: all test standards

standards: style check-static-analysis-policy standards-core

standards-core: check-unsafe-calls check-project-structure check-test-inventory \
	check-legacy-unused check-current-renderer

asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O1 -g -fsanitize=address -fno-omit-frame-pointer" test

ubsan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O1 -g -fsanitize=undefined -fno-omit-frame-pointer" test

sanitize: asan ubsan

leak:
	@VALGRIND_IMAGE="$(VALGRIND_IMAGE)" tools/valgrind-container/docker-gate.sh

leak-native:
	@VALGRIND="$(VALGRIND)" tools/valgrind-container/native-gate.sh

leak-image:
	@if ! command -v $(DOCKER) >/dev/null 2>&1; then \
		echo "FAIL-MISSING-TOOL: reason=docker-not-found"; exit 2; \
	fi
	@$(DOCKER) build --pull=false -t "$(VALGRIND_IMAGE)" tools/valgrind-container

leak-image-self-test:
	@if ! command -v $(DOCKER) >/dev/null 2>&1; then \
		echo "FAIL-MISSING-TOOL: reason=docker-not-found"; exit 2; \
	fi
	@status=0; $(DOCKER) run --rm --network none --cap-drop ALL \
		--security-opt no-new-privileges --read-only \
		--tmpfs /tmp:rw,exec,nosuid,nodev,mode=1777 \
		"$(VALGRIND_IMAGE)" --self-test || status=$$?; \
	tools/valgrind-container/classify.sh docker $$status /dev/null

test-leak-classifier:
	@tools/valgrind-container/test-classify.sh

test-platform-harness:
	@tools/platform-profiles/test-classify.sh
	@tools/platform-profiles/test-clang-diagnostics.sh
	@tools/platform-profiles/test-clang-sweep-integration.sh
	@tools/platform-profiles/test-check.sh
	@tools/platform-profiles/test-make-platform-libs.sh
	@tools/platform-profiles/test-clean-preserves-acceptance.sh
	@tools/platform-profiles/test-provider.sh
	@tools/platform-profiles/test-libvirt-lifecycle.sh
	@PYTHONDONTWRITEBYTECODE=1 tools/platform-profiles/test-windows-qga.py
	@PYTHONDONTWRITEBYTECODE=1 tools/platform-profiles/test-windows-guest.py
	@PYTHONDONTWRITEBYTECODE=1 tools/platform-profiles/test-windows-dependencies.py
	@PYTHONDONTWRITEBYTECODE=1 tools/platform-profiles/test-windows-product.py
	@PYTHONDONTWRITEBYTECODE=1 python3 tools/display-acceptance/test_x11_drive.py
	@tools/platform-profiles/test-survey.sh

platform-image-ubuntu-gcc:
	@tools/platform-profiles/build-image.sh tools/platform-profiles/profiles/ubuntu-24.04-gcc.env

platform-image-ubuntu-clang:
	@tools/platform-profiles/build-image.sh tools/platform-profiles/profiles/ubuntu-24.04-clang.env

platform-image-fedora-gcc:
	@tools/platform-profiles/build-image.sh tools/platform-profiles/profiles/fedora-gcc.env

platform-image-alpine-gcc:
	@tools/platform-profiles/build-image.sh tools/platform-profiles/profiles/alpine-musl-gcc.env

platform-images: platform-image-ubuntu-gcc platform-image-ubuntu-clang \
	platform-image-fedora-gcc platform-image-alpine-gcc

platform-test-ubuntu-gcc:
	@tools/platform-profiles/run-one.sh tools/platform-profiles/profiles/ubuntu-24.04-gcc.env

platform-test-ubuntu-clang:
	@tools/platform-profiles/run-one.sh tools/platform-profiles/profiles/ubuntu-24.04-clang.env

platform-test-fedora-gcc:
	@tools/platform-profiles/run-one.sh tools/platform-profiles/profiles/fedora-gcc.env

platform-test-alpine-gcc:
	@tools/platform-profiles/run-one.sh tools/platform-profiles/profiles/alpine-musl-gcc.env

platform-test-windows:
	@tools/platform-profiles/run-provider.sh tools/platform-profiles/profiles/windows-10-x64-gcc.env

platform-bootstrap-windows-dependencies:
	@test -n "$(WINDOWS_PROFILE)" || { echo "FAIL-MISSING-TOOL: reason=windows-profile-not-configured"; exit 2; }
	@WINDOWS_DEPENDENCY_STEP="$(WINDOWS_DEPENDENCY_STEP)" tools/platform-profiles/bootstrap-windows-dependencies.sh "$(WINDOWS_PROFILE)"

platform-survey:
	@tools/platform-profiles/survey.sh

platform-check:
	@tools/platform-profiles/check.sh

coverage:
	@if ! command -v $(GCOV) >/dev/null 2>&1; then \
		echo "FAIL-MISSING-TOOL: $(GCOV) was not found; coverage evidence was not produced"; \
		exit 2; \
	fi
	@$(MAKE) clean
	@if ! $(MAKE) CFLAGS="$(CFLAGS) -O0 -g --coverage" test; then \
		echo "FAIL-PRODUCT: coverage build or test execution failed"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)/coverage
	@status=0; $(GCOV) -o $(BUILD_DIR) $(BUILD_DIR)/*.gcno || status=$$?; \
	for report in ./*.gcov; do \
		if test -f "$$report"; then mv "$$report" $(BUILD_DIR)/coverage/; fi; \
	done; \
	if test $$status -ne 0; then \
		echo "FAIL-TOOL: $(GCOV) could not produce coverage reports (status $$status)"; \
		exit 3; \
	fi
	@echo "PASS: coverage gate passed"

style:
	@if ! command -v $(CPPCHECK) >/dev/null 2>&1; then \
		echo "FAIL-MISSING-TOOL: $(CPPCHECK) was not found; static-analysis evidence was not produced"; \
		exit 2; \
	fi
	@status=0; $(CPPCHECK) $(CPPCHECK_FLAGS) src || status=$$?; \
	if test $$status -eq 100; then \
		echo "FAIL-PRODUCT: cppcheck reported source defects"; \
		exit 1; \
	elif test $$status -ne 0; then \
		echo "FAIL-TOOL: cppcheck could not complete analysis (status $$status)"; \
		exit 3; \
	fi
	@echo "PASS: cppcheck static analysis passed"

check-static-analysis-policy:
	@echo "Checking cppcheck policy keeps defect diagnostics strict..."
	@printf '%s\n' '$(CPPCHECK_FLAGS)' | grep -F -- '--error-exitcode=100' >/dev/null
	@printf '%s\n' '$(CPPCHECK_FLAGS)' | grep -F -- '--std=c11' >/dev/null
	@printf '%s\n' '$(CPPCHECK_FLAGS)' | grep -F -- '--suppress=normalCheckLevelMaxBranches' >/dev/null
	@printf '%s\n' '$(CPPCHECK_FLAGS)' | grep -F -- '--suppress=toomanyconfigs' >/dev/null
	@! printf '%s\n' '$(CPPCHECK_FLAGS)' | grep -E -- '--suppress=(\*|error|warning|style|performance|portability)([[:space:]]|$$)' >/dev/null
	@echo "PASS: cppcheck suppressions are limited to analysis-scope information"

check-unsafe-calls:
	@echo "Checking handwritten production C for banned unsafe conversion and string calls..."
	@! grep -RsnE "\\b(sscanf|strcpy|strcat|sprintf|gets|scanf|atoi|atof)[[:space:]]*[(]" src/*.c src/*.h >/dev/null
	@echo "PASS: banned unsafe calls are absent from handwritten production C"

check-project-structure:
	@echo "Checking project C/build structure..."
	@! find src tests -maxdepth 1 -type f \( -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \) | grep . >/dev/null
	@! find . -path './vendor' -prune -o -path './build' -prune -o -type f \
		\( -name CMakeLists.txt -o -name meson.build -o -name build.ninja \) -print | grep . >/dev/null
	@! grep -RsnE '^#[[:space:]]*include[[:space:]]+".*_test[.]h"' src/*.h >/dev/null
	@! grep -RsnE '^#[[:space:]]*include[[:space:]]+".*_test[.]h"' src/*.c | \
		grep -v '^src/unified_editor[.]c:.*"unified_editor_test[.]h"' >/dev/null
	@grep -F -- '-Wall -Wextra -Wpedantic -Werror' Makefile >/dev/null
	@echo "PASS: project uses C sources, Make, production-safe headers, and warnings-as-errors"

check-test-inventory:
	@echo "Checking that every focused test source is in the canonical aggregate..."
	@set -e; runners=" $(notdir $(TEST_RUNNERS)) "; \
	for source in tests/test_*.c; do \
		runner=$$(basename "$$source" .c | tr '_' '-'); \
		case "$$runners" in *" $$runner "*) ;; \
			*) echo "FAIL-PRODUCT: $$source is missing from TEST_RUNNERS as $$runner"; exit 1;; \
		esac; \
	done
	@echo "PASS: every focused test source is included in make test"

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

check-current-renderer:
	@echo "Checking that production, tests, and benchmarks use current render/lighting APIs..."
	@! grep -RsnE "\\braycast_render_height[[:space:]]*[(]" src/*.c tests/*.c | grep -v "src/raycast.c:" >/dev/null
	@! grep -RsnE "\\blighting_update[[:space:]]*[(]" src/*.c tests/*.c | grep -v "src/lighting.c:" >/dev/null
	@echo "OK: deprecated renderer and lighting entry points have no callers"

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

display-acceptance-linux: all
	@tools/display-acceptance/run-x11.sh

benchmark: $(APP)
	./$(APP) --benchmark-raycast 5

verification-environment:
	@printf 'verification_environment:\n'
	@printf '  system: '; uname -srm
	@printf '  compiler: '; $(CC) --version | head -n 1
	@printf '  cflags: %s\n' '$(CFLAGS)'
	@printf '  processors: '; getconf _NPROCESSORS_ONLN 2>/dev/null || echo unknown
	@printf '  sdl_artifact: '; file "$(VENDOR_DIR)/lib64/libSDL3.so"
	@printf '  cmocka_artifact: '; file "$(VENDOR_DIR)/lib64/libcmocka.so"

benchmark-headless: verification-environment benchmark-editor-highlight benchmark-surface-render \
	benchmark-colored-lighting benchmark-sprite-render \
	benchmark-optical-runtime-view benchmark-heightfield-selective \
	benchmark-optical-render benchmark-r9-mirror-trace

stability: $(APP)
	./$(APP) --stability-test 30

stability-fast: test smoke

stability-headless: stability-fast stability-editor-highlight \
	stability-surface-render stability-optical-render

benchmark-editor-highlight: $(BENCH_EDITOR_HIGHLIGHT_RUNNER)
	./$(BENCH_EDITOR_HIGHLIGHT_RUNNER)

stability-editor-highlight: $(BENCH_EDITOR_HIGHLIGHT_RUNNER)
	./$(BENCH_EDITOR_HIGHLIGHT_RUNNER) --stability

benchmark-surface-render: $(BENCH_SURFACE_RENDER_RUNNER)
	./$(BENCH_SURFACE_RENDER_RUNNER)

benchmark-colored-lighting: $(BENCH_COLORED_LIGHTING_RUNNER)
	./$(BENCH_COLORED_LIGHTING_RUNNER)

stability-surface-render: $(BENCH_SURFACE_RENDER_RUNNER)
	./$(BENCH_SURFACE_RENDER_RUNNER) --stability

stability-optical-render: $(BENCH_OPTICAL_RENDER_RUNNER)
	./$(BENCH_OPTICAL_RENDER_RUNNER) --stability

clean:
	@if test -d "$(BUILD_DIR)"; then \
		find "$(BUILD_DIR)" -mindepth 1 -maxdepth 1 \
			! -name display-acceptance \
			-exec rm -rf -- {} +; \
	fi
	rm -f -- *.gcov
