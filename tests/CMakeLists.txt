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
    ${CMAKE_SOURCE_DIR}/src/TipBubbleWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/TipBubbleWidget.h
    ${CMAKE_SOURCE_DIR}/src/TipsEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/TipsEngine.h
    ${CMAKE_SOURCE_DIR}/src/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ConfigManager.h
    ${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.c
)

set(TEST_SOURCES
    test_ipc_animations.cpp
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
            Qt::Test
            rlottie::rlottie
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${rlottie_SOURCE_DIR}/inc
    )
    target_compile_definitions(${test_name} PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
