---
description: Create a new release - bumps version, creates tag, and triggers CI for PPA upload
argument-hint: "[patch|minor|major|X.Y.Z]"
allowed-tools:
  - Bash
  - Read
  - Grep
---

# Release Skill

Create a new AutoWhisper release. This will:

1. Bump version in `pyproject.toml`, `src/autowhisper/__init__.py`, and `debian/changelog`
2. Commit the version bump
3. Create an annotated git tag
4. Push to origin (triggers CI for PPA upload and GitHub release)

## Usage

The argument specifies the version bump type:
- `patch` (default): 0.2.6 → 0.2.7
- `minor`: 0.2.6 → 0.3.0
- `major`: 0.2.6 → 1.0.0
- `X.Y.Z`: Set explicit version (e.g., `1.0.0`)

## Instructions

1. First, check the current version and git status:

```bash
grep -Po '(?<=^version = ")[^"]+' pyproject.toml
git status --short
```

2. If the working directory is clean, run the release script with the specified bump type (default to `patch` if no argument provided):

```bash
./scripts/release.sh $ARGUMENTS
```

3. The script is interactive - it will:
   - Show current and new version
   - Ask for confirmation before proceeding
   - Ask if you want to push after creating the tag

4. After pushing, remind the user:
   - CI will automatically build and upload to PPA
   - Monitor at: https://github.com/rabotinc/autowhisper/actions
   - PPA builds at: https://launchpad.net/~primemanifold/+archive/ubuntu/autowhisper/+packages

## Important

- Ensure `GPG_PRIVATE_KEY` secret is configured in GitHub repository settings
- The working directory must be clean (no uncommitted changes)
- Only run from the `core` or `main` branch
