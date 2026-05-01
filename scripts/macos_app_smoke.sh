#!/usr/bin/env bash
# Validate a locally built AutoWhisper macOS .app bundle without installing it.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-audit}"
APP="$BUILD_DIR/AutoWhisper.app"
# Expected bundle payloads:
#   AutoWhisper.app/Contents/MacOS/autowhisper
#   AutoWhisper.app/Contents/Info.plist
BIN="$APP/Contents/MacOS/autowhisper"
INFO="$APP/Contents/Info.plist"
ENTITLEMENTS="$ROOT/platform/macos/entitlements.plist"

fail() {
    echo "macOS app smoke failed: $*" >&2
    exit 1
}

[[ "$(uname -s)" == "Darwin" ]] || fail "requires macOS/Darwin host"
[[ -d "$APP" ]] || fail "$APP not found; run cmake --build <build-dir> first"
[[ -x "$BIN" ]] || fail "$BIN is missing or not executable"
[[ -f "$INFO" ]] || fail "$INFO is missing"
[[ -f "$ENTITLEMENTS" ]] || fail "$ENTITLEMENTS is missing"

plutil -lint "$INFO" "$ENTITLEMENTS" >/dev/null

bundle_id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$INFO")"
[[ "$bundle_id" == "us.primemanifold.autowhisper" ]] || fail "unexpected CFBundleIdentifier: $bundle_id"

package_type="$(/usr/libexec/PlistBuddy -c 'Print :CFBundlePackageType' "$INFO")"
[[ "$package_type" == "APPL" ]] || fail "unexpected CFBundlePackageType: $package_type"

executable="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$INFO")"
[[ "$executable" == "autowhisper" ]] || fail "unexpected CFBundleExecutable: $executable"

lsui="$(/usr/libexec/PlistBuddy -c 'Print :LSUIElement' "$INFO")"
[[ "$lsui" == "true" ]] || fail "LSUIElement should be true for menu-bar app"

/usr/libexec/PlistBuddy -c 'Print :NSMicrophoneUsageDescription' "$INFO" >/dev/null
/usr/libexec/PlistBuddy -c 'Print :NSAppleEventsUsageDescription' "$INFO" >/dev/null
/usr/libexec/PlistBuddy -c 'Print :com.apple.security.device.audio-input' "$ENTITLEMENTS" >/dev/null

"$BIN" --version >/dev/null
codesign --verify --deep --strict "$APP"

# spctl can reject ad-hoc, Apple Development, or unnotarized Developer ID
# signatures; keep it as evidence without making locally built bundles look broken.
spctl --assess --type execute "$APP" >/dev/null 2>&1 || echo "spctl assessment did not accept this bundle (expected for ad-hoc, Apple Development, or unnotarized Developer ID builds)" >&2

printf 'AutoWhisper macOS app smoke passed: %s\n' "$APP"
