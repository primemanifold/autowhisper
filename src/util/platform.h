#pragma once

// Platform detection macros
#if defined(__linux__)
    #define AW_PLATFORM_LINUX 1
    #define AW_PLATFORM_NAME "linux"
#elif defined(__APPLE__)
    #define AW_PLATFORM_MACOS 1
    #define AW_PLATFORM_NAME "macos"
#elif defined(_WIN32)
    #define AW_PLATFORM_WINDOWS 1
    #define AW_PLATFORM_NAME "windows"
#else
    #error "Unsupported platform"
#endif
