# MacOSBundle.cmake
#
# Assembles AutoWhisper.app from the built binary:
#   build/AutoWhisper.app/Contents/
#     MacOS/autowhisper           <- the CLI binary
#     Resources/                  <- icons (future)
#     Info.plist                  <- rendered from Info.plist.in
#
# Also renders the LaunchAgent plist template so install scripts can copy
# it to ~/Library/LaunchAgents/.
#
# Signing policy:
#   - If AUTOWHISPER_SIGN_IDENTITY is set (e.g. "Developer ID Application:
#     Channa Battad (TEAMID)") the bundle is signed with that identity,
#     hardened runtime, and entitlements.plist.
#   - If AUTOWHISPER_SIGN_IDENTITY is empty or "-", ad-hoc sign.
#   - Signing always runs so TCC treats the binary as a stable identity on
#     the dev box.

if(NOT APPLE)
    return()
endif()

set(AUTOWHISPER_BUNDLE_DIR "${CMAKE_BINARY_DIR}/AutoWhisper.app")
set(AUTOWHISPER_MACOS_DIR  "${AUTOWHISPER_BUNDLE_DIR}/Contents/MacOS")
set(AUTOWHISPER_RESOURCES_DIR "${AUTOWHISPER_BUNDLE_DIR}/Contents/Resources")

# Default signing identity: ad-hoc. Override with
#   -DAUTOWHISPER_SIGN_IDENTITY="Developer ID Application: ..."
if(NOT DEFINED AUTOWHISPER_SIGN_IDENTITY)
    set(AUTOWHISPER_SIGN_IDENTITY "-" CACHE STRING "codesign identity; '-' means ad-hoc")
endif()

# Info.plist rendering
set(AUTOWHISPER_INFO_PLIST_IN  "${CMAKE_SOURCE_DIR}/platform/macos/Info.plist.in")
set(AUTOWHISPER_INFO_PLIST_OUT "${AUTOWHISPER_BUNDLE_DIR}/Contents/Info.plist")
set(AUTOWHISPER_ENTITLEMENTS   "${CMAKE_SOURCE_DIR}/platform/macos/entitlements.plist")
set(AUTOWHISPER_SETTINGS_SWIFT "${CMAKE_SOURCE_DIR}/platform/macos/SettingsApp.swift")
set(AUTOWHISPER_SETTINGS_HELPER "${CMAKE_BINARY_DIR}/AutoWhisperSettings")
set(AUTOWHISPER_ICON_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/generate_macos_icon.py")
set(AUTOWHISPER_ICONSET "${CMAKE_BINARY_DIR}/AutoWhisper.iconset")
set(AUTOWHISPER_ICON_FILE "${CMAKE_BINARY_DIR}/AutoWhisper.icns")
find_program(AUTOWHISPER_SWIFTC swiftc REQUIRED)
find_program(AUTOWHISPER_ICONUTIL iconutil REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(AUTOWHISPER_SWIFT_TARGET "${CMAKE_SYSTEM_PROCESSOR}-apple-macos${CMAKE_OSX_DEPLOYMENT_TARGET}")

add_custom_command(
    OUTPUT "${AUTOWHISPER_SETTINGS_HELPER}"
    COMMAND "${AUTOWHISPER_SWIFTC}"
            -O
            -target "${AUTOWHISPER_SWIFT_TARGET}"
            -framework SwiftUI
            -framework AppKit
            "${AUTOWHISPER_SETTINGS_SWIFT}"
            -o "${AUTOWHISPER_SETTINGS_HELPER}"
    DEPENDS "${AUTOWHISPER_SETTINGS_SWIFT}"
    VERBATIM
)
add_custom_target(autowhisper_settings_helper DEPENDS "${AUTOWHISPER_SETTINGS_HELPER}")

add_custom_command(
    OUTPUT "${AUTOWHISPER_ICON_FILE}"
    COMMAND ${Python3_EXECUTABLE} "${AUTOWHISPER_ICON_SCRIPT}" "${AUTOWHISPER_ICONSET}"
    COMMAND "${AUTOWHISPER_ICONUTIL}" -c icns -o "${AUTOWHISPER_ICON_FILE}" "${AUTOWHISPER_ICONSET}"
    DEPENDS "${AUTOWHISPER_ICON_SCRIPT}"
    VERBATIM
)
add_custom_target(autowhisper_macos_icon DEPENDS "${AUTOWHISPER_ICON_FILE}")

add_custom_target(autowhisper_bundle ALL
    DEPENDS autowhisper autowhisper_settings_helper autowhisper_macos_icon
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOWHISPER_MACOS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOWHISPER_RESOURCES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:autowhisper>" "${AUTOWHISPER_MACOS_DIR}/autowhisper"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${AUTOWHISPER_SETTINGS_HELPER}" "${AUTOWHISPER_MACOS_DIR}/AutoWhisperSettings"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/config.toml" "${AUTOWHISPER_RESOURCES_DIR}/config.toml"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${AUTOWHISPER_ICON_FILE}" "${AUTOWHISPER_RESOURCES_DIR}/AutoWhisper.icns"
    COMMAND ${CMAKE_COMMAND}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
            -DPROJECT_VERSION=${PROJECT_VERSION}
            -DINPUT=${AUTOWHISPER_INFO_PLIST_IN}
            -DOUTPUT=${AUTOWHISPER_INFO_PLIST_OUT}
            -P "${CMAKE_SOURCE_DIR}/cmake/RenderInfoPlist.cmake"
    COMMAND codesign --force
            --sign "${AUTOWHISPER_SIGN_IDENTITY}"
            --options runtime
            --timestamp
            "${AUTOWHISPER_MACOS_DIR}/AutoWhisperSettings"
    COMMAND codesign --force
            --sign "${AUTOWHISPER_SIGN_IDENTITY}"
            --options runtime
            --entitlements "${AUTOWHISPER_ENTITLEMENTS}"
            --timestamp
            "${AUTOWHISPER_BUNDLE_DIR}"
    COMMAND codesign --verify --deep --strict "${AUTOWHISPER_BUNDLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "AutoWhisper.app built at ${AUTOWHISPER_BUNDLE_DIR}"
    VERBATIM
)

# Render the LaunchAgent plist into the build dir so install scripts pick it up.
set(AUTOWHISPER_LAUNCHAGENT_IN  "${CMAKE_SOURCE_DIR}/platform/macos/us.primemanifold.autowhisper.plist.in")
set(AUTOWHISPER_LAUNCHAGENT_OUT "${CMAKE_BINARY_DIR}/us.primemanifold.autowhisper.plist")

# Defaults: these can be overridden at install time by re-configuring.
if(NOT DEFINED AUTOWHISPER_INSTALL_BIN)
    set(AUTOWHISPER_INSTALL_BIN "/Applications/AutoWhisper.app/Contents/MacOS/autowhisper")
endif()
if(NOT DEFINED AUTOWHISPER_LOG_PATH)
    set(AUTOWHISPER_LOG_PATH "$ENV{HOME}/Library/Logs/autowhisper/autowhisper.log")
endif()

configure_file(${AUTOWHISPER_LAUNCHAGENT_IN} ${AUTOWHISPER_LAUNCHAGENT_OUT} @ONLY)
