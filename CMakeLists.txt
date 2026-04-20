cmake_minimum_required(VERSION 3.19)
project(Clippy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools)

qt_standard_project_setup()

# --- rlottie via FetchContent ------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    rlottie
    URL      https://github.com/Samsung/rlottie/archive/refs/tags/v0.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(RLOTTIE_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(RLOTTIE_BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rlottie)

# --- Executable --------------------------------------------------------------
qt_add_executable(Clippy
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/SpriteAnimationEngine.cpp
    src/SpriteAnimationEngine.h
    src/SpeechBubble.cpp
    src/SpeechBubble.h
    src/LottieEffectOverlay.cpp
    src/LottieEffectOverlay.h
    src/IpcServer.cpp
    src/IpcServer.h
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

target_include_directories(Clippy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Lottie effect resources -------------------------------------------------
# Only effects are Lottie — character animations use sprite sheets at runtime
qt_add_resources(Clippy "lottie_effects"
    PREFIX "/lottie"
    BASE "assets/lottie"
    FILES
        assets/lottie/effects/sparkles.json
        assets/lottie/effects/confetti.json
        assets/lottie/effects/alert-pulse.json
        assets/lottie/effects/thinking-dots.json
        assets/lottie/effects/wave-lines.json
        assets/lottie/effects/speech-pop.json
)

# --- Translations ------------------------------------------------------------
qt_add_translations(
    TARGETS Clippy
    TS_FILES Clippy_zh_CN.ts
)

# --- Link libraries ----------------------------------------------------------
target_link_libraries(Clippy
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        rlottie::rlottie
)

# rlottie header path (FetchContent puts it in build/_deps)
target_include_directories(Clippy PRIVATE
    ${rlottie_SOURCE_DIR}/inc
)

# --- Install -----------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS Clippy
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Also install runtime assets (sprite sheet, animations.json, lottie effects)
install(DIRECTORY assets/
    DESTINATION assets
)

qt_generate_deploy_app_script(
    TARGET Clippy
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
