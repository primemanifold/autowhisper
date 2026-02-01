#!/bin/bash
# Release script for AutoWhisper
# Usage: ./scripts/release.sh [patch|minor|major|X.Y.Z]

set -e

# Configuration
GPG_KEY="9D95F673AAED28443AAF932A0252321A2401D829"
PPA="ppa:primemanifold/autowhisper"
MAINTAINER="AutoWhisper Contributors <autowhisper@users.noreply.github.com>"

# Ubuntu series to build for
UBUNTU_SERIES=("noble")

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

error() { echo -e "${RED}Error: $1${NC}" >&2; exit 1; }
info() { echo -e "${GREEN}$1${NC}"; }
warn() { echo -e "${YELLOW}$1${NC}"; }
header() { echo -e "${BLUE}=== $1 ===${NC}"; }

# Get script directory and repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# Check prerequisites
command -v debuild >/dev/null 2>&1 || error "debuild not found. Install: sudo apt install devscripts"
command -v dput >/dev/null 2>&1 || error "dput not found. Install: sudo apt install dput"

# Check git status
if [[ -n $(git status --porcelain) ]]; then
    error "Working directory is not clean. Commit or stash changes first."
fi

# Get current version from pyproject.toml
get_current_version() {
    grep -Po '(?<=^version = ")[^"]+' pyproject.toml
}

# Parse version into components
parse_version() {
    local version="$1"
    if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        error "Invalid version format: $version (expected X.Y.Z)"
    fi
    echo "$version"
}

# Bump version
bump_version() {
    local current="$1"
    local bump_type="$2"

    IFS='.' read -r major minor patch <<< "$current"

    case "$bump_type" in
        major) echo "$((major + 1)).0.0" ;;
        minor) echo "${major}.$((minor + 1)).0" ;;
        patch) echo "${major}.${minor}.$((patch + 1))" ;;
        *) parse_version "$bump_type" ;;
    esac
}

# Update version in source files (not changelog - that's done per-series)
update_source_versions() {
    local new_version="$1"

    info "Updating versions to $new_version..."

    # pyproject.toml
    sed -i "s/^version = \".*\"/version = \"$new_version\"/" pyproject.toml

    # src/autowhisper/__init__.py
    sed -i "s/__version__ = \".*\"/__version__ = \"$new_version\"/" src/autowhisper/__init__.py

    info "Versions updated in pyproject.toml, __init__.py"
}

# Build source package for a specific Ubuntu series
build_for_series() {
    local version="$1"
    local series="$2"
    local revision="$3"
    local build_dir="$4"

    header "Building for Ubuntu $series"

    local pkg_dir="$build_dir/autowhisper-${version}-${series}"

    # Clone current state
    git clone --depth 1 "$REPO_ROOT" "$pkg_dir" 2>/dev/null
    rm -rf "$pkg_dir/.git" "$pkg_dir/.hotkey-venv"

    # Update debian/changelog for this series
    local date_str=$(date -R)
    cat > "$pkg_dir/debian/changelog" << EOF
autowhisper (${version}-${revision}~${series}1) ${series}; urgency=medium

  * Release ${version}

 -- ${MAINTAINER}  ${date_str}
EOF

    # Create orig tarball (only needed once per version)
    cd "$build_dir"
    if [[ ! -f "autowhisper_${version}.orig.tar.gz" ]]; then
        tar czf "autowhisper_${version}.orig.tar.gz" -C "$pkg_dir" . --transform "s,^\.,autowhisper-${version},"
    fi

    # Build source package
    cd "$pkg_dir"
    debuild -S -sa -d -k"$GPG_KEY" 2>&1 | tail -5

    info "Built: autowhisper_${version}-${revision}~${series}1_source.changes"
}

# Upload all packages to PPA
upload_all() {
    local build_dir="$1"

    header "Uploading to PPA"

    for changes in "$build_dir"/*_source.changes; do
        if [[ -f "$changes" ]]; then
            info "Uploading $(basename "$changes")..."
            dput "$PPA" "$changes"
            echo ""
        fi
    done
}

# Main
main() {
    local bump_type="${1:-patch}"
    local current_version=$(get_current_version)
    local new_version=$(bump_version "$current_version" "$bump_type")

    echo ""
    header "AutoWhisper Release"
    echo "Current version: $current_version"
    echo "New version:     $new_version"
    echo "PPA:             $PPA"
    echo "Ubuntu series:   ${UBUNTU_SERIES[*]}"
    echo ""

    # Confirm
    read -p "Proceed with release? [y/N] " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        warn "Aborted."
        exit 0
    fi

    # Update source versions
    update_source_versions "$new_version"

    # Update main changelog (for git history)
    local date_str=$(date -R)
    local changelog_entry="autowhisper (${new_version}-1) noble; urgency=medium

  * Release ${new_version}

 -- ${MAINTAINER}  ${date_str}
"
    echo -e "${changelog_entry}\n$(cat debian/changelog)" > debian/changelog

    # Commit changes
    info "Committing version bump..."
    git add pyproject.toml src/autowhisper/__init__.py debian/changelog
    git commit -m "Release v${new_version}"

    # Create tag
    info "Creating tag v${new_version}..."
    git tag -a "v${new_version}" -m "Release ${new_version}"

    # Create build directory
    local build_dir=$(mktemp -d)
    info "Build directory: $build_dir"

    # Build for each series
    local revision=1
    for series in "${UBUNTU_SERIES[@]}"; do
        build_for_series "$new_version" "$series" "$revision" "$build_dir"
    done

    # List built packages
    echo ""
    header "Built Packages"
    ls -la "$build_dir"/*_source.changes 2>/dev/null || warn "No packages built"

    # Upload
    echo ""
    read -p "Upload all packages to PPA? [y/N] " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        upload_all "$build_dir"

        # Push to origin
        info "Pushing to origin..."
        git push origin main
        git push origin "v${new_version}"

        echo ""
        info "Release v${new_version} complete!"
        echo "Monitor builds at: https://launchpad.net/~primemanifold/+archive/ubuntu/autowhisper/+packages"
    else
        warn "Skipped PPA upload."
        echo "Packages are in: $build_dir"
        echo "Don't forget to push:"
        echo "  git push origin main"
        echo "  git push origin v${new_version}"
    fi
}

main "$@"
