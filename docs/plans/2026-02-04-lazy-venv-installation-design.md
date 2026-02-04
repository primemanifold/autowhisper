# Lazy Virtual Environment Installation with uv

## Problem

The original debian package installation was slow because it:

1. Created a Python virtual environment during `dpkg -i`
2. Used pip for dependency resolution and installation
3. Downloaded ~2GB of PyTorch/CUDA dependencies synchronously
4. Blocked the package manager until completion

This made package installation take several minutes, which is a poor user experience.

## Solution

Two optimizations were implemented:

1. **Lazy installation** - Defer venv setup to first CLI invocation
2. **Use uv instead of pip** - 10-100x faster dependency resolution and installation

## Architecture

```
Package Install (instant)
    │
    ├── Copies files to /usr/share/autowhisper/
    │   ├── autowhisper-*.whl
    │   ├── requirements.lock
    │   ├── autowhisper-setup
    │   └── autowhisper-wrapper
    │
    ├── Creates /opt/autowhisper/venv/ directory
    │
    └── Symlinks /usr/local/bin/autowhisper → wrapper

First CLI Run
    │
    ├── Wrapper checks for marker file
    │   └── /opt/autowhisper/venv/.setup-complete
    │
    ├── If missing, runs autowhisper-setup
    │   ├── Creates venv
    │   ├── Installs uv
    │   ├── Detects GPU for PyTorch index
    │   ├── Installs dependencies with uv
    │   └── Creates marker file
    │
    └── Execs to /opt/autowhisper/venv/bin/autowhisper

Subsequent Runs
    │
    └── Wrapper sees marker, execs directly to venv CLI
```

## Files

### debian/autowhisper-wrapper

Lightweight shell script installed as the main CLI entry point.

- Checks for `.setup-complete` marker file
- Runs setup script if marker is missing
- Execs to real autowhisper CLI in venv

Location: `/usr/share/autowhisper/autowhisper-wrapper`
Symlinked to: `/usr/local/bin/autowhisper`

### debian/autowhisper-setup

Setup script that creates and populates the virtual environment.

- Detects NVIDIA GPU architecture (Blackwell vs older)
- Selects appropriate PyTorch index URL
- Installs uv first for fast subsequent installs
- Uses `uv pip install` with:
  - `--python` to target the venv
  - `--index-strategy unsafe-best-match` to handle multiple package indexes
  - `UV_HTTP_TIMEOUT=300` for large CUDA downloads
- Creates marker file on completion

Location: `/usr/share/autowhisper/autowhisper-setup`

Can be run manually: `/usr/share/autowhisper/autowhisper-setup`

### debian/postinst

Simplified to only:

- Create necessary directories
- Install the wrapper symlink
- Display instructions

No longer performs venv creation or dependency installation.

### debian/rules

Updated to install the new scripts with executable permissions (755).

## GPU Detection

The setup script detects GPU architecture to select the correct PyTorch wheel index:

| GPU | PyTorch Index |
|-----|---------------|
| RTX 50xx (Blackwell) | `https://download.pytorch.org/whl/nightly/cu128` |
| RTX 40xx and older | `https://download.pytorch.org/whl/cu124` |

## User Experience

### Before (old behavior)

```
$ sudo dpkg -i autowhisper_0.2.3_amd64.deb
...
AutoWhisper: Setting up isolated Python environment...
Installing dependencies (this may take a few minutes)...
[waits several minutes]
AutoWhisper installed successfully!
```

### After (new behavior)

```
$ sudo dpkg -i autowhisper_0.2.3_amd64.deb
...
AutoWhisper installed!

The Python environment will be set up on first run.
This uses 'uv' for fast installation (~10x faster than pip).

$ autowhisper doctor
AutoWhisper: Setting up Python environment (first run)...
Installing uv package manager...
Installing dependencies with uv...
[faster installation with uv]
AutoWhisper environment ready!

[doctor command runs]
```

## Benefits

1. **Instant package installation** - No waiting during `dpkg -i` or `apt install`
2. **Faster dependency installation** - uv is significantly faster than pip
3. **User choice** - Users can run setup immediately or wait for first use
4. **Transparent** - Setup happens automatically, no manual intervention required

## Manual Setup

Users who prefer to set up immediately after package install can run:

```bash
/usr/share/autowhisper/autowhisper-setup
```

This is useful for:
- Scripted/automated deployments
- Users who want to complete setup before going offline
- CI/CD pipelines
