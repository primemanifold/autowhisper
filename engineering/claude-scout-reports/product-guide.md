# Claude Scout Report — product-guide

Source JSON: `/tmp/autowhisper-claude-scouts/reports/product.json`

Model/session metadata: subtype `success`, turns `17`, cost `0.243187`.

---

## AutoWhisper User-Facing Documentation Scout Report

I've completed a read-only inspection of AutoWhisper's user-facing docs, cross-platform readiness, and media/guide planning. Here are the findings.

---

## 1. User-Facing Claims That May Overstate Platform Support

### Current state vs. honest boundaries

| Surface | Current language | Honest status | Risk |
| --- | --- | --- | --- |
| **README.md header** | "GPU-accelerated voice-to-text for Ubuntu" | Linear-focused Linux product, but macOS v0.7.1 publicly available | LOW — header is accurate but incomplete for cross-platform users |
| **README quick-start** | Only shows Linux PPA install; no macOS/iOS/Windows guidance | macOS notarized app available, iOS app shell runnable, Windows buildable but unproven | MEDIUM — users expecting cross-platform guidance find none |
| **site/index.html landing** | "Desktop power users" + macOS download + Linux PPA shown side-by-side | Accurate; macOS v0.7.1 link is current. | LOW — landing page is honest about two platforms |
| **iOS claims** (implicit absence) | No iOS docs/claims visible in README or site | iOS app shell is runnable + microphone recording + audio decoder, but NO real Whisper transcription | LOW — silence is safer than claiming "local transcription" |
| **Windows** (implicit absence) | No Windows mention | Windows cross-build proven but runtime unproven | LOW — not claimed, correctly omitted |
| **"Local-first" on landing** | Copy says "Audio is processed on-device" | True for Linux/macOS; iOS remains bridge-pending; Windows untested at runtime | MEDIUM — iOS claim is aspirational, not proven |

### Specific claims to audit

**docs/MACOS.md:**
- ✅ Honest about permissions (Input Monitoring, Accessibility, Microphone).
- ✅ Accurate hotkey/text-injection behavior with fallback to clipboard.
- ✅ Clear troubleshooting steps and known limitations.

**site/index.html:**
- ✅ `v0.7.1` asset link is current and notarized.
- ✅ "Local-first by default" is true for macOS/Linux (iOS pending).
- ⚠️ "Native desktop path" suggests macOS/Linux parity; macOS app bundle is real, Linux PPA delivery is complete.

**README.md:**
- ⚠️ "GPU-accelerated... for Ubuntu" is header-accurate but hides macOS/iOS/Windows existence.
- ✅ Linux PPA install is straightforward and correct.
- ⚠️ No mention of macOS as a release platform for users (only `MACOS.md` for developers).
- ❌ No iOS user guidance (only development scaffold in separate branch).

**Validation test gaps:**
- `tests/static/test_website_assets.py` validates v0.7.1 asset presence and landing structure.
- `tests/static/test_ios_app_shell.py` enforces iOS honesty: "transcribe locally" is forbidden, "Whisper bridge pending" is required.
- No static test yet enforces README/landing platform-status consistency.

---

## 2. README Quick-Start Guide Outline with Screenshot Captions

### Proposed structure

