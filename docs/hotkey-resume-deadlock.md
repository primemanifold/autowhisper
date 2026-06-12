# Hotkey Resume Deadlock After Settings Dialog

## Status: Resolved — 2026-06-12

Two things closed this defect:

1. **The triggering path no longer exists.** The GTK settings dialog (and its
   pause→resume→`reconfigure_hotkey()` flow in the daemon) was removed when
   settings moved to the browser UI launched as a separate process. The daemon
   no longer stops/restarts the hotkey listener mid-run.

2. **`HotkeyManager::stop()` was hardened so the deadlock class cannot recur**
   (`src/hotkey/platform/hotkey_x11.cpp`):
   - `stop()`/`signal_stop()` now call `XRecordDisableContext` on the control
     display before waiting. This is the documented way to unblock a listener
     sitting inside `XRecordProcessReplies()`: the server ends the record
     stream and the call returns. Display/context lifetime is guarded by a
     timed mutex so the nudge cannot race the thread's own cleanup.
   - The join is bounded (5 s). If the X server is truly unresponsive, the
     listener thread is detached and its `Impl` deliberately leaked so the
     zombie never touches freed memory — a bounded leak instead of a deadlocked
     or `std::terminate`d daemon.
   - The listener thread works through a raw `Impl` pointer captured at spawn,
     so an abandoned thread keeps using the leaked `Impl` even after the
     manager re-creates its own.

## Regression coverage

`tests/cpp/test_hotkey_x11.cpp` runs the listener against a private Xvfb with
XTest-synthesized keys:

- push-to-talk chord delivers `START`/`STOP` end to end;
- repeated start→traffic→stop cycles (the old reconfigure shape) assert
  `stop()` returns within the bound;
- `stop()` without `start()` is a no-op.

The tests skip gracefully when Xvfb is not installed; CI installs `xvfb` so
they always run there.

## Historical analysis (for archaeology)

The original failure: after closing the tray settings dialog,
`reconfigure_hotkey()` called `hotkey_->stop()`, which blocked forever in
`listener_thread.join()` while the listener sat in `XRecordProcessReplies()`
waiting for more data. A subsequent quit from the tray thread then crashed
with `std::system_error: Resource deadlock avoided`. Root cause candidates and
thread dumps are preserved in the git history of this file.
