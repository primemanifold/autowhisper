# whisper.cpp v1.7.3 - Speech recognition inference
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(AUTOWHISPER_ENABLE_CUDA)
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/whisper.cpp whisper)

# toml++ v3.4.0 - TOML configuration parsing
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/tomlplusplus)

# CLI11 v2.4.2 - Command line parsing
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/CLI11)

# spdlog v1.14.1 - Logging
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/spdlog)

# miniaudio 0.11.21 - Audio capture/playback (header-only)
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_SOURCE_DIR}/deps/miniaudio)

# cpp-httplib (header-only HTTP server)
add_library(cpp_httplib INTERFACE)
target_include_directories(cpp_httplib INTERFACE ${CMAKE_SOURCE_DIR}/deps/cpp-httplib)

# nlohmann-json (header-only JSON)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_SOURCE_DIR}/deps/nlohmann-json/single_include
)

# Catch2 v3.5.2 - Testing framework (only when tests are enabled)
if(AUTOWHISPER_ENABLE_TESTS)
    add_subdirectory(${CMAKE_SOURCE_DIR}/deps/Catch2)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/deps/Catch2/extras)
endif()
