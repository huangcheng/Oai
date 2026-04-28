cmake_minimum_required(VERSION 3.19)
project(Oai VERSION 1.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools Test)

qt_standard_project_setup()

# --- Analytics provider selection -----------------------------------------------
set(OAI_ANALYTICS_ENABLED ON CACHE BOOL "Enable analytics compilation")
set(OAI_ANALYTICS_PROVIDER "ga4" CACHE STRING "Analytics provider (ga4|mock)")
set_property(CACHE OAI_ANALYTICS_PROVIDER PROPERTY STRINGS ga4 mock)
set(OAI_ANALYTICS_MEASUREMENT_ID "G-1Y5F7JVJF4" CACHE STRING "GA4 Measurement ID (e.g. G-XXXXXXXXXX)")
set(OAI_ANALYTICS_API_SECRET "CTWwLksiT7udY9b_WCKpmg" CACHE STRING "GA4 API Secret")

if(OAI_ANALYTICS_ENABLED)
    message(STATUS "Analytics enabled, provider: ${OAI_ANALYTICS_PROVIDER}")
    if(OAI_ANALYTICS_PROVIDER STREQUAL "ga4")
        if(OAI_ANALYTICS_MEASUREMENT_ID STREQUAL "" OR OAI_ANALYTICS_API_SECRET STREQUAL "")
            message(FATAL_ERROR "GA4 provider requires OAI_ANALYTICS_MEASUREMENT_ID and OAI_ANALYTICS_API_SECRET")
        endif()
    endif()
else()
    message(STATUS "Analytics disabled — zero binary footprint")
endif()

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
    # FetchContent_Populate is needed (not MakeAvailable) because we patch
    # rlottie source files between population and add_subdirectory.  Silence
    # CMP0169 deprecation — the two-step pattern is intentional here.
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
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

# --- Live2D Cubism SDK -------------------------------------------------------
set(CUBISM_SDK_DIR ${CMAKE_SOURCE_DIR}/thirdparty/CubismNativeSamples)
set(CUBISM_FRAMEWORK_DIR ${CMAKE_SOURCE_DIR}/thirdparty/CubismNativeFramework)
set(CUBISM_CORE_DIR ${CUBISM_SDK_DIR}/Core)

# Live2D is non-optional. Every shipped pack is Live2D, so a build without
# the SDK produces an Oai.exe that ships unrenderable assets and shows a
# transparent window at runtime. Refuse to configure rather than ship that.

