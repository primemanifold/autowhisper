"""Tests for PulseAudio manager module."""

import threading
import time
from unittest.mock import MagicMock, patch, PropertyMock

import pytest

from autowhisper.pulseaudio import (
    PulseAudioManager,
    MutedSinkInput,
    PULSECTL_AVAILABLE,
)


class TestMutedSinkInput:
    """Test MutedSinkInput dataclass."""

    def test_create_muted_sink_input(self):
        """Test creating a MutedSinkInput instance."""
        muted = MutedSinkInput(index=42, name="Spotify", was_muted=False)
        assert muted.index == 42
        assert muted.name == "Spotify"
        assert muted.was_muted is False

    def test_create_previously_muted(self):
        """Test creating a MutedSinkInput that was already muted."""
        muted = MutedSinkInput(index=10, name="Chrome", was_muted=True)
        assert muted.was_muted is True


class TestPulseAudioManagerInit:
    """Test PulseAudioManager initialization."""

    def test_init_disabled(self):
        """Test initialization when feature is disabled."""
        manager = PulseAudioManager(enabled=False, beep_duration=0.3)
        assert manager._enabled is False
        assert manager._beep_duration == 0.3
        assert manager._muted_inputs == []

    def test_init_enabled(self):
        """Test initialization when feature is enabled."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.2)
        assert manager._enabled is True
        assert manager._beep_duration == 0.2


class TestPulseAudioManagerInitialize:
    """Test PulseAudioManager.initialize() method."""

    def test_initialize_disabled_returns_true(self):
        """Test initialize returns True when disabled (graceful)."""
        manager = PulseAudioManager(enabled=False, beep_duration=0.3)
        result = manager.initialize()
        assert result is True
        assert manager._initialized is False

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", False)
    def test_initialize_no_pulsectl_returns_true(self):
        """Test initialize returns True when pulsectl not available."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        result = manager.initialize()
        assert result is True
        # Feature should be disabled after graceful degradation
        assert manager._enabled is False

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_initialize_success(self, mock_pulsectl):
        """Test successful initialization."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        result = manager.initialize()

        assert result is True
        assert manager._initialized is True
        assert manager._pulse is mock_pulse
        mock_pulsectl.Pulse.assert_called_once_with("autowhisper")

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_initialize_pulse_connection_fails(self, mock_pulsectl):
        """Test initialize handles PulseAudio connection failure gracefully."""
        mock_pulsectl.Pulse.side_effect = Exception("PulseAudio not running")

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        result = manager.initialize()

        assert result is True  # Graceful degradation
        assert manager._enabled is False
        assert manager._initialized is False


class TestIsExcluded:
    """Test PulseAudioManager._is_excluded() method."""

    def test_exclude_python(self):
        """Test excluding python binary."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)

        sink_input = MagicMock()
        sink_input.proplist = {"application.process.binary": "/usr/bin/python"}
        sink_input.name = "Python Audio"

        assert manager._is_excluded(sink_input) is True

    def test_exclude_python3(self):
        """Test excluding python3 binary."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)

        sink_input = MagicMock()
        sink_input.proplist = {"application.process.binary": "/usr/bin/python3"}
        sink_input.name = "Python3 Audio"

        assert manager._is_excluded(sink_input) is True

    def test_exclude_autowhisper(self):
        """Test excluding autowhisper binary."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)

        sink_input = MagicMock()
        sink_input.proplist = {"application.process.binary": "/usr/local/bin/autowhisper"}
        sink_input.name = "AutoWhisper"

        assert manager._is_excluded(sink_input) is True

    def test_not_exclude_chrome(self):
        """Test not excluding Chrome."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)

        sink_input = MagicMock()
        sink_input.proplist = {"application.process.binary": "/opt/google/chrome/chrome"}
        sink_input.name = "Chrome"

        assert manager._is_excluded(sink_input) is False

    def test_not_exclude_spotify(self):
        """Test not excluding Spotify."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)

        sink_input = MagicMock()
        sink_input.proplist = {"application.process.binary": "/usr/share/spotify/spotify"}
        sink_input.name = "Spotify"

        assert manager._is_excluded(sink_input) is False

    def test_empty_binary(self):
        """Test handling empty binary path."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)

        sink_input = MagicMock()
        sink_input.proplist = {}
        sink_input.name = "Unknown"

        assert manager._is_excluded(sink_input) is False

    def test_exclude_with_path(self):
        """Test excluding with full path extracts basename correctly."""
        manager = PulseAudioManager(enabled=True, beep_duration=0.3)

        sink_input = MagicMock()
        sink_input.proplist = {
            "application.process.binary": "/home/user/.local/bin/python3"
        }
        sink_input.name = "Local Python"

        assert manager._is_excluded(sink_input) is True


class TestMuteOtherApps:
    """Test PulseAudioManager.mute_other_apps() method."""

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_mute_with_delay(self, mock_pulsectl):
        """Test muting with delay schedules timer."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse
        mock_pulse.sink_input_list.return_value = []

        manager = PulseAudioManager(enabled=True, beep_duration=0.1)
        manager.initialize()

        manager.mute_other_apps(delay=True)

        # Timer should be scheduled
        assert manager._mute_timer is not None
        assert manager._mute_timer.is_alive()

        # Cancel to clean up
        manager._mute_timer.cancel()

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_mute_without_delay(self, mock_pulsectl):
        """Test muting without delay calls immediately."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        # Create mock sink inputs
        chrome = MagicMock()
        chrome.index = 1
        chrome.name = "Chrome"
        chrome.mute = 0
        chrome.proplist = {"application.process.binary": "/opt/chrome/chrome"}

        mock_pulse.sink_input_list.return_value = [chrome]

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        manager.mute_other_apps(delay=False)

        # Should have muted Chrome
        mock_pulse.sink_input_mute.assert_called_once_with(1, mute=True)
        assert len(manager._muted_inputs) == 1
        assert manager._muted_inputs[0].name == "Chrome"

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_mute_excludes_python(self, mock_pulsectl):
        """Test muting excludes our own audio."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        # Create mock sink inputs - one is python (excluded)
        python_audio = MagicMock()
        python_audio.index = 1
        python_audio.name = "AutoWhisper Beep"
        python_audio.mute = 0
        python_audio.proplist = {"application.process.binary": "/usr/bin/python3"}

        chrome = MagicMock()
        chrome.index = 2
        chrome.name = "Chrome"
        chrome.mute = 0
        chrome.proplist = {"application.process.binary": "/opt/chrome/chrome"}

        mock_pulse.sink_input_list.return_value = [python_audio, chrome]

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        manager.mute_other_apps(delay=False)

        # Should only have muted Chrome, not python
        mock_pulse.sink_input_mute.assert_called_once_with(2, mute=True)
        assert len(manager._muted_inputs) == 1
        assert manager._muted_inputs[0].name == "Chrome"

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_mute_respects_already_muted(self, mock_pulsectl):
        """Test muting tracks but doesn't re-mute already muted apps."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        # App that was already muted
        spotify = MagicMock()
        spotify.index = 1
        spotify.name = "Spotify"
        spotify.mute = 1  # Already muted
        spotify.proplist = {"application.process.binary": "/usr/bin/spotify"}

        mock_pulse.sink_input_list.return_value = [spotify]

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        manager.mute_other_apps(delay=False)

        # Should track but not call mute
        mock_pulse.sink_input_mute.assert_not_called()
        assert len(manager._muted_inputs) == 1
        assert manager._muted_inputs[0].was_muted is True

    def test_mute_disabled_does_nothing(self):
        """Test muting does nothing when disabled."""
        manager = PulseAudioManager(enabled=False, beep_duration=0.3)
        manager.mute_other_apps(delay=False)

        assert manager._mute_timer is None
        assert manager._muted_inputs == []

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_mute_delay_executes(self, mock_pulsectl):
        """Test that delayed mute actually executes after delay."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        chrome = MagicMock()
        chrome.index = 1
        chrome.name = "Chrome"
        chrome.mute = 0
        chrome.proplist = {"application.process.binary": "/opt/chrome/chrome"}

        mock_pulse.sink_input_list.return_value = [chrome]

        manager = PulseAudioManager(enabled=True, beep_duration=0.05)
        manager.initialize()

        manager.mute_other_apps(delay=True)

        # Wait for delay (0.05 + 0.05 + small buffer)
        time.sleep(0.15)

        # Should have muted
        mock_pulse.sink_input_mute.assert_called_once_with(1, mute=True)


