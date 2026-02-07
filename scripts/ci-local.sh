#!/bin/bash
# Local CI runner - mimics what GitHub Actions CI does
# Run this before pushing to catch issues early

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

# Use project venv if available
if [ -d "/opt/autowhisper/venv" ]; then
    source /opt/autowhisper/venv/bin/activate
elif [ -d "$PROJECT_DIR/.venv" ]; then
    source "$PROJECT_DIR/.venv/bin/activate"
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}! $1${NC}"
}

FAILED=0

# Track what to run
RUN_LINT=true
RUN_TEST=true
RUN_BUILD=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --lint-only)
            RUN_TEST=false
            RUN_BUILD=false
            shift
            ;;
        --test-only)
            RUN_LINT=false
            RUN_BUILD=false
            shift
            ;;
        --build)
            RUN_BUILD=true
            shift
            ;;
        --all)
            RUN_LINT=true
            RUN_TEST=true
            RUN_BUILD=true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --lint-only   Run only linting"
            echo "  --test-only   Run only tests"
            echo "  --build       Include Debian package build (slow)"
            echo "  --all         Run everything including build"
            echo "  --help        Show this help"
            echo ""
            echo "Default: runs lint and test (no build)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# ============================================================================
# LINT
# ============================================================================
if $RUN_LINT; then
    print_header "LINT: Running ruff"

    if ! command -v ruff &> /dev/null; then
        print_warning "ruff not found, installing..."
        pip install ruff --quiet
    fi

    if ruff check src/; then
        print_success "Linting passed"
    else
        print_error "Linting failed"
        FAILED=1
    fi
fi

# ============================================================================
# TEST
# ============================================================================
if $RUN_TEST; then
    print_header "TEST: Running pytest"

    # Check for test dependencies
    if ! python -c "import pytest" 2>/dev/null; then
        print_warning "pytest not found, installing..."
        pip install pytest pytest-cov --quiet
    fi

    # Run tests (excluding slow benchmarks)
    if pytest tests/ -v --tb=short -x --ignore=tests/test_pipeline_performance.py 2>/dev/null; then
        print_success "Unit tests passed"
    else
        print_error "Unit tests failed"
        FAILED=1
    fi

    # Run config tests from performance file (fast ones only)
    if pytest tests/test_pipeline_performance.py -v --tb=short -k "not Benchmark and not Transcription and not Model" 2>/dev/null; then
        print_success "Integration tests passed"
    else
        print_warning "Some integration tests failed (may need GPU)"
    fi
fi

# ============================================================================
# BUILD
# ============================================================================
if $RUN_BUILD; then
    print_header "BUILD: Building Debian package"

    # Check for build dependencies
    MISSING_DEPS=""
    for pkg in debhelper dh-python pybuild-plugin-pyproject; do
        if ! dpkg -l "$pkg" &>/dev/null; then
            MISSING_DEPS="$MISSING_DEPS $pkg"
        fi
    done

    if [ -n "$MISSING_DEPS" ]; then
        print_warning "Missing build dependencies:$MISSING_DEPS"
        echo "Install with: sudo apt-get install -y$MISSING_DEPS"
        print_error "Skipping build"
        FAILED=1
    else
        # Clean previous builds
        rm -f ../autowhisper_*.deb ../autowhisper_*.dsc ../autowhisper_*.tar.* ../autowhisper_*.changes ../autowhisper_*.buildinfo 2>/dev/null || true

        if dpkg-buildpackage -us -uc -b 2>&1 | tail -20; then
            if ls ../autowhisper_*.deb &>/dev/null; then
                print_success "Debian package built successfully"
                ls -lh ../autowhisper_*.deb
            else
                print_error "Debian package not found after build"
                FAILED=1
            fi
        else
            print_error "Debian package build failed"
            FAILED=1
        fi
    fi
fi

# ============================================================================
# SUMMARY
# ============================================================================
print_header "SUMMARY"

if [ $FAILED -eq 0 ]; then
    print_success "All CI checks passed!"
    echo ""
    echo "Safe to push."
    exit 0
else
    print_error "Some CI checks failed!"
    echo ""
    echo "Fix the issues above before pushing."
    exit 1
fi
