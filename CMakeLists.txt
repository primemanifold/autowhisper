cmake_minimum_required(VERSION 3.20)
project(autowhisper VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# Options
option(AUTOWHISPER_ENABLE_CUDA "Enable CUDA support via whisper.cpp" OFF)
option(AUTOWHISPER_ENABLE_TESTS "Build tests" ON)

# Include our CMake modules
include(cmake/CompilerWarnings.cmake)
include(cmake/FetchDependencies.cmake)
include(cmake/PlatformSetup.cmake)
include(cmake/EmbedAssets.cmake)
autowhisper_embed_web_assets(${CMAKE_BINARY_DIR}/generated/settings)

# Threads (needed by core lib and main executable)
find_package(Threads REQUIRED)

# === Testable core sources (platform-independent) ===
set(CORE_LIB_SOURCES
    src/config/config.cpp
    src/config/schema.cpp
    src/hotkey/hotkey_common.cpp
    src/models/models.cpp
    src/settings/handlers.cpp
    src/settings/sidecar.cpp
    src/tray/tray_common.cpp
    src/util/logging.cpp
    src/util/subprocess.cpp
)

add_library(autowhisper_core STATIC ${CORE_LIB_SOURCES})
target_include_directories(autowhisper_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(autowhisper_core PUBLIC
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    nlohmann_json
    Threads::Threads
)
target_compile_definitions(autowhisper_core PUBLIC
    AUTOWHISPER_VERSION="${PROJECT_VERSION}"
    AUTOWHISPER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    AUTOWHISPER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    AUTOWHISPER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
autowhisper_set_warnings(autowhisper_core)

# === Application sources (need platform libs, whisper, miniaudio) ===
set(APP_SOURCES
    src/main.cpp
    src/daemon/daemon.cpp
    src/audio/audio.cpp
    src/inference/inference.cpp
    src/feedback/feedback.cpp
    src/cli/cli.cpp
    src/doctor/doctor.cpp
    src/output/output_common.cpp
)

# === Platform-specific sources ===
if(LINUX)
    list(APPEND APP_SOURCES
        src/hotkey/platform/hotkey_x11.cpp
        src/output/platform/output_x11.cpp
        src/tray/platform/tray_gtk.cpp
        src/pulseaudio/pulseaudio.cpp
    )
elseif(APPLE)
    list(APPEND APP_SOURCES
        src/hotkey/platform/hotkey_macos.mm
        src/output/platform/output_macos.mm
        src/tray/platform/tray_macos.mm
        src/pulseaudio/pulseaudio.cpp  # stub on macOS
    )
elseif(WIN32)
    list(APPEND APP_SOURCES
        src/hotkey/platform/hotkey_win32.cpp
        src/output/platform/output_win32.cpp
        src/tray/platform/tray_win32.cpp
        src/pulseaudio/pulseaudio.cpp  # stub on Windows
    )
endif()

# === Main executable ===
add_executable(autowhisper ${APP_SOURCES})

target_include_directories(autowhisper PRIVATE
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(autowhisper PRIVATE
    autowhisper_core
    whisper
    CLI11::CLI11
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

# Apply compiler warnings
autowhisper_set_warnings(autowhisper)

# Install
install(TARGETS autowhisper RUNTIME DESTINATION bin)
install(DIRECTORY icons/ DESTINATION share/autowhisper/icons)
install(PROGRAMS tools/autowhisper-settings DESTINATION bin)

# Tests
if(AUTOWHISPER_ENABLE_TESTS)
    enable_testing()
    include(CTest)
    include(Catch)

    add_executable(autowhisper_tests
        tests/cpp/test_config.cpp
        tests/cpp/test_keycombo.cpp
        tests/cpp/test_logging.cpp
        tests/cpp/test_models.cpp
        tests/cpp/test_schema.cpp
        tests/cpp/test_settings_handlers.cpp
        tests/cpp/test_sidecar.cpp
        tests/cpp/test_subprocess.cpp
        tests/cpp/test_tray_display.cpp
    )
    target_link_libraries(autowhisper_tests PRIVATE
        autowhisper_core
        Catch2::Catch2WithMain
    )
    autowhisper_set_warnings(autowhisper_tests)

    catch_discover_tests(autowhisper_tests)
endif()