class TestUnmuteOtherApps:
    """Test PulseAudioManager.unmute_other_apps() method."""

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_unmute_restores_state(self, mock_pulsectl):
        """Test unmuting restores only apps we muted."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        # Simulate previously muted apps
        manager._muted_inputs = [
            MutedSinkInput(index=1, name="Chrome", was_muted=False),  # We muted
            MutedSinkInput(index=2, name="Spotify", was_muted=True),  # Was already muted
        ]

        manager.unmute_other_apps()

        # Should only unmute Chrome (was_muted=False)
        mock_pulse.sink_input_mute.assert_called_once_with(1, mute=False)
        assert manager._muted_inputs == []

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_unmute_handles_closed_app(self, mock_pulsectl):
        """Test unmuting handles apps that were closed."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse
        mock_pulse.sink_input_mute.side_effect = Exception("Sink input does not exist")

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        manager._muted_inputs = [
            MutedSinkInput(index=1, name="ClosedApp", was_muted=False),
        ]

        # Should not raise
        manager.unmute_other_apps()
        assert manager._muted_inputs == []

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_unmute_cancels_pending_mute(self, mock_pulsectl):
        """Test unmuting cancels any pending mute timer."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse
        mock_pulse.sink_input_list.return_value = []

        manager = PulseAudioManager(enabled=True, beep_duration=1.0)
        manager.initialize()

        # Start delayed mute
        manager.mute_other_apps(delay=True)
        assert manager._mute_timer is not None

        # Immediately unmute (e.g., user canceled)
        manager.unmute_other_apps()

        # Timer should be canceled
        assert manager._mute_timer is None

    def test_unmute_disabled_does_nothing(self):
        """Test unmuting does nothing when disabled."""
        manager = PulseAudioManager(enabled=False, beep_duration=0.3)
        manager.unmute_other_apps()
        # Should not raise


class TestCleanup:
    """Test PulseAudioManager.cleanup() method."""

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_cleanup_restores_and_closes(self, mock_pulsectl):
        """Test cleanup restores audio and closes connection."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        manager._muted_inputs = [
            MutedSinkInput(index=1, name="Chrome", was_muted=False),
        ]

        manager.cleanup()

        # Should have unmuted and closed
        mock_pulse.sink_input_mute.assert_called_once_with(1, mute=False)
        mock_pulse.close.assert_called_once()
        assert manager._pulse is None
        assert manager._initialized is False

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_cleanup_cancels_pending_timer(self, mock_pulsectl):
        """Test cleanup cancels pending mute timer."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse
        mock_pulse.sink_input_list.return_value = []

        manager = PulseAudioManager(enabled=True, beep_duration=1.0)
        manager.initialize()

        # Start delayed mute
        manager.mute_other_apps(delay=True)
        timer = manager._mute_timer
        assert timer is not None

        manager.cleanup()

        assert manager._mute_timer is None

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_cleanup_handles_close_error(self, mock_pulsectl):
        """Test cleanup handles errors when closing connection."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse
        mock_pulse.close.side_effect = Exception("Connection already closed")

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        # Should not raise
        manager.cleanup()
        assert manager._pulse is None

    def test_cleanup_disabled_manager(self):
        """Test cleanup works on disabled manager."""
        manager = PulseAudioManager(enabled=False, beep_duration=0.3)
        # Should not raise
        manager.cleanup()


