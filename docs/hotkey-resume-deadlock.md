# Hotkey Resume Deadlock After Settings Dialog

## Status: Open — investigation in progress

## Summary

After opening and closing the tray settings dialog, the hotkey listener becomes unresponsive. The daemon process stays alive but hotkeys no longer trigger recording. On subsequent quit from the tray menu, the process aborts with a deadlock error.

## Reproduction

1. Start autowhisper normally — hotkeys work (tested: recording + transcription OK)
2. Open settings from the tray icon
3. Close settings
4. Try pressing the hotkey — nothing happens
5. Quit from tray menu — crash:

```
terminate called after throwing an instance of 'std::system_error'
  what():  Resource deadlock avoided
```

Systemd reports: `Main process exited, code=dumped, status=6/ABRT`

## Log Evidence

From PID 3455543 (journalctl):

```
01:00:04 Settings opened - pausing daemon
01:00:04 Pausing hotkey handling
01:00:09 Settings closed - resuming daemon
01:00:09 Scheduling hotkey listener resume
01:00:09 Resuming hotkey listener
         ← NO "Starting hotkey listener" log follows (should come from start())
         ← ~1 minute of silence, hotkeys dead
01:01:13 Quit requested from tray menu
01:01:13 terminate called after throwing an instance of 'std::system_error'
01:01:13   what():  Resource deadlock avoided
```

The absence of "Starting hotkey listener: trigger=..." after "Resuming hotkey listener" confirms that `reconfigure_hotkey()` never gets past `hotkey_->stop()`.

## Thread State (PID 3455868, second instance, same stuck state)

```
Thread 3455868 (main):    futex_wait_queue   ← blocked on join()
Thread 3455869:           do_poll.constprop.0
Thread 3455873:           futex_wait_queue
Thread 3455874-3455882:   do_poll / futex_wait_queue (GTK, audio, etc.)
```

Main thread stuck in `futex_wait_queue` = blocked on `pthread_join` inside `HotkeyManager::stop()`.

## Code Flow

### Pause/resume path

```
GTK thread: tray settings "close" callback
  → AutoWhisperDaemon::resume_hotkey()           [daemon.cpp:263]
    → hotkey_reconfigure_requested_ = true
    → queue_cv_.notify_all()

Main loop thread: process_events()               [daemon.cpp:120]
  → sees hotkey_reconfigure_requested_
  → calls reconfigure_hotkey()                    [daemon.cpp:269]
    → logs "Resuming hotkey listener"
    → hotkey_->stop()                             ← BLOCKS HERE
      → running_ = false
      → write to wake_pipe
      → listener_thread.join()                    ← DEADLOCK
    → (never reaches) new HotkeyManager + start()
```

### HotkeyManager::stop() [hotkey_x11.cpp:276]

```cpp
void HotkeyManager::stop() {
    running_.store(false);
    // write wake pipe
    if (impl_->wake_pipe[1] >= 0) {
        char c = 1;
        write(impl_->wake_pipe[1], &c, 1);
    }
    // THIS JOIN NEVER RETURNS:
    if (impl_->listener_thread.joinable()) {
        impl_->listener_thread.join();
    }
    // ... cleanup never reached
}
```

### Listener thread loop [hotkey_x11.cpp:220]

```cpp
while (running_.load()) {
    // select() on x11_fd + pipe_fd, 200ms timeout
    // if pipe wakeup → break
    // if x11 data → XRecordProcessReplies(data_display)
}
// cleanup: close displays, free XRecord context
```

## Crash on Quit

The `std::system_error: Resource deadlock avoided` (EDEADLK) is thrown when the quit handler (GTK thread) tries to join a thread that results in a deadlock condition, while the main thread is already stuck in `join()` from `reconfigure_hotkey()`.

## Likely Root Cause Candidates

1. **XRecord listener thread not exiting the select loop.** Even though `running_` is set to `false` and the wake pipe is written to, the thread may be stuck in `XRecordProcessReplies()` which blocks inside Xlib waiting for more data from the X server. The async XRecord API can hold the `data_display` connection lock.

2. **XCloseDisplay or XRecordFreeContext blocking.** If the thread does exit the loop, the cleanup code closes `data_display` first, then calls `XRecordFreeContext` on `ctrl_display`. Either of these X calls could block on internal Xlib locks or server round-trips.

3. **Xlib thread-safety issue.** `XInitThreads()` is called, but the XRecord async callback (`record_callback`) accesses `ctrl_display` to resolve keycodes (via `XkbKeycodeToKeysym`). If the main thread or cleanup code is simultaneously using `ctrl_display`, Xlib's internal locking could deadlock.

## Key Files

| File | Relevance |
|------|-----------|
| `src/hotkey/platform/hotkey_x11.cpp` | XRecord listener, start/stop/cleanup |
| `src/hotkey/hotkey.h` | HotkeyManager interface |
| `src/daemon/daemon.cpp:253-291` | pause_hotkey, resume_hotkey, reconfigure_hotkey |
| `src/daemon/daemon.cpp:110-124` | process_events main loop |
| `src/tray/platform/tray_gtk_settings.cpp` | Settings dialog callbacks (triggers pause/resume) |

## Uncommitted Fix Already Applied (separate issue)

The committed change `6d65523` fixed XRecord cleanup ordering (close `data_display` before freeing context) and made `stop()` write to wake pipe directly. This fix is correct but does not resolve the deadlock — it addresses a different shutdown path.