if(EXISTS ${CUBISM_CORE_DIR}/include/Live2DCubismCore.h)
    message(STATUS "Live2D Cubism SDK found")

    # GLEW (downloaded by Samples/OpenGL/thirdParty/scripts/setup_glew_glfw)
    set(GLEW_DIR ${CUBISM_SDK_DIR}/Samples/OpenGL/thirdParty/glew)
    if(NOT EXISTS ${GLEW_DIR}/include)
        message(FATAL_ERROR "GLEW not found. Run: cd thirdparty/CubismNativeSamples/Samples/OpenGL/thirdParty && bash scripts/setup_glew_glfw")
    endif()

    # Build GLEW as static library
    set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GLEW_DIR}/build/cmake ${CMAKE_CURRENT_BINARY_DIR}/glew)
    target_compile_definitions(glew_s PRIVATE GLEW_NO_GLU)
    if(APPLE)
        # Suppress macOS OpenGL deprecation warnings
        add_compile_definitions(GL_SILENCE_DEPRECATION)
    endif()

    # Import Cubism Core (pre-built binary) — platform-specific paths
    # Use UNKNOWN IMPORTED so both static (.a/.lib) and shared (.dll) linking works
    add_library(Live2DCubismCore UNKNOWN IMPORTED)
    if(APPLE)
        EXECUTE_PROCESS(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE)
        set_target_properties(Live2DCubismCore PROPERTIES
            IMPORTED_LOCATION ${CUBISM_CORE_DIR}/lib/macos/${ARCHITECTURE}/libLive2DCubismCore.a
            INTERFACE_INCLUDE_DIRECTORIES ${CUBISM_CORE_DIR}/include
        )
    elseif(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_CUBISM_ARCH x86_64)
        else()
            set(_CUBISM_ARCH x86)
        endif()
        if(MSVC)
            # MSVC: use static library from lib/ directory
            if(MSVC_TOOLSET_VERSION)
                set(_CUBISM_MSVC ${MSVC_TOOLSET_VERSION})
            else()
                set(_CUBISM_MSVC 143)
            endif()
            set_target_properties(Live2DCubismCore PROPERTIES
                IMPORTED_LOCATION ${CUBISM_CORE_DIR}/lib/windows/${_CUBISM_ARCH}/${_CUBISM_MSVC}/Live2DCubismCore_MD.lib
                IMPORTED_LOCATION_DEBUG ${CUBISM_CORE_DIR}/lib/windows/${_CUBISM_ARCH}/${_CUBISM_MSVC}/Live2DCubismCore_MDd.lib
                INTERFACE_INCLUDE_DIRECTORIES ${CUBISM_CORE_DIR}/include
            )
        else()
            # MinGW/Clang: use DLL import library from dll/ directory.
            # llvm-mingw's LLD linker can consume MSVC import libraries directly.
            # At runtime, Live2DCubismCore.dll must be next to the executable.
            set_target_properties(Live2DCubismCore PROPERTIES
                IMPORTED_LOCATION ${CUBISM_CORE_DIR}/dll/windows/${_CUBISM_ARCH}/Live2DCubismCore.dll
                IMPORTED_IMPLIB ${CUBISM_CORE_DIR}/dll/windows/${_CUBISM_ARCH}/Live2DCubismCore.lib
                INTERFACE_INCLUDE_DIRECTORIES ${CUBISM_CORE_DIR}/include
            )
            # Bundle the runtime DLL into the install tree so the packaged
            # build can find it. windeployqt only handles Qt's own DLLs;
            # without this, an installed Oai.exe fails at startup with
            #   "Live2DCubismCore.dll was not found".
            install(FILES ${CUBISM_CORE_DIR}/dll/windows/${_CUBISM_ARCH}/Live2DCubismCore.dll
                    DESTINATION ${CMAKE_INSTALL_BINDIR})
            # Also stage it next to Oai.exe in the build dir so a direct-run
            # (Qt Creator / `./Oai.exe`) works without a manual copy.
            set(_OAI_CUBISM_RUNTIME_DLL
                ${CUBISM_CORE_DIR}/dll/windows/${_CUBISM_ARCH}/Live2DCubismCore.dll
                CACHE INTERNAL "Cubism runtime DLL to copy beside Oai.exe")
        endif()
    else()
        # Linux
        set_target_properties(Live2DCubismCore PROPERTIES
            IMPORTED_LOCATION ${CUBISM_CORE_DIR}/lib/linux/x86_64/libLive2DCubismCore.a
            INTERFACE_INCLUDE_DIRECTORIES ${CUBISM_CORE_DIR}/include
        )
    endif()

    # Patch Cubism Framework for MinGW: wglGetProcAddress returns PROC, not void*
    set(_RENDERER_FILE "${CUBISM_FRAMEWORK_DIR}/src/Rendering/OpenGL/CubismRenderer_OpenGLES2.cpp")
    file(READ "${_RENDERER_FILE}" _RENDERER_CONTENT)
    if(NOT _RENDERER_CONTENT MATCHES "reinterpret_cast.*wglGetProcAddress")
        string(REPLACE "void* ptr = wglGetProcAddress(name);"
               "void* ptr = reinterpret_cast<void*>(wglGetProcAddress(name));"
               _RENDERER_CONTENT "${_RENDERER_CONTENT}")
        file(WRITE "${_RENDERER_FILE}" "${_RENDERER_CONTENT}")
    endif()

    # Patch Cubism Framework: InitializeGlFunctions() normally resolves GL function
    # pointers via wglGetProcAddress(), but this conflicts with QOpenGLContext/GLEW.
    # GLEW already resolves all pointers via glewInit(). However, this file declares
    # anonymous-namespace local variables that SHADOW GLEW's global __glew* pointers.
    # We must initialize these locals from GLEW's already-valid globals.
    #
    # Strategy: Use a Python script to replace each WinGlGetProcAddress("name") call
    # with a direct assignment from the corresponding GLEW __glew* global variable.
    file(READ "${_RENDERER_FILE}" _RENDERER_CONTENT2)
    if(NOT _RENDERER_CONTENT2 MATCHES "OAI_GLEW_COPY_FROM_GLEW")
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/patch_cubism_gl.py" "${_RENDERER_FILE}"
            OUTPUT_VARIABLE _PATCH_OUTPUT
            RESULT_VARIABLE _PATCH_RESULT
        )
        message(STATUS "Cubism GL patch: ${_PATCH_OUTPUT}")
        if(NOT _PATCH_RESULT EQUAL 0)
            message(WARNING "Failed to patch Cubism GL functions: ${_PATCH_RESULT}")
        endif()
    endif()

    # Build Cubism Framework
    set(FRAMEWORK_SOURCE OpenGL)
    add_subdirectory(${CUBISM_FRAMEWORK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/Framework)
    # Platform-specific OpenGL target defines
    if(APPLE)
        target_compile_definitions(Framework PUBLIC CSM_TARGET_MAC_GL)
    elseif(WIN32)
        target_compile_definitions(Framework PUBLIC CSM_TARGET_WIN_GL)
    else()
        target_compile_definitions(Framework PUBLIC CSM_TARGET_LINUX_GL)
    endif()
    target_include_directories(Framework PUBLIC ${GLEW_DIR}/include)
    target_link_libraries(Framework Live2DCubismCore glew_s)

    # OpenGL
    find_package(OpenGL REQUIRED)

    set(OAI_HAS_LIVE2D TRUE)
else()
    message(FATAL_ERROR
        "\n"
        "Live2D Cubism SDK Core is not installed.\n"
        "\n"
        "Expected:  ${CUBISM_CORE_DIR}/include/Live2DCubismCore.h\n"
        "\n"
        "The SDK is mandatory — every shipped pack is Live2D, so a build\n"
        "without it would produce an Oai.exe that renders a transparent\n"
        "window at runtime. To install:\n"
        "\n"
        "  1. Download the Cubism SDK for Native (free, requires sign-in):\n"
        "       https://www.live2d.com/en/sdk/download/native/\n"
        "  2. Extract the archive's `Core/` folder over\n"
        "       ${CUBISM_CORE_DIR}/\n"
        "     so that the path above exists.\n"
        "  3. Fetch the SDK's bundled GLEW/GLFW (one-time, separate script):\n"
        "       cd thirdparty/CubismNativeSamples/Samples/OpenGL/thirdParty\n"
        "       bash scripts/setup_glew_glfw\n"
        "  4. Re-run configure.\n")
endif()

# --- Executable --------------------------------------------------------------
if(WIN32)
    # VERSIONINFO + icon resource: configure_file substitutes
    # @PROJECT_VERSION@ etc. so right-click → Properties → Details on
    # Oai.exe shows the same version that's compiled in.
    configure_file(${CMAKE_SOURCE_DIR}/assets/oai.rc.in
                   ${CMAKE_BINARY_DIR}/oai.rc
                   @ONLY)
    set(PLATFORM_SOURCES ${CMAKE_BINARY_DIR}/oai.rc)
    # MinGW's windres doesn't emit dep info for ICON-included files,
    # so ninja can't tell oai.rc.obj depends on oai.ico. Without this,
    # touching oai.ico does NOT rebuild oai.rc.obj — the linked binary
    # then ships with a stale embedded icon.
    set_source_files_properties(${CMAKE_BINARY_DIR}/oai.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/assets/oai.ico)
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
    src/Live2DAnimationEngine.cpp
    src/Live2DAnimationEngine.h
    src/CharacterPack.cpp
    src/CharacterPack.h
    src/CharacterPackManager.cpp
    src/CharacterPackManager.h
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
    src/TipsCatalog.cpp
    src/TipsCatalog.h
    src/ConfigManager.cpp
    src/ConfigManager.h
    src/SystemTray.cpp
    src/SystemTray.h
    src/TipBubbleWidget.cpp
    src/TipBubbleWidget.h
    src/SettingsPanelWidget.cpp
    src/SettingsPanelWidget.h
    src/AnalyticsTracker.cpp
    src/AnalyticsTracker.h
    src/AnalyticsProvider.h
    $<$<BOOL:${OAI_ANALYTICS_ENABLED}>:src/Ga4Provider.cpp>
    $<$<BOOL:${OAI_ANALYTICS_ENABLED}>:src/Ga4Provider.h>
    $<$<BOOL:${OAI_ANALYTICS_ENABLED}>:src/MockProvider.cpp>
    $<$<BOOL:${OAI_ANALYTICS_ENABLED}>:src/MockProvider.h>
    src/MacFocusFix.h
    $<$<PLATFORM_ID:Darwin>:src/MacFocusFix.mm>
    thirdparty/miniz/miniz.c
    thirdparty/miniz/miniz.h
)

