cmake_minimum_required(VERSION 3.20)

project(RXMediaPlayer
    VERSION 2.4.0
    DESCRIPTION "RX Media Player - Native Windows 64-bit Multimedia Player by Rituraj"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/library/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging
)

set(RX_CORE_SOURCES
    src/core/logger.cpp
    src/core/settings.cpp
    src/core/utils.cpp
    src/core/track_formatter.cpp
    src/core/shortcut_manager.cpp
    src/subtitles/opensubtitles_client.cpp
    src/engine/mpv_backend.cpp
    src/playlist/playlist_manager.cpp
    src/audio/wasapi_enumerator.cpp
    src/audio/equalizer.cpp
    src/library/sqlite/sqlite3.c
    src/library/media_database.cpp
)

set(RX_UI_SOURCES
    src/ui/controls.cpp
    src/ui/panels/playlist_panel.cpp
    src/ui/panels/video_panel.cpp
    src/ui/panels/audio_panel.cpp
    src/ui/dialogs/info_dialog.cpp
    src/ui/dialogs/sub_dialog.cpp
    src/ui/dialogs/open_url_dialog.cpp
    src/ui/dialogs/bookmark_dialog.cpp
    src/ui/dialogs/preferences_dialog.cpp
    src/ui/dialogs/shortcut_help_dialog.cpp
    src/ui/dialogs/online_subtitles_dialog.cpp
    src/ui/menus/track_popup_menu.cpp
    src/ui/main_window.cpp
)

# Win32 GUI Executable (subsystem:windows)
add_executable(rx_player WIN32 src/main.cpp ${RX_CORE_SOURCES} ${RX_UI_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rx_player_rc.o")

target_compile_definitions(rx_player PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(rx_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/lib/libmpv.dll.a
    dwmapi
    comctl32
    shlwapi
    shell32
    ole32
    oleaut32
    uuid
    gdi32
    user32
    kernel32
    dxgi
    psapi
    winhttp
)

add_custom_command(TARGET rx_player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/bin/libmpv-2.dll"
        "$<TARGET_FILE_DIR:rx_player>/libmpv-2.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/bin/mpv-2.dll"
        "$<TARGET_FILE_DIR:rx_player>/mpv-2.dll"
    COMMENT "Deploying libmpv runtime DLLs to output directory"
)

# Automated Test Suite executable
add_executable(rx_tests tests/test_core.cpp ${RX_CORE_SOURCES})
target_compile_definitions(rx_tests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_link_libraries(rx_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/lib/libmpv.dll.a
    shlwapi
    shell32
    ole32
    oleaut32
    uuid
    winhttp
)
