from __future__ import annotations

import unittest
from unittest.mock import patch

from autowhisper_fabric.protocol import AutoWhisperServiceError, ServiceSettings
from autowhisper_fabric.provider import AutoWhisperProvider


class _FakeClient:
    def __init__(self, result: dict) -> None:
        self.result = result
        self.calls = 0
        self.closed = False

    def transcribe_file(self, path: str, language: str | None = None) -> dict:
        self.calls += 1
        return self.result

    def close(self) -> None:
        self.closed = True


class ProviderTests(unittest.TestCase):
    def test_maps_completed_result_without_losing_v1_metadata(self) -> None:
        result = {
            "schema": "fabric.transcription",
            "version": 1,
            "request_id": "req-1",
            "status": "completed",
            "text": "Fabric voice note",
            "provider": "autowhisper",
            "segments": [{"start_ms": 0, "end_ms": 1000, "text": "Fabric voice note"}],
            "warnings": [],
        }
        provider = AutoWhisperProvider()
        fake = _FakeClient(result)
        with (
            patch("autowhisper_fabric.provider._load_plugin_config", return_value={}),
            patch.object(provider, "_client_for", return_value=fake),
        ):
            response = provider.transcribe("/tmp/note.wav")
        provider.close()
        self.assertTrue(response["success"])
        self.assertEqual(response["transcript"], "Fabric voice note")
        self.assertIs(response["transcription_result"], result)

    def test_maps_structured_runtime_failure(self) -> None:
        result = {
            "schema": "fabric.transcription",
            "version": 1,
            "request_id": "req-2",
            "status": "failed",
            "text": "",
            "provider": "autowhisper",
            "segments": [],
            "warnings": [],
            "error": {
                "code": "invalid_audio",
                "message": "Bad audio",
                "retryable": False,
            },
        }
        provider = AutoWhisperProvider()
        response = provider._fabric_result(result)
        provider.close()
        self.assertFalse(response["success"])
        self.assertEqual(response["error_code"], "invalid_audio")
        self.assertFalse(response["retryable"])

    def test_replaces_the_child_when_selected_model_changes(self) -> None:
        provider = AutoWhisperProvider()
        first = provider._client_for(ServiceSettings(model="tiny.en"))
        with patch.object(first, "close") as close:
            second = provider._client_for(ServiceSettings(model="small.en"))
        provider.close()
        self.assertIsNot(first, second)
        close.assert_called_once_with()

    def test_service_errors_remain_actionable(self) -> None:
        provider = AutoWhisperProvider()
        with (
            patch("autowhisper_fabric.provider._load_plugin_config", return_value={}),
            patch.object(
                provider,
                "_client_for",
                side_effect=AutoWhisperServiceError(
                    "model_missing", "Download the model", False
                ),
            ),
        ):
            response = provider.transcribe("/tmp/note.wav")
        provider.close()
        self.assertFalse(response["success"])
        self.assertEqual(response["error_code"], "model_missing")
        self.assertEqual(response["error"], "Download the model")


if __name__ == "__main__":
    unittest.main()
