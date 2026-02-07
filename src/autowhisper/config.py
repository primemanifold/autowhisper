"""Configuration management for AutoWhisper."""

import logging
from dataclasses import dataclass, field
from pathlib import Path

import toml

logger = logging.getLogger(__name__)


@dataclass
class ModelConfig:
    """Model configuration."""
    size: str = "distil-large-v3"
    device: str = "cuda"
    compute_type: str = "float16"
    beam_size: int = 1
    language: str = "en"
    num_threads: int = 4


@dataclass
class AudioConfig:
    """Audio capture configuration."""
    sample_rate: int = 16000
    channels: int = 1
    buffer_size: int = 512
    device: str | None = None  # Input device (microphone)
    output_device: str | None = None  # Output device (speaker/feedback)
    vad_enabled: bool = True
    vad_threshold: float = 0.5
    silence_duration: float = 0.3
    max_duration: float = 60.0
    mute_other_apps: bool = False  # Mute other audio sources during recording


@dataclass
class HotkeyConfig:
    """Hotkey configuration."""
    mode: str = "push_to_talk"
    trigger: list[str] = field(default_factory=lambda: ["shift+super"])
    cancel: list[str] = field(default_factory=lambda: ["esc"])
    escape_to_cancel: bool = True  # Allow Escape key to cancel recording

    def __post_init__(self):
        # Normalize strings to lists for backwards compatibility
        if isinstance(self.trigger, str):
            self.trigger = [self.trigger]
        if isinstance(self.cancel, str):
            self.cancel = [self.cancel]


@dataclass
class OutputConfig:
    """Text output configuration."""
    method: str = "inject"
    auto_paste: bool = True
    paste_delay: float = 0.05
    ending_action: str = "none"  # "none", "newline", or "return_key"
    lowercase: bool = False
    also_copy_to_clipboard: bool = True  # Also store in clipboard (inject)


@dataclass
class FeedbackConfig:
    """Audio feedback configuration."""
    enabled: bool = True
    frequency_start: int = 800
    frequency_stop: int = 400
    frequency_error: int = 600
    duration: float = 0.1
    volume: float = 0.3


@dataclass
class DaemonConfig:
    """Daemon configuration."""
    log_level: str = "info"
    log_file: str | None = None
    pid_file: str = "/tmp/autowhisper.pid"
    work_dir: str = "/opt/autowhisper"


@dataclass
class TrayConfig:
    """System tray configuration."""
    enabled: bool = True


@dataclass
class Config:
    """Main configuration container."""
    model: ModelConfig = field(default_factory=ModelConfig)
    audio: AudioConfig = field(default_factory=AudioConfig)
    hotkeys: HotkeyConfig = field(default_factory=HotkeyConfig)
    output: OutputConfig = field(default_factory=OutputConfig)
    feedback: FeedbackConfig = field(default_factory=FeedbackConfig)
    daemon: DaemonConfig = field(default_factory=DaemonConfig)
    tray: TrayConfig = field(default_factory=TrayConfig)

    @classmethod
    def load(cls, path: str | Path) -> "Config":
        """Load configuration from a TOML file."""
        path = Path(path)
        if not path.exists():
            raise FileNotFoundError(f"Config file not found: {path}")

        with open(path) as f:
            data = toml.load(f)

        config = cls._from_dict(data)
        config.validate()
        return config

    @classmethod
    def _from_dict(cls, data: dict) -> "Config":
        """Create Config from a dictionary."""
        # Handle output config with backwards compatibility
        output_data = dict(data.get("output", {}))
        # Migrate append_newline -> ending_action
        if "append_newline" in output_data and "ending_action" not in output_data:
            if output_data["append_newline"]:
                output_data["ending_action"] = "newline"
            else:
                output_data["ending_action"] = "none"
        # Filter to valid fields only
        output_data = {
            k: v for k, v in output_data.items()
            if k in OutputConfig.__dataclass_fields__
        }

        return cls(
            model=ModelConfig(**data.get("model", {})),
            audio=AudioConfig(**data.get("audio", {})),
            hotkeys=HotkeyConfig(**data.get("hotkeys", {})),
            output=OutputConfig(**output_data),
            feedback=FeedbackConfig(**{
                k: v for k, v in data.get("feedback", {}).items()
                if k in FeedbackConfig.__dataclass_fields__
            }),
            daemon=DaemonConfig(**{
                k: v for k, v in data.get("daemon", {}).items()
                if k in DaemonConfig.__dataclass_fields__
            }),
            tray=TrayConfig(**{
                k: v for k, v in data.get("tray", {}).items()
                if k in TrayConfig.__dataclass_fields__
            }),
        )

    def validate(self) -> None:
        """Validate configuration values."""
        valid_models = [
            "tiny", "tiny.en",
            "base", "base.en",
            "small", "small.en",
            "medium", "medium.en",
            "large", "large-v1", "large-v2", "large-v3",
            "distil-large-v2", "distil-large-v3",
            "distil-medium.en", "distil-small.en",
        ]
        if self.model.size not in valid_models:
            raise ValueError(
                f"Invalid model size: {self.model.size}. "
                f"Must be one of: {valid_models}"
            )

        valid_devices = ["cuda", "cpu", "auto"]
        if self.model.device not in valid_devices:
            raise ValueError(
                f"Invalid device: {self.model.device}. "
                f"Must be one of: {valid_devices}"
            )

        valid_compute_types = [
            "float16", "float32", "int8", "int8_float16",
            "int8_float32", "int8_bfloat16", "bfloat16",
        ]
        if self.model.compute_type not in valid_compute_types:
            raise ValueError(
                f"Invalid compute_type: {self.model.compute_type}. "
                f"Must be one of: {valid_compute_types}"
            )

        if self.audio.sample_rate != 16000:
            logger.warning(
                f"Sample rate {self.audio.sample_rate} is not Whisper's native 16kHz. "
                "Performance may be affected."
            )

        valid_modes = ["push_to_talk", "toggle"]
        if self.hotkeys.mode not in valid_modes:
            raise ValueError(
                f"Invalid hotkey mode: {self.hotkeys.mode}. "
                f"Must be one of: {valid_modes}"
            )

        valid_methods = ["inject", "clipboard"]
        if self.output.method not in valid_methods:
            raise ValueError(
                f"Invalid output method: {self.output.method}. "
                f"Must be one of: {valid_methods}"
            )

        valid_ending_actions = ["none", "newline", "return_key"]
        if self.output.ending_action not in valid_ending_actions:
            raise ValueError(
                f"Invalid ending_action: {self.output.ending_action}. "
                f"Must be one of: {valid_ending_actions}"
            )

        if not 0.0 <= self.feedback.volume <= 1.0:
            raise ValueError(
                f"Invalid volume: {self.feedback.volume}. Must be between 0.0 and 1.0"
            )

    @classmethod
    def default(cls) -> "Config":
        """Create a default configuration."""
        return cls()


def find_config_file() -> Path:
    """Find the configuration file in standard locations."""
    search_paths = [
        Path("config.toml"),
        Path.home() / ".config" / "autowhisper" / "config.toml",
        Path("/etc/autowhisper/config.toml"),
        Path("/opt/autowhisper/config.toml"),
    ]

    for path in search_paths:
        if path.exists():
            return path

    raise FileNotFoundError(
        f"No config file found. Searched: {[str(p) for p in search_paths]}"
    )
