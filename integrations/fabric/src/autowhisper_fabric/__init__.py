"""Fabric plugin entry point for the AutoWhisper transcription provider."""

from .provider import AutoWhisperProvider

__all__ = ["AutoWhisperProvider", "register"]


def register(ctx) -> None:
    """Register AutoWhisper with Fabric's transcription-provider registry."""
    ctx.register_transcription_provider(AutoWhisperProvider())
