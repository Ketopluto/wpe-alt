cmake_minimum_required(VERSION 3.20)

# Use vcpkg if available. Must be set before project() so dependency discovery works.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
   AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(wpe-lite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if(NOT Qt6_FOUND)
    if(WIN32)
        message(FATAL_ERROR "Qt6 not found. Install dependencies with: vcpkg install qtbase:x64-windows")
    else()
        message(FATAL_ERROR "Qt6 not found. Install Qt6 development packages or provide Qt6_DIR/CMAKE_PREFIX_PATH.")
    endif()
endif()

find_package(FFmpeg QUIET COMPONENTS AVFORMAT AVCODEC SWSCALE AVUTIL)
if(NOT FFmpeg_FOUND AND NOT FFMPEG_FOUND)
    if(WIN32)
        message(FATAL_ERROR "FFmpeg not found. Install dependencies with: vcpkg install ffmpeg:x64-windows")
    else()
        message(FATAL_ERROR "FFmpeg not found. Install FFmpeg development packages for your platform.")
    endif()
endif()

add_executable(wpe-lite
    src/main.cpp
    src/core/wallpaper_engine.h
    src/core/wallpaper_engine.cpp
    src/core/playlist.h
    src/platform/platform_utils.h
    src/platform/windows_utils.cpp
    src/platform/stub_utils.cpp
    src/platform/system_integration.h
    src/renderers/wallpaper_renderer.h
    src/renderers/color_renderer.h
    src/renderers/image_renderer.h
    src/renderers/video_renderer.h
    src/renderers/video_renderer.cpp
    src/renderers/gif_renderer.h
    src/gui/tray_icon.h
    src/gui/settings_dialog.h
    src/gui/wallpaper_gallery.h
)

target_include_directories(wpe-lite PRIVATE ${FFMPEG_INCLUDE_DIRS})

target_link_libraries(wpe-lite PRIVATE 
    Qt6::Core Qt6::Gui Qt6::Widgets
    ${FFMPEG_LIBRARIES}
)

if(WIN32)
    target_link_libraries(wpe-lite PRIVATE user32 dwmapi advapi32 wtsapi32)
endif()
