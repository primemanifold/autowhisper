# AutoWhisper Platform Validation Commands

These commands document what to run and what each result proves. A passing command never promotes claims beyond the proof boundary listed here.

## macOS

### Source and app-bundle checks

```bash
cmake -S . -B build-audit -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build-audit -j$(sysctl -n hw.ncpu)
ctest --test-dir build-audit --output-on-failure
python3 -m unittest tests.static.test_macos_app_bundle_assets -v
```

Proves: source build, unit/static app-bundle guardrails, and macOS packaging assertions compile locally.

Does not prove: the public v0.7.1 ZIP was downloaded, installed, opened, granted permissions, downloaded a model, captured microphone audio, or produced text on a clean Mac.

### Public artifact validation

```bash
curl -L -o /tmp/AutoWhisper-macOS-v0.7.1.zip \
  https://github.com/primemanifold/autowhisper/releases/download/v0.7.1/AutoWhisper-macOS-v0.7.1.zip
rm -rf /tmp/AutoWhisper-v0.7.1
unzip -q /tmp/AutoWhisper-macOS-v0.7.1.zip -d /tmp/AutoWhisper-v0.7.1
spctl --assess --type execute --verbose /tmp/AutoWhisper-v0.7.1/AutoWhisper.app
```

Proves: the v0.7.1 public release ZIP is reachable and can be inspected locally when run on macOS.

Does not prove: v0.7.2 fixes are public, nor full runtime_e2e transcription.

## Linux

### Source/static validation

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build-linux -j$(nproc)
ctest --test-dir build-linux --output-on-failure
python3 -m unittest discover -s tests/static -v
```

Proves: Linux source build and static claim tests pass in the current checkout.

Does not prove: PPA install, desktop permissions, X11/Wayland text output, microphone capture, or local transcription runtime_e2e.

### PPA install path

```bash
sudo add-apt-repository ppa:primemanifold/autowhisper
sudo apt update
sudo apt install autowhisper
autowhisper doctor
systemctl --user enable --now autowhisper
```

Proves: the documented PPA path installs and the service starts on the target Linux environment.

Does not prove: Wayland text injection; AutoWhisper is currently safest to validate on X11 for hotkey/text-output smoke tests.

## Windows

### Cross-build boundary

```bash
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake
cmake --build build-windows --config Release
```

Proves: Windows build_proven status only when the toolchain exists and build succeeds.

Does not prove: Windows runtime support, installer behavior, microphone capture, text insertion, model management, or runtime_e2e.

### Runtime gold standard

```powershell
# On a real Windows machine, after obtaining a Windows artifact:
.\autowhisper.exe doctor
.\autowhisper.exe run
```

Proves: only the specific Windows runtime behavior exercised during the manual smoke.

Does not prove: broad Windows support until install, permissions, microphone, model, and text output are documented.

## iOS

### Swift package and static checks

```bash
swift run --package-path ios AutoWhisperCoreChecks
python3 -m unittest tests.static.test_ios_app_shell -v
```

Proves: iOS core contract, app-shell guardrails, widget/deep-link claim boundaries, and placeholder transcription seams.

Does not prove: real local transcription or App Store readiness.

### Simulator and generic device builds

```bash
cd ios
xcodegen generate
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp   -destination 'generic/platform=iOS Simulator'   -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO build
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp   -destination 'generic/platform=iOS'   -sdk iphoneos CODE_SIGNING_ALLOWED=NO build
```

Proves: simulator and generic device buildability.

Does not prove: physical-device install, microphone permission flow, widget tap, TestFlight, App Store review, or real whisper.cpp transcript output.
