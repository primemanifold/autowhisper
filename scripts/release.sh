#!/bin/bash
# Release script for AutoWhisper
# Usage: ./scripts/release.sh [patch|minor|major|X.Y.Z]

set -e

# Configuration
GPG_KEY="9D95F673AAED28443AAF932A0252321A2401D829"
PPA="ppa:primemanifold/autowhisper"
MAINTAINER="AutoWhisper Contributors <autowhisper@users.noreply.github.com>"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

error() { echo -e "${RED}Error: $1${NC}" >&2; exit 1; }
info() { echo -e "${GREEN}$1${NC}"; }
warn() { echo -e "${YELLOW}$1${NC}"; }

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

# Update version in all files
update_versions() {
    local new_version="$1"

    info "Updating versions to $new_version..."

    # pyproject.toml
    sed -i "s/^version = \".*\"/version = \"$new_version\"/" pyproject.toml

    # src/autowhisper/__init__.py
    sed -i "s/__version__ = \".*\"/__version__ = \"$new_version\"/" src/autowhisper/__init__.py

    # debian/changelog - prepend new entry
    local date_str=$(date -R)
    local changelog_entry="autowhisper (${new_version}-1) noble; urgency=medium

  * Release ${new_version}

 -- ${MAINTAINER}  ${date_str}
"
    echo -e "${changelog_entry}\n$(cat debian/changelog)" > debian/changelog

    info "Versions updated in pyproject.toml, __init__.py, debian/changelog"
}

# Build source package
build_package() {
    local version="$1"

    info "Building source package..."

    # Create clean build directory
    local build_dir=$(mktemp -d)
    local pkg_dir="$build_dir/autowhisper-${version}"

    # Clone current state
    git clone --depth 1 "$REPO_ROOT" "$pkg_dir"
    rm -rf "$pkg_dir/.git" "$pkg_dir/.hotkey-venv"

    # Create orig tarball
    cd "$build_dir"
    tar czf "autowhisper_${version}.orig.tar.gz" "autowhisper-${version}"

    # Build source package
    cd "$pkg_dir"
    debuild -S -sa -d -k"$GPG_KEY"

    # Return path to changes file
    echo "$build_dir/autowhisper_${version}-1_source.changes"
}

# Upload to PPA
upload_ppa() {
    local changes_file="$1"

    info "Uploading to PPA..."
    dput "$PPA" "$changes_file"
}

# Main
main() {
    local bump_type="${1:-patch}"
    local current_version=$(get_current_version)
    local new_version=$(bump_version "$current_version" "$bump_type")

    echo ""
    info "AutoWhisper Release"
    echo "==================="
    echo "Current version: $current_version"
    echo "New version:     $new_version"
    echo "PPA:             $PPA"
    echo ""

    # Confirm
    read -p "Proceed with release? [y/N] " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        warn "Aborted."
        exit 0
    fi

    # Update versions
    update_versions "$new_version"

    # Commit changes
    info "Committing version bump..."
    git add pyproject.toml src/autowhisper/__init__.py debian/changelog
    git commit -m "Release v${new_version}"

    # Create tag
    info "Creating tag v${new_version}..."
    git tag -a "v${new_version}" -m "Release ${new_version}"

    # Build package
    local changes_file=$(build_package "$new_version")

    # Upload
    echo ""
    info "Package built: $changes_file"
    read -p "Upload to PPA? [y/N] " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        upload_ppa "$changes_file"

        # Push to origin
        info "Pushing to origin..."
        git push origin main
        git push origin "v${new_version}"

        echo ""
        info "Release v${new_version} complete!"
        echo "Monitor build at: https://launchpad.net/~primemanifold/+archive/ubuntu/autowhisper/+packages"
    else
        warn "Skipped PPA upload. Don't forget to push:"
        echo "  git push origin main"
        echo "  git push origin v${new_version}"
    fi
}

main "$@"
