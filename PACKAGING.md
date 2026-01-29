# Debian Package Creation Guide

This guide explains how to create a `.deb` package for AutoWhisper and optionally publish it to a PPA for easy `apt` installation.

---

## Quick Build

```bash
# Install build tools
sudo apt install -y debhelper devscripts build-essential python3 python3-pip python3-venv

# Build package
./build-deb.sh

# Install
sudo dpkg -i ../autowhisper_*.deb
sudo apt install -f  # Fix dependencies if needed
```

---

## Package Structure

```
autowhisper_0.1.0-1_amd64.deb
├── /usr/bin/autowhisper                      # Entry point script
├── /opt/autowhisper/                         # Application directory
│   ├── venv/                                 # Python virtual environment
│   └── src/                                  # Source code
├── /etc/autowhisper/config.toml              # Configuration
├── /lib/systemd/system/autowhisper@.service  # systemd service
├── /usr/share/autowhisper/
│   └── download-models.sh                    # Model downloader
└── /usr/share/doc/autowhisper/               # Documentation
    ├── README.md
    ├── QUICKSTART.md
    └── BUILD.md
```

---

## Building the .deb Package

### Prerequisites

```bash
# Install build dependencies
sudo apt install -y \
    debhelper \
    devscripts \
    build-essential \
    python3 \
    python3-pip \
    python3-venv \
    pkg-config \
    libssl-dev \
    libasound2-dev \
    portaudio19-dev \
    nvidia-cuda-toolkit
```

### Build Process

```bash
cd /home/isura/autowhisper

# Method 1: Use build script (recommended)
./build-deb.sh

# Method 2: Manual build
dpkg-buildpackage -us -uc -b

# Package will be created in parent directory:
# ../autowhisper_0.1.0-1_amd64.deb
```

### What Happens During Build

1. **Clean** - `debian/rules clean` cleans previous builds
2. **Build** - Creates Python virtual environment and installs dependencies
3. **Install** - Files copied to `debian/autowhisper/` staging area
4. **Package** - `.deb` created with proper permissions and metadata

---

## Testing the Package

### Local Installation

```bash
# Install package
sudo dpkg -i ../autowhisper_0.1.0-1_amd64.deb

# Fix any missing dependencies
sudo apt install -f

# Download model
sudo /usr/share/autowhisper/download-models.sh distil-large-v3

# Enable and start
sudo systemctl enable --now autowhisper@$USER

# Test
# Press Shift+Super and speak
```

### Clean Environment Testing

Use LXC/LXD for isolated testing:

```bash
# Create container
lxc launch ubuntu:24.04 test-autowhisper

# Copy package
lxc file push ../autowhisper_*.deb test-autowhisper/tmp/

# Install and test
lxc exec test-autowhisper -- bash
apt update
apt install -y /tmp/autowhisper_*.deb
/usr/share/autowhisper/download-models.sh distil-large-v3
systemctl enable autowhisper@root
# Test...
```

### Verify Package Contents

```bash
# List files
dpkg -c ../autowhisper_*.deb

# Show info
dpkg -I ../autowhisper_*.deb

# Extract without installing
dpkg-deb -x ../autowhisper_*.deb /tmp/extract
dpkg-deb -e ../autowhisper_*.deb /tmp/extract/DEBIAN
```

---

## Publishing to Launchpad PPA

### Prerequisites

1. **Launchpad Account** - Create at https://launchpad.net
2. **GPG Key** - For signing packages
3. **Ubuntu One Account** - For Launchpad auth

### Setup GPG Key

```bash
# Generate GPG key if you don't have one
gpg --full-generate-key
# Choose: RSA, 4096 bits, no expiration
# Enter your name and email (must match Launchpad)

# List keys
gpg --list-keys

# Upload to keyserver
gpg --send-keys YOUR_KEY_ID
```

### Setup Launchpad

1. Go to https://launchpad.net/~/+editpgpkeys
2. Paste your GPG fingerprint: `gpg --fingerprint YOUR_EMAIL`
3. Launchpad will send encrypted confirmation
4. Decrypt: `gpg -d confirmation_email.txt`
5. Paste confirmation code back to Launchpad

### Create PPA

1. Go to https://launchpad.net/~yourname/+activate-ppa
2. Create PPA: `autowhisper`
3. Note URL: `ppa:yourname/autowhisper`

### Build Source Package

```bash
# Update debian/changelog with your details
dch -i

# Build source package
debuild -S -sa

# This creates:
# ../autowhisper_0.1.0-1.dsc
# ../autowhisper_0.1.0-1.tar.xz
# ../autowhisper_0.1.0-1_source.changes
```

### Upload to PPA

```bash
# Upload source package
dput ppa:yourname/autowhisper ../autowhisper_0.1.0-1_source.changes

# Launchpad will:
# 1. Verify GPG signature
# 2. Build for multiple architectures
# 3. Publish to PPA
# 4. Send email notifications

# Check build status:
# https://launchpad.net/~yourname/+archive/ubuntu/autowhisper
```

