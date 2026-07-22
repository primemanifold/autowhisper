"""Bounded synchronous client for AutoWhisper's local stdio protocol."""

from __future__ import annotations

import json
import os
import queue
import shutil
import signal
import subprocess
import threading
import uuid
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Any, BinaryIO, Mapping

PROTOCOL_NAME = "autowhisper.local"
PROTOCOL_VERSION = 1
TRANSCRIPTION_SCHEMA = "fabric.transcription"
TRANSCRIPTION_VERSION = 1
MAX_RESPONSE_BYTES = 1024 * 1024
MAX_STDERR_LINES = 40


class AutoWhisperProtocolError(RuntimeError):
    """Raised when a child response violates the negotiated protocol."""


class AutoWhisperTransportError(RuntimeError):
    """Raised when the child cannot be started or reached."""


class AutoWhisperServiceError(RuntimeError):
    """Structured error returned by the AutoWhisper service."""

    def __init__(self, code: str, message: str, retryable: bool = False) -> None:
        super().__init__(message)
        self.code = code
        self.retryable = retryable


@dataclass(frozen=True)
class ServiceSettings:
    """Process and timeout settings loaded from Fabric config.yaml."""

    executable: str = "autowhisper"
    ffmpeg_executable: str = "ffmpeg"
    config_path: str = ""
    device: str = "auto"
    model: str = "distil-small.en"
    startup_timeout_seconds: float = 180.0
    request_timeout_seconds: float = 600.0
    conversion_timeout_seconds: float = 120.0
    shutdown_timeout_seconds: float = 2.0

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any] | None) -> "ServiceSettings":
        config = value if isinstance(value, Mapping) else {}
        settings = cls(
            executable=_string(config, "executable", cls.executable),
            ffmpeg_executable=_string(
                config, "ffmpeg_executable", cls.ffmpeg_executable
            ),
            config_path=_string(config, "config_path", cls.config_path),
            device=_string(config, "device", cls.device),
            model=_string(config, "model", cls.model),
            startup_timeout_seconds=_bounded_seconds(
                config,
                "startup_timeout_seconds",
                cls.startup_timeout_seconds,
                1.0,
                900.0,
            ),
            request_timeout_seconds=_bounded_seconds(
                config,
                "request_timeout_seconds",
                cls.request_timeout_seconds,
                1.0,
                3600.0,
            ),
            conversion_timeout_seconds=_bounded_seconds(
                config,
                "conversion_timeout_seconds",
                cls.conversion_timeout_seconds,
                1.0,
                900.0,
            ),
            shutdown_timeout_seconds=_bounded_seconds(
                config,
                "shutdown_timeout_seconds",
                cls.shutdown_timeout_seconds,
                0.1,
                30.0,
            ),
        )
        if settings.device not in {"auto", "cpu", "cuda"}:
            raise ValueError("stt.autowhisper.device must be auto, cpu, or cuda")
        if not settings.executable.strip():
            raise ValueError("stt.autowhisper.executable must not be empty")
        if not settings.ffmpeg_executable.strip():
            raise ValueError("stt.autowhisper.ffmpeg_executable must not be empty")
        return settings

    def resolve_executable(self) -> str | None:
        return _resolve_executable(self.executable)

    def resolve_ffmpeg_executable(self) -> str | None:
        return _resolve_executable(self.ffmpeg_executable)

    def command(self) -> list[str]:
        executable = self.resolve_executable()
        if executable is None:
            raise AutoWhisperTransportError(
                f"AutoWhisper executable not found: {self.executable}"
            )
        command = [executable, "serve", "--stdio"]
        if self.config_path:
            command.extend(["--config", os.path.expanduser(self.config_path)])
        if self.device:
            command.extend(["--device", self.device])
        if self.model:
            command.extend(["--model", self.model])
        return command


def _resolve_executable(value: str) -> str | None:
    candidate = os.path.expanduser(value)
    if (
        Path(candidate).is_absolute()
        or os.sep in candidate
        or (os.altsep and os.altsep in candidate)
    ):
        path = Path(candidate)
        return str(path) if path.is_file() else None
    return shutil.which(candidate)


@dataclass(frozen=True)
class _StreamFailure:
    message: str


def _string(config: Mapping[str, Any], key: str, default: str) -> str:
    value = config.get(key, default)
    if not isinstance(value, str):
        raise ValueError(f"stt.autowhisper.{key} must be a string")
    return value.strip()


def _bounded_seconds(
    config: Mapping[str, Any], key: str, default: float, minimum: float, maximum: float
) -> float:
    value = config.get(key, default)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"stt.autowhisper.{key} must be a number")
    number = float(value)
    if number < minimum or number > maximum:
        raise ValueError(
            f"stt.autowhisper.{key} must be between {minimum:g} and {maximum:g}"
        )
    return number


