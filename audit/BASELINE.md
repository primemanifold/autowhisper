# AutoWhisper Phase 0 Audit Baseline

Generated: 2026-04-30 on macOS audit host.

## Repository

- Branch: `primeodin/design-system-settings-ui` tracking `origin/primeodin/design-system-settings-ui`
- Commit: `b32d5612cb8b7e8403f3e6ed5a099480c6bc67bc`
- Build directory: `build-audit/`
- Configure log: `configure-audit.log`
- Build log: `build-audit.log`
- Test log: `ctest-audit.log`
- Warning audit: `audit/build-warnings.md`
- Static analysis: `audit/static-analysis.txt`

## Toolchain

- OS: `Darwin Channas-Mac-mini.attlocal.net 25.2.0 Darwin Kernel Version 25.2.0: Tue Nov 18 21:08:48 PST 2025; root:xnu-12377.61.12~1/RELEASE_ARM64_T8132 arm64`
- CMake: `cmake version 4.3.2`
- Compiler: `Apple clang version 21.0.0 (clang-2100.0.123.102)`
- cppcheck: `Cppcheck 2.20.0`

## Third-party dependency versions / submodule SHAs

Declared in `cmake/FetchDependencies.cmake`; checked out SHAs:

```text
6c7b07a878ad834957b98d0f9ce1dbe0cb204fc9 deps/CLI11 (6c7b07a)
 05e10dfccc28c7f973727c54f850237d07d5e10f deps/Catch2 (05e10dfc)
 8438df4a953da398fc794387618a3c9676b2ba7e deps/cpp-httplib (8438df4)
 4a5b74bef029b3592c54b6048650ee5f972c1a48 deps/miniaudio (4a5b74b)
 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03 deps/nlohmann-json (9cca280a)
 27cb4c76708608465c413f6d0e6b8d99a4d84302 deps/spdlog (27cb4c76)
 30172438cee64926dc41fdd9c11fb3ba5b2ba9de deps/tomlplusplus (3017243)
 3de9deead5759eb038966990e3cb5d83984ae467 deps/whisper.cpp (3de9dee)
```

Declared dependency versions from CMake comments:

- whisper.cpp v1.7.3
- toml++ v3.4.0
- CLI11 v2.4.2
- spdlog v1.14.1 with bundled fmt
- miniaudio 0.11.21
- cpp-httplib header-only vendored submodule
- nlohmann-json header-only vendored submodule
- Catch2 v3.5.2

## Phase 0 gate results

- Configure: PASS — `cmake -B build-audit -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON`
- Build: PASS — `cmake --build build-audit -j$(sysctl -n hw.ncpu || nproc || echo 2)`
- Tests: PASS — `ctest --output-on-failure` passed 83/83 tests.
- Static analysis: COMPLETED — `cppcheck --enable=all --suppress=missingIncludeSystem --std=c++20 src/` completed; findings are recorded in `audit/static-analysis.txt`.

## Warning / finding counts

- Build compiler warning lines: 11
- Build archiver warning lines: 6
- Configure warning/info lines captured: 8
- cppcheck warning lines: 3
- cppcheck error lines: 1 (not build errors; cppcheck configuration/parser findings)
- cppcheck style lines: 35
- cppcheck information lines: 58

## Fixes needed to pass the macOS audit build

- Added an AppleClang 21+ CMake guard that defines `FMT_CONSTEVAL=` for vendored `spdlog`/bundled fmt. This avoids AppleClang 21 rejecting fmt 10.2.1 consteval compile-time format checking while preserving the existing vendored dependency model and Linux behavior.
- Added missing `<unistd.h>` includes to two C++ test files that use `::getpid()`/`::getuid()`.

## Known gaps carried forward

[HAT: Engineering]
- macOS support is not production-ready despite the audit build passing on macOS: global hotkey, text insertion, and tray/menu bar files remain placeholders; permission flow, packaging, signing/notarization, and platform smoke tests remain open.
- Ubuntu/X11 remains the only platform implied by current product workflow and packaging; cross-platform support must not be marketed as complete.
- Settings UI still needs diagnostics-backed model, microphone, platform permission, and doctor-result panels; current privacy/local-first proof copy needs evidence-backed diagnostics before becoming a product claim.
- Static UI tests exist, but a real DOM/browser regression suite, keyboard-only acceptance checks, field-level validation/error association, and tray accessibility audit remain future work.

[HAT: Research]
- README model speed claims and any “fastest”/accuracy/privacy superiority claims remain unverified until a reproducible benchmark harness and sourced competitor research exist.
- Closest competitor and positioning versus Wispr Flow, Superwhisper, MacWhisper, Aqua Voice, and Apple Dictation remain unverified.

[HAT: Marketing]
- README currently leads with “GPU-accelerated,” while source-build CUDA defaults to OFF. Marketing/docs should qualify GPU acceleration as optional until packaging/defaults are aligned.
- README says `autowhisper config` opens the settings GUI, while current architecture notes browser UI is routed through `autowhisper config ui`; CLI/docs drift remains to be resolved.

[HAT: CEO]
- Phase 0 audit gate is now satisfied on this macOS host, but the production 1.0.0 target still requires Phase 1+ research, core hardening, daemon/CLI polish, diagnostics-rich settings UI, tray/desktop integration, Ubuntu packaging validation, and macOS platform implementation beyond placeholders.
