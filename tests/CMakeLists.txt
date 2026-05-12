# Source files shared between the app and tests (excluding main.cpp)
set(OAIPET_LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/IpcServer.cpp
    ${CMAKE_SOURCE_DIR}/src/IpcServer.h
    ${CMAKE_SOURCE_DIR}/src/UdpWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/UdpWorker.h
    ${CMAKE_SOURCE_DIR}/src/EventRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/EventRouter.h
    ${CMAKE_SOURCE_DIR}/src/SpriteAnimationEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/SpriteAnimationEngine.h
    ${CMAKE_SOURCE_DIR}/src/LottieAnimationEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/LottieAnimationEngine.h
    ${CMAKE_SOURCE_DIR}/src/LottieEffectOverlay.cpp
    ${CMAKE_SOURCE_DIR}/src/LottieEffectOverlay.h
    ${CMAKE_SOURCE_DIR}/src/CharacterPack.cpp
    ${CMAKE_SOURCE_DIR}/src/CharacterPack.h
    ${CMAKE_SOURCE_DIR}/src/CharacterPackManager.cpp
    ${CMAKE_SOURCE_DIR}/src/CharacterPackManager.h
    ${CMAKE_SOURCE_DIR}/src/TipWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/TipWidget.h
    ${CMAKE_SOURCE_DIR}/src/EcgWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/EcgWidget.h
    ${CMAKE_SOURCE_DIR}/src/MacFocusFix.h
    $<$<PLATFORM_ID:Darwin>:${CMAKE_SOURCE_DIR}/src/MacFocusFix.mm>
    ${CMAKE_SOURCE_DIR}/src/TipsEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/TipsEngine.h
    ${CMAKE_SOURCE_DIR}/src/TipsCatalog.cpp
    ${CMAKE_SOURCE_DIR}/src/TipsCatalog.h
    ${CMAKE_SOURCE_DIR}/src/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ConfigManager.h
    ${CMAKE_SOURCE_DIR}/src/FullscreenWatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/FullscreenWatcher.h
    ${CMAKE_SOURCE_DIR}/src/PetStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/PetStateMachine.h
    ${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.c
)

set(TEST_SOURCES
    test_ipc_animations.cpp
    test_ecg.cpp
    test_gaming_mode.cpp
    test_pet_state_machine.cpp
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    qt_add_executable(${test_name}
        ${test_src}
        ${OAIPET_LIB_SOURCES}
    )
    target_link_libraries(${test_name}
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            Qt::Multimedia
            Qt::Test
            rlottie::rlottie
    )
    if(WIN32)
        # TipWidget.cpp is shared with the main app and uses
        # DwmSetWindowAttribute to opt out of Win11 DWM chrome.
        target_link_libraries(${test_name} PRIVATE dwmapi)
    endif()
    if(APPLE)
        # MacFocusFix.mm uses NSWindow / NSPanel from AppKit.
        # FullscreenWatcher.cpp uses CGWindowListCopyWindowInfo from CoreGraphics.
        target_link_libraries(${test_name} PRIVATE "-framework AppKit" "-framework CoreGraphics")
    endif()
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${rlottie_SOURCE_DIR}/inc
    )
    target_compile_definitions(${test_name} PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        PROJECT_VERSION="${PROJECT_VERSION}"
        OAI_DEFAULT_UPDATE_ENDPOINT="${OAI_DEFAULT_UPDATE_ENDPOINT}"
    )
    add_test(NAME ${test_name} COMMAND ${test_name})

    # On Windows the Qt6 DLLs live in Qt's bin/, which is not on the PATH that
    # ctest inherits — without this, the test exe exits 0xc0000135 (DLL not
    # found) before main() runs. Prepend Qt's bin dir to PATH for the test.
    if(WIN32)
        get_target_property(_qt_qmake_loc Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qt_qmake_loc}" DIRECTORY)
        # Escape ';' in $ENV{PATH} so CMake doesn't split it as a list when
        # serialising the ENVIRONMENT property.
        string(REPLACE ";" "\\;" _ctest_path "$ENV{PATH}")
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "PATH=${_qt_bin_dir}\\;${_ctest_path}")
    endif()
endforeach()