### Users Can Then Install

```bash
sudo add-apt-repository ppa:yourname/autowhisper
sudo apt update
sudo apt install autowhisper
```

---

## Alternative: GitHub Releases

If you don't want to maintain a PPA, use GitHub Releases:

### Create Release Script

```bash
#!/bin/bash
# release.sh - Build and create GitHub release

VERSION="0.1.0"

# Build .deb
./build-deb.sh

# Create checksum
sha256sum ../autowhisper_${VERSION}-1_amd64.deb > ../autowhisper_${VERSION}-1_amd64.deb.sha256

# Use GitHub CLI to create release
gh release create v${VERSION} \
    ../autowhisper_${VERSION}-1_amd64.deb \
    ../autowhisper_${VERSION}-1_amd64.deb.sha256 \
    --title "AutoWhisper v${VERSION}" \
    --notes "GPU-accelerated voice-to-text daemon"
```

### Users Install via wget

```bash
# Download .deb from GitHub releases
wget https://github.com/yourname/autowhisper/releases/download/v0.1.0/autowhisper_0.1.0-1_amd64.deb

# Verify checksum
wget https://github.com/yourname/autowhisper/releases/download/v0.1.0/autowhisper_0.1.0-1_amd64.deb.sha256
sha256sum -c autowhisper_0.1.0-1_amd64.deb.sha256

# Install
sudo dpkg -i autowhisper_0.1.0-1_amd64.deb
sudo apt install -f
```

---

## Debian Package Best Practices

### Version Naming

```
autowhisper_<upstream>-<debian>_<arch>.deb

Examples:
- autowhisper_0.1.0-1_amd64.deb    # First Debian release
- autowhisper_0.1.0-2_amd64.deb    # Packaging fix
- autowhisper_0.1.1-1_amd64.deb    # New upstream version
```

### Changelog Format

```bash
# Update changelog
dch -v 0.1.1-1 "New upstream release"
dch -a "Added feature X"
dch -a "Fixed bug Y"
dch -r ""  # Mark as released
```

### Dependencies

**Build-Depends** - Needed to compile:
- python3, python3-pip, python3-venv
- libssl-dev, libasound2-dev, etc.

**Depends** - Needed at runtime:
- python3, libasound2
- xdotool, xclip
- nvidia-driver, cuda-toolkit

**Recommends** - Optional but suggested:
- pulseaudio

### File Locations (FHS Standard)

```
/usr/bin/                   # Entry point scripts
/opt/<package>/             # Self-contained applications
/etc/<package>/             # Configuration
/var/lib/<package>/         # Variable state data
/var/log/<package>/         # Logs
/usr/share/doc/<package>/   # Documentation
/lib/systemd/system/        # systemd units
```

---

## Updating the Package

### New Version

```bash
# Update version in pyproject.toml

# Update changelog
dch -v 0.2.0-1 "New upstream release"
dch -a "Added streaming mode"
dch -a "Improved GPU performance"
dch -r ""

# Build
./build-deb.sh

# Upload to PPA or GitHub
dput ppa:yourname/autowhisper ../autowhisper_0.2.0-1_source.changes
```

### Patch Release

```bash
# Packaging fix only (no code changes)
dch -v 0.1.0-2 "Fix systemd service permissions"
dch -r ""

# Build
./build-deb.sh
```

---

## Troubleshooting

### Build Fails

```bash
# Check build dependencies
dpkg-checkbuilddeps

# Install missing deps
sudo apt build-dep .

# Clean and retry
debian/rules clean
./build-deb.sh
```

### Lintian Errors

```bash
# Check package quality
lintian ../autowhisper_*.deb

# Fix common issues:
# - Missing copyright info → Update debian/copyright
# - Wrong permissions → Fix in debian/rules install
# - Missing manpage → Add debian/autowhisper.1
```

### PPA Upload Rejected

Common reasons:
- **GPG signature invalid** - Re-sign: `debsign -k YOUR_KEY ../autowhisper_*.changes`
- **Version already exists** - Bump version: `dch -i`
- **Missing orig tarball** - Build with `-sa`: `debuild -S -sa`
- **Wrong distribution** - Update `debian/changelog` codename

---

## Summary

**Best approach for distribution:**

1. **Short term**: GitHub Releases with .deb downloads
2. **Long term**: Launchpad PPA for `apt install autowhisper`

**File locations:**
- Package: `../autowhisper_0.1.0-1_amd64.deb`
- Build script: `./build-deb.sh`
- Debian files: `debian/` directory

**Users install with:**
```bash
# From PPA (best)
sudo add-apt-repository ppa:yourname/autowhisper
sudo apt install autowhisper

# From .deb file
sudo dpkg -i autowhisper_0.1.0-1_amd64.deb
sudo apt install -f
```

Ready to build your first package!
