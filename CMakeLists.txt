cmake_minimum_required(VERSION 3.19)
project(Oai VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools Test)

qt_standard_project_setup()

# --- rlottie via FetchContent ------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    rlottie
    URL      https://github.com/Samsung/rlottie/archive/refs/tags/v0.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# NOTE: rlottie v0.2 does NOT have a RLOTTIE_BUILD_EXAMPLE option.
# We patch out the example directory below after FetchContent populates it.
# rlottie declares cmake_minimum_required(3.3) which is incompatible with CMake 4.x
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE INTERNAL "" FORCE)

# Patch rlottie before building: its ARMv7 NEON assembly is incompatible with Apple Silicon (ARM64)
FetchContent_GetProperties(rlottie)
if(NOT rlottie_POPULATED)
    FetchContent_Populate(rlottie)
    if(APPLE)
        # rlottie's ARMv7 NEON assembly is incompatible with Apple Silicon (ARM64/AArch64).
        # Provide C++ fallback implementations for the missing assembly symbols.
        set(NEON_PATCH_FILE "${rlottie_SOURCE_DIR}/src/vector/vdrawhelper_neon.cpp")
        file(READ "${NEON_PATCH_FILE}" NEON_PATCH_CONTENT)
        if(NOT NEON_PATCH_CONTENT MATCHES "pixman_composite_src_n_8888_asm_neon.*fallback")
            file(APPEND "${NEON_PATCH_FILE}" "

/* macOS fallback for missing ARMv7 NEON assembly symbols */
#if defined(__APPLE__)
extern \"C\" void pixman_composite_src_n_8888_asm_neon(int32_t w, int32_t h,
                                                     uint32_t *dst,
                                                     int32_t   dst_stride,
                                                     uint32_t  src)
{
    for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
            dst[y * dst_stride + x] = src;
        }
    }
}

