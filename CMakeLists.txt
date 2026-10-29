cmake_minimum_required(VERSION 3.30.5)
project(AllMusicPlayer VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Deliberately require the approved version; never silently use another Qt.
find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.12)

qt_add_executable(allmusic-player src/app/main.cpp)
if(ANDROID)
    set(app_qml ui/android/Main.qml)
else()
    set(app_qml ui/desktop/Main.qml)
endif()
set_source_files_properties(${app_qml} PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(allmusic-player
    URI AllMusicPlayer
    VERSION 1.0
    QML_FILES ${app_qml}
)
target_link_libraries(allmusic-player PRIVATE Qt6::Quick)
set_target_properties(allmusic-player PROPERTIES WIN32_EXECUTABLE TRUE)
if(MSVC)
    target_compile_options(allmusic-player PRIVATE /utf-8 /W4)
else()
    target_compile_options(allmusic-player PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ANDROID)
    set(ALLMUSIC_BUILD_LABEL "001-C-local" CACHE STRING "Non-secret test build identity")
    set(ALLMUSIC_ANDROID_VERSION_CODE "1" CACHE STRING "Android test version code")
    if(NOT ALLMUSIC_BUILD_LABEL MATCHES "^[a-zA-Z0-9._-]+$")
        message(FATAL_ERROR "Invalid test build label")
    endif()
    if(NOT ALLMUSIC_ANDROID_VERSION_CODE MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "Invalid Android version code")
    endif()
    target_compile_definitions(allmusic-player PRIVATE
        ALLMUSIC_BUILD_LABEL="${ALLMUSIC_BUILD_LABEL}")
    set_target_properties(allmusic-player PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.metravolokos.allmusicplayer.spike001c"
        QT_ANDROID_APP_NAME "AllMusic Player — тест 001-C"
        QT_ANDROID_VERSION_NAME "0.0.1-${ALLMUSIC_BUILD_LABEL}"
        QT_ANDROID_VERSION_CODE "${ALLMUSIC_ANDROID_VERSION_CODE}"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_COMPILE_SDK_VERSION 36
        QT_ANDROID_SDK_BUILD_TOOLS_REVISION "36.0.0")
    # Android is only a launcher screen. Do not create the loopback spike/tools
    # or register native desktop executables as cross-platform CTest tests.
    return()
endif()

find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Network Multimedia QuickControls2)
add_library(local-playback STATIC src/playback/LocalPlayback.cpp src/playback/LocalPlayback.h)
target_link_libraries(local-playback PUBLIC Qt6::Multimedia)
target_include_directories(local-playback PUBLIC src/playback)
target_link_libraries(allmusic-player PRIVATE local-playback Qt6::QuickControls2)
if(MSVC)
    target_compile_options(local-playback PRIVATE /utf-8 /W4)
endif()
include(CTest)
add_library(webdav-spike STATIC src/cloud/WebDavProbe.cpp src/cloud/WebDavProbe.h)
target_link_libraries(webdav-spike PUBLIC Qt6::Network)
target_include_directories(webdav-spike PUBLIC src/cloud)
add_executable(webdav-probe src/cloud/probe-main.cpp)
target_link_libraries(webdav-probe PRIVATE webdav-spike)
if(MSVC)
    target_compile_options(webdav-spike PRIVATE /utf-8 /W4)
    target_compile_options(webdav-probe PRIVATE /utf-8 /W4)
else()
    target_compile_options(webdav-spike PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
    find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Test)
    add_executable(webdav-mock tests/webdav/mock-main.cpp tests/webdav/MockWebDav.h)
    target_link_libraries(webdav-mock PRIVATE Qt6::Network)
    add_executable(webdav-tests tests/webdav/tst_webdav.cpp tests/webdav/MockWebDav.h)
    target_link_libraries(webdav-tests PRIVATE webdav-spike Qt6::Test)
    qt_add_resources(webdav-tests webdav-fixtures PREFIX "/webdav" BASE tests/webdav
        FILES tests/webdav/localhost.crt tests/webdav/localhost.testkey)
    if(MSVC)
        target_compile_options(webdav-tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(playback-tests tests/playback/tst_playback.cpp)
    target_link_libraries(playback-tests PRIVATE local-playback Qt6::Test)
    if(MSVC)
        target_compile_options(playback-tests PRIVATE /utf-8 /W4)
    endif()
    add_test(NAME playback-decode COMMAND playback-tests)
    set_tests_properties(playback-decode PROPERTIES TIMEOUT 150
        ENVIRONMENT "QT_MEDIA_BACKEND=ffmpeg;QT_FFMPEG_PROTOCOL_WHITELIST=file;ALLMUSIC_FIXTURES=${CMAKE_SOURCE_DIR}/build/audio-fixtures")
    add_test(NAME webdav-safety COMMAND webdav-tests)
    set_tests_properties(webdav-safety PROPERTIES TIMEOUT 90)
    add_test(NAME window-smoke COMMAND allmusic-player --smoke-test
        "${CMAKE_CURRENT_BINARY_DIR}/window-smoke.png")
    set_tests_properties(window-smoke PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QUICK_BACKEND=software")
endif()

install(TARGETS allmusic-player RUNTIME DESTINATION .)

