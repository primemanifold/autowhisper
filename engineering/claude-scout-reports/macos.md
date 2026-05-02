# Claude Scout Report — macos

Source JSON: `/tmp/autowhisper-claude-scouts/reports/macos.rerun.json`

Model/session metadata: subtype `success`, turns `33`, cost `0.60065995`.

---

I now have a complete picture. Here is the full scout report.

---

## AutoWhisper macOS Readiness Scout Report

### 1. Current macOS Proof Levels

| Proof level | Status | Key evidence |
|---|---|---|
| **Source build** | `ready` | `cmake --build` + CTest 117/117 on macOS host; universal arm64+x86_64 binary confirmed (hermes state 2026-05-01T17:38) |
| **App bundle** | `ready` | `MacOSBundle.cmake:57` — `add_custom_target(autowhisper_bundle ALL …)`; smoke script passes; `Info.plist`, `LSUIElement`, entitlements all present |
| **Developer ID signing** | `ready` | `codesign --verify --deep --strict` passes; authority chain `Developer ID Application → WWDRCA → Apple Root CA`; hermes state 2026-05-01T14:35 |
| **Notarization** | `release_proven` | v0.7.1: `xcrun notarytool` → `Accepted`; stapled; `spctl` → `source=Notarized Developer ID`; hermes state 2026-05-01T23:52 |
| **Public artifact** | `release_proven` | v0.7.1 ZIP downloaded fresh, checksum validated, Gatekeeper accepted, clean HOME first-run exited 0 and created `~/.config/autowhisper/config.toml`; CHANGELOG.md:4–9 documents validation steps |
| **First-run / model download** | `partial` | `SettingsApp.swift:239` — `autowhisper model download` subprocess wired; `onboarding.mm` creates user config and launches setup helper; no confirmed proof that download completes and app runs with model |
| **Microphone capture** | `partial` | `entitlements.plist` has `com.apple.security.device.audio-input`; `onboarding.mm:44` requests AVFoundation access; `desktop-platform-foundation.md:20` still lists audio as "partial — miniaudio compiles but release UX still needs microphone permission/onboarding" |
| **Hotkey (CGEventTap)** | `partial` | `hotkey_macos.mm` is a real CGEventTap implementation (not a placeholder), with tap-disabled re-enable, Input Monitoring preflight, listener-thread teardown; but no reproducible runtime smoke test against a real macOS user session |
| **Text injection** | `partial` | `output_macos.mm` is a real `CGEventPost` Unicode-chunk injector with clipboard fallback; no runtime test against an actual target app |
| **Menu bar** | `partial` | `tray_macos.mm` is a real `NSStatusItem` with SF Symbols state icons and action handlers; no runtime smoke confirming tray appears in a user session |
| **Runtime E2E** | `partial` | All individual seams are real code; CHANGELOG v0.7.0 `Boundaries` section (line 37): "does not yet claim … full hotkey→record→transcribe→insert product acceptance testing" |
| **DMG / PKG installer** | `unverified` | `platform/macos/install.sh` is dev-mode / unsigned-OK only; releases ship as ZIP; no DMG artifact, no Homebrew cask |
| **LaunchAgent service start** | `unverified` | `us.primemanifold.autowhisper.plist.in` exists and is rendered by `MacOSBundle.cmake:101`; `install.sh` copies it to `~/Library/LaunchAgents`; no smoke test verifies `launchctl load + start` cleanly |

---

### 2. Top 5 Gaps Preventing a Stronger macOS Release Claim

**Gap 1 — No runtime E2E test (highest impact)**

There is no documented, reproducible command that proves the full loop: press hotkey → microphone opens → audio captured → whisper.cpp transcribes → text injected at cursor. `hotkey_macos.mm`, `output_macos.mm`, and `tray_macos.mm` are real implementations, but CTest only exercises logic/unit tests, and no CI or smoke script runs them against a real macOS session. Without this, any claim that AutoWhisper "works on macOS" is signed-and-notarized packaging proof, not product proof.

**Gap 2 — Audio capture on macOS not smoke-proven**

`desktop-platform-foundation.md:20` explicitly lists audio as `partial` ("miniaudio compiles but release UX still needs microphone permission/onboarding"). There is no test or script that opens the default macOS input device, captures 1 second of PCM, and confirms non-silent data. This is a pre-requisite for any runtime E2E claim.

**Gap 3 — README is Ubuntu-only**

