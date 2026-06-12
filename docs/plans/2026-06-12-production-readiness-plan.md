# AutoWhisper Production Readiness & Wispr Flow Parity Plan

Date: 2026-06-12
Status: PROPOSED — supersedes the provisional milestones in `.hermes/roadmap.md` once ratified
Scope: take AutoWhisper from "Linux-ready, macOS-beta, Windows-stub" to a production-level,
public-ready product on macOS, Windows, and Linux, with accounts/sign-on and feature parity
with Wispr Flow.

Evidence base: repository audit at commit `44b7fc6` (v0.7.1), `engineering/architecture.md`,
`audit/BASELINE.md`, `.hermes/state.md`, `TODOS.md`, and June 2026 research of
wisprflow.ai (features, pricing, docs).

---

## 1. Method

This plan uses a goal-gap method, consistent with the phase-gate style already used in
`.hermes/roadmap.md`:

1. **North Star** — one sentence describing the 1.0 product.
2. **Goals (G1–G8)** — each with a measurable success metric and a verification method.
   A goal is binary: met or not met. No goal is "done" on the strength of code existing;
   it is done when the verification passes.
3. **Gap analysis** — current state vs. target, grounded in file-level evidence.
4. **Milestones (M1–M7)** — ordered delivery gates. Each milestone lists exit criteria
   that map back to goals. A milestone ships as a tagged release.
5. **Decision log** — choices that need explicit owner sign-off before the affected
   milestone starts.

---

## 2. North Star

> **AutoWhisper 1.0**: a signed, auto-updating, system-wide voice-dictation app for
> macOS, Windows, and Linux that matches Wispr Flow's core experience (instant
> push-to-talk dictation anywhere, AI auto-edits, personal dictionary, 100+ languages)
> while keeping its differentiators: **local-first/offline inference, open source, and
> first-class Linux support** — with optional account sign-on powering sync, AI polish,
> and team features.

Positioning note: Wispr Flow has **no Linux app** (Mac/Windows/iOS/Android only) and is
cloud-only. AutoWhisper's wedge is "Wispr Flow experience, runs locally, also on Linux."
Parity therefore means *experience parity*, not *architecture copying*: we do not need a
cloud ASR pipeline to match the user-visible feature set.

---

## 3. Current state (audited 2026-06-12)

### 3.1 Platform reality

| Capability | Linux/X11 | macOS | Windows |
|---|---|---|---|
| Global hotkey (PTT + toggle) | ✅ real (`src/hotkey/platform/hotkey_x11.cpp`) | ✅ real (CGEventTap, `hotkey_macos.mm`) | ❌ stub (warn + no-op, `hotkey_win32.cpp`) |
| Text injection | ✅ real (XTest + xdotool/xclip fallback) | ✅ real (CGEventPost + NSPasteboard) | ❌ stub (all methods `return false`) |
| Tray / menu bar | ✅ real (GTK3 + AppIndicator) | ✅ real (NSStatusItem + SF Symbols) | ❌ stub (empty methods) |
| Audio capture | ✅ miniaudio/PulseAudio | ✅ miniaudio + mic permission flow | ⚠️ miniaudio compiles (WASAPI untested) |
| Service / autostart | ✅ systemd user unit | ⚠️ launchd plist exists, not wired to app launch | ❌ stub |
| Inference | ✅ CPU + optional CUDA | ✅ Metal | ⚠️ builds (CPU), no runtime proof |
| Packaging | ✅ deb + PPA workflow (Launchpad FTP currently blocked) | ✅ notarized .app zip (v0.7.1) | ❌ none |
| Signing | GPG (PPA) | ✅ Developer ID + notarization + staple | ❌ none |
| Wayland | ❌ none (X11 only; no portal/libei/ydotool code) | n/a | n/a |

Tests: 117/117 C++ (Catch2) + 35 static checks pass; CI builds Linux only.
Open defect: X11 hotkey resume deadlock after settings open/close
(`docs/hotkey-resume-deadlock.md`) — XRecord thread blocks in
`XRecordProcessReplies()` during `HotkeyManager::stop()` join.

### 3.2 Feature reality vs. Wispr Flow

