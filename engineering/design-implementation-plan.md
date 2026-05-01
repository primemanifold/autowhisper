# Settings Design Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement later phases task-by-task.

**Goal:** Turn the uploaded AutoWhisper design concept into a production-quality settings experience without adding frontend dependencies.

**Architecture:** Keep the C++ schema and settings API as source of truth. The embedded UI in `src/settings/web/` renders intent-based panes from `/api/schema`, `/api/config`, and `/api/defaults`. React artboards remain design references under `design/source-concept/`.

**Tech Stack:** C++20 settings server, embedded static HTML/CSS/JS, Python unittest static checks for web assets, Catch2 for C++ behavior.

---

## Task 1: Preserve the design input

**Objective:** Keep the uploaded concept available to future agents and designers.

**Files:**
- Create: `design/source-concept/*`
- Create: `design/README.md`
- Create: `design/design-system.md`

**Verification:**
- `design/source-concept/tokens.css` exists.
- `design/design-system.md` documents principles, tokens, IA, and accessibility.

## Task 2: Add static regression tests for the settings UI

**Objective:** Prevent the production UI from regressing to a framework-based or tokenless implementation.

**Files:**
- Create: `tests/static/test_settings_design_assets.py`

**Command:**

```bash
python3 -m unittest tests.static.test_settings_design_assets -v
```

**Expected:** fails before the production slice, passes after.

## Task 3: Port tokens and shell UI

**Objective:** Implement the paper-light AutoWhisper foundation in plain CSS and HTML.

**Files:**
- Modify: `src/settings/web/index.html`
- Modify: `src/settings/web/style.css`

**Acceptance:**
- `--aw-` tokens exist.
- There is no React, Babel, Tailwind, or external frontend dependency.
- The shell contains sidebar navigation, topbar, dirty pill, and live status region.

## Task 4: Preserve schema-driven rendering with intent panes

**Objective:** Reorganize settings around user intent without changing the config backend.

**Files:**
- Modify: `src/settings/web/app.js`

**Acceptance:**
- `/api/schema`, `/api/config`, and `/api/defaults` are still used.
- Current schema sections are mapped into eight intent groups.
- Advanced pane exposes every schema-backed key.
- Save and reset still write schema-shaped JSON.

## Task 5: Add real diagnostics-backed panels

**Objective:** Replace placeholder proof panels with API-backed status.

**Files likely touched:**
- `src/settings/handlers.*`
- `src/settings/sidecar.*`
- `src/settings/web/app.js`
- `src/doctor/doctor.cpp`
- `tests/cpp/test_settings_http.cpp`

**Acceptance:**
- Settings UI can show model availability, microphone availability, X11 or platform access, and doctor results.
- Claims remain evidence-backed.

## Task 6: Build onboarding

**Objective:** Create a first-run path that gets a user to successful dictation quickly.

**Files likely touched:**
- `src/settings/web/app.js`
- `src/settings/web/style.css`
- `src/models/models.*`
- `src/doctor/doctor.cpp`

**Acceptance:**
- Missing model, missing microphone, and missing output permissions each produce clear next actions.

## Task 7: Visual QA and accessibility pass

**Objective:** Verify the embedded UI behaves under realistic states and sizes.

**Commands:**

```bash
python3 -m unittest tests.static.test_settings_design_assets -v
git diff --check
```

If Linux dependencies are available:

```bash
cmake -S . -B build -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
