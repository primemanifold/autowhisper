"""Fabric TranscriptionProvider backed by a warm AutoWhisper child process."""

from __future__ import annotations

import atexit
import contextlib
import os
import subprocess
import tempfile
import threading
from dataclasses import replace
from pathlib import Path
from typing import Any, Mapping

from agent.transcription_provider import TranscriptionProvider

from .protocol import (
    AutoWhisperProtocolError,
    AutoWhisperServiceError,
    AutoWhisperTransportError,
    LocalServiceClient,
    ServiceSettings,
)

_MODELS = [
    {
        "id": "distil-small.en",
        "display": "Distil Small (recommended)",
        "languages": ["en"],
    },
    {"id": "tiny.en", "display": "Tiny English", "languages": ["en"]},
    {"id": "tiny", "display": "Tiny Multilingual"},
    {"id": "base.en", "display": "Base English", "languages": ["en"]},
    {"id": "base", "display": "Base Multilingual"},
    {"id": "small.en", "display": "Small English", "languages": ["en"]},
    {"id": "small", "display": "Small Multilingual"},
    {"id": "medium.en", "display": "Medium English", "languages": ["en"]},
    {"id": "medium", "display": "Medium Multilingual"},
    {"id": "distil-medium.en", "display": "Distil Medium", "languages": ["en"]},
    {"id": "distil-large-v3", "display": "Distil Large v3", "languages": ["en"]},
    {"id": "large-v3-turbo", "display": "Large v3 Turbo"},
    {"id": "large-v3", "display": "Large v3"},
]

_NATIVE_AUDIO_EXTENSIONS = frozenset({".wav", ".mp3", ".flac"})


def _load_plugin_config() -> Mapping[str, Any]:
    """Read the provider's non-secret settings from Fabric config.yaml."""
    from fabric_cli.config import load_config

    config = load_config()
    stt = config.get("stt", {}) if isinstance(config, dict) else {}
    if not isinstance(stt, dict):
        return {}
    provider = stt.get("autowhisper", {})
    return provider if isinstance(provider, dict) else {}


class AutoWhisperProvider(TranscriptionProvider):
    """Local Fabric STT provider using AutoWhisper protocol v1."""

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._settings: ServiceSettings | None = None
        self._client: LocalServiceClient | None = None
        atexit.register(self.close)

    @property
    def name(self) -> str:
        return "autowhisper"

    @property
    def display_name(self) -> str:
        return "AutoWhisper (local)"

    def is_available(self) -> bool:
        try:
            return (
                ServiceSettings.from_mapping(_load_plugin_config()).resolve_executable()
                is not None
            )
        except Exception:
            return False

    def list_models(self) -> list[dict[str, Any]]:
        return [dict(model) for model in _MODELS]

    def default_model(self) -> str:
        return "distil-small.en"

    def get_setup_schema(self) -> dict[str, Any]:
        return {
            "name": self.display_name,
            "badge": "local",
            "tag": "Private on-device Whisper through AutoWhisper",
            "env_vars": [],
        }

    def transcribe(
        self,
        file_path: str,
        *,
        model: str | None = None,
        language: str | None = None,
        **extra: Any,
    ) -> dict[str, Any]:
        del extra
        try:
            settings = ServiceSettings.from_mapping(_load_plugin_config())
            if model:
                settings = replace(settings, model=model)
            # Hold the provider lock through the call so a concurrent model
            # change or shutdown cannot retire this client between selection
            # and use. The child itself also serializes protocol requests.
            with self._lock:
                client = self._client_for(settings)
                with self._prepared_audio(file_path, settings) as prepared_path:
                    result = client.transcribe_file(prepared_path, language=language)
            return self._fabric_result(result)
        except (
            ValueError,
            AutoWhisperTransportError,
            AutoWhisperProtocolError,
        ) as error:
            return self._failure(str(error), retryable=True)
        except AutoWhisperServiceError as error:
            return self._failure(str(error), code=error.code, retryable=error.retryable)
        except Exception as error:
            return self._failure(
                f"Unexpected AutoWhisper failure: {error}", retryable=False
            )

    def close(self) -> None:
        with self._lock:
            client = self._client
            self._client = None
            self._settings = None
        if client is not None:
            client.close()

    def _client_for(self, settings: ServiceSettings) -> LocalServiceClient:
        stale: LocalServiceClient | None = None
        with self._lock:
            if self._client is None or self._settings != settings:
                stale = self._client
                self._client = LocalServiceClient(settings)
                self._settings = settings
            client = self._client
        if stale is not None:
            stale.close()
        assert client is not None
        return client

    @contextlib.contextmanager
    def _prepared_audio(self, file_path: str, settings: ServiceSettings):
        """Yield a native decoder input, normalizing browser/mobile formats."""
        input_path = Path(file_path)
        if input_path.suffix.lower() in _NATIVE_AUDIO_EXTENSIONS:
            yield str(input_path)
            return

        ffmpeg = settings.resolve_ffmpeg_executable()
        if ffmpeg is None:
            raise AutoWhisperTransportError(
                "This recording needs ffmpeg conversion, but the configured "
                f"executable was not found: {settings.ffmpeg_executable}"
            )

        output_path = ""
        try:
            with tempfile.NamedTemporaryFile(
                prefix="autowhisper-fabric-", suffix=".wav", delete=False
            ) as output:
                output_path = output.name
            command = [
                ffmpeg,
                "-nostdin",
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-i",
                str(input_path),
                "-t",
                "3600",
                "-ac",
                "1",
                "-ar",
                "16000",
                "-f",
                "wav",
                output_path,
            ]
            try:
                completed = subprocess.run(
                    command,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    timeout=settings.conversion_timeout_seconds,
                    check=False,
                )
            except subprocess.TimeoutExpired as error:
                raise AutoWhisperTransportError(
                    "Audio conversion timed out after "
                    f"{settings.conversion_timeout_seconds:g} seconds"
                ) from error
            if completed.returncode != 0:
                detail = completed.stderr.decode("utf-8", errors="replace").strip()
                suffix = f": {detail[:1000]}" if detail else ""
                raise AutoWhisperTransportError(
                    f"ffmpeg could not decode the recording{suffix}"
                )
            if not Path(output_path).is_file() or Path(output_path).stat().st_size == 0:
                raise AutoWhisperTransportError(
                    "ffmpeg completed without producing a recording"
                )
            yield output_path
        finally:
            if output_path:
                try:
                    os.unlink(output_path)
                except OSError:
                    pass

    def _fabric_result(self, result: dict[str, Any]) -> dict[str, Any]:
        status = result["status"]
        if status in {"completed", "no_speech"}:
            return {
                "success": True,
                "transcript": result["text"],
                "provider": self.name,
                "transcription_result": result,
            }
        error = result.get("error")
        if isinstance(error, dict):
            message = error.get("message") or f"AutoWhisper transcription {status}"
            code = error.get("code") or status
            retryable = error.get("retryable") is True
        else:
            message = f"AutoWhisper transcription {status}"
            code = status
            retryable = False
        return self._failure(
            str(message),
            code=str(code),
            retryable=retryable,
            transcription_result=result,
        )

    def _failure(
        self,
        message: str,
        *,
        code: str = "autowhisper_unavailable",
        retryable: bool,
        transcription_result: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        response: dict[str, Any] = {
            "success": False,
            "transcript": "",
            "error": message,
            "provider": self.name,
            "error_code": code,
            "retryable": retryable,
        }
        if transcription_result is not None:
            response["transcription_result"] = transcription_result
        return response