| Wispr Flow feature (June 2026) | AutoWhisper today | Gap addressed in |
|---|---|---|
| Dictate into any app (Mac/Win) | ✅ Linux, ✅ macOS, ❌ Windows | M2 |
| 100+ languages, auto-detect, code-switching | ❌ English-only catalog (`src/models/models.cpp`, all `.en`) | M1 |
| Auto-edits: filler removal, auto-punctuation | ❌ none (only `lowercase` flag) | M4 |
| Backtracking ("at 2… actually 3") | ❌ none | M4 (LLM stage) |
| Numbered/bulleted list formatting | ❌ none | M4 |
| Personal dictionary (auto-learn + manual) | ❌ none | M4 |
| Snippets (voice shortcuts → text blocks) | ❌ none | M4 |
| Styles/tones per app context | ❌ none | M4 (LLM stage) |
| Command mode (voice-edit selected text) | ❌ none | M4 (LLM stage) |
| Whisper mode (quiet-voice dictation) | ⚠️ partial (VAD threshold config; not a tuned mode) | M4 |
| Context-aware name spelling | ❌ none | M4 (dictionary + prompt biasing) |
| Dev awareness (camelCase, file tags) | ❌ none | M4 / post-1.0 |
| Transcription history | ❌ none (fire-and-forget) | M4 (local, opt-in) |
| Accounts: Google/Apple/Microsoft/email sign-in | ❌ none (zero auth code) | M5 |
| Cross-device sync (dictionary/settings) | ❌ none | M5 |
| Free tier + Pro plan + billing | ❌ none | M5 |
| Team shared dictionary/snippets, dashboards | ❌ none | M7 (post-1.0) |
| Enterprise SSO/SAML, SOC 2, HIPAA | ❌ none | M7 (post-1.0) |
| Auto-update | ❌ none | M2/M3/M6 |
| Crash reporting / telemetry | ❌ none (also a privacy differentiator — keep opt-in) | M6 |
| iOS/Android apps | ❌ (iOS Swift foundation only) | out of scope for 1.0 |
| **AutoWhisper differentiators** | offline inference, open source, Linux support, no per-word metering of local dictation | preserve in all milestones |

---

## 4. Goals

Each goal states: what, success metric, verification.

### G1 — Three first-class desktop platforms
All of macOS, Windows, and Linux (X11 **and** Wayland) support the full loop:
hotkey → record → transcribe → insert at cursor, with tray, settings UI, autostart,
and onboarding.
- **Metric:** scripted E2E smoke (hotkey → text lands in a focused editor) passes on
  macOS 13+ (arm64+x86_64), Windows 10/11 x64, Ubuntu 24.04 X11, and Ubuntu 24.04
  GNOME Wayland.
- **Verification:** per-platform smoke scripts in `scripts/` run on real hardware/VM
  for every release candidate; results recorded in the release checklist.

### G2 — Dictation quality at parity
Multilingual recognition, measured accuracy, and bounded latency.
- **Metric:** multilingual models (`large-v3-turbo` class) in the catalog with language
  auto-detect; WER benchmark harness reports per-model accuracy; p95 end-of-speech →
  text-inserted latency ≤ 1.5 s for the recommended model on baseline hardware
  (M1, RTX 3060, modern 8-core CPU).
- **Verification:** `bench/` harness in CI (nightly) + published numbers replacing the
  unverified README speed table.

### G3 — Intelligence layer (the Wispr Flow "feel")
Filler removal, auto-punctuation/casing, list formatting, backtracking corrections,
personal dictionary, snippets, styles/tones, command mode, opt-in local history.
- **Metric:** parity rows in §3.2 marked M4 all shipped; deterministic formatter has
  golden-file tests; LLM features work fully offline with a bundled small model and
  optionally with a user-supplied cloud API key.
- **Verification:** golden-transcript test suite (spoken-input fixtures → expected
  cleaned text) in `ctest`; manual parity script comparing against Wispr Flow on the
  same utterances.

### G4 — Complete sign-on (accounts done correctly)
OAuth 2.0 Authorization Code + PKCE via the system browser (RFC 8252), supporting
Google, Apple, Microsoft, and email sign-in; tokens stored in the OS credential store
(Keychain / Windows Credential Manager / Secret Service); account powers entitlements,
device management, and encrypted sync of dictionary/snippets/settings.
- **Metric:** a user can sign in on any platform in < 30 s, sync a dictionary entry
  across two machines, sign out cleanly; **the core dictation loop never requires an
  account and never breaks offline.**
- **Verification:** auth E2E tests against a staging identity provider; offline-mode
  regression test (network disabled → dictation still works, UI degrades gracefully).

