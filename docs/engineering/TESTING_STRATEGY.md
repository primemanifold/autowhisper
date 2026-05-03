# Testing Strategy

Last updated: 2026-05-03

## Existing tests/checks

- Python static/docs tests: `python3 -m unittest discover -s tests/static -v`.
- Swift package checks: `swift run --package-path ios AutoWhisperCoreChecks`.
- iOS app/widget build: `cd ios && xcodegen generate && xcodebuild ...`.
- C++ CMake/CTest where dependencies are available.
- GitHub Actions: Ubuntu `build`; macOS `ios-build`; release/source/deb jobs are conditional/skipped outside release contexts.

## Weak spots

- iOS tests rely heavily on static string checks.
- Runtime microphone/hotkey/output E2E tests are platform-specific and incomplete.
- Benchmark/performance tests are not yet first-class.
- Visual regression testing is absent.

## Next testing improvements

1. Swift/XCTest or injectable Swift seams for iOS recorder/transcriber state.
2. Desktop benchmark fixture harness.
3. macOS public artifact smoke validation script.
4. Browser/DOM smoke tests for settings UI where practical.
