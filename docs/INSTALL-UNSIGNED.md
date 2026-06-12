# Download and run — no signing keys edition

These builds are **not code-signed** (no Apple Developer ID, no Windows
Authenticode yet — signing lands with the 1.0 release plan). Your OS will
warn once; this page shows the one-time skip on each system, and what to do
on a machine that has **no Whisper model downloaded yet**.

## Where to download

- **From CI (always freshest):** GitHub → *Actions* → pick the latest green
  run → *Artifacts*:
  `autowhisper-linux-x86_64`, `autowhisper-windows-x86_64-msvc` (native
  MSVC build; `autowhisper-windows-x86_64` is the MinGW build of the same
  code), `autowhisper-macos-universal-unsigned`.
- **From the repo:** the `dist/` folder on the feature branch carries this
  session's verified Linux/Windows builds with `SHA256SUMS`.

## Windows — skip SmartScreen (no keys needed)

1. Download and unzip `autowhisper.exe` anywhere.
2. First launch: **"Windows protected your PC"** → click **More info** →
   **Run anyway**. (Once per download; it's the unsigned-binary prompt.)
3. If the file came from a browser, you can also clear the mark instead:
   right-click → Properties → check **Unblock** → OK.
4. The exe is fully self-contained — no VC++ redistributable, no DLLs to
   copy.

## macOS — open an unsigned app (no keys needed)

1. Unzip. First launch will say the app "cannot be opened because the
   developer cannot be verified".
2. Either: **right-click → Open → Open** (the dialog gains an Open button),
   or System Settings → Privacy & Security → **Open Anyway**,
   or in Terminal: `xattr -d com.apple.quarantine ./autowhisper` (or the
   `.app`).
3. Note: the signed + notarized v0.7.1 release on the Releases page opens
   with no warnings at all if you'd rather skip this.

## Linux

```bash
tar -xzf autowhisper-*-linux-x86_64.tar.gz
chmod +x autowhisper   # usually already set
./autowhisper doctor
```

## First run on a machine with NO model downloaded

The binary ships **without** a Whisper model (they're 78 MB–3 GB). Nothing
is broken — you just need one model, once:

```bash
# see what's available (sizes + languages)
./autowhisper model list

# recommended starter (English, ~336 MB, SHA-256 verified download)
./autowhisper model download distil-small.en

# smallest possible (~78 MB) if bandwidth is tight
./autowhisper model download tiny.en
```

If you skip this and run anyway, AutoWhisper tells you exactly the command
above instead of crashing
(`Model 'distil-small.en' not found. Download with: …`).

**Want to see it do something with zero setup?** The companion needs no
model, no microphone, no permissions:

```bash
./autowhisper avatar demo        # Echo appears and walks every state
```

Then the real loop:

```bash
./autowhisper run                # hold Shift+Super, speak, release
```

## Android and iOS — the honest version

- **Android:** debug APKs need no purchased keys (Android auto-generates a
  debug keystore, and "Install unknown apps" allows sideloading). The
  mechanism is free — but **there is no Android app yet** (mobile is
  post-1.0/M7 in `docs/plans/2026-06-12-production-readiness-plan.md`;
  `design/mobile/android.html` is the design contract). We won't ship a
  hollow APK just to have a file.
- **iOS:** there is **no way to distribute a runnable iOS app without
  Apple signing** — even free-tier sideloading requires a per-device
  certificate created on *your* Mac with *your* Apple ID (Xcode or
  AltStore), and it expires in 7 days. Unsigned IPAs will not install,
  period. What exists today builds without any keys: the `ios/` Swift
  foundation package (`swift run AutoWhisperCoreChecks` on a Mac).

## Why is it unsigned, and when does that change?

Signing needs paid identities we haven't provisioned yet: Apple Developer
Program for macOS/iOS, Azure Trusted Signing for Windows (decisions D4/D6
in the production plan). Until then the skips above are safe for builds you
got from this repo's CI or `dist/` — verify with `SHA256SUMS` if in doubt.