target_include_directories(Oai PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Pass version and source directory to C++ code
target_compile_definitions(Oai PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
    OAI_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# --- Analytics compile definitions ----------------------------------------------
if(OAI_ANALYTICS_ENABLED)
    target_compile_definitions(Oai PRIVATE OAI_ANALYTICS_ENABLED=1)
    if(OAI_ANALYTICS_PROVIDER STREQUAL "ga4")
        target_compile_definitions(Oai PRIVATE
            OAI_ANALYTICS_PROVIDER_GA4=1
            OAI_ANALYTICS_MEASUREMENT_ID=${OAI_ANALYTICS_MEASUREMENT_ID}
            OAI_ANALYTICS_API_SECRET=${OAI_ANALYTICS_API_SECRET}
        )
    elseif(OAI_ANALYTICS_PROVIDER STREQUAL "mock")
        target_compile_definitions(Oai PRIVATE OAI_ANALYTICS_PROVIDER_MOCK=1)
    endif()
endif()

# --- Icon resource -----------------------------------------------------------
qt_add_resources(Oai "icons"
    PREFIX "/icons"
    BASE "assets/icons"
    FILES
        assets/icons/oai.png
)

# Per-locale tip messages (event tips + random greetings). Authoring lives
# in plain JSON instead of Qt's .ts/.qm so non-engineers can add a language
# by dropping in another `tips.<locale>.json`.
qt_add_resources(Oai "tips_i18n"
    PREFIX "/i18n"
    BASE "assets"
    FILES
        assets/i18n/tips.en.json
        assets/i18n/tips.zh_CN.json
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

# Live2D libraries (conditional)
if(OAI_HAS_LIVE2D)
    target_link_libraries(Oai PRIVATE Framework ${OPENGL_LIBRARIES})
    target_include_directories(Oai PRIVATE
        ${CUBISM_FRAMEWORK_DIR}/src
        ${CUBISM_CORE_DIR}/include
        ${GLEW_DIR}/include
    )
    target_compile_definitions(Oai PRIVATE OAI_LIVE2D_SUPPORT)
    if(APPLE)
        target_compile_definitions(Oai PRIVATE CSM_TARGET_MAC_GL)
    elseif(WIN32)
        target_compile_definitions(Oai PRIVATE CSM_TARGET_WIN_GL)
        # Stage Live2DCubismCore.dll next to Oai.exe in the build dir so
        # direct-run (Qt Creator / `./Oai.exe`) works. The install rule
        # above handles the packaged case; this handles dev iteration.
        if(_OAI_CUBISM_RUNTIME_DLL)
            add_custom_command(TARGET Oai POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${_OAI_CUBISM_RUNTIME_DLL}
                        $<TARGET_FILE_DIR:Oai>
                COMMENT "Staging Live2DCubismCore.dll next to Oai.exe"
                VERBATIM)
        endif()
    else()
        target_compile_definitions(Oai PRIVATE CSM_TARGET_LINUX_GL)
    endif()
    # Qt OpenGL support
    find_package(Qt6 COMPONENTS OpenGL OpenGLWidgets QUIET)
    if(Qt6OpenGL_FOUND)
        target_link_libraries(Oai PRIVATE Qt::OpenGL)
    endif()
    if(Qt6OpenGLWidgets_FOUND)
        target_link_libraries(Oai PRIVATE Qt::OpenGLWidgets)
    endif()

    if(WIN32)
        # dwmapi: DwmSetWindowAttribute() is used in MainWindow::showEvent
        # to opt out of Windows 11's automatic rounded corners, drop shadow,
        # and Mica/Acrylic backdrop on the frameless pet window.
        target_link_libraries(Oai PRIVATE dwmapi)
    endif()

    # Bundle Cubism Framework shader sources as Qt resources.
    # CubismShader_OpenGLES2::LoadShaderProgramFromFile requests paths like
    # "FrameworkShaders/VertShaderSrc.vert"; our loader maps that to
    # ":/live2d/FrameworkShaders/<name>", so we alias each file accordingly.
    set(_LIVE2D_SHADER_DIR
        ${CUBISM_FRAMEWORK_DIR}/src/Rendering/OpenGL/Shaders/Standard)
    file(GLOB _LIVE2D_SHADER_FILES
        ${_LIVE2D_SHADER_DIR}/*.vert
        ${_LIVE2D_SHADER_DIR}/*.frag)
    foreach(_shader ${_LIVE2D_SHADER_FILES})
        get_filename_component(_shader_name ${_shader} NAME)
        set_source_files_properties(${_shader} PROPERTIES
            QT_RESOURCE_ALIAS "FrameworkShaders/${_shader_name}")
    endforeach()
    qt_add_resources(Oai "live2d_shaders"
        PREFIX "/live2d"
        FILES ${_LIVE2D_SHADER_FILES}
    )
endif()

# --- Install -----------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS Oai
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# --- Generate official packs -------------------------------------------------
# Generate .opk packs from pack directories during build
set(PACKS_DIR ${CMAKE_BINARY_DIR}/packs)
file(MAKE_DIRECTORY ${PACKS_DIR})

# Sprite packs (Clippy + ClippyJS agents) are currently disabled. The pack
# directories were moved to assets/packs-disabled/ so the runtime
# CharacterPackManager scan in dev builds skips them too. To re-enable,
# move the directories back to assets/packs/ and uncomment the two blocks
# below plus the macOS bundle loops further down.
#
# # Clippy pack (has external animations.json definitions file)
# add_custom_command(
#     OUTPUT ${PACKS_DIR}/clippy.opk
#     COMMAND ${CMAKE_COMMAND} -E tar "cf" ${PACKS_DIR}/clippy.opk --format=zip --
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/manifest.json
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/preview.png
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/map.png
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/animations.json
#     DEPENDS
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/manifest.json
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/preview.png
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/map.png
#         ${CMAKE_SOURCE_DIR}/assets/packs/clippy/sprites/animations.json
#     COMMENT "Generating clippy.opk"
#     WORKING_DIRECTORY ${PACKS_DIR}
# )
#
# # ClippyJS agent packs (inline animations in manifest.json, no definitions file)
# set(CLIPPYJS_PACKS bonzi f1 genie genius links merlin peedy rocky rover)
#
# foreach(pack ${CLIPPYJS_PACKS})
#     add_custom_command(
#         OUTPUT ${PACKS_DIR}/${pack}.opk
#         COMMAND ${CMAKE_COMMAND} -E tar "cf" ${PACKS_DIR}/${pack}.opk --format=zip --
#             ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/manifest.json
#             ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/preview.png
#             ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/sprites/map.png
#         DEPENDS
#             ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/manifest.json
#             ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/preview.png
#             ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/sprites/map.png
#         COMMENT "Generating ${pack}.opk"
#         WORKING_DIRECTORY ${PACKS_DIR}
#     )
#     list(APPEND ALL_PACK_OPKS ${PACKS_DIR}/${pack}.opk)
# endforeach()

set(ALL_PACK_OPKS "")

# --- Optional: bulk-import packs from the upstream submodule ---------------
# The 21 first-party packs in assets/packs/ are tracked in git. Everything
# else (Azur Lane, Girls Frontline, Idol Dimension, Konosuba, Live2D Free
# Material samples) is a build artifact regenerated from the upstream repo:
#
#   git submodule update --init thirdparty/upstream-live2d
#   cmake --build build --target import_packs
#   cmake --build build --target generate_packs   # then build .opks
#
# `import_packs` is a manual target — it is NOT part of the default build,
# so a fresh clone without the submodule still builds Oai.exe + the 21
# original .opks fine.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND EXISTS ${CMAKE_SOURCE_DIR}/thirdparty/upstream-live2d/.git)
    add_custom_target(import_packs
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/import_live2d.py
                --local ${CMAKE_SOURCE_DIR}/thirdparty/upstream-live2d
                --cap 50
                --out-dir ${CMAKE_SOURCE_DIR}/assets/packs
        COMMENT "Importing Live2D packs from upstream submodule (curated PICKS + bulk per-category)"
        USES_TERMINAL
        VERBATIM)
endif()

# Live2D packs (model3.json + moc3 + textures/ + motions/ + optional physics/pose/expressions)
# These ship the whole pack directory, so we enumerate files with GLOB_RECURSE
# and archive paths relative to the pack root.
#
# Auto-discovered: any directory under assets/packs/ with a manifest.json is
# treated as a pack. CONFIGURE_DEPENDS makes CMake re-glob when files change,
# so running `import_packs` and rebuilding picks up new packs automatically.
file(GLOB _PACK_MANIFESTS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/assets/packs/*/manifest.json)
set(LIVE2D_PACKS "")
foreach(_manifest ${_PACK_MANIFESTS})
    get_filename_component(_pack_dir ${_manifest} DIRECTORY)
    get_filename_component(_pack_name ${_pack_dir} NAME)
    list(APPEND LIVE2D_PACKS ${_pack_name})
endforeach()
list(SORT LIVE2D_PACKS)
list(LENGTH LIVE2D_PACKS _live2d_pack_count)
message(STATUS "Live2D packs discovered: ${_live2d_pack_count}")
foreach(pack ${LIVE2D_PACKS})
    file(GLOB_RECURSE _${pack}_FILES RELATIVE
        ${CMAKE_SOURCE_DIR}/assets/packs/${pack}
        ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/*)
    set(_${pack}_DEPS "")
    foreach(f ${_${pack}_FILES})
        list(APPEND _${pack}_DEPS ${CMAKE_SOURCE_DIR}/assets/packs/${pack}/${f})
    endforeach()
    add_custom_command(
        OUTPUT ${PACKS_DIR}/${pack}.opk
        COMMAND ${CMAKE_COMMAND} -E tar "cf" ${PACKS_DIR}/${pack}.opk --format=zip -- ${_${pack}_FILES}
        DEPENDS ${_${pack}_DEPS}
        COMMENT "Generating ${pack}.opk"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/packs/${pack}
    )
    list(APPEND ALL_PACK_OPKS ${PACKS_DIR}/${pack}.opk)
endforeach()

add_custom_target(generate_packs ALL DEPENDS ${ALL_PACK_OPKS})
add_dependencies(Oai generate_packs)

# Install the generated packs
install(DIRECTORY ${PACKS_DIR}/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/packs
)

# --- macOS: app icon & hide Dock icon ----------------------------------------
if(APPLE)
    # AppKit is needed for MacFocusFix.mm (NSWindow / NSPanel APIs).
    target_link_libraries(Oai PRIVATE "-framework AppKit")
    # Bake the @executable_path rpath into the binary at link time. macdeployqt
    # is supposed to do this post-build via install_name_tool, but it fails on
    # malformed bundled QtQml frameworks and aborts before fixing rpath. And
    # post-build install_name_tool can't run anyway: the linker ad-hoc signs
    # binaries on Apple Silicon, so install_name_tool refuses with "malformed
    # object" without first stripping the signature. Embedding rpath at link
    # time sidesteps both issues.
    set_target_properties(Oai PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
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

    # Include packs in the macOS bundle Resources.
    # Sprite packs disabled — see the generation block above. Re-enable both
    # together.
    # set_source_files_properties(${PACKS_DIR}/clippy.opk PROPERTIES
    #     MACOSX_PACKAGE_LOCATION "Resources/packs"
    # )
    # target_sources(Oai PRIVATE ${PACKS_DIR}/clippy.opk)
    #
    # foreach(pack ${CLIPPYJS_PACKS})
    #     set_source_files_properties(${PACKS_DIR}/${pack}.opk PROPERTIES
    #         MACOSX_PACKAGE_LOCATION "Resources/packs"
    #     )
    #     target_sources(Oai PRIVATE ${PACKS_DIR}/${pack}.opk)
    # endforeach()

    foreach(pack ${LIVE2D_PACKS})
        set_source_files_properties(${PACKS_DIR}/${pack}.opk PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/packs"
        )
        target_sources(Oai PRIVATE ${PACKS_DIR}/${pack}.opk)
    endforeach()

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

# --- Qt Installer Framework packaging ---------------------------------------
# Produces an offline, multilingual installer using QtIFW's binarycreator.
# Build with:  cmake --build . --target package_installer
#
# Locale handling:
#   * Qt IFW's bundled chrome translations (Next/Cancel/...) auto-load from
#     the user's system locale — no work needed for those.
#   * Localized package metadata (DisplayName/Description/License/Title) is
#     declared inline via xml:lang attributes in installer/config.xml and
#     installer/packages/<id>/meta/package.xml.
#   * qsTr() strings inside installscript.qs / LaunchCheckBoxForm.ui pick up
#     compiled .qm files in the component's meta/ dir; we lrelease them
#     from installer/translations/*.ts as part of this target.
find_program(OAI_QTIFW_BINARYCREATOR
    NAMES binarycreator
    HINTS
        $ENV{QTIFW_DIR}/bin
        "C:/Qt/Tools/QtInstallerFramework/4.11/bin"
        "C:/Qt/Tools/QtInstallerFramework/4.10/bin"
        "C:/Qt/Tools/QtInstallerFramework/4.9/bin"
        "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.11/bin"
        "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.10/bin"
        "/Applications/Qt/Tools/QtInstallerFramework/4.11/bin"
        "/opt/Qt/Tools/QtInstallerFramework/4.11/bin"
    DOC "Path to Qt Installer Framework binarycreator")

if(OAI_QTIFW_BINARYCREATOR)
    set(OAI_IFW_DIR        ${CMAKE_SOURCE_DIR}/installer)
    set(OAI_IFW_PKG_ID     im.cheng.oai.desktop)
    set(OAI_IFW_PKG_META   ${OAI_IFW_DIR}/packages/${OAI_IFW_PKG_ID}/meta)
    set(OAI_IFW_PKG_DATA   ${OAI_IFW_DIR}/packages/${OAI_IFW_PKG_ID}/data)
    set(OAI_IFW_STAGE_TS   ${CMAKE_BINARY_DIR}/installer_stage.stamp)

    # Single source of truth for installer version: project(Oai VERSION ...).
    # config.xml.in / package.xml.in carry @PROJECT_VERSION@ placeholders;
    # CMake renders them next to the templates so binarycreator's existing
    # paths (`-c installer/config.xml` etc.) still resolve. Generated copies
    # are gitignored — only the .in templates are tracked.
    string(TIMESTAMP _OAI_BUILD_DATE "%Y-%m-%d" UTC)
    set(OAI_RELEASE_DATE "${_OAI_BUILD_DATE}")
    # Qt IFW resolves @ApplicationsDir@ at runtime (C:\Program Files on
    # Windows, /Applications on macOS, /opt on Linux).  Must pass it
    # through CMake's configure_file as a literal — define it as a
    # CMake variable whose *value* is the IFW variable reference.
    set(IFW_TARGET_DIR "@ApplicationsDir@/Oai")
    configure_file(
        ${OAI_IFW_DIR}/config.xml.in
        ${OAI_IFW_DIR}/config.xml
        @ONLY)
    configure_file(
        ${OAI_IFW_PKG_META}/package.xml.in
        ${OAI_IFW_PKG_META}/package.xml
        @ONLY)

    # Replacement qt.conf for the flat install layout.
    #
    # windeployqt assumes Oai.exe lives in <root>/bin/ and writes
    #   [Paths]
    #   Prefix = ..
    # so Qt walks up one level to find <root>/plugins/. Our staging step
    # below flattens bin/ into the data root (Windows installers conventionally
    # don't have a bin/ subdir under TargetDir), which leaves
    # `Prefix = ..` pointing at the *parent* of the install dir — Qt then
    # can't find qwindows.dll and aborts with
    #   "This application failed to start because no Qt platform plugin
    #    could be initialized."
    # Overwrite qt.conf with a flat-layout version that points Prefix at
    # the install dir itself.
    file(WRITE ${CMAKE_BINARY_DIR}/qt.conf.flat "[Paths]\nPrefix = .\n")

    # Compile installer-specific translations into meta/<basename>.qm.
    # Filename matters: Qt IFW auto-loads <lang>.qm based on system locale.
    set(OAI_IFW_TS_FILES
        ${OAI_IFW_DIR}/translations/oai_installer_zh_CN.ts)

    set(OAI_IFW_QM_FILES "")
    foreach(_ts ${OAI_IFW_TS_FILES})
        get_filename_component(_base ${_ts} NAME_WE)            # oai_installer_zh_CN
        string(REGEX REPLACE "^oai_installer_" "" _lang ${_base})  # zh_CN
        # Two outputs: one named for the locale (auto-loaded by IFW for
        # installscript.qs/UI translations) and one matching config.xml's
        # <Translation> entry so config-level lookups also resolve.
        set(_qm_lang   ${OAI_IFW_PKG_META}/${_lang}.qm)
        set(_qm_named  ${OAI_IFW_DIR}/oai_installer_${_lang}.qm)
        add_custom_command(
            OUTPUT  ${_qm_lang} ${_qm_named}
            COMMAND Qt6::lrelease ${_ts} -qm ${_qm_lang}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_qm_lang} ${_qm_named}
            DEPENDS ${_ts}
            COMMENT "lrelease installer translation: ${_lang}"
            VERBATIM)
        list(APPEND OAI_IFW_QM_FILES ${_qm_lang} ${_qm_named})
    endforeach()
    add_custom_target(installer_translations DEPENDS ${OAI_IFW_QM_FILES})

    # Stage the deployed app into packages/<id>/data/ with a flat layout
    # (Oai.exe at the root, packs/ next to it) — that matches @TargetDir@
    # references in installscript.qs and feels native on Windows.
    add_custom_command(
        OUTPUT ${OAI_IFW_STAGE_TS}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${OAI_IFW_PKG_DATA}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OAI_IFW_PKG_DATA}
        COMMAND ${CMAKE_COMMAND}
                --install ${CMAKE_BINARY_DIR}
                --prefix  ${OAI_IFW_PKG_DATA}
                --config  $<CONFIG>
        # Flatten the GNUInstallDirs bin/ layer so Oai.exe sits at TargetDir root.
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${OAI_IFW_PKG_DATA}/bin ${OAI_IFW_PKG_DATA}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${OAI_IFW_PKG_DATA}/bin
        # Replace the bin/-relative qt.conf windeployqt wrote (Prefix=..)
        # with the flat-layout version (Prefix=.).
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/qt.conf.flat
                ${OAI_IFW_PKG_DATA}/qt.conf
        # Drop transitive install artifacts (rlottie headers, glew/.a stubs,
        # CMake configs, pkgconfig) — runtime install doesn't need them.
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${OAI_IFW_PKG_DATA}/include ${OAI_IFW_PKG_DATA}/lib
        # Ship the portable Oai.ini template — ConfigManager reads it on
        # startup as a default-values layer underneath user-scope settings,
        # so the installer can pre-set updateServerEndpoint / language /
        # ipcEndpoint per build flavor without forcing every user to edit
        # their roaming config.
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/installer/oai.ini.template
                ${OAI_IFW_PKG_DATA}/Oai.ini
        COMMAND ${CMAKE_COMMAND} -E touch ${OAI_IFW_STAGE_TS}
        DEPENDS Oai ${ALL_PACK_OPKS}
        COMMENT "Staging Oai for installer packaging"
        VERBATIM)
    add_custom_target(installer_stage DEPENDS ${OAI_IFW_STAGE_TS})

    # Final installer artifact name follows the project version.
    if(WIN32)
        set(_installer_ext ".exe")
    elseif(APPLE)
        set(_installer_ext ".app")
    else()
        set(_installer_ext ".run")
    endif()
    set(OAI_INSTALLER_OUTPUT
        ${CMAKE_BINARY_DIR}/OaiInstaller-${PROJECT_VERSION}${_installer_ext})

    add_custom_target(package_installer
        COMMAND ${OAI_QTIFW_BINARYCREATOR}
                --offline-only
                -c ${OAI_IFW_DIR}/config.xml
                -p ${OAI_IFW_DIR}/packages
                ${OAI_INSTALLER_OUTPUT}
        DEPENDS installer_stage installer_translations
        COMMENT "Building offline installer: ${OAI_INSTALLER_OUTPUT}"
        VERBATIM)

    message(STATUS "Qt IFW found: ${OAI_QTIFW_BINARYCREATOR}")
    message(STATUS "  -> cmake --build . --target package_installer")
else()
    message(STATUS
        "Qt Installer Framework not found — `package_installer` target unavailable. "
        "Install QtIFW from the Qt online installer or set QTIFW_DIR.")
endif()