`README.md` has zero macOS content. A user who downloads `AutoWhisper-macOS-v0.7.1.zip` from the GitHub release page finds install instructions for `add-apt-repository` and `systemctl`. There is no macOS quick-start, no model download command, no permission guidance, and no mention of the `.app` bundle. This is an active risk of macOS users being blocked before first use.

**Gap 4 — No DMG/PKG installer or Homebrew cask**

The public release is a raw ZIP of `AutoWhisper.app`. `install.sh` requires `sudo`, is labelled "dev-mode, unsigned-OK," and is not referenced from the README or release notes. There is no `brew install --cask autowhisper` path. This is a significant friction point versus competitors (Wispr Flow, Superwhisper) that ship proper macOS installers.

**Gap 5 — LaunchAgent service flow unverified**

`MacOSBundle.cmake:101` renders `us.primemanifold.autowhisper.plist` into the build directory, and `install.sh` copies it to `~/Library/LaunchAgents`. But there is no `launchctl load + start` smoke and no documented command proving the service auto-starts on login and is manageable via `autowhisper start/stop/status` on macOS. The service commands in `cli.cpp:114–152` call `make_service_manager()`, and `platform/macos/` contains no `service_macos.mm` — meaning the macOS service manager path is still unclear.

---

### 3. Smallest Safe Implementation Slice for macOS in This Batch

**Slice: docs + platform matrix + claim-boundary static tests (no code changes)**

This is the PRD's own recommended first slice (§12). It is:

- Purely additive files
- Zero changes to C++, Swift, or CMake
- Completely reversible (delete the files to revert)
- Unlocks all future implementation lanes with an honest baseline

**Deliverables:**

1. `engineering/platform-readiness-matrix.md` — one row per platform, one column per proof level, using PRD §6 vocabulary (`ready`, `partial`, `unverified`, etc.), with explicit "what proves this" and "what this does not prove" notes for macOS.
2. `engineering/platform-validation-commands.md` — exact commands for each macOS proof level, noting what each proves and does not prove.
3. `tests/static/test_platform_readiness_claims.py` — claim-boundary tests (see §5 below).
4. MacOS section in `README.md` — minimal: install from ZIP, first-run steps, model download command, known permission requirements (Input Monitoring, Accessibility, Microphone), and explicit platform-limit statement.

---

### 4. Exact Commands to Verify the Slice

**After creating the docs + static tests:**

```bash
# Step 1 — static tests (including the new claim-boundary tests) must pass
python3 -m unittest discover -s tests/static -v

# Step 2 — existing macOS bundle asset tests must still pass
python3 -m unittest tests.static.test_macos_app_bundle_assets -v

# Step 3 — local source build proof (proves source build claim is accurate)
cmake -S . -B build-audit \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build-audit -j$(sysctl -n hw.ncpu)
ctest --test-dir build-audit --output-on-failure

# Step 4 — app bundle smoke (proves packaged claim is accurate)
BUILD_DIR=$PWD/build-audit scripts/macos_app_smoke.sh

# Step 5 — codesign verification of ad-hoc build (proves signing claim)
codesign --verify --deep --strict build-audit/AutoWhisper.app

# Step 6 — public artifact proof: fresh download of v0.7.1
# (run from a machine/user with a clean HOME or new shell environment)
curl -L -o /tmp/aw071.zip \
  "https://github.com/primemanifold/autowhisper/releases/download/v0.7.1/AutoWhisper-macOS-v0.7.1.zip"
sha256sum /tmp/aw071.zip  # compare against .sha256 asset
unzip -q /tmp/aw071.zip -d /tmp/aw071
spctl --assess --type execute --verbose=4 /tmp/aw071/AutoWhisper.app
/tmp/aw071/AutoWhisper.app/Contents/MacOS/autowhisper --version

# Step 7 — whitespace and diff hygiene
git diff --check
```

What each proves / does not prove:

| Command | Proves | Does NOT prove |
|---|---|---|
| `cmake --build` + `ctest` | Source compiles; unit/integration tests pass | App runs as a product on macOS |
| `macos_app_smoke.sh` | Bundle structure, plist fields, codesign verify, spctl assessment | Runtime behavior with mic/hotkey/injection |
| `codesign --verify` | Signature integrity | Notarization or Gatekeeper acceptance |
| `spctl --assess` on v0.7.1 ZIP | Gatekeeper accepts notarized artifact | First-run behavior, model download, E2E |
| `--version` from fresh download | Binary runs from install path | Audio capture, hotkey, text injection |

---

### 5. Claim-Boundary Tests That Should Exist