### G5 — Trusted distribution on every platform
Signed, notarized, auto-updating installers through native channels.
- **Metric:** macOS DMG + Homebrew cask (Developer ID, notarized, Sparkle 2);
  Windows installer + winget (Authenticode via Azure Trusted Signing, WinSparkle);
  Linux deb/PPA + Flatpak on Flathub (GPG; updates via apt/Flatpak). Zero scary OS
  warnings on first launch.
- **Verification:** clean-VM install test per platform per release; Gatekeeper
  `spctl --assess`, SmartScreen reputation check, `flatpak install` from Flathub.

### G6 — Production-grade reliability and security
- **Metric:** known deadlock fixed with regression test; 8-hour soak (incl. sleep/wake
  cycles) with zero hangs/leaks on all platforms; opt-in crash reporting wired;
  settings HTTP server hardened (loopback + per-session bearer token, CSRF-safe);
  model downloads verified by SHA-256; no Criticals in a pre-launch security review.
- **Verification:** soak harness logs, crash-reporter test event, security checklist
  signed off in `.hermes/decisions.md`.

### G7 — Repeatable release engineering
- **Metric:** one tag (`vX.Y.Z`) produces signed artifacts for all three platforms from
  CI (macOS/Windows/Linux runners), publishes release notes, appcasts, PPA, and Flathub
  automatically; beta channel exists.
- **Verification:** dry-run release from a release-candidate tag with no manual steps
  other than approvals.

### G8 — Public-ready surface
- **Metric:** docs site (install, permissions, troubleshooting per platform), privacy
  policy + terms, SECURITY.md, issue/PR templates, code of conduct, support channel,
  landing page live (un-guard `pages.yml`), name/trademark check resolved
  ("Whisper" is an OpenAI model name — legal review of "AutoWhisper" before launch).
- **Verification:** public-launch checklist complete; external smoke test by a
  non-developer following only the public docs.

---

## 5. Workstreams and technical decisions

### W1 — Windows port (closes the biggest platform gap)
Replace the three stub files with real Win32 implementations, mirroring the
per-platform file pattern used for macOS:

- **Hotkey:** `SetWindowsHookEx(WH_KEYBOARD_LL)` on a dedicated message-pump thread
  (push-to-talk needs key-release events; `RegisterHotKey` cannot deliver them).
  Reuse `hotkey_common.cpp` combo parsing; add a Win32 keycode map alongside the
  existing X11/macOS maps in `test_keycombo_platform.cpp`.
- **Output:** `SendInput` with `KEYEVENTF_UNICODE` (surrogate-pair aware, mirroring
  the UTF-16 handling already in `output_macos.mm`); clipboard via
  `CF_UNICODETEXT`; paste via synthesized Ctrl+V; keep the inject→clipboard
  fallback order contract in `test_output_fallback_order.cpp`.
- **Tray:** `Shell_NotifyIcon` + hidden message window; four state icons (reuse
  `icons/*.svg` rendered to .ico).
- **Audio:** miniaudio already targets WASAPI — validate device enumeration and the
  16 kHz mono capture path on real hardware; handle the Windows microphone privacy
  toggle in `doctor`.
- **Mute-other-apps parity:** `IAudioSessionManager2` per-session volume duck
  (Windows analog of the PulseAudio module).
- **Autostart:** `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` entry managed
  by the settings UI (a full Windows service is wrong for a per-user tray app with
  mic + input access).
- **GPU:** enable whisper.cpp Vulkan backend (`GGML_VULKAN`) for vendor-neutral
  acceleration; keep CUDA optional for NVIDIA.
- **CI:** add a `windows-latest` job (MSVC, not MinGW, for shippable binaries) running
  the 117-test core suite; keep the MinGW docker cross-build as a fast smoke.

### W2 — Linux Wayland support (protects the differentiator)
X11-only is a launch risk: Ubuntu/Fedora default to Wayland.
- **Hotkeys:** XDG Desktop Portal `GlobalShortcuts` where available (KDE, GNOME 45+);
  fallback: evdev listener (document the `input` group requirement in `doctor`).
- **Injection:** portal `RemoteDesktop`/libei virtual keyboard as primary; `ydotool`
  and `wtype` (wlroots) as fallbacks; clipboard via `wl-clipboard`. Keep the existing
  fallback-chain architecture — this is adding backends, not redesigning.
- **Tray:** StatusNotifierItem already works via AppIndicator on KDE; document the
  GNOME AppIndicator-extension requirement.
- **Packaging:** Flatpak (Flathub) with portal permissions; keep deb/PPA.
- Fix the X11 resume deadlock first (`XRecordDisableContext` from the control display
  + bounded join) — it's the only open Linux defect.