```markdown
# AutoWhisper

Offline, local-first speech-to-text for power users. Press a hotkey, speak, release — text appears.

## Quick Start by Platform

### macOS (notarized app, ready to use)
**Caption:** "Download and launch in seconds. No cloud, no signup."

1. Download [AutoWhisper-macOS-v0.7.1.zip](https://github.com/primemanifold/autowhisper/releases/download/v0.7.1/AutoWhisper-macOS-v0.7.1.zip)
2. Unzip, drag `AutoWhisper.app` to `/Applications`
3. Launch and grant permissions (Microphone, Input Monitoring, Accessibility)
4. Default hotkey: `⇧⌘` (Shift+Command)

[Screenshot: macOS first-run permission sheet + menu-bar icon + settings panel]

### Linux (PPA, Ubuntu/Debian)
**Caption:** "Command-line power user setup. Runs as a systemd user service."

```bash
sudo add-apt-repository ppa:primemanifold/autowhisper
sudo apt update && sudo apt install autowhisper
systemctl --user enable --now autowhisper
```

Default hotkey: `Shift+Super`

[Screenshot: terminal output from `autowhisper doctor` showing all checks OK]

### iOS (beta app shell, recording ready, transcription coming soon)
**Caption:** "Early-stage iOS app. Tap Record, speak, decode audio — real Whisper transcription is next."

- Tap the Quick Record widget or the app.
- Grant microphone permission.
- Recorded audio is decoded for the next transcription bridge.
- **Not yet:** real local Whisper transcription, background recording, app-store availability.

[Screenshot: iOS simulator showing app shell, record button, decoded-audio pending message]

### Windows
Status: buildable from source, runtime untested. See [Windows feasibility scout](engineering/windows-feasibility-scout.md).

## Model & Performance

[Table with model name, speed, accuracy — same as current README]

[Screenshot: settings panel showing model download and selection]

## Troubleshooting

- `autowhisper doctor` — diagnose system state
- Logs: Linux `journalctl --user -u autowhisper -f`; macOS `autowhisper logs -f`
- Common issue: hotkey doesn't fire → check Input Monitoring permission

[Screenshot: settings Diagnostics panel showing platform readiness + permission status]

## Where Your Data Stays

- Audio is transcribed on-device using local Whisper models.
- No cloud uploads. No telemetry. No analytics.
- All data (config, logs, models) lives in `~/.config/autowhisper/` (Linux/macOS).

---

## License

Apache 2.0
```

### Key caption principles
- **Before/after pairs:** show before-permission state and after-success state.
- **Platform honesty:** each platform shows exactly what works, and what's coming next.
- **Proof context:** settings screenshots prove diagnostics, permission states, and model availability.
- **Call to action:** make it clear what the user can do **right now** vs. what they're waiting for.

---

## 3. docs/usage-guide.md Outline

```markdown
# AutoWhisper User Guide

## Installation

[Link to README quick-start by platform]

## First Run: Permissions & Setup

### macOS
1. Launch the `.app`.
2. Grant **Microphone** (system will prompt).
3. Grant **Input Monitoring** (Settings > Privacy & Security > Input Monitoring).
4. Grant **Accessibility** (Settings > Privacy & Security > Accessibility).
5. Restart the app.

[Screenshot: macOS privacy panel + microphone icon in menu bar]

### Linux
```bash
autowhisper doctor
```
If any checks fail, follow the CLI guidance to grant D-Bus/hotkey permissions.

### iOS
Tap the app > "Request Microphone Permission" > grant in system dialog.

## Recording & Transcription

### Basic workflow: push-to-talk
1. Hold the hotkey (default: `⇧⌘` on macOS, `Shift+Super` on Linux).
2. Speak.
3. Release. Text appears at your cursor (or clipboard if injection is blocked).

[Screenshot/GIF: cursor in text editor, hotkey pressed, text inserted]

### Adjusting the hotkey
- macOS: Settings > Hotkeys > Trigger
- Linux: `~/.config/autowhisper/config.toml` > `[hotkeys] trigger = ["shift+super"]`

### Model selection
1. Open Settings (macOS: menu-bar icon > Settings; Linux: `autowhisper config`).
2. Choose a model: `tiny.en` (fast), `distil-small.en` (balanced), `distil-large-v3` (best).
3. Choose compute: `cuda` (GPU), `metal` (macOS GPU), `cpu` (safe).
4. Download the model.

[Screenshot: Settings panel > Models section showing download status]

## Troubleshooting

### "Hotkey doesn't fire"
**Cause:** Missing Input Monitoring permission.
**Fix:**
- macOS: Settings > Privacy & Security > Input Monitoring, add AutoWhisper.
- Linux: Check `autowhisper doctor` output for D-Bus/hotkey errors.

### "Text doesn't appear at cursor"
**Cause:** Missing Accessibility permission.
**Fix:**
- macOS: Settings > Privacy & Security > Accessibility, add AutoWhisper.
- Fallback: text is copied to clipboard; use Cmd+V to paste.

### "Model download fails"
**Cause:** Network issue or disk space.
**Fix:** Check `autowhisper logs` (macOS) or `journalctl --user -u autowhisper -f` (Linux).

### "Audio sounds wrong or whisper doesn't recognize me"
**Cause:** Microphone level, accent, background noise, or model limitations.
**Tip:** Try recording a clear, quiet sentence. If it still fails, try the larger model.

[Screenshot: Settings > Audio > Microphone volume meter]

## Advanced Config

[Defer to `README.md` config section; keep usage guide user-focused]

## Platform Limits

- **macOS:** Requires macOS 12+; M1/M2/Intel supported.
- **Linux:** X11 or Wayland desktop required; tested on Ubuntu 22.04+.
- **iOS:** App-shell ready; real Whisper transcription coming soon.
- **Windows:** Coming soon; cross-compile proof exists.

## Privacy Policy & Data Retention

[Single paragraph: no uploads, no telemetry, all local]
```