These should live in `tests/static/test_platform_readiness_claims.py`. Each is a guard against the specific regressions the PRD flags:

```python
# T1 — README must mention macOS prerequisites before runtime claims
def test_readme_macos_section_mentions_permission_requirements():
    """README must list Input Monitoring, Accessibility, Microphone before
    any macOS usage copy, so a first-time user isn't silently blocked."""
    readme = README.read_text()
    if "macOS" in readme:
        assert "Input Monitoring" in readme, "macOS section must mention Input Monitoring"
        assert "Accessibility" in readme, "macOS section must mention Accessibility"
        assert "Microphone" in readme, "macOS section must mention Microphone"

# T2 — Any macOS release claim must name the exact tag
def test_readme_macos_release_claims_cite_exact_tag():
    """A version number like 'v0.7.x' in macOS context must be adjacent to
    a download URL or asset name; prevents vague 'latest release' copy."""
    readme = README.read_text()
    version_mentions = re.findall(r'v\d+\.\d+\.\d+', readme)
    for version in version_mentions:
        idx = readme.index(version)
        context = readme[max(0, idx - 200):idx + 200]
        assert any(x in context for x in [
            "releases/download", ".zip", ".dmg", "sha256"
        ]), f"Version {version} cited without download artifact reference"

# T3 — macOS E2E must not be claimed as 'ready' in any platform matrix
def test_platform_matrix_does_not_claim_macos_e2e_ready():
    """runtime_e2e for macOS must not be 'ready' until a runtime smoke test
    proves hotkey→record→transcribe→insert works on a real macOS session."""
    matrix = (ROOT / "engineering" / "platform-readiness-matrix.md").read_text()
    # Find the macOS row; the runtime_e2e column must not be 'ready'
    macos_section = re.search(r'\|\s*macOS.*?\n', matrix)
    if macos_section:
        row = macos_section.group(0)
        assert "ready" not in row.lower() or "runtime_e2e" not in matrix.lower(), \
            "macOS runtime_e2e must not be 'ready' without a documented runtime test"

# T4 — audio capture must not be promoted to 'ready' without evidence note
def test_platform_foundation_doc_does_not_claim_macos_audio_ready():
    """desktop-platform-foundation.md must not change audio from 'partial' 
    to 'ready' without an adjacent evidence comment."""
    doc = (ROOT / "engineering" / "desktop-platform-foundation.md").read_text()
    # If the word 'ready' appears next to 'audio' in the macOS section, 
    # an evidence note must also be present
    macos_audio_matches = re.findall(
        r'(?i)(audio.{0,40}ready|ready.{0,40}audio)', doc)
    for match in macos_audio_matches:
        assert "proof" in doc or "smoke" in doc or "verified" in doc, \
            f"'audio ready' claim found without adjacent evidence: {match}"

# T5 — DMG / installer must not be claimed until artifact exists
def test_no_dmg_claim_without_artifact_evidence():
    """README and release docs must not say 'DMG installer available' until 
    a DMG artifact has been built and validated."""
    for path in [README, ROOT / "CHANGELOG.md"]:
        if not path.exists():
            continue
        content = path.read_text()
        if re.search(r'\bDMG\b', content, re.IGNORECASE):
            # If DMG is mentioned, a release asset URL or build command must accompany it
            assert re.search(r'releases/download.*\.dmg|cmake.*\.dmg|hdiutil', content), \
                f"{path.name} mentions DMG without an artifact URL or build command"

# T6 — LaunchAgent install must not claim 'start-on-login works' without smoke
def test_install_script_does_not_claim_service_start_is_verified():
    """install.sh is dev-mode only; it must not contain language implying
    the service has been runtime-proven on a fresh machine."""
    install = (ROOT / "platform" / "macos" / "install.sh").read_text()
    assert "verified" not in install.lower() or "dev" in install.lower(), \
        "install.sh must not claim verification without evidence"
    # Positive: the script should note its dev-mode scope
    assert "dev" in install.lower() or "unsigned" in install.lower(), \
        "install.sh must explicitly identify its dev-mode / unsigned-OK scope"
```

---

**Summary:** v0.7.1 is the strongest macOS claim achievable from source evidence — notarized artifact, Gatekeeper-accepted, first-run onboarding. The remaining gap is runtime E2E (mic → transcribe → inject), which the code is now structurally ready for but no test has proven end-to-end. The safest next slice is the docs/matrix/claim-boundary test layer, which closes the documentation drift risk before any new platform code lands.
