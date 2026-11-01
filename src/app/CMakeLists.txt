# Keep the application target SOURCE_DIR narrow: Qt Android deployment always
# scans it recursively, in addition to the declared platform QML roots.
qt_add_executable(allmusic-player main.cpp)
if(ANDROID)
    set(ALLMUSIC_ANDROID_OPENSSL_DIR "" CACHE PATH
        "Directory containing the audited Android OpenSSL 3 shared libraries")
    if(NOT ALLMUSIC_ANDROID_OPENSSL_DIR)
        message(FATAL_ERROR "ALLMUSIC_ANDROID_OPENSSL_DIR is required for Android TLS")
    endif()
    foreach(openssl_library IN ITEMS libcrypto_3.so libssl_3.so)
        if(NOT EXISTS "${ALLMUSIC_ANDROID_OPENSSL_DIR}/${openssl_library}")
            message(FATAL_ERROR "Missing packaged OpenSSL runtime: ${ALLMUSIC_ANDROID_OPENSSL_DIR}/${openssl_library}")
        endif()
    endforeach()
    target_sources(allmusic-player PRIVATE
        AppearanceController.cpp
        AppearanceController.h
        AndroidCloudTrackListModel.cpp
        AndroidCloudTrackListModel.h
        AndroidFavoritesModel.cpp
        AndroidFavoritesModel.h
        AudioMetadataReader.cpp
        AudioMetadataReader.h
        LibraryModel.cpp
        LibraryModel.h
        PlaybackQueueModel.cpp
        PlaybackQueueModel.h
        TrackIdentity.h
        AndroidMedia3Controller.cpp
        AndroidMedia3Controller.h
        AndroidMedia3State.h
        AndroidMailRangeRegistry.cpp
        AndroidMailRangeRegistry.h
        MailWebDavController.cpp
        MailWebDavController.h)
    target_link_libraries(allmusic-player PRIVATE mail-webdav-range)
    set(app_qml ${PROJECT_SOURCE_DIR}/ui/android/Main.qml)
    set_source_files_properties(${app_qml} PROPERTIES QT_RESOURCE_ALIAS Main.qml)
    set(qml_files ${app_qml})
else()
    target_sources(allmusic-player PRIVATE
        AppearanceController.cpp
        AppearanceController.h
        AudioMetadataReader.cpp
        AudioMetadataReader.h
        LibraryModel.cpp
        LibraryModel.h
        PlaybackQueueModel.cpp
        PlaybackQueueModel.h
        MailWebDavController.cpp
        MailWebDavController.h)
    target_link_libraries(allmusic-player PRIVATE mail-webdav-diagnostic)
    set(app_qml ${PROJECT_SOURCE_DIR}/ui/desktop/Main.qml)
    set_source_files_properties(${app_qml} PROPERTIES QT_RESOURCE_ALIAS Main.qml)
    # QML module type names come from source file names. Give the Android
    # preview a distinct generated filename so it does not collide with the
    # Desktop Main.qml type inside the same module.
    set(android_preview_source ${PROJECT_SOURCE_DIR}/ui/android/Main.qml)
    set(android_preview_qml ${CMAKE_CURRENT_BINARY_DIR}/AndroidPreview.qml)
    configure_file(${android_preview_source} ${android_preview_qml} COPYONLY)
    set(qml_files ${app_qml} ${android_preview_qml})
endif()
qt_add_qml_module(allmusic-player
    URI AllMusicPlayer
    VERSION 1.0
    QML_FILES ${qml_files}
)
target_link_libraries(allmusic-player PRIVATE Qt6::Quick Qt6::QuickControls2)
set_target_properties(allmusic-player PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
if(MSVC)
    target_compile_options(allmusic-player PRIVATE /utf-8 /W4)
else()
    target_compile_options(allmusic-player PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ANDROID)
    set(ALLMUSIC_BUILD_LABEL "002-D-cloud" CACHE STRING "Non-secret test build identity")
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
        QT_QML_ROOT_PATH "${PROJECT_SOURCE_DIR}/ui/android"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.metravolokos.allmusicplayer.cloud002dmedia3"
        QT_ANDROID_APP_NAME "AllMusic Player — Cloud Mail Media3"
        QT_ANDROID_VERSION_NAME "0.0.1-${ALLMUSIC_BUILD_LABEL}"
        QT_ANDROID_VERSION_CODE "${ALLMUSIC_ANDROID_VERSION_CODE}"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_COMPILE_SDK_VERSION 36
        QT_ANDROID_SDK_BUILD_TOOLS_REVISION "36.0.0"
        QT_ANDROID_EXTRA_LIBS
            "${ALLMUSIC_ANDROID_OPENSSL_DIR}/libcrypto_3.so;${ALLMUSIC_ANDROID_OPENSSL_DIR}/libssl_3.so")
    # Keep the Android Cloud Mail target isolated from Desktop playback,
    # loopback tools, and native Desktop CTest targets.
    return()
endif()

add_library(local-playback STATIC
    ${PROJECT_SOURCE_DIR}/src/playback/LocalPlayback.cpp
    ${PROJECT_SOURCE_DIR}/src/playback/LocalPlayback.h
    ${PROJECT_SOURCE_DIR}/src/playback/MailWebDavRangeDevice.cpp
    ${PROJECT_SOURCE_DIR}/src/playback/MailWebDavRangeDevice.h
    ${PROJECT_SOURCE_DIR}/src/playback/WebDavRangeDevice.cpp
    ${PROJECT_SOURCE_DIR}/src/playback/WebDavRangeDevice.h)
target_link_libraries(local-playback PUBLIC Qt6::Multimedia mail-webdav-diagnostic)
target_include_directories(local-playback PUBLIC ${PROJECT_SOURCE_DIR}/src/playback)
target_link_libraries(allmusic-player PRIVATE local-playback)
if(MSVC)
    target_compile_options(local-playback PRIVATE /utf-8 /W4)
endif()
