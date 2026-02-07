#!/bin/bash
# Release script for AutoWhisper
# Usage: ./scripts/release.sh [patch|minor|major|X.Y.Z]
#
# This script handles the local release workflow:
# 1. Bump version in source files
# 2. Update debian/changelog
# 3. Commit changes
# 4. Create annotated tag
# 5. Push to origin (triggers CI for PPA upload)

set -e

# Configuration
MAINTAINER="AutoWhisper Contributors <autowhisper@users.noreply.github.com>"

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

# Update version in source files
update_source_versions() {
    local new_version="$1"

    info "Updating versions to $new_version..."

    # pyproject.toml
    sed -i "s/^version = \".*\"/version = \"$new_version\"/" pyproject.toml

    # src/autowhisper/__init__.py
    sed -i "s/__version__ = \".*\"/__version__ = \"$new_version\"/" src/autowhisper/__init__.py

    info "  - pyproject.toml"
    info "  - src/autowhisper/__init__.py"
}

# Update debian/changelog
update_debian_changelog() {
    local new_version="$1"
    local date_str=$(date -R)

    info "Updating debian/changelog..."

    local changelog_entry="autowhisper (${new_version}-1) noble; urgency=medium

  * Release ${new_version}

 -- ${MAINTAINER}  ${date_str}
"
    echo -e "${changelog_entry}\n$(cat debian/changelog)" > debian/changelog
    info "  - debian/changelog"
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
    echo ""
    echo "This will:"
    echo "  1. Bump version in source files"
    echo "  2. Update debian/changelog"
    echo "  3. Commit changes"
    echo "  4. Create tag v${new_version}"
    echo "  5. Push to origin (triggers CI for PPA upload)"
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

    # Update debian changelog
    update_debian_changelog "$new_version"

    # Commit changes
    info "Committing version bump..."
    git add pyproject.toml src/autowhisper/__init__.py debian/changelog
    git commit -m "Release v${new_version}"

    # Create tag
    info "Creating tag v${new_version}..."
    git tag -a "v${new_version}" -m "Release ${new_version}"

    echo ""
    header "Release Prepared"
    echo ""
    echo "Local changes are ready. To complete the release:"
    echo ""
    echo "  git push origin $(git branch --show-current)"
    echo "  git push origin v${new_version}"
    echo ""
    echo "This will trigger CI to:"
    echo "  - Build signed source package"
    echo "  - Upload to PPA"
    echo "  - Create GitHub release"
    echo ""

    # Offer to push
    read -p "Push now? [y/N] " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        local branch=$(git branch --show-current)
        info "Pushing to origin..."
        git push origin "$branch"
        git push origin "v${new_version}"

        echo ""
        info "Release v${new_version} pushed!"
        echo ""
        echo "Monitor the release:"
        echo "  - CI: https://github.com/$(git remote get-url origin | sed 's/.*github.com[:/]\(.*\)\.git/\1/')/actions"
        echo "  - PPA: https://launchpad.net/~primemanifold/+archive/ubuntu/autowhisper/+packages"
    else
        warn "Push skipped. Don't forget to push when ready!"
    fi
}

main "$@"