def validate_response(payload: Any, request_id: str) -> dict[str, Any]:
    """Validate one response envelope and return its result."""
    if not isinstance(payload, dict):
        raise AutoWhisperProtocolError("AutoWhisper response must be a JSON object")
    if payload.get("protocol") != PROTOCOL_NAME:
        raise AutoWhisperProtocolError(
            "AutoWhisper returned an unsupported protocol name"
        )
    if payload.get("version") != PROTOCOL_VERSION:
        raise AutoWhisperProtocolError(
            "AutoWhisper returned an unsupported protocol version"
        )
    if payload.get("id") != request_id:
        raise AutoWhisperProtocolError(
            "AutoWhisper response ID did not match the request"
        )
    has_result = "result" in payload
    has_error = "error" in payload
    if has_result == has_error:
        raise AutoWhisperProtocolError(
            "AutoWhisper response must contain exactly one of result or error"
        )
    if has_error:
        error = payload["error"]
        if not isinstance(error, dict):
            raise AutoWhisperProtocolError("AutoWhisper error must be a JSON object")
        code = error.get("code")
        message = error.get("message")
        retryable = error.get("retryable", False)
        if (
            not isinstance(code, str)
            or not isinstance(message, str)
            or not isinstance(retryable, bool)
        ):
            raise AutoWhisperProtocolError("AutoWhisper returned a malformed error")
        raise AutoWhisperServiceError(code, message, retryable)
    result = payload["result"]
    if not isinstance(result, dict):
        raise AutoWhisperProtocolError("AutoWhisper result must be a JSON object")
    return result


def validate_transcription_result(result: Any, request_id: str) -> dict[str, Any]:
    """Fail closed on incompatible or malformed transcription results."""
    if not isinstance(result, dict):
        raise AutoWhisperProtocolError(
            "AutoWhisper transcription result must be an object"
        )
    if result.get("schema") != TRANSCRIPTION_SCHEMA:
        raise AutoWhisperProtocolError(
            "AutoWhisper returned an unsupported transcription schema"
        )
    if result.get("version") != TRANSCRIPTION_VERSION:
        raise AutoWhisperProtocolError(
            "AutoWhisper returned an unsupported transcription version"
        )
    if result.get("request_id") != request_id:
        raise AutoWhisperProtocolError(
            "Transcription result ID did not match the request"
        )
    if result.get("status") not in {"completed", "no_speech", "cancelled", "failed"}:
        raise AutoWhisperProtocolError(
            "AutoWhisper returned an invalid transcription status"
        )
    if not isinstance(result.get("text"), str):
        raise AutoWhisperProtocolError(
            "AutoWhisper transcription text must be a string"
        )
    return result


