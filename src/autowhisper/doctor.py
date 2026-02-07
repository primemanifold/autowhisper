"""System diagnostics for AutoWhisper."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

import click


class DiagnosticResult:
    """Result of a diagnostic check."""

    def __init__(self):
        self.passed = 0
        self.warnings = 0
        self.failed = 0

    def ok(self, msg: str):
        click.echo(f"  {click.style('✓', fg='green')} {msg}")
        self.passed += 1

    def warn(self, msg: str, fix: str | None = None):
        click.echo(f"  {click.style('!', fg='yellow')} {msg}")
        if fix:
            click.echo(f"    → {click.style(fix, dim=True)}")
        self.warnings += 1

    def fail(self, msg: str, fix: str | None = None):
        click.echo(f"  {click.style('✗', fg='red')} {msg}")
        if fix:
            click.echo(f"    → {click.style(fix, dim=True)}")
        self.failed += 1

    def info(self, msg: str):
        click.echo(f"  {click.style('•', fg='blue')} {msg}")


def header(title: str):
    """Print a section header."""
    click.echo()
    click.secho(title, bold=True)
    click.echo("─" * len(title))


def run_cmd(cmd: list[str], timeout: int = 5) -> tuple[int, str, str]:
    """Run a command and return (returncode, stdout, stderr)."""
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "timeout"
    except FileNotFoundError:
        return -2, "", "not found"


def check_gpu(result: DiagnosticResult, fix: bool) -> dict:
    """Check NVIDIA GPU and driver."""
    header("NVIDIA GPU")

    gpu_info = {}

    # Check for nvidia-smi
    if not shutil.which("nvidia-smi"):
        result.fail(
            "nvidia-smi not found",
            "sudo apt install nvidia-driver-535 && sudo reboot",
        )
        return gpu_info

    # Run nvidia-smi
    rc, stdout, stderr = run_cmd(["nvidia-smi"])

    if rc != 0:
        if "Driver/library version mismatch" in stderr:
            result.fail(
                "NVIDIA driver/library version mismatch",
                "sudo reboot (or run: autowhisper doctor --fix)",
            )
            if fix:
                click.echo("    Attempting fix: reloading nvidia modules...")
                subprocess.run(
                    ["sudo", "rmmod", "nvidia_uvm", "nvidia_drm", "nvidia_modeset", "nvidia"],
                    capture_output=True,
                )
                subprocess.run(["sudo", "modprobe", "nvidia"], capture_output=True)
        else:
            result.fail(f"nvidia-smi failed: {stderr.strip()}")
        return gpu_info

    # Get GPU details
    rc, stdout, _ = run_cmd([
        "nvidia-smi",
        "--query-gpu=name,driver_version,memory.total",
        "--format=csv,noheader",
    ])

    if rc == 0 and stdout.strip():
        parts = stdout.strip().split(", ")
        if len(parts) >= 3:
            gpu_info["name"] = parts[0]
            gpu_info["driver"] = parts[1]
            gpu_info["vram"] = parts[2]

            result.info(f"GPU: {gpu_info['name']}")
            result.info(f"Driver: {gpu_info['driver']}")
            result.info(f"VRAM: {gpu_info['vram']}")
            result.ok("NVIDIA GPU detected and working")

            # Check VRAM
            vram_mb = int("".join(c for c in gpu_info["vram"] if c.isdigit()))
            if vram_mb < 2000:
                result.warn("Low VRAM - use tiny.en model")
        else:
            result.ok("NVIDIA GPU detected")

    return gpu_info


def check_cuda(result: DiagnosticResult) -> bool:
    """Check CUDA availability via ctranslate2."""
    header("CUDA")

    try:
        import ctranslate2

        cuda_count = ctranslate2.get_cuda_device_count()
        if cuda_count > 0:
            result.ok(f"CUDA available ({cuda_count} device(s))")
            return True
        else:
            result.warn(
                "No CUDA devices found by ctranslate2",
                "Check nvidia-smi and driver installation",
            )
            return False
    except ImportError:
        result.warn("ctranslate2 not installed (install autowhisper package first)")
        return False
    except Exception as e:
        result.fail(f"CUDA check failed: {e}")
        return False


def check_audio(result: DiagnosticResult) -> bool:
    """Check audio capture devices."""
    header("Audio")

    try:
        import sounddevice as sd

        devices = sd.query_devices()
        input_devices = [d for d in devices if d["max_input_channels"] > 0]

        if input_devices:
            default = sd.query_devices(kind="input")
            result.ok(f"Found {len(input_devices)} input device(s)")
            result.info(f"Default: {default['name']}")
            return True
        else:
            result.fail(
                "No audio input devices found",
                "Check microphone connection",
            )
            return False
    except ImportError:
        result.warn("sounddevice not installed (install autowhisper package first)")
        return False
    except Exception as e:
        result.fail(f"Audio check failed: {e}")
        return False


def check_display(result: DiagnosticResult) -> bool:
    """Check X11 display access."""
    header("Display")

    display = os.environ.get("DISPLAY")
    if not display:
        result.fail(
            "No DISPLAY environment variable",
            "Run from a graphical session, not SSH",
        )
        return False

    result.ok(f"X11 display: {display}")

    # Check if we can connect
    try:
        from Xlib import display as xdisplay

        d = xdisplay.Display()
        d.close()
        result.ok("X11 connection successful")
        return True
    except ImportError:
        result.warn("python-xlib not installed")
        return True  # Not fatal
    except Exception as e:
        result.warn(f"X11 connection failed: {e}")
        return True  # Not fatal, xdotool may still work


def check_tools(result: DiagnosticResult, fix: bool) -> bool:
    """Check required command-line tools."""
    header("Tools")

    all_ok = True

    tools = [
        ("xdotool", "Text injection", "sudo apt install xdotool"),
        ("xclip", "Clipboard access", "sudo apt install xclip"),
    ]

    for tool, desc, fix_cmd in tools:
        if shutil.which(tool):
            result.ok(f"{desc} ({tool})")
        else:
            result.fail(f"{desc} ({tool}) not found", fix_cmd)
            all_ok = False
            if fix:
                click.echo(f"    Installing {tool}...")
                subprocess.run(["sudo", "apt", "install", "-y", tool])

    return all_ok


def check_model(result: DiagnosticResult) -> bool:
    """Check if a Whisper model is downloaded."""
    header("Model")

    try:
        from .config import Config, find_config_file
        from .models import MODELS, is_model_downloaded

        config_path = find_config_file()
        config = Config.load(config_path)
        model_name = config.model.size

        result.info(f"Configured model: {model_name}")

        if model_name not in MODELS:
            result.warn(
                f"Unknown model: {model_name}",
                f"Use one of: {', '.join(MODELS.keys())}",
            )
            return False

        if is_model_downloaded(model_name):
            result.ok(f"Model {model_name} is downloaded")
            return True
        else:
            result.warn(
                f"Model {model_name} not found in cache",
                f"autowhisper model download {model_name}",
            )
            return False

    except FileNotFoundError:
        result.warn("No config file found", "Create config.toml")
        return False
    except Exception as e:
        result.warn(f"Model check failed: {e}")
        return False


def check_service(result: DiagnosticResult, fix: bool) -> bool:
    """Check systemd service status."""
    header("Service")

    rc, stdout, stderr = run_cmd(["systemctl", "--user", "is-active", "autowhisper"])

    if rc == 0 and stdout.strip() == "active":
        result.ok("AutoWhisper service is running")
        return True
    else:
        # Check if service file exists
        service_file = Path.home() / ".config" / "systemd" / "user" / "autowhisper.service"
        if not service_file.exists():
            result.warn(
                "Service not installed",
                "Run: autowhisper setup (or scripts/setup-daemon.sh)",
            )
        else:
            result.info("Service is stopped")
            result.info("Start with: autowhisper start")

        if fix:
            click.echo("    Starting service...")
            subprocess.run(["systemctl", "--user", "start", "autowhisper"])

        return False


def run_diagnostics(fix: bool = False):
    """Run all diagnostic checks."""
    click.echo()
    click.secho("AutoWhisper System Check", bold=True)
    click.secho("=" * 24)

    result = DiagnosticResult()

    # Run all checks
    check_gpu(result, fix)
    check_cuda(result)
    check_audio(result)
    check_display(result)
    check_tools(result, fix)
    check_model(result)
    check_service(result, fix)

    # Summary
    header("Summary")
    click.echo()
    click.echo(f"  {click.style('Passed:', fg='green')}   {result.passed}")
    click.echo(f"  {click.style('Warnings:', fg='yellow')} {result.warnings}")
    click.echo(f"  {click.style('Failed:', fg='red')}   {result.failed}")
    click.echo()

    if result.failed == 0:
        if result.warnings == 0:
            click.secho("System is fully ready for AutoWhisper!", fg="green")
        else:
            click.secho("System is ready with some warnings.", fg="yellow")
        click.echo()
        click.echo("Start with: autowhisper start")
    else:
        click.secho("Please fix the issues above.", fg="red")
        if not fix:
            click.echo("Try: autowhisper doctor --fix")

    click.echo()
    sys.exit(result.failed)