---

## 4. docs/media/ Inventory Proposal

```
docs/media/
├── README.md                            # Inventory + versioning strategy
├── screenshots/
│   ├── macos-app-launcher.png           # First-run permission sheet + settings
│   ├── macos-settings-models.png        # Model selection panel
│   ├── macos-settings-hotkeys.png       # Hotkey config
│   ├── macos-diagnostics-status.png     # Platform readiness + microphone
│   ├── linux-terminal-doctor.png        # `autowhisper doctor` output
│   ├── linux-settings-ui.png            # Browser settings (screenshot from --run mode)
│   ├── linux-hotkey-in-editor.png       # Text insertion after hotkey press
│   └── ios-app-shell-record.png         # iOS app + record button
├── gifs/
│   ├── macos-hotkey-to-text.gif         # Hotkey → record → text insertion (2-3s)
│   ├── model-download.gif               # Settings > Models > Download flow (5-7s)
│   └── linux-systemd-start.gif          # `systemctl --user enable --now` + logs (3-4s)
├── videos/
│   └── (reserved for Manim explainer, see below)
└── licenses.md                          # Screenshot source/attribution
```

### Screenshot specifications

| Screenshot | Purpose | Proof level | Update frequency |
| --- | --- | --- | --- |
| `macos-app-launcher.png` | First-run permission prompts + menu bar | Proof: app bundle launch + permission request | Each macOS release |
| `macos-settings-models.png` | Model availability + download UX | Proof: settings HTTP API + model catalog | When catalog changes |
| `macos-diagnostics-status.png` | Platform readiness + permission status | Proof: platform capability JSON + diagnostics | Each platform change |
| `linux-terminal-doctor.png` | `autowhisper doctor` output | Proof: CLI diagnostics + system check | Each doctor change |
| `linux-settings-ui.png` | Browser settings interface | Proof: embedded settings server + rendering | Design system changes |
| `linux-hotkey-in-editor.png` | Text insertion after hotkey | Proof: live X11 hotkey capture + text inject | Regression test |
| `ios-app-shell-record.png` | iOS app + record button + permission state | Proof: simulator build + app shell rendering | Each iOS slice |

### GIF specifications

| GIF | Purpose | Duration | Frame rate | Proof |
| --- | --- | --- | --- | --- |
| `macos-hotkey-to-text.gif` | Hotkey hold → record light → text insert | 2–3s | 10 fps | Live hotkey + text inject |
| `model-download.gif` | Settings > Models > select > download > ready | 5–7s | 10 fps | HTTP API + progress event |
| `linux-systemd-start.gif` | Service enable → systemd logs → ready | 3–4s | 10 fps | Service activation + log output |

### Capture automation

