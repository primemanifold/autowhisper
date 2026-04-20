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

add_custom_target(autowhisper_bundle
    DEPENDS autowhisper
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOWHISPER_MACOS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOWHISPER_RESOURCES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:autowhisper>" "${AUTOWHISPER_MACOS_DIR}/autowhisper"
    COMMAND ${CMAKE_COMMAND}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
            -DPROJECT_VERSION=${PROJECT_VERSION}
            -DINPUT=${AUTOWHISPER_INFO_PLIST_IN}
            -DOUTPUT=${AUTOWHISPER_INFO_PLIST_OUT}
            -P "${CMAKE_SOURCE_DIR}/cmake/RenderInfoPlist.cmake"
    COMMAND codesign --force
            --sign "${AUTOWHISPER_SIGN_IDENTITY}"
            --options runtime
            --entitlements "${AUTOWHISPER_ENTITLEMENTS}"
            --timestamp=none
            "${AUTOWHISPER_BUNDLE_DIR}" || echo "codesign failed (ignored for dev)"
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
