"""AutoWhisper CLI - unified command-line interface."""

import os
import subprocess
import sys
from pathlib import Path

import click

from . import __version__
from .config import find_config_file

SERVICE_NAME = "autowhisper"


def get_config_path() -> Path:
    """Get the config file path, checking user config first."""
    user_config = Path.home() / ".config" / "autowhisper" / "config.toml"
    if user_config.exists():
        return user_config
    return find_config_file()


def run_systemctl(*args: str) -> subprocess.CompletedProcess:
    """Run systemctl --user command."""
    return subprocess.run(
        ["systemctl", "--user", *args],
        capture_output=True,
        text=True,
    )


@click.group()
@click.version_option(__version__, prog_name="autowhisper")
def cli():
    """AutoWhisper - GPU-accelerated voice-to-text for Ubuntu."""
    pass


# =============================================================================
# Service commands
# =============================================================================


@cli.command()
def start():
    """Start the AutoWhisper service."""
    result = run_systemctl("start", SERVICE_NAME)
    if result.returncode == 0:
        click.secho("AutoWhisper started", fg="green")
    else:
        click.secho(f"Failed to start: {result.stderr.strip()}", fg="red")
        sys.exit(1)


@cli.command()
def stop():
    """Stop the AutoWhisper service."""
    result = run_systemctl("stop", SERVICE_NAME)
    if result.returncode == 0:
        click.secho("AutoWhisper stopped", fg="green")
    else:
        click.secho(f"Failed to stop: {result.stderr.strip()}", fg="red")
        sys.exit(1)


@cli.command()
def restart():
    """Restart the AutoWhisper service."""
    result = run_systemctl("restart", SERVICE_NAME)
    if result.returncode == 0:
        click.secho("AutoWhisper restarted", fg="green")
    else:
        click.secho(f"Failed to restart: {result.stderr.strip()}", fg="red")
        sys.exit(1)


@cli.command()
def status():
    """Show AutoWhisper service status."""
    result = run_systemctl("status", SERVICE_NAME)
    click.echo(result.stdout)
    if result.stderr:
        click.echo(result.stderr)


@cli.command()
@click.option("-f", "--follow", is_flag=True, help="Follow log output")
@click.option("-n", "--lines", default=50, help="Number of lines to show")
def logs(follow: bool, lines: int):
    """Show AutoWhisper service logs."""
    cmd = ["journalctl", "--user", "-u", SERVICE_NAME, f"-n{lines}"]
    if follow:
        cmd.append("-f")
    subprocess.run(cmd)


@cli.command()
@click.option(
    "-c", "--config",
    type=click.Path(exists=True),
    help="Config file path",
)
@click.option("--device", type=click.Choice(["cuda", "cpu", "auto"]))
@click.option("--model", type=str, help="Model name")
@click.option("-v", "--verbose", is_flag=True, help="Verbose logging")
def run(config: str | None, device: str | None, model: str | None, verbose: bool):
    """Run AutoWhisper in foreground (for testing)."""
    from .__main__ import main as daemon_main

    # Build args for the daemon main
    sys.argv = ["autowhisper"]
    if config:
        sys.argv.extend(["--config", config])
    if device:
        sys.argv.extend(["--device", device])
    if model:
        sys.argv.extend(["--model", model])
    if verbose:
        sys.argv.append("--verbose")

    sys.exit(daemon_main())


# =============================================================================
# Config commands
# =============================================================================


@cli.group()
def config():
    """View and edit configuration."""
    pass


@config.command("show")
def config_show():
    """Show current configuration."""
    try:
        config_path = get_config_path()
        click.echo(f"Config file: {config_path}\n")
        click.echo(config_path.read_text())
    except FileNotFoundError:
        click.secho("No config file found", fg="red")
        sys.exit(1)


@config.command("edit")
def config_edit():
    """Open configuration in editor."""
    try:
        config_path = get_config_path()
    except FileNotFoundError:
        # Create user config directory
        user_config = Path.home() / ".config" / "autowhisper" / "config.toml"
        user_config.parent.mkdir(parents=True, exist_ok=True)

        # Copy system config if exists
        system_config = Path("/etc/autowhisper/config.toml")
        if system_config.exists():
            user_config.write_text(system_config.read_text())
        else:
            click.secho("No config file found to edit", fg="red")
            sys.exit(1)
        config_path = user_config

    editor = os.environ.get("EDITOR", "nano")
    subprocess.run([editor, str(config_path)])


@config.command("set")
@click.argument("key")
@click.argument("value")
def config_set(key: str, value: str):
    """Set a configuration value (e.g., model.size distil-large-v3)."""
    try:
        config_path = get_config_path()
        import toml

        data = toml.load(config_path)

        # Parse key like "model.size"
        parts = key.split(".")
        if len(parts) != 2:
            click.secho("Key must be in format: section.key", fg="red")
            sys.exit(1)

        section, option = parts
        if section not in data:
            click.secho(f"Unknown section: {section}", fg="red")
            sys.exit(1)

        # Convert value to appropriate type
        if value.lower() in ("true", "false"):
            value = value.lower() == "true"
        elif value.isdigit():
            value = int(value)
        elif value.replace(".", "", 1).isdigit():
            value = float(value)

        data[section][option] = value

        with open(config_path, "w") as f:
            toml.dump(data, f)

        click.secho(f"Set {key} = {value}", fg="green")
        click.echo("Restart service for changes to take effect: autowhisper restart")

    except FileNotFoundError:
        click.secho("No config file found", fg="red")
        sys.exit(1)


@config.command("path")
def config_path():
    """Show config file path."""
    try:
        click.echo(get_config_path())
    except FileNotFoundError:
        click.secho("No config file found", fg="red")
        sys.exit(1)


# =============================================================================
# Doctor command
# =============================================================================


@cli.command()
@click.option("--fix", is_flag=True, help="Attempt to fix issues")
def doctor(fix: bool):
    """Diagnose system configuration."""
    from .doctor import run_diagnostics

    run_diagnostics(fix=fix)


# =============================================================================
# Model commands
# =============================================================================


@cli.group()
def model():
    """Manage Whisper models."""
    pass


@model.command("list")
def model_list():
    """List available models."""
    from .models import list_models

    list_models()


@model.command("download")
@click.argument("name")
def model_download(name: str):
    """Download a Whisper model."""
    from .models import download_model

    download_model(name)


if __name__ == "__main__":
    cli()