### W3 — macOS completion (beta → production)
- Wire launchd LaunchAgent autostart via `SMAppService` (macOS 13+) with `launchctl`
  fallback; "Start at login" toggle in settings.
- Sparkle 2 auto-update (EdDSA-signed appcast hosted on the Pages site).
- DMG installer + Homebrew cask (both already scoped S in `TODOS.md`).
- `.icns` bundle icon, `os_log` bridging, stress-test the CGEventTap timeout recovery
  across sleep/wake (items already tracked in `TODOS.md`).

### W4 — Intelligence layer (Wispr Flow feel, local-first)
Introduce a post-ASR **transcript pipeline** stage between `WhisperInference` and
`OutputManager` (new `src/pipeline/`), with two tiers:

- **Tier 1 — deterministic, offline, default-on (no LLM):**
  - filler-word removal (token filter, per-language lists);
  - punctuation/casing normalization (whisper already emits punctuation; normalize
    spacing, sentence case, terminal punctuation);
  - spoken-form conversion: numbers, "new line/new paragraph", numbered/bulleted lists;
  - **personal dictionary**: user terms applied two ways — biasing via whisper
    `initial_prompt` and post-ASR fuzzy replacement; auto-suggest entries from
    repeated user corrections (local heuristic);
  - **snippets**: trigger-phrase → text-block expansion;
  - whisper mode: low-gain capture profile + tuned VAD threshold preset.
- **Tier 2 — LLM-assisted, optional:**
  - backtracking corrections, tone/styles, command mode ("make this more concise"
    on selected text), per-app tone profiles (active-app detection: AXUIElement /
    `GetForegroundWindow` / portal — capability already partially present for focus
    handling);
  - **engine choice (Decision D3):** bundled small local instruct model via llama.cpp
    (e.g. Qwen-class 1.5–3B, downloaded like whisper models through the existing
    model manager) as the default; optional user-supplied cloud API key
    (OpenAI/Anthropic-compatible) for best quality. Cloud is opt-in per the privacy
    posture; nothing leaves the machine by default.
- **History (parity item):** opt-in local transcription history (SQLite, encrypted at
  rest, retention setting, one-click purge). Default off to honor the privacy story.
- Config schema, settings UI panes, and golden-file tests extend the existing
  `schema.cpp` / settings-UI machinery — no new UI framework.

### W5 — Accounts, sign-on, sync, monetization plumbing
The core stays usable forever without an account (open-source trust + Linux audience).
The account unlocks sync, cloud AI, and (later) teams.

- **Client:** OAuth 2.0 Authorization Code + PKCE in the system browser with loopback
  redirect (RFC 8252). Providers: Google, Apple, Microsoft, email (magic link).
  Tokens in Keychain / Credential Manager / libsecret. New `src/account/` module +
  settings UI "Account" pane showing plan, devices, sync status.
- **Backend (new, small, separate repo):** identity via a managed provider
  (**Decision D2**: Auth0 / Clerk / Supabase Auth class product — buy, don't build;
  must support all four providers + later SAML), plus a thin API for: entitlements,
  device registry, dictionary/snippet/settings sync (E2E-encrypted blobs; server
  stores ciphertext only), Stripe billing webhooks.