```bash
# macOS app + settings screenshot
BUILD_DIR=$PWD/build-release ./scripts/macos_app_smoke.sh --screenshot

# Linux settings + doctor output
autowhisper doctor > docs/media/screenshots/linux-doctor-output.txt
python3 scripts/settings_ui_screenshot.py > docs/media/screenshots/linux-settings-ui.png

# iOS simulator
xcodebuild ... -derivedDataPath /tmp/autowhisper-ios-derived build
xcrun simctl launch booted com.primemanifold.autowhisper.ios
xcrun simctl io booted screenshot docs/media/screenshots/ios-app-shell.png
```

---

## 5. Manim Explainer Video Concept: "How AutoWhisper Turns Hotkey into Text"

### Narrative arc

**Title:** "AutoWhisper: From Hotkey to Local Text in One Tap"  
**Target duration:** 90–120 seconds  
**Target audience:** new users deciding if AutoWhisper fits their workflow

### Scenes

**Scene 1: Problem (8 seconds)**
- Fade in: user typing in email/code editor.
- Annotation: "Want to capture thoughts faster?"
- Visual: cursor blinking, user hesitates.
- Narration: "Switching between typing and talking breaks focus."

**Scene 2: Solution intro (10 seconds)**
- AutoWhisper icon/logo appears.
- Narration: "AutoWhisper runs on your machine. Hotkey. Speak. Done."
- Visual: big hotkey symbol (⇧⌘) with motion.

**Scene 3: The flow — hotkey down (12 seconds)**
- Timeline diagram: `User presses hotkey` → `Hotkey listener captures press`.
- Visual: hotkey icon highlights, audio waveform starts animating.
- Narration: "When you press the hotkey, AutoWhisper listens."

**Scene 4: The flow — audio capture (12 seconds)**
- Audio waveform grows as microphone captures sound.
- Timeline: `Microphone records audio` → `Silence trimmed` → `Audio buffer ready`.
- Narration: "Your voice is captured locally. Silence is trimmed."
- Visual: waveform animates, trailing silence fades.

**Scene 5: The flow — transcription (15 seconds)**
- Timeline: `Audio buffer` → `Whisper.cpp model processes` → `Text output`.
- Visual: waveform → Whisper logo → text appearing in editor.
- Narration: "Your device runs Whisper AI to transcribe locally. No cloud, no uploads."
- Data privacy callout: lock icon, "100% local" badge.

**Scene 6: The flow — text insertion (12 seconds)**
- Timeline: `Text ready` → `Inject into app` → `Editor shows text`.
- Visual: text object moves from AutoWhisper UI into the editor under the cursor.
- Narration: "Text is inserted at your cursor. Keep typing."
- Fallback callout: "If injection is blocked, text goes to clipboard."

**Scene 7: Platform coverage (15 seconds)**
- Icons appear: Linux penguin ✅, macOS apple ✅, iOS phone ⏳ (spinner), Windows ⏳.
- Narration: "Works on Ubuntu, macOS, and coming soon to iOS and Windows."
- Callout: "See docs for your platform."

**Scene 8: Call-to-action (12 seconds)**
- Big link/button: `autowhisper.primemanifold.us` or GitHub release.
- Narration: "Download AutoWhisper today. Local transcription, always on your terms."
- Fade to logo.

**Scene 9: Footer (5 seconds)**
- "No cloud. No telemetry. No vendor lock-in."
- GitHub repo link.
- Creative Commons or license callout.

### Visual design notes

- **Color palette:** Soft dark blue (AutoWhisper brand), accent orange/green for "ready" state, gray for "disabled/pending".
- **Typography:** Clear sans-serif labels, minimal animations, avoid motion sickness.
- **Icons:** Microphone → waveform → Whisper logo → text symbol, all using consistent stroke weight.
- **Timeline visual:** horizontal left-to-right pipeline with smooth stage transitions.

### Output path

```
docs/media/videos/autowhisper-hotkey-to-text-90s.mp4
```

### Rendering command (Manim)

```bash
manim -pql docs/media/autowhisper_explainer.py AutoWhisperExplainerScene \
  -o autowhisper-hotkey-to-text-90s.mp4
```

### Hosting & embedding

