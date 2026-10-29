cmake_minimum_required(VERSION 3.30.5)
project(AllMusicPlayer VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Deliberately require the approved version; never silently use another Qt.
find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.12)

qt_add_executable(allmusic-player src/app/main.cpp)
set_source_files_properties(ui/desktop/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(allmusic-player
    URI AllMusicPlayer
    VERSION 1.0
    QML_FILES ui/desktop/Main.qml
)
target_link_libraries(allmusic-player PRIVATE Qt6::Quick)
set_target_properties(allmusic-player PROPERTIES WIN32_EXECUTABLE TRUE)
if(MSVC)
    target_compile_options(allmusic-player PRIVATE /utf-8 /W4)
else()
    target_compile_options(allmusic-player PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME window-smoke COMMAND allmusic-player --smoke-test
        "${CMAKE_CURRENT_BINARY_DIR}/window-smoke.png")
    set_tests_properties(window-smoke PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QUICK_BACKEND=software")
endif()

install(TARGETS allmusic-player RUNTIME DESTINATION .)
