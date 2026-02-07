#!/bin/bash
# Build signed source packages for Launchpad PPA
# Usage: ./scripts/build-source-package.sh <version> [--series <series>] [--gpg-key <key>]
#
# This script is used by both:
# - Local development (via release.sh)
# - GitHub Actions CI (via ppa-release.yml)

set -e

# Default configuration
GPG_KEY="${GPG_KEY:-9D95F673AAED28443AAF932A0252321A2401D829}"
MAINTAINER="AutoWhisper Contributors <autowhisper@users.noreply.github.com>"
DEFAULT_SERIES="noble"

# Colors (disabled in CI)
if [[ -t 1 ]] && [[ -z "$CI" ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

error() { echo -e "${RED}Error: $1${NC}" >&2; exit 1; }
info() { echo -e "${GREEN}$1${NC}"; }
warn() { echo -e "${YELLOW}$1${NC}"; }
header() { echo -e "${BLUE}=== $1 ===${NC}"; }

usage() {
    cat << EOF
Usage: $(basename "$0") <version> [options]

Build signed source packages for Launchpad PPA.

Arguments:
    version             Version number (e.g., 0.2.7)

Options:
    --series SERIES     Ubuntu series to build for (default: $DEFAULT_SERIES)
    --gpg-key KEY       GPG key ID for signing (default: from GPG_KEY env or hardcoded)
    --output-dir DIR    Directory for build output (default: temp directory)
    --revision REV      Package revision number (default: 1)
    -h, --help          Show this help message

Environment:
    GPG_KEY             GPG key ID (can be overridden with --gpg-key)

Examples:
    $(basename "$0") 0.2.7
    $(basename "$0") 0.2.7 --series noble --output-dir ./build
    GPG_KEY=ABCD1234 $(basename "$0") 0.2.7
EOF
    exit 0
}

# Parse arguments
VERSION=""
SERIES="$DEFAULT_SERIES"
OUTPUT_DIR=""
REVISION="1"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            ;;
        --series)
            SERIES="$2"
            shift 2
            ;;
        --gpg-key)
            GPG_KEY="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --revision)
            REVISION="$2"
            shift 2
            ;;
        -*)
            error "Unknown option: $1"
            ;;
        *)
            if [[ -z "$VERSION" ]]; then
                VERSION="$1"
            else
                error "Unexpected argument: $1"
            fi
            shift
            ;;
    esac
done

# Validate version
if [[ -z "$VERSION" ]]; then
    error "Version is required. Use --help for usage."
fi

if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9]+)?$ ]]; then
    error "Invalid version format: $VERSION (expected X.Y.Z or X.Y.Z-suffix)"
fi

# Get repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Set up output directory
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR=$(mktemp -d)
    info "Build directory: $OUTPUT_DIR"
else
    mkdir -p "$OUTPUT_DIR"
fi

# Build source package for a specific Ubuntu series
build_for_series() {
    local version="$1"
    local series="$2"
    local revision="$3"
    local build_dir="$4"

    header "Building for Ubuntu $series"

    local pkg_dir="$build_dir/autowhisper-${version}-${series}"

    # Clone current state (without .git to avoid issues)
    info "Preparing source tree..."
    mkdir -p "$pkg_dir"

    # Copy source files, excluding build artifacts and git
    rsync -a \
        --exclude='.git' \
        --exclude='build' \
        "$REPO_ROOT/" "$pkg_dir/"

    # Update debian/changelog for this series
    local date_str=$(date -R)
    cat > "$pkg_dir/debian/changelog" << EOF
autowhisper (${version}-${revision}~${series}1) ${series}; urgency=medium

  * Release ${version}

 -- ${MAINTAINER}  ${date_str}
EOF

    # Create orig tarball (only needed once per version)
    cd "$build_dir"
    local orig_tarball="autowhisper_${version}.orig.tar.gz"
    if [[ ! -f "$orig_tarball" ]]; then
        info "Creating orig tarball..."
        tar czf "$orig_tarball" \
            --exclude='debian' \
            -C "$pkg_dir" .
    fi

    # Build source package
    cd "$pkg_dir"
    info "Building source package (GPG key: ${GPG_KEY:0:8}...)..."
    debuild -S -sa -d -k"$GPG_KEY" 2>&1 | tail -10

    local changes_file="$build_dir/autowhisper_${version}-${revision}~${series}1_source.changes"
    if [[ -f "$changes_file" ]]; then
        info "Built: $(basename "$changes_file")"
    else
        error "Build failed - changes file not found"
    fi
}

# Main
main() {
    header "Building AutoWhisper Source Package"
    echo "Version:    $VERSION"
    echo "Series:     $SERIES"
    echo "Revision:   $REVISION"
    echo "GPG Key:    ${GPG_KEY:0:8}..."
    echo "Output:     $OUTPUT_DIR"
    echo ""

    # Check prerequisites
    command -v debuild >/dev/null 2>&1 || error "debuild not found. Install: sudo apt install devscripts"
    command -v rsync >/dev/null 2>&1 || error "rsync not found. Install: sudo apt install rsync"

    # Check GPG key is available
    if ! gpg --list-secret-keys "$GPG_KEY" >/dev/null 2>&1; then
        error "GPG key $GPG_KEY not found in keyring"
    fi

    # Build
    build_for_series "$VERSION" "$SERIES" "$REVISION" "$OUTPUT_DIR"

    echo ""
    header "Build Complete"
    echo "Output directory: $OUTPUT_DIR"
    ls -la "$OUTPUT_DIR"/*.changes "$OUTPUT_DIR"/*.dsc 2>/dev/null || true

    # Output path for CI consumption
    echo ""
    echo "CHANGES_FILE=$OUTPUT_DIR/autowhisper_${VERSION}-${REVISION}~${SERIES}1_source.changes"
}

main
