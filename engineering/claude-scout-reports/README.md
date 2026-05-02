# Claude Scout Reports — Cross-Platform Readiness

These reports were generated as the Phase 1 **read-only** scout pass for the AutoWhisper cross-platform readiness plan. They are evidence-gathering inputs for selecting the next implementation lane; they are not implementation work and do not promote any platform claim by themselves.

## Reports

| Report | Focus |
| --- | --- |
| [`macos.md`](macos.md) | macOS release, app bundle, notarization, first-run, permissions, and release-artifact proof gaps. |
| [`linux.md`](linux.md) | Linux source/package/readiness, PPA/runtime proof, X11/Wayland caveats, and validation commands. |
| [`windows.md`](windows.md) | Windows cross-build evidence, runtime support boundary, Wine/manual validation options, and claim risks. |
| [`ios.md`](ios.md) | iOS app shell, widget/deep-link launcher, simulator/device build proof, real inference gap, and App Store/TestFlight boundaries. |
| [`product-guide.md`](product-guide.md) | README, usage-guide, media inventory, screenshots, feature-claim boundaries, and local-first wording. |
| [`synthesis.md`](synthesis.md) | Consolidated lane order and recommended first implementation batch. |

## Synthesized recommendation

Start with **Lane A + Lane B only**:

1. **Lane A:** platform readiness matrix, validation command pack, and static claim-boundary tests.
2. **Lane B:** README usage-guide scaffold, media/screenshot inventory, and Manim explainer plan.

This keeps the first implementation batch docs/tests-only, gives all platforms coverage, and avoids claiming unsupported Windows/iOS/macOS runtime states before stronger evidence exists.

## Stop conditions before implementation

Ask Channa before proceeding if implementation would require:

- C++, Swift, CMake, packaging, installer, or shell-script changes.
- Promoting Windows beyond `build_proven` / `unverified` runtime support.
- Promoting iOS beyond widget/deep-link launcher and placeholder transcription seams.
- Claiming macOS public release fixes without validating the downloaded public artifact.
- Editing release URLs or public landing claims.
- Pushing to GitHub.