- **Plans (Decision D1, recommendation):** local dictation stays free and unlimited
  (differentiator vs. Wispr's 2,000-words/week free cap); **Pro** (~$8–12/mo) = sync,
  hosted AI polish, priority support; **Teams/Enterprise** post-1.0 (shared
  dictionary/snippets, dashboards, SSO/SAML). Open-core boundary: client fully
  open source; backend proprietary or source-available.
- **Sign in with Apple** requires an Apple Developer account configuration
  (Services ID) — already have the Developer ID account from notarization work.

### W6 — Reliability, observability, security
- Crash reporting: Sentry Native (Crashpad) — **opt-in at onboarding**, scrubbed,
  never includes audio or transcripts. Same consent gate for minimal usage telemetry
  (feature counters only).
- Soak/stress harness: scripted 8 h run with sleep/wake, device switching,
  rapid PTT; leak checks (ASan/LSan job in CI).
- Security hardening before public launch:
  - settings server: keep 127.0.0.1 bind, add per-session bearer token to the URL and
    require it on `/api/*` (blocks drive-by-browser/CSRF on localhost);
  - SHA-256 verification of model downloads;
  - secrets handling for auth tokens (no tokens in config.toml or logs);
  - third-party license inventory (vendored submodules) + SBOM in releases;
  - external or structured self security review gate (G6).

### W7 — Release engineering & CI matrix
- CI: `ubuntu-24.04` (X11 + Wayland headless tests), `macos-14` (arm64 build +
  tests), `windows-latest` (MSVC build + tests). Nightly: bench + soak-lite + ASan.
- Release on tag: build, sign (Developer ID + notarize; Azure Trusted Signing;
  GPG), package (DMG/zip, installer exe/MSI, deb, Flatpak), generate appcasts,
  upload GitHub Release, push PPA (fix the Launchpad FTP egress blocker or move to
  `dput` over SFTP/builder with network access), update winget + Homebrew + Flathub
  manifests. Channels: `stable` + `beta` appcasts.
- Versioning: keep semver; 1.0 is gated on G1–G8, not on a date.

### W8 — Public launch surface
- Docs site expansion of `site/` (per-platform install + permissions guides with
  screenshots, troubleshooting, FAQ, comparison page vs. Wispr Flow that is honest
  about cloud features).
- Legal: privacy policy, ToS, **trademark/name review** ("Whisper" association),
  imprint for billing.
- Community: SECURITY.md, CONTRIBUTING.md, issue templates, discussions/Discord,
  code of conduct.
- Un-guard `pages.yml` when the repo goes public; publish the benchmark numbers.

---

## 6. Milestones

Effort scale: S ≤ 1 wk · M ≈ 2–3 wk · L ≈ 4–6 wk · XL > 6 wk (single engineer + AI
agents; parallelize where workstreams are independent).

### M1 — v0.8 "Truth & foundations" (S+M)
Exit criteria:
- [x] X11 hotkey resume deadlock fixed with regression test (W2) — listener
      rewritten to canonical sync XRecord pattern; Xvfb+XTest integration tests.
- [x] Multilingual models in catalog + `language = "auto"` detect (G2) — plus
      SHA-256-pinned downloads and a language/model mismatch warning.
- [x] Benchmark harness (`bench/`) producing WER + latency; README table replaced
      with measured numbers (G2) — nightly bench workflow added.
- [~] CI matrix: macOS and Windows build+test jobs green alongside Linux (W7) —
      jobs added (linux+xvfb, macos-14, windows-cross MinGW, windows-msvc
      experimental); first MSVC findings already fixed (S_ISDIR, getpid).
- [ ] macOS: launchd autostart wired, `.icns` (`TODOS.md` items) — needs a
      macOS host for honest runtime verification; float-precision fix done.
- [x] Settings server bearer-token hardening + model SHA-256 verification (W6) —
      plus loopback-Host DNS-rebinding defense.

### M2 — v0.9 "Windows is real" (L)
Exit criteria:
- [ ] Win32 hotkey/output/tray/autostart implemented; E2E smoke passes on
      Windows 10 + 11 hardware (G1).
- [ ] WASAPI capture + mic-privacy doctor checks verified on hardware.
- [ ] Vulkan inference path benchmarked on one NVIDIA + one AMD GPU.
- [ ] Signed installer (Azure Trusted Signing) + winget manifest; clean-VM install
      with zero SmartScreen block (G5).
- [ ] WinSparkle auto-update from the beta appcast.

### M3 — v0.10 "All of Linux" (M/L)
Exit criteria:
- [ ] Wayland hotkeys (portal GlobalShortcuts + evdev fallback) and injection
      (libei/portal + ydotool/wtype fallback) pass E2E smoke on GNOME and KDE (G1).
- [ ] Flatpak on Flathub; PPA upload pipeline unblocked (G5).
- [ ] Soak harness runs green on all three platforms (G6).

### M4 — v0.11 "Intelligence" (XL — start in parallel with M2/M3; it is
platform-independent core work)
Exit criteria:
- [ ] Tier-1 pipeline (fillers, punctuation, lists, spoken commands, dictionary,
      snippets, whisper-mode preset) default-on with golden-file tests (G3).
- [ ] Opt-in local history with retention controls.
- [ ] Tier-2 LLM features (backtracking, styles, command mode, per-app tones) working
      offline via bundled small model; BYO cloud key optional (G3, D3).
- [ ] Side-by-side parity script vs. Wispr Flow documented in `research/`.

### M5 — v0.12 "Sign-on & sync" (L–XL, includes backend)
Exit criteria:
- [ ] Sign in with Google/Apple/Microsoft/email via PKCE + system browser on all
      three platforms; tokens in OS credential stores (G4).
