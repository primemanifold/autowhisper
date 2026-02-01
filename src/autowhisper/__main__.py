"""AutoWhisper entry point."""

import argparse
import logging
import sys
from pathlib import Path

from . import __version__
from .config import Config, find_config_file
from .daemon import AutoWhisperDaemon


def setup_logging(level: str, log_file: str | None = None) -> None:
    """Configure logging."""
    level_map = {
        "trace": logging.DEBUG,
        "debug": logging.DEBUG,
        "info": logging.INFO,
        "warn": logging.WARNING,
        "warning": logging.WARNING,
        "error": logging.ERROR,
    }

    log_level = level_map.get(level.lower(), logging.INFO)

    # Format
    fmt = "%(asctime)s [%(levelname)s] %(name)s: %(message)s"
    datefmt = "%Y-%m-%d %H:%M:%S"

    handlers: list[logging.Handler] = []

    # Console handler
    console_handler = logging.StreamHandler(sys.stderr)
    console_handler.setFormatter(logging.Formatter(fmt, datefmt))
    handlers.append(console_handler)

    # File handler (optional)
    if log_file:
        try:
            file_handler = logging.FileHandler(log_file)
            file_handler.setFormatter(logging.Formatter(fmt, datefmt))
            handlers.append(file_handler)
        except Exception as e:
            print(f"Warning: Could not open log file {log_file}: {e}", file=sys.stderr)

    # Configure root logger
    logging.basicConfig(
        level=log_level,
        format=fmt,
        datefmt=datefmt,
        handlers=handlers,
    )

    # Reduce noise from libraries
    logging.getLogger("urllib3").setLevel(logging.WARNING)
    logging.getLogger("filelock").setLevel(logging.WARNING)


def parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        prog="autowhisper",
        description="Real-time speech to text with faster-whisper",
    )

    parser.add_argument(
        "-V", "--version",
        action="version",
        version=f"%(prog)s {__version__}",
    )

    parser.add_argument(
        "-c", "--config",
        type=str,
        help="Path to configuration file",
    )

    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose (debug) logging",
    )

    parser.add_argument(
        "--log-level",
        type=str,
        choices=["trace", "debug", "info", "warn", "error"],
        help="Set log level (overrides config)",
    )

    parser.add_argument(
        "--log-file",
        type=str,
        help="Path to log file (overrides config)",
    )

    parser.add_argument(
        "--device",
        type=str,
        choices=["cuda", "cpu", "auto"],
        help="Inference device (overrides config)",
    )

    parser.add_argument(
        "--model",
        type=str,
        help="Model size/name (overrides config)",
    )

    return parser.parse_args()


def main() -> int:
    """Main entry point."""
    args = parse_args()

    # Load configuration
    try:
        if args.config:
            config_path = Path(args.config)
        else:
            config_path = find_config_file()

        print(f"Loading configuration from {config_path}")
        config = Config.load(config_path)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        print("Create a config.toml file or specify one with --config", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Error loading configuration: {e}", file=sys.stderr)
        return 1

    # Apply CLI overrides
    if args.verbose:
        config.daemon.log_level = "debug"
    if args.log_level:
        config.daemon.log_level = args.log_level
    if args.log_file:
        config.daemon.log_file = args.log_file
    if args.device:
        config.model.device = args.device
    if args.model:
        config.model.size = args.model

    # Set up logging
    setup_logging(config.daemon.log_level, config.daemon.log_file)

    logger = logging.getLogger(__name__)
    logger.info(f"AutoWhisper v{__version__} starting")
    logger.info(f"Model: {config.model.size} on {config.model.device}")
    logger.info(f"Hotkey: {config.hotkeys.trigger} ({config.hotkeys.mode})")

    # Create and run daemon
    try:
        daemon = AutoWhisperDaemon(config, config_path=str(config_path))
        daemon.initialize()
        daemon.run()
        return 0
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
        return 0
    except Exception as e:
        logger.exception(f"Fatal error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