extern \"C\" void pixman_composite_over_n_8888_asm_neon(int32_t w, int32_t h,
                                                      uint32_t *dst,
                                                      int32_t   dst_stride,
                                                      uint32_t  src)
{
    uint8_t src_a = src >> 24;
    if (src_a == 0) return;
    for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
            uint32_t d = dst[y * dst_stride + x];
            uint8_t da = d >> 24;
            uint8_t dr = (d >> 16) & 0xFF;
            uint8_t dg = (d >> 8) & 0xFF;
            uint8_t db = d & 0xFF;
            uint8_t sa = src >> 24;
            uint8_t sr = (src >> 16) & 0xFF;
            uint8_t sg = (src >> 8) & 0xFF;
            uint8_t sb = src & 0xFF;
            uint16_t inv_sa = 255 - sa;
            uint8_t a = sa + (da * inv_sa / 255);
            uint8_t r = sr + (dr * inv_sa / 255);
            uint8_t g = sg + (dg * inv_sa / 255);
            uint8_t b = sb + (db * inv_sa / 255);
            dst[y * dst_stride + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}
#endif
")
        endif()
    endif()

    # Patch rlottie vrle.cpp: GCC 13+ requires explicit <limits> include
    set(VRLE_FILE "${rlottie_SOURCE_DIR}/src/vector/vrle.cpp")
    file(READ "${VRLE_FILE}" VRLE_CONTENT)
    if(NOT VRLE_CONTENT MATCHES "#include <limits>")
        string(REPLACE "#include <vector>" "#include <vector>\n#include <limits>"
               VRLE_PATCHED "${VRLE_CONTENT}")
        file(WRITE "${VRLE_FILE}" "${VRLE_PATCHED}")
    endif()

    # Patch rlottie CMakeLists.txt to remove version script linker flag.
    # It uses ${CMAKE_SOURCE_DIR}/rlottie.expmap which resolves to the
    # top-level project dir (not rlottie's) when used via FetchContent.
    set(RLOTTIE_CMAKELISTS "${rlottie_SOURCE_DIR}/CMakeLists.txt")
    file(READ "${RLOTTIE_CMAKELISTS}" RLOTTIE_CMAKE_CONTENT)
    string(REGEX REPLACE "\"-Wl,--version-script=[^\"]*rlottie\\.expmap\"" "\"\""
           RLOTTIE_CMAKE_CONTENT "${RLOTTIE_CMAKE_CONTENT}")

    # Also patch out the example directory — rlottie v0.2 unconditionally builds
    # lottie2gif which Qt Creator auto-detects as a run target.
    string(REPLACE "add_subdirectory(example)"
                    "# add_subdirectory(example)  # disabled by Qlippy"
                    RLOTTIE_CMAKE_PACHED "${RLOTTIE_CMAKE_CONTENT}")
    if(NOT RLOTTIE_CMAKE_CONTENT STREQUAL RLOTTIE_CMAKE_PACHED)
        file(WRITE "${RLOTTIE_CMAKELISTS}" "${RLOTTIE_CMAKE_PACHED}")
    endif()

    add_subdirectory(${rlottie_SOURCE_DIR} ${rlottie_BINARY_DIR})
endif()

# --- Executable --------------------------------------------------------------
if(WIN32)
    set(PLATFORM_SOURCES assets/qlippy.rc)
endif()

qt_add_executable(Oai
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${PLATFORM_SOURCES}
    src/mainwindow.cpp
    src/mainwindow.h
    src/SpriteAnimationEngine.cpp
    src/SpriteAnimationEngine.h
    src/LottieAnimationEngine.cpp
    src/LottieAnimationEngine.h
    src/LottieEffectOverlay.cpp
    src/LottieEffectOverlay.h
    src/SpritePack.cpp
    src/SpritePack.h
    src/SpritePackManager.cpp
    src/SpritePackManager.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
    src/IpcServer.cpp
    src/IpcServer.h
    src/UdpWorker.cpp
    src/UdpWorker.h
    src/EventRouter.cpp
    src/EventRouter.h
    src/TipsEngine.cpp
    src/TipsEngine.h
    src/ConfigManager.cpp
    src/ConfigManager.h
    src/SystemTray.cpp
    src/SystemTray.h
    src/TipBubbleWidget.cpp
    src/TipBubbleWidget.h
    src/SettingsPanelWidget.cpp
    src/SettingsPanelWidget.h
)

target_include_directories(Oai PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Pass version to C++ code
target_compile_definitions(Oai PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

# --- Icon resource -----------------------------------------------------------
qt_add_resources(Oai "icons"
    PREFIX "/icons"
    BASE "assets/icons"
    FILES
        assets/icons/clippy.png
)

# --- Translations ------------------------------------------------------------
qt_add_translations(
    TARGETS Oai
    TS_FILES Oai_zh_CN.ts
)

# --- Link libraries ----------------------------------------------------------
target_link_libraries(Oai
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        rlottie::rlottie
)

# rlottie header path (FetchContent puts it in build/_deps)
target_include_directories(Oai PRIVATE
    ${rlottie_SOURCE_DIR}/inc
)

# --- Install -----------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS Oai
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# --- Generate official packs -------------------------------------------------
# Generate clippy.opk from the pack directory during build
set(PACKS_DIR ${CMAKE_BINARY_DIR}/packs)
file(MAKE_DIRECTORY ${PACKS_DIR})

add_custom_command(
    OUTPUT ${PACKS_DIR}/clippy.opk
    COMMAND ${CMAKE_COMMAND} -E tar "cf" ${PACKS_DIR}/clippy.opk --format=zip --
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/manifest.json
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/preview.png
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/map.png
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/animations.json
    DEPENDS
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/manifest.json
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/preview.png
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/map.png
        ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/animations.json
    COMMENT "Generating clippy.opk"
    WORKING_DIRECTORY ${PACKS_DIR}
)
add_custom_target(generate_packs ALL DEPENDS ${PACKS_DIR}/clippy.opk)
add_dependencies(Oai generate_packs)

# Install the generated packs
install(DIRECTORY ${PACKS_DIR}/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/packs
)

# --- macOS: app icon & hide Dock icon ----------------------------------------
if(APPLE)
    set_target_properties(Oai PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "oai.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "im.cheng.oai"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    set_source_files_properties(assets/oai.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(Oai PRIVATE assets/oai.icns)

    # Include packs in the macOS bundle Resources
    set_source_files_properties(${PACKS_DIR}/clippy.opk PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources/packs"
    )
    target_sources(Oai PRIVATE ${PACKS_DIR}/clippy.opk)

    add_custom_command(TARGET Oai POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env python3
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/hide_dock_icon.py"
            "$<TARGET_BUNDLE_DIR:Oai>/Contents/Info.plist"
        COMMENT "Hiding Oai from Dock (LSUIElement)"
    )
endif()

qt_generate_deploy_app_script(
    TARGET Oai
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# --- Set Oai as default target for Qt Creator -----------------------------
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Oai)

# --- Tests -------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
