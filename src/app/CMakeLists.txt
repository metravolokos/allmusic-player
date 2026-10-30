# Keep the application target SOURCE_DIR narrow: Qt Android deployment always
# scans it recursively, in addition to the declared platform QML roots.
qt_add_executable(allmusic-player main.cpp)
if(ANDROID)
    target_sources(allmusic-player PRIVATE
        AndroidAudioTestController.cpp
        AndroidAudioTestController.h
        AndroidAudioTestState.h)
    set(app_qml ${PROJECT_SOURCE_DIR}/ui/android/Main.qml)
else()
    set(app_qml ${PROJECT_SOURCE_DIR}/ui/desktop/Main.qml)
endif()
set_source_files_properties(${app_qml} PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(allmusic-player
    URI AllMusicPlayer
    VERSION 1.0
    QML_FILES ${app_qml}
)
target_link_libraries(allmusic-player PRIVATE Qt6::Quick)
set_target_properties(allmusic-player PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
if(MSVC)
    target_compile_options(allmusic-player PRIVATE /utf-8 /W4)
else()
    target_compile_options(allmusic-player PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ANDROID)
    set(ALLMUSIC_BUILD_LABEL "002-A-local" CACHE STRING "Non-secret test build identity")
    set(ALLMUSIC_ANDROID_VERSION_CODE "1" CACHE STRING "Android test version code")
    if(NOT ALLMUSIC_BUILD_LABEL MATCHES "^[a-zA-Z0-9._-]+$")
        message(FATAL_ERROR "Invalid test build label")
    endif()
    if(NOT ALLMUSIC_ANDROID_VERSION_CODE MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "Invalid Android version code")
    endif()
    target_compile_definitions(allmusic-player PRIVATE
        ALLMUSIC_BUILD_LABEL="${ALLMUSIC_BUILD_LABEL}")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(android_audio_tone "${CMAKE_CURRENT_BINARY_DIR}/android-test-tone.wav")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/scripts/generate_android_tone.py"
                "${android_audio_tone}"
        RESULT_VARIABLE tone_generation_result)
    if(NOT tone_generation_result EQUAL 0)
        message(FATAL_ERROR "Could not generate the synthetic Android audio test tone")
    endif()
    qt_add_resources(allmusic-player android_audio_test_tone
        PREFIX "/audio-test"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES "${android_audio_tone}")
    target_link_libraries(allmusic-player PRIVATE Qt6::Multimedia)
    set_target_properties(allmusic-player PROPERTIES
        QT_QML_ROOT_PATH "${PROJECT_SOURCE_DIR}/ui/android"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.metravolokos.allmusicplayer.spike002a"
        QT_ANDROID_APP_NAME "AllMusic Player — тест 002-A"
        QT_ANDROID_VERSION_NAME "0.0.1-${ALLMUSIC_BUILD_LABEL}"
        QT_ANDROID_VERSION_CODE "${ALLMUSIC_ANDROID_VERSION_CODE}"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_COMPILE_SDK_VERSION 36
        QT_ANDROID_SDK_BUILD_TOOLS_REVISION "36.0.0")
    # Keep the Android local-audio experiment isolated from the Desktop engine,
    # loopback tools, and native Desktop CTest targets.
    return()
endif()

add_library(local-playback STATIC
    ${PROJECT_SOURCE_DIR}/src/playback/LocalPlayback.cpp
    ${PROJECT_SOURCE_DIR}/src/playback/LocalPlayback.h
    ${PROJECT_SOURCE_DIR}/src/playback/WebDavRangeDevice.cpp
    ${PROJECT_SOURCE_DIR}/src/playback/WebDavRangeDevice.h)
target_link_libraries(local-playback PUBLIC Qt6::Multimedia)
target_include_directories(local-playback PUBLIC ${PROJECT_SOURCE_DIR}/src/playback)
target_link_libraries(allmusic-player PRIVATE local-playback Qt6::QuickControls2)
if(MSVC)
    target_compile_options(local-playback PRIVATE /utf-8 /W4)
endif()
