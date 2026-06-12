#!/usr/bin/env python3
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import json
import os

ROOT = Path(__file__).resolve().parents[2] / "src" / "settings" / "web"
SCHEMA = {
    "model": [
        {"key": "size", "type": "enum", "enum_values": ["tiny.en", "base.en", "small.en"], "min_numeric": None, "max_numeric": None, "description": "Whisper model variant. Smaller is faster, larger is more accurate."},
        {"key": "device", "type": "enum", "enum_values": ["auto", "cpu", "cuda"], "min_numeric": None, "max_numeric": None, "description": "Inference device."},
        {"key": "num_threads", "type": "int", "enum_values": [], "min_numeric": 1, "max_numeric": None, "description": "CPU threads for inference."},
    ],
    "audio": [
        {"key": "device", "type": "string", "enum_values": [], "min_numeric": None, "max_numeric": None, "description": "Input device name. Empty uses the default microphone."},
        {"key": "vad_enabled", "type": "bool", "enum_values": [], "min_numeric": None, "max_numeric": None, "description": "Enable voice activity detection."},
    ],
    "hotkeys": [
        {"key": "mode", "type": "enum", "enum_values": ["push_to_talk", "toggle"], "min_numeric": None, "max_numeric": None, "description": "Hotkey activation mode."},
        {"key": "trigger", "type": "string_array", "enum_values": [], "min_numeric": None, "max_numeric": None, "description": "One or more trigger hotkeys."},
    ],
    "output": [
        {"key": "method", "type": "enum", "enum_values": ["inject", "clipboard"], "min_numeric": None, "max_numeric": None, "description": "How transcribed text reaches the cursor."},
    ],
    "feedback": [
        {"key": "enabled", "type": "bool", "enum_values": [], "min_numeric": None, "max_numeric": None, "description": "Play tones on record start, stop, and error."},
    ],
    "daemon": [
        {"key": "log_level", "type": "enum", "enum_values": ["info", "warn", "error", "debug"], "min_numeric": None, "max_numeric": None, "description": "Log level."},
    ],
    "tray": [
        {"key": "enabled", "type": "bool", "enum_values": [], "min_numeric": None, "max_numeric": None, "description": "Show the system tray icon."},
    ],
}
CONFIG = {
    "model": {"size": "tiny.en", "device": "cpu", "num_threads": 8},
    "audio": {"device": "", "vad_enabled": True},
    "hotkeys": {"mode": "push_to_talk", "trigger": ["shift+super"]},
    "output": {"method": "inject"},
    "feedback": {"enabled": True},
    "daemon": {"log_level": "info"},
    "tray": {"enabled": True},
}
PLATFORM = {
    "platform": "linux",
    "build_target": "linux",
    "summary": "Mock platform readiness for settings UI development.",
    "features": [
        {"id": "hotkeys", "name": "Global hotkeys", "state": "ready", "detail": "X11 push-to-talk verified."},
        {"id": "output", "name": "Text insertion", "state": "ready", "detail": "XTest injection with clipboard fallback."},
        {"id": "tray", "name": "Tray icon", "state": "partial", "detail": "AppIndicator when available."},
        {"id": "wayland", "name": "Wayland", "state": "unsupported", "detail": "X11 only for now."},
    ],
}

class Handler(SimpleHTTPRequestHandler):
    def translate_path(self, path):
        return str(ROOT / path.lstrip("/"))

    def do_GET(self):
        if self.path == "/api/schema":
            return self.send_json(SCHEMA)
        if self.path in ("/api/config", "/api/defaults"):
            return self.send_json(CONFIG)
        if self.path == "/api/platform":
            return self.send_json(PLATFORM)
        return super().do_GET()

    def do_PUT(self):
        if self.path == "/api/config":
            self.send_response(204)
            self.end_headers()
            return
        self.send_error(404)

    def send_json(self, body):
        data = json.dumps(body).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

if __name__ == "__main__":
    port = int(os.environ.get("PORT", "8765"))
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
