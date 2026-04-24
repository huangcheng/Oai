cmake_minimum_required(VERSION 3.19)
project(Oai VERSION 1.2.0 LANGUAGES CXX)

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

# --- Live2D Cubism SDK -------------------------------------------------------
set(CUBISM_SDK_DIR ${CMAKE_SOURCE_DIR}/thirdparty/CubismNativeSamples)
set(CUBISM_FRAMEWORK_DIR ${CMAKE_SOURCE_DIR}/thirdparty/CubismNativeFramework)
set(CUBISM_CORE_DIR ${CUBISM_SDK_DIR}/Core)

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
    message(STATUS "Live2D Cubism SDK not found — building without Live2D support")
    set(OAI_HAS_LIVE2D FALSE)
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

    # Bundle Cubism Framework shader sources as Qt resources.
    # CubismShader_OpenGLES2::LoadShaderProgramFromFile requests paths like
    # "FrameworkShaders/VertShaderSrc.vert"; our loader maps that to
    # ":/live2d/FrameworkShaders/<name>", so we alias each file accordingly.
    set(_LIVE2D_SHADER_DIR
        ${CUBISM_SDK_DIR}/Framework/src/Rendering/OpenGL/Shaders/Standard)
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

# Live2D packs (model3.json + moc3 + textures/ + motions/ + optional physics/pose/expressions)
# These ship the whole pack directory, so we enumerate files with GLOB_RECURSE
# and archive paths relative to the pack root.
set(LIVE2D_PACKS
    chitose epsilon gantzert_felixander haru haru_greeter haruto hibiki hiyori
    izumi kei koharu mao mark miara miku natori ren rice unitychan wanko)
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
