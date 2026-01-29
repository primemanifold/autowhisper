.PHONY: all test clean install uninstall help
.PHONY: daemon-setup daemon-start daemon-stop daemon-restart daemon-status daemon-logs

# Default target
all: test

# Run tests
test:
	python3 -m pytest tests/ -v --no-cov || echo "Tests failed or not found"

# Run with debug logging
run:
	python3 -m autowhisper --config config.toml -v

# Clean build artifacts
clean:
	find . -type d -name __pycache__ -exec rm -r {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
	find . -type f -name "*.pyo" -delete 2>/dev/null || true
	find . -type d -name "*.egg-info" -exec rm -r {} + 2>/dev/null || true
	find . -type d -name ".pytest_cache" -exec rm -r {} + 2>/dev/null || true
	rm -rf build/ dist/ .eggs/ .coverage htmlcov/

# Install system-wide (requires root)
install:
	@echo "Installing AutoWhisper..."
	sudo ./install.sh
	@echo ""
	@echo "Now run: make daemon-setup"

# Uninstall
uninstall:
	@echo "Uninstalling AutoWhisper..."
	systemctl --user stop autowhisper || true
	systemctl --user disable autowhisper || true
	rm -f ~/.config/systemd/user/autowhisper.service
	systemctl --user daemon-reload
	@echo "Uninstallation complete"

# Daemon management (run as regular user, NOT root)
daemon-setup:
	@./scripts/setup-daemon.sh

daemon-start:
	systemctl --user start autowhisper

daemon-stop:
	systemctl --user stop autowhisper

daemon-restart:
	@./scripts/restart-daemon.sh

daemon-status:
	@./scripts/status-daemon.sh

daemon-logs:
	@./scripts/logs-daemon.sh

# Check system dependencies
check-deps:
	@echo "Checking system dependencies..."
	@command -v python3 >/dev/null 2>&1 || { echo "❌ Python 3 not installed"; exit 1; }
	@command -v pip3 >/dev/null 2>&1 || { echo "❌ pip3 not installed"; exit 1; }
	@command -v nvidia-smi >/dev/null 2>&1 || { echo "⚠️  NVIDIA drivers not found"; }
	@command -v xdotool >/dev/null 2>&1 || { echo "❌ xdotool not installed"; exit 1; }
	@command -v xclip >/dev/null 2>&1 || { echo "❌ xclip not installed"; exit 1; }
	@echo "✅ All dependencies found"

# Show help
help:
	@echo "AutoWhisper Makefile (Python/faster-whisper)"
	@echo ""
	@echo "Available targets:"
	@echo "  make test            - Run tests"
	@echo "  make run             - Run manually with verbose logging"
	@echo "  make clean           - Clean build artifacts"
	@echo "  make install         - Install system-wide (requires sudo)"
	@echo "  make uninstall       - Uninstall"
	@echo "  make check-deps      - Check system dependencies"
	@echo ""
	@echo "Daemon management (run as regular user):"
	@echo "  make daemon-setup    - Set up and start the daemon"
	@echo "  make daemon-start    - Start the daemon"
	@echo "  make daemon-stop     - Stop the daemon"
	@echo "  make daemon-restart  - Restart the daemon"
	@echo "  make daemon-status   - Show daemon status and recent logs"
	@echo "  make daemon-logs     - Follow daemon logs (Ctrl+C to exit)"