class LocalServiceClient:
    """Own one serial AutoWhisper child and keep its selected model warm."""

    def __init__(self, settings: ServiceSettings) -> None:
        self.settings = settings
        self._lock = threading.Lock()
        self._process: subprocess.Popen[bytes] | None = None
        self._responses: queue.Queue[bytes | _StreamFailure] = queue.Queue()
        self._stderr: deque[str] = deque(maxlen=MAX_STDERR_LINES)

    def transcribe_file(self, path: str, language: str | None = None) -> dict[str, Any]:
        params: dict[str, Any] = {"path": str(Path(path).resolve())}
        if language:
            params["language"] = language
        if self.settings.model:
            params["model"] = self.settings.model

        last_error: AutoWhisperTransportError | AutoWhisperProtocolError | None = None
        for attempt in range(2):
            try:
                with self._lock:
                    self._ensure_started_locked()
                    request_id, result = self._request_locked(
                        "transcribe_file", params, self.settings.request_timeout_seconds
                    )
                    return validate_transcription_result(result, request_id)
            except (AutoWhisperTransportError, AutoWhisperProtocolError) as error:
                last_error = error
                with self._lock:
                    self._stop_locked(polite=False)
                if attempt == 1:
                    raise
        assert last_error is not None
        raise last_error

    def capabilities(self) -> dict[str, Any]:
        with self._lock:
            self._ensure_started_locked()
            _, result = self._request_locked(
                "capabilities", {}, self.settings.request_timeout_seconds
            )
            return result

    def close(self) -> None:
        with self._lock:
            self._stop_locked(polite=True)

    def _ensure_started_locked(self) -> None:
        if self._process is not None and self._process.poll() is None:
            return
        self._stop_locked(polite=False)
        command = self.settings.command()
        popen_options: dict[str, Any] = {
            "stdin": subprocess.PIPE,
            "stdout": subprocess.PIPE,
            "stderr": subprocess.PIPE,
            "bufsize": 0,
        }
        if os.name == "nt":
            popen_options["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
        else:
            popen_options["start_new_session"] = True
        try:
            process = subprocess.Popen(command, **popen_options)
        except OSError as error:
            raise AutoWhisperTransportError(
                f"Could not start AutoWhisper: {error}"
            ) from error
        self._process = process
        self._responses = queue.Queue()
        self._stderr = deque(maxlen=MAX_STDERR_LINES)
        assert process.stdout is not None
        assert process.stderr is not None
        threading.Thread(
            target=self._read_stdout,
            args=(process.stdout, self._responses),
            name="autowhisper-stdout",
            daemon=True,
        ).start()
        threading.Thread(
            target=self._read_stderr,
            args=(process.stderr, self._stderr),
            name="autowhisper-stderr",
            daemon=True,
        ).start()
        try:
            _, health = self._request_locked(
                "health", {}, self.settings.startup_timeout_seconds
            )
            if health.get("status") != "ready":
                raise AutoWhisperProtocolError("AutoWhisper did not report ready")
        except Exception:
            self._stop_locked(polite=False)
            raise

    def _request_locked(
        self, method: str, params: Mapping[str, Any], timeout: float
    ) -> tuple[str, dict[str, Any]]:
        process = self._process
        if process is None or process.poll() is not None or process.stdin is None:
            raise AutoWhisperTransportError(self._stopped_message())
        request_id = str(uuid.uuid4())
        request = {
            "protocol": PROTOCOL_NAME,
            "version": PROTOCOL_VERSION,
            "id": request_id,
            "method": method,
            "params": dict(params),
        }
        encoded = json.dumps(request, separators=(",", ":")).encode("utf-8") + b"\n"
        try:
            process.stdin.write(encoded)
            process.stdin.flush()
        except (BrokenPipeError, OSError) as error:
            raise AutoWhisperTransportError(self._stopped_message()) from error

        try:
            item = self._responses.get(timeout=timeout)
        except queue.Empty as error:
            raise AutoWhisperTransportError(
                f"AutoWhisper timed out after {timeout:g} seconds"
            ) from error
        if isinstance(item, _StreamFailure):
            raise AutoWhisperTransportError(f"{item.message}{self._stderr_suffix()}")
        try:
            payload = json.loads(item.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise AutoWhisperProtocolError(
                "AutoWhisper returned invalid JSON"
            ) from error
        return request_id, validate_response(payload, request_id)

    @staticmethod
    def _read_stdout(
        stream: BinaryIO, responses: queue.Queue[bytes | _StreamFailure]
    ) -> None:
        while True:
            try:
                line = stream.readline(MAX_RESPONSE_BYTES + 1)
            except (OSError, ValueError):
                responses.put(_StreamFailure("AutoWhisper closed its response stream"))
                return
            if not line:
                responses.put(_StreamFailure("AutoWhisper closed its response stream"))
                return
            if len(line) > MAX_RESPONSE_BYTES or not line.endswith(b"\n"):
                try:
                    while line and not line.endswith(b"\n"):
                        line = stream.readline(MAX_RESPONSE_BYTES + 1)
                except (OSError, ValueError):
                    pass
                responses.put(
                    _StreamFailure("AutoWhisper response exceeded the 1 MiB limit")
                )
                return
            responses.put(line[:-1])

    @staticmethod
    def _read_stderr(stream: BinaryIO, stderr_lines: deque[str]) -> None:
        while True:
            try:
                line = stream.readline(8193)
            except (OSError, ValueError):
                return
            if not line:
                return
            decoded = line.decode("utf-8", errors="replace").strip()
            if decoded:
                stderr_lines.append(decoded[:8192])

    def _stop_locked(self, polite: bool) -> None:
        process = self._process
        if process is None:
            return
        if polite and process.poll() is None:
            try:
                self._request_locked(
                    "shutdown", {}, self.settings.shutdown_timeout_seconds
                )
            except Exception:
                pass
        self._process = None
        try:
            if process.poll() is not None:
                return
            try:
                process.wait(timeout=self.settings.shutdown_timeout_seconds)
                return
            except subprocess.TimeoutExpired:
                pass
            self._signal_process(process, force=False)
            try:
                process.wait(timeout=self.settings.shutdown_timeout_seconds)
                return
            except subprocess.TimeoutExpired:
                self._signal_process(process, force=True)
                try:
                    process.wait(timeout=self.settings.shutdown_timeout_seconds)
                except subprocess.TimeoutExpired:
                    pass
        finally:
            for stream in (process.stdin, process.stdout, process.stderr):
                if stream is not None:
                    try:
                        stream.close()
                    except OSError:
                        pass

    @staticmethod
    def _signal_process(process: subprocess.Popen[bytes], force: bool) -> None:
        try:
            if os.name != "nt":
                os.killpg(process.pid, signal.SIGKILL if force else signal.SIGTERM)
            elif force:
                process.kill()
            else:
                process.terminate()
        except (OSError, ProcessLookupError):
            pass

    def _stopped_message(self) -> str:
        return f"AutoWhisper service is not running{self._stderr_suffix()}"

    def _stderr_suffix(self) -> str:
        if not self._stderr:
            return ""
        excerpt = " | ".join(list(self._stderr)[-3:])
        return f" (recent diagnostics: {excerpt[:1200]})"