- Embed in README with YouTube/GitHub video link.
- Alternative: link to GitHub Release assets if self-hosted.
- Subtitle/caption in English + future locales.

---

## 6. Static Claim-Boundary Tests That Should Enforce Guide & Docs Honesty

### Proposed test file: `tests/static/test_docs_claims.py`

```python
import unittest
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


class UserDocsClaimsTest(unittest.TestCase):
    """Prevent documentation from overclaiming platform support or feature readiness."""

    def setUp(self):
        self.readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.site = (ROOT / "site" / "index.html").read_text(encoding="utf-8")
        self.macos_docs = (ROOT / "docs" / "MACOS.md").read_text(encoding="utf-8")

    # macOS claims
    def test_readme_does_not_claim_macos_as_release_without_artifact_proof(self):
        """README should not claim macOS works without linking to a real release asset."""
        # Current: README says "For macOS, see docs/MACOS.md" OR direct link to app.
        # This test ensures no claim of "macOS ready" without a real download link.
        if "macOS" in self.readme or "macos" in self.readme.lower():
            self.assertRegex(
                self.readme,
                r"(https://github\.com.*AutoWhisper-macOS.*\.zip|/docs/MACOS\.md)",
                "Any macOS mention must link to real artifact or platform docs."
            )

    def test_site_macos_download_link_matches_released_version(self):
        """The landing page macOS download must point to an actual released asset."""
        # Ensures we don't claim a version that doesn't exist.
        self.assertRegex(
            self.site,
            r"AutoWhisper-macOS-v0\.7\.[0-9]+\.zip",
            "Site must link to a released macOS version, not a placeholder."
        )

    # iOS claims — critical boundaries
    def test_readme_does_not_claim_ios_real_transcription(self):
        """README must not claim iOS supports real Whisper transcription yet."""
        ios_section = re.search(r"iOS|iPhone|iPad|mobile", self.readme, re.IGNORECASE)
        if ios_section:
            # If iOS is mentioned, it must not claim local transcription.
            for forbidden in [
                "transcribe locally",
                "local whisper",
                "offline speech-to-text on ios",
                "real transcription",
            ]:
                self.assertNotIn(
                    forbidden.lower(),
                    self.readme.lower(),
                    f"iOS docs must not overclaim: '{forbidden}'"
                )

    def test_ios_app_shell_readme_is_honest_about_bridge_pending(self):
        """ios/README.md must explicitly say real Whisper bridge is pending."""
        ios_readme = (ROOT / "ios" / "README.md").read_text(encoding="utf-8")
        self.assertIn(
            "whisper bridge pending",
            ios_readme.lower(),
            "iOS README must say real Whisper transcription is still pending."
        )
        self.assertNotIn(
            "transcribe locally on this device",
            ios_readme.lower(),
            "iOS README must not claim local transcription is working."
        )

    # Widget claims
    def test_ios_widget_docs_do_not_claim_microphone_recording(self):
        """Docs must not claim iOS widget can record microphone audio directly."""
        docs_text = "\n".join([
            self.readme,
            (ROOT / "ios" / "README.md").read_text(encoding="utf-8"),
        ])
        self.assertNotIn(
            "widget can record",
            docs_text.lower(),
            "Widget docs must not claim direct microphone recording."
        )
        self.assertIn(
            "widget opens the foreground app",
            docs_text.lower(),
            "Widget docs must explain that widget is a launcher only."
        )

    # Windows claims
    def test_readme_does_not_claim_windows_ready(self):
        """README must not claim Windows runtime works without explicit caveat."""
        if "windows" in self.readme.lower() or "windows" in self.site.lower():
            combined = self.readme + self.site
            # If Windows is mentioned, it must be explicitly not-ready or deferred.
            self.assertTrue(
                any(phrase in combined.lower() for phrase in [
                    "windows: coming soon",
                    "windows: buildable",
                    "windows: not yet",
                    "windows runtime unproven",
                ]),
                "Any Windows mention must include explicit status caveat."
            )

    # Linux claims
    def test_readme_linux_install_is_current(self):
        """Linux PPA reference must point to the actual PPA namespace."""
        self.assertIn(
            "ppa:primemanifold/autowhisper",
            self.readme,
            "Linux install must use the correct PPA reference."
        )

    # Platform coverage
    def test_landing_page_does_not_claim_feature_parity(self):
        """Site should not claim all platforms have equal feature support."""
        # "Native desktop path" is OK (macOS + Linux); "all platforms" would be wrong.
        self.assertNotIn(
            "all platforms supported",
            self.site.lower(),
            "Landing should not claim feature parity across all platforms."
        )

    # Privacy claims
    def test_local_first_claim_is_backed_by_architecture(self):
        """Any 'offline' or 'local-first' claim must be architecturally sound."""
        if "offline" in self.readme.lower() or "local-first" in self.readme.lower():
            # Ensure docs acknowledge cloud-upload is not an option.
            self.assertTrue(
                any(phrase in self.readme.lower() for phrase in [
                    "no cloud",
                    "no uploads",
                    "on-device",
                    "offline",
                ]),
                "'Local-first' claims must be supported by no-cloud architecture language."
            )

    # Macos-specific platform gates
    def test_macos_docs_are_honest_about_permissions(self):
        """macOS docs must list all three required permissions."""
        for perm in ["microphone", "input monitoring", "accessibility"]:
            self.assertIn(
                perm.lower(),
                self.macos_docs.lower(),
                f"macOS docs must mention {perm} permission requirement."
            )

    def test_macos_docs_explain_permission_degradation(self):
        """macOS docs should explain what happens if a permission is denied."""
        self.assertIn(
            "clipboard",
            self.macos_docs.lower(),
            "macOS docs should mention clipboard fallback when Accessibility is denied."
        )


if __name__ == "__main__":
    unittest.main()
```