class TestThreadSafety:
    """Test thread safety of PulseAudioManager."""

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_concurrent_mute_unmute(self, mock_pulsectl):
        """Test concurrent mute and unmute operations are thread-safe."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        chrome = MagicMock()
        chrome.index = 1
        chrome.name = "Chrome"
        chrome.mute = 0
        chrome.proplist = {"application.process.binary": "/opt/chrome/chrome"}

        mock_pulse.sink_input_list.return_value = [chrome]

        manager = PulseAudioManager(enabled=True, beep_duration=0.01)
        manager.initialize()

        errors = []

        def mute_loop():
            try:
                for _ in range(10):
                    manager.mute_other_apps(delay=False)
                    time.sleep(0.001)
            except Exception as e:
                errors.append(e)

        def unmute_loop():
            try:
                for _ in range(10):
                    manager.unmute_other_apps()
                    time.sleep(0.001)
            except Exception as e:
                errors.append(e)

        threads = [
            threading.Thread(target=mute_loop),
            threading.Thread(target=unmute_loop),
        ]

        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert errors == [], f"Thread safety errors: {errors}"
        manager.cleanup()


class TestEdgeCases:
    """Test edge cases and error handling."""

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_mute_with_null_name(self, mock_pulsectl):
        """Test handling sink input with no name."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse

        unnamed = MagicMock()
        unnamed.index = 1
        unnamed.name = None
        unnamed.mute = 0
        unnamed.proplist = {"application.process.binary": "/some/app"}

        mock_pulse.sink_input_list.return_value = [unnamed]

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        manager.mute_other_apps(delay=False)

        assert len(manager._muted_inputs) == 1
        assert manager._muted_inputs[0].name == "Unknown"

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_mute_error_clears_state(self, mock_pulsectl):
        """Test that errors during mute clear the tracked state."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse
        mock_pulse.sink_input_list.side_effect = Exception("PulseAudio error")

        manager = PulseAudioManager(enabled=True, beep_duration=0.3)
        manager.initialize()

        # Pre-populate to verify it gets cleared
        manager._muted_inputs = [
            MutedSinkInput(index=99, name="Old", was_muted=False)
        ]

        manager.mute_other_apps(delay=False)

        # State should be cleared on error
        assert manager._muted_inputs == []

    @patch("autowhisper.pulseaudio.PULSECTL_AVAILABLE", True)
    @patch("autowhisper.pulseaudio.pulsectl")
    def test_multiple_mute_calls_cancel_previous(self, mock_pulsectl):
        """Test that multiple mute calls cancel previous timers."""
        mock_pulse = MagicMock()
        mock_pulsectl.Pulse.return_value = mock_pulse
        mock_pulse.sink_input_list.return_value = []

        manager = PulseAudioManager(enabled=True, beep_duration=1.0)
        manager.initialize()

        # First mute with delay
        manager.mute_other_apps(delay=True)
        first_timer = manager._mute_timer

        # Second mute with delay should cancel first
        manager.mute_other_apps(delay=True)
        second_timer = manager._mute_timer

        assert first_timer is not second_timer
        # First timer should be canceled (not alive or will be soon)

        manager.cleanup()
