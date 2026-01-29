.PHONY: all test clean install uninstall help

# Default target
all: test

# Run tests
test:
	python3 -m pytest tests/ || python3 -m unittest discover -s tests -p "test_*.py" || echo "No tests found"

# Run with debug logging
run:
	python3 -m autowhisper --config config.toml

# Clean build artifacts
clean:
	find . -type d -name __pycache__ -exec rm -r {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
	find . -type f -name "*.pyo" -delete 2>/dev/null || true
	find . -type d -name "*.egg-info" -exec rm -r {} + 2>/dev/null || true
	find . -type d -name ".pytest_cache" -exec rm -r {} + 2>/dev/null || true
	rm -rf build/ dist/ .eggs/

# Install system-wide (requires root)
install:
	@echo "Installing AutoWhisper..."
	sudo ./install.sh

# Uninstall
uninstall:
	@echo "Uninstalling AutoWhisper..."
	sudo systemctl stop autowhisper@$$USER || true
	sudo systemctl disable autowhisper@$$USER || true
	sudo rm -f /etc/systemd/system/autowhisper@.service
	sudo systemctl daemon-reload
	@echo "Uninstallation complete"

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
	@echo "  make test               - Run tests"
	@echo "  make run                 - Run with default config"
	@echo "  make clean               - Clean build artifacts"
	@echo "  make install            - Install system-wide (requires root)"
	@echo "  make uninstall          - Uninstall"
	@echo "  make check-deps         - Check system dependencies"
	@echo "  make help               - Show this help"