### Test coverage strategy

| Claim boundary | Test | Enforces |
| --- | --- | --- |
| iOS real transcription | `test_readme_does_not_claim_ios_real_transcription` | Prevents "local transcription" in any iOS docs |
| iOS widget recording | `test_ios_widget_docs_do_not_claim_microphone_recording` | Widget is launcher only |
| Windows readiness | `test_readme_does_not_claim_windows_ready` | Explicit unproven/coming-soon status |
| macOS asset link | `test_site_macos_download_link_matches_released_version` | Download link matches actual release |
| Platform parity | `test_landing_page_does_not_claim_feature_parity` | No false claims of feature parity |
| Local-first claim | `test_local_first_claim_is_backed_by_architecture` | Privacy language is consistent |
| macOS permissions | `test_macos_docs_are_honest_about_permissions` | All three perms listed + degradation explained |

### Running in CI

```yaml
# .github/workflows/ci.yml
- name: Validate docs claim boundaries
  run: python3 -m unittest tests.static.test_docs_claims -v
```

---

## Summary: Proof levels by platform

| Platform | Build | Package | Public artifact | Install/first-run | Runtime E2E | Status |
| --- | --- | --- | --- | --- | --- | --- |
| **Linux** | ✅ Tested | ✅ PPA package | ✅ v0.7.1 PPA | ✅ systemd service | ⚠️ Hotkey/X11 partial | `partial` |
| **macOS** | ✅ Tested | ✅ .app bundle | ✅ v0.7.1 notarized ZIP | ✅ Tested with first-run helper | ⚠️ Permissions/injection tested locally | `ready` for app bundle; `partial` for full product |
| **Windows** | ✅ MinGW cross-build | ❌ No runtime | ❌ No release | ❌ Not tested | ❌ Unproven | `build_proven` |
| **iOS** | ✅ Simulator/device build | ✅ Xcode project | ❌ No TestFlight/App Store | ❌ Simulator only, no device signing | ❌ No real Whisper, placeholder bridge only | `partial` (shell ready, transcription pending) |

---

**Next steps:** Implement the proposed docs structure, static tests, and media inventory. Once approved, the cross-platform-readiness PRD can move from plan-only to implementation lanes.
