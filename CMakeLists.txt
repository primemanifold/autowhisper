cmake_minimum_required(VERSION 3.20)
project(autowhisper VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(AUTOWHISPER_ENABLE_CUDA "Enable CUDA support via whisper.cpp" OFF)
option(AUTOWHISPER_ENABLE_TESTS "Build tests" ON)

# Include our CMake modules
include(cmake/CompilerWarnings.cmake)
include(cmake/FetchDependencies.cmake)
include(cmake/PlatformSetup.cmake)

# === Core sources (cross-platform) ===
set(CORE_SOURCES
    src/main.cpp
    src/config/config.cpp
    src/util/logging.cpp
    src/util/subprocess.cpp
    src/daemon/daemon.cpp
    src/audio/audio.cpp
    src/inference/inference.cpp
    src/feedback/feedback.cpp
    src/cli/cli.cpp
    src/doctor/doctor.cpp
    src/models/models.cpp
    src/hotkey/hotkey_common.cpp
    src/output/output_common.cpp
    src/tray/tray_common.cpp
)

# === Platform-specific sources ===
if(LINUX)
    list(APPEND CORE_SOURCES
        src/hotkey/platform/hotkey_x11.cpp
        src/output/platform/output_x11.cpp
        src/tray/platform/tray_gtk.cpp
        src/tray/platform/tray_gtk_settings.cpp
        src/pulseaudio/pulseaudio.cpp
    )
elseif(APPLE)
    list(APPEND CORE_SOURCES
        src/hotkey/platform/hotkey_macos.mm
        src/output/platform/output_macos.mm
        src/tray/platform/tray_macos.mm
        src/pulseaudio/pulseaudio.cpp  # stub on macOS
    )
elseif(WIN32)
    list(APPEND CORE_SOURCES
        src/hotkey/platform/hotkey_win32.cpp
        src/output/platform/output_win32.cpp
        src/tray/platform/tray_win32.cpp
        src/pulseaudio/pulseaudio.cpp  # stub on Windows
    )
endif()

# === Main executable ===
add_executable(autowhisper ${CORE_SOURCES})

target_include_directories(autowhisper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(autowhisper PRIVATE
    whisper
    tomlplusplus::tomlplusplus
    CLI11::CLI11
    spdlog::spdlog
    miniaudio
)

# Platform libraries
if(LINUX)
    # X11, XTest, XRecord for hotkeys and text injection
    find_package(X11 REQUIRED)
    target_link_libraries(autowhisper PRIVATE
        ${X11_LIBRARIES}
        ${X11_Xtst_LIB}
    )
    target_include_directories(autowhisper PRIVATE ${X11_INCLUDE_DIR})

    # GTK3 + AppIndicator for tray
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(autowhisper PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(autowhisper PRIVATE ${GTK3_LIBRARIES})
    target_link_directories(autowhisper PRIVATE ${GTK3_LIBRARY_DIRS})

    # Try AyatanaAppIndicator3 first, fall back to AppIndicator3
    pkg_check_modules(AYATANA_APPINDICATOR ayatana-appindicator3-0.1)
    if(AYATANA_APPINDICATOR_FOUND)
        target_compile_definitions(autowhisper PRIVATE HAVE_AYATANA_APPINDICATOR)
        target_include_directories(autowhisper PRIVATE ${AYATANA_APPINDICATOR_INCLUDE_DIRS})
        target_link_libraries(autowhisper PRIVATE ${AYATANA_APPINDICATOR_LIBRARIES})
    else()
        pkg_check_modules(APPINDICATOR appindicator3-0.1)
        if(APPINDICATOR_FOUND)
            target_compile_definitions(autowhisper PRIVATE HAVE_APPINDICATOR)
            target_include_directories(autowhisper PRIVATE ${APPINDICATOR_INCLUDE_DIRS})
            target_link_libraries(autowhisper PRIVATE ${APPINDICATOR_LIBRARIES})
        endif()
    endif()

    # PulseAudio
    pkg_check_modules(PULSE libpulse)
    if(PULSE_FOUND)
        target_compile_definitions(autowhisper PRIVATE HAVE_PULSEAUDIO)
        target_include_directories(autowhisper PRIVATE ${PULSE_INCLUDE_DIRS})
        target_link_libraries(autowhisper PRIVATE ${PULSE_LIBRARIES})
    endif()
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(CARBON_FRAMEWORK Carbon)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    target_link_libraries(autowhisper PRIVATE
        ${COCOA_FRAMEWORK}
        ${CARBON_FRAMEWORK}
        ${COREGRAPHICS_FRAMEWORK}
    )
endif()

# Threads
find_package(Threads REQUIRED)
target_link_libraries(autowhisper PRIVATE Threads::Threads)

# Version info
target_compile_definitions(autowhisper PRIVATE
    AUTOWHISPER_VERSION="${PROJECT_VERSION}"
    AUTOWHISPER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    AUTOWHISPER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    AUTOWHISPER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Apply compiler warnings
autowhisper_set_warnings(autowhisper)

# Install
install(TARGETS autowhisper RUNTIME DESTINATION bin)

# Tests
if(AUTOWHISPER_ENABLE_TESTS)
    enable_testing()
    # Tests will be added in Phase 2+
endif()
