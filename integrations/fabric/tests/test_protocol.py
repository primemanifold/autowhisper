from __future__ import annotations

import os
import stat
import tempfile
import textwrap
import unittest
from pathlib import Path

from autowhisper_fabric.protocol import (
    AutoWhisperProtocolError,
    AutoWhisperServiceError,
    LocalServiceClient,
    ServiceSettings,
    validate_response,
    validate_transcription_result,
)


class ProtocolValidationTests(unittest.TestCase):
    def test_validates_correlated_result(self) -> None:
        payload = {
            "protocol": "autowhisper.local",
            "version": 1,
            "id": "req-1",
            "result": {"status": "ready"},
        }
        self.assertEqual(validate_response(payload, "req-1"), {"status": "ready"})

    def test_rejects_incompatible_or_uncorrelated_responses(self) -> None:
        with self.assertRaises(AutoWhisperProtocolError):
            validate_response(
                {
                    "protocol": "autowhisper.local",
                    "version": 2,
                    "id": "req-1",
                    "result": {},
                },
                "req-1",
            )
        with self.assertRaises(AutoWhisperProtocolError):
            validate_response(
                {
                    "protocol": "autowhisper.local",
                    "version": 1,
                    "id": "wrong",
                    "result": {},
                },
                "req-1",
            )

    def test_preserves_structured_service_errors(self) -> None:
        payload = {
            "protocol": "autowhisper.local",
            "version": 1,
            "id": "req-1",
            "error": {"code": "busy", "message": "Try later", "retryable": True},
        }
        with self.assertRaises(AutoWhisperServiceError) as raised:
            validate_response(payload, "req-1")
        self.assertEqual(raised.exception.code, "busy")
        self.assertTrue(raised.exception.retryable)

    def test_transcription_v1_fails_closed(self) -> None:
        result = {
            "schema": "fabric.transcription",
            "version": 1,
            "request_id": "req-1",
            "status": "completed",
            "text": "hello",
        }
        self.assertEqual(validate_transcription_result(result, "req-1"), result)
        future = dict(result, version=2)
        with self.assertRaises(AutoWhisperProtocolError):
            validate_transcription_result(future, "req-1")


class SettingsTests(unittest.TestCase):
    def test_builds_an_argument_vector_without_a_shell(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "autowhisper"
            executable.write_bytes(b"binary")
            executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
            settings = ServiceSettings.from_mapping(
                {
                    "executable": str(executable),
                    "config_path": "~/voice.toml",
                    "device": "cpu",
                    "model": "tiny.en",
                }
            )
            command = settings.command()
            self.assertEqual(command[:3], [str(executable), "serve", "--stdio"])
            self.assertIn("tiny.en", command)
            self.assertNotIn("sh", command)

    def test_rejects_invalid_behavioral_config(self) -> None:
        with self.assertRaises(ValueError):
            ServiceSettings.from_mapping({"device": "metal"})
        with self.assertRaises(ValueError):
            ServiceSettings.from_mapping({"request_timeout_seconds": 0})


@unittest.skipIf(os.name == "nt", "the test service uses a POSIX shebang")
class LocalServiceLifecycleTests(unittest.TestCase):
    def test_reuses_one_child_for_two_transcriptions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            launches = root / "launches.txt"
            executable = root / "autowhisper"
            executable.write_text(
                "#!/usr/bin/env python3\n"
                + textwrap.dedent(
                    f"""
                    import json
                    from pathlib import Path

                    launches = Path({str(launches)!r})
                    launches.write_text(launches.read_text() + "1\\n" if launches.exists() else "1\\n")
                    for line in __import__("sys").stdin:
                        request = json.loads(line)
                        method = request["method"]
                        if method == "health":
                            result = {{"status": "ready"}}
                        elif method == "transcribe_file":
                            result = {{
                                "schema": "fabric.transcription",
                                "version": 1,
                                "request_id": request["id"],
                                "status": "completed",
                                "text": "warm model",
                                "provider": "autowhisper",
                                "segments": [],
                                "warnings": [],
                            }}
                        elif method == "shutdown":
                            result = {{"status": "stopping"}}
                        else:
                            result = {{}}
                        print(json.dumps({{
                            "protocol": "autowhisper.local",
                            "version": 1,
                            "id": request["id"],
                            "result": result,
                        }}), flush=True)
                        if method == "shutdown":
                            break
                    """
                )
            )
            executable.chmod(0o755)
            audio = root / "audio.wav"
            audio.write_bytes(b"not read by fixture")
            client = LocalServiceClient(
                ServiceSettings(executable=str(executable), model="tiny.en")
            )
            try:
                first = client.transcribe_file(str(audio), language="en")
                second = client.transcribe_file(str(audio), language="en")
            finally:
                client.close()
            self.assertEqual(first["text"], "warm model")
            self.assertEqual(second["text"], "warm model")
            self.assertEqual(launches.read_text().splitlines(), ["1"])


if __name__ == "__main__":
    unittest.main()
