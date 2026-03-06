# Repo Cleanup Design

Date: 2026-03-06

## Problem

The repository has accumulated inconsistencies from the Python-to-C++ migration and org rename (rabotinc to primemanifold). URLs are wrong, .gitignore has Python cruft, branches are unclear, and empty directories remain.

## Changes

### 1. Fix all rabotinc / wrong URLs
- `README.md`: clone URL → primemanifold
- `debian/control`: Homepage, Vcs-Browser, Vcs-Git → primemanifold
- `autowhisper.service`: Documentation URL → primemanifold
- `scripts/uninstall-legacy.sh`: PPA name → primemanifold

### 2. Clean .gitignore
- Remove all Python-specific entries (__pycache__/, venv/, .pytest_cache/, etc.)

### 3. Update debian/changelog
- Add v0.4.0 entry reflecting submodule migration

### 4. Update CI
- `.github/workflows/ci.yml`: trigger only on `core`, remove `main`

### 5. Remove cruft
- Delete empty `packaging/debian/` directory

### 6. Branch cleanup
- Delete `cpp-rewrite` locally and from remote

### 7. Update CLAUDE.md
- Set main branch to `core`
- Document branch strategy: core (main), next (dev), legacy (old Python)
