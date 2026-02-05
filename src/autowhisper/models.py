"""Model management for AutoWhisper."""

import sys
from pathlib import Path

import click

# Available models with metadata
MODELS = {
    "tiny.en": {
        "description": "Fastest, good accuracy",
        "size": "~75MB",
        "speed": "78ms",
        "repo": "Systran/faster-whisper-tiny.en",
    },
    "base.en": {
        "description": "Fast, better accuracy",
        "size": "~150MB",
        "speed": "143ms",
        "repo": "Systran/faster-whisper-base.en",
    },
    "small.en": {
        "description": "Balanced",
        "size": "~500MB",
        "speed": "211ms",
        "repo": "Systran/faster-whisper-small.en",
    },
    "distil-small.en": {
        "description": "Optimized small (recommended)",
        "size": "~400MB",
        "speed": "198ms",
        "repo": "Systran/faster-distil-whisper-small.en",
    },
    "distil-medium.en": {
        "description": "Optimized medium",
        "size": "~800MB",
        "speed": "381ms",
        "repo": "Systran/faster-distil-whisper-medium.en",
    },
    "distil-large-v3": {
        "description": "Best accuracy",
        "size": "~1.5GB",
        "speed": "448ms",
        "repo": "Systran/faster-distil-whisper-large-v3",
    },
    "large-v3": {
        "description": "Maximum accuracy",
        "size": "~3GB",
        "speed": "926ms",
        "repo": "Systran/faster-whisper-large-v3",
    },
}


def get_hf_cache_dir_name(repo: str) -> str:
    """Convert HuggingFace repo name to cache directory name.

    HuggingFace caches repos in directories like:
    models--Systran--faster-whisper-small.en
    """
    return "models--" + repo.replace("/", "--")


def is_model_downloaded(model_name: str) -> bool:
    """Check if a specific model is downloaded."""
    if model_name not in MODELS:
        return False

    cache_dir = Path.home() / ".cache" / "huggingface" / "hub"
    if not cache_dir.exists():
        return False

    repo = MODELS[model_name]["repo"]
    cache_name = get_hf_cache_dir_name(repo)
    return (cache_dir / cache_name).exists()


def get_downloaded_models() -> list[str]:
    """Get list of models already downloaded."""
    return [name for name in MODELS if is_model_downloaded(name)]


def list_models():
    """List available Whisper models."""
    downloaded = get_downloaded_models()

    click.echo()
    click.secho("Available Models", bold=True)
    click.echo("─" * 60)
    click.echo()

    for name, info in MODELS.items():
        status = click.style("✓", fg="green") if name in downloaded else " "
        recommended = " (recommended)" if name == "distil-small.en" else ""

        click.echo(f"  {status} {click.style(name, bold=True)}{recommended}")
        click.echo(f"      {info['description']}")
        click.echo(f"      Size: {info['size']} | Speed: {info['speed']}")
        click.echo()

    click.echo("─" * 60)
    click.echo(f"  ✓ = downloaded")
    click.echo()
    click.echo("Download a model: autowhisper model download <name>")
    click.echo()


def download_model(name: str):
    """Download a Whisper model."""
    if name not in MODELS:
        click.secho(f"Unknown model: {name}", fg="red")
        click.echo(f"Available models: {', '.join(MODELS.keys())}")
        sys.exit(1)

    info = MODELS[name]
    click.echo()
    click.secho(f"Downloading {name}", bold=True)
    click.echo(f"  {info['description']}")
    click.echo(f"  Size: {info['size']}")
    click.echo()

    try:
        from faster_whisper import WhisperModel

        click.echo("Loading model (this will download if not cached)...")

        # Determine device
        try:
            import ctranslate2

            if ctranslate2.get_cuda_device_count() > 0:
                device = "cuda"
                compute_type = "float16"
            else:
                device = "cpu"
                compute_type = "int8"
        except Exception:
            device = "cpu"
            compute_type = "int8"

        click.echo(f"  Device: {device}")

        # Load model (triggers download)
        model = WhisperModel(name, device=device, compute_type=compute_type)

        click.echo()
        click.secho(f"✓ Model {name} ready!", fg="green")
        click.echo()

        # Clean up
        del model

    except Exception as e:
        click.secho(f"Download failed: {e}", fg="red")
        sys.exit(1)