- [ ] E2E-encrypted sync of dictionary/snippets/settings across devices (G4).
- [ ] Entitlement service + Stripe billing for Pro; free/local path untouched and
      fully functional signed-out/offline (D1).
- [ ] Auth E2E + offline-regression suites green (G4).

### M6 — v1.0 "Public launch" (M)
Exit criteria:
- [ ] Sparkle/WinSparkle/Flatpak auto-update verified upgrade v0.12 → 1.0 on all
      platforms (G5).
- [ ] Opt-in crash reporting + telemetry live with consent UX (G6).
- [ ] Security review gate passed; SBOM published (G6).
- [ ] One-tag full release pipeline dry-run with no manual steps (G7).
- [ ] Docs site, privacy policy, ToS, name/trademark check, support channel,
      community files; repo public; landing page live (G8).
- [ ] Launch checklist executed by a non-developer using only public docs (G8).

### M7 — post-1.0 "Teams & enterprise" (XL)
Shared dictionary/snippets, admin dashboards, SSO/SAML (WorkOS-class integration),
SOC 2 program, mobile (build on `ios/` foundation). Out of 1.0 scope by design —
matches Wispr's enterprise tier, pursued once individual product has traction.

Dependency notes: M4 is core-only and runs in parallel with M2/M3. M5 depends on D1/D2
sign-off and can start backend work during M3. M6 depends on everything.

---

## 7. Decision log (owner sign-off required)

| # | Decision | Options | Recommendation | Needed before |
|---|---|---|---|---|
| D1 | Monetization & open-core boundary | (a) mirror Wispr word-cap free tier; (b) local-free-unlimited + paid cloud/Pro | **(b)** — preserves differentiator and OSS trust; word caps on local compute are unenforceable in an Apache-2.0 client | M5 |
| D2 | Identity provider | Auth0 / Clerk / Supabase Auth / self-hosted Keycloak | Managed provider with Apple+Microsoft+Google+email and a SAML path (Auth0 or Clerk); revisit self-hosting post-1.0 | M5 |
| D3 | LLM strategy for Tier-2 | local-only / cloud-only / hybrid (local default, BYO-key cloud opt-in) | **hybrid** | M4 |
| D4 | Windows signing | OV cert / EV cert / Azure Trusted Signing | Azure Trusted Signing (cheapest path to SmartScreen reputation, CI-friendly) | M2 |
| D5 | Crash/telemetry vendor | Sentry / Bugsnag / none | Sentry Native, strictly opt-in | M6 |
| D6 | Product name | keep "AutoWhisper" / rename | run trademark review (OpenAI "Whisper" mark); rename only if counsel advises | M6 |
| D7 | History default | on (Wispr parity) / off (privacy) | **off by default**, easy to enable | M4 |

## 8. Risk register

| Risk | Impact | Mitigation |
|---|---|---|
| Wayland injection fragmentation (GNOME vs KDE vs wlroots) | Linux parity slips | multi-backend fallback chain (already the codebase pattern); clipboard fallback always works; document per-DE support matrix |
| No Windows hardware in current dev loop | M2 unverifiable | acquire Windows test machine/VM early in M1; add `windows-latest` CI in M1 so compile drift never accumulates |
| SmartScreen reputation lag even with signing | scary first-run UX at launch | Azure Trusted Signing + submit installer to Microsoft for malware-analysis allowlisting; ship via winget |
| Small local LLM quality below Wispr's cloud edits | parity perception | golden-suite quality bar before enabling by default; BYO cloud key as quality escape hatch; market honestly |
| Backend scope creep (auth/sync/billing) | M5 balloons | buy managed identity (D2); sync = encrypted blob store, not per-field merge; defer teams to M7 |
| Solo-maintainer bus factor at public launch | support load | docs-first support, issue templates, community channel, crash reporting to triage |
| `XRecord`/CGEventTap-style OS regressions | core loop breaks on OS updates | soak harness in nightly CI; beta channel catches OS-update breakage before stable |

## 9. Immediate next actions (M1 kickoff)

1. Ratify this plan + decisions D1–D7 in `.hermes/decisions.md`.
2. Fix the X11 resume deadlock (highest-severity open defect; design in
   `docs/hotkey-resume-deadlock.md`).
3. Add macOS + Windows CI jobs (compile + core tests) so platform drift stops now.
4. Add multilingual models + `auto` language to the catalog.
5. Stand up the benchmark harness and replace unverified README claims.
6. Procure: Windows test hardware, Azure Trusted Signing account, staging tenant for
   the chosen identity provider.
