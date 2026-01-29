# AutoWhisper Distribution Guide

## ✅ Debian Package Ready!

Your project now has complete Debian packaging support for distribution via `apt`.

---

## Quick Start: Build Your First .deb

```bash
cd /home/isura/autowhisper

# Build the package
./build-deb.sh

# Test install
sudo dpkg -i ../autowhisper_0.1.0-1_amd64.deb
sudo /usr/share/autowhisper/download-models.sh small
sudo systemctl enable --now autowhisper@$USER
```

---

## Distribution Options

### Option 1: Launchpad PPA (Recommended) ⭐

**Users install with:**
```bash
sudo add-apt-repository ppa:yourname/autowhisper
sudo apt update
sudo apt install autowhisper
```

**Pros:**
- Official Ubuntu distribution method
- Automatic updates via `apt upgrade`
- Built for multiple Ubuntu versions
- Signed packages (security)

**Setup time:** ~1 hour (first time)

See [PACKAGING.md](PACKAGING.md) section "Publishing to Launchpad PPA"

---

### Option 2: GitHub Releases (Easiest)

**Users install with:**
```bash
wget https://github.com/yourname/autowhisper/releases/latest/download/autowhisper_0.1.0-1_amd64.deb
sudo dpkg -i autowhisper_0.1.0-1_amd64.deb
sudo apt install -f
```

**Pros:**
- Quick setup (5 minutes)
- No additional infrastructure
- Download statistics

**Setup:**
```bash
# 1. Build package
./build-deb.sh

# 2. Create GitHub release
gh release create v0.1.0 \
    ../autowhisper_0.1.0-1_amd64.deb \
    --title "AutoWhisper v0.1.0" \
    --notes "GPU-accelerated voice-to-text daemon"
```

---

### Option 3: Custom APT Repository

**Users install with:**
```bash
curl -fsSL https://yourdomain.com/autowhisper/KEY.gpg | sudo gpg --dearmor -o /usr/share/keyrings/autowhisper.gpg
echo "deb [signed-by=/usr/share/keyrings/autowhisper.gpg] https://yourdomain.com/autowhisper noble main" | sudo tee /etc/apt/sources.list.d/autowhisper.list
sudo apt update
sudo apt install autowhisper
```

**Pros:**
- Full control
- Can host on your own domain
- Works with GitHub Pages (free)

See [PACKAGING.md](PACKAGING.md) section "Custom APT Repository"

---

## What's Included

### Debian Package Files Created

```
debian/
├── changelog         # Version history
├── control           # Package metadata & dependencies
├── rules             # Build instructions
├── postinst          # Post-installation script
├── prerm             # Pre-removal script
├── postrm            # Post-removal script
├── copyright         # License information
├── compat            # Debhelper compatibility level
└── download-models.sh # Whisper model downloader
```

### Package Contents

When users install, they get:

```
/usr/bin/autowhisperd                     # Main binary
/etc/autowhisper/config.toml              # Configuration
/lib/systemd/system/autowhisper@.service  # systemd service
/usr/share/autowhisper/
  ├── models/                             # Model storage
  └── download-models.sh                  # Model downloader
/usr/share/doc/autowhisper/               # Documentation
  ├── README.md
  ├── QUICKSTART.md
  ├── BUILD.md
  └── PERFORMANCE.md
```

---

## User Installation Experience

### From PPA (Best Experience)

```bash
# Step 1: Add PPA
sudo add-apt-repository ppa:yourname/autowhisper
sudo apt update

# Step 2: Install
sudo apt install autowhisper

# Step 3: Download model (one-time)
sudo /usr/share/autowhisper/download-models.sh small

# Step 4: Enable service
sudo systemctl enable --now autowhisper@$USER

# Step 5: Test
# Press Ctrl+Alt+V and speak!
```

**Total time:** ~2-3 minutes

### From .deb File

```bash
# Step 1: Download
wget https://github.com/yourname/autowhisper/releases/latest/download/autowhisper_0.1.0-1_amd64.deb

# Step 2: Install
sudo dpkg -i autowhisper_0.1.0-1_amd64.deb
sudo apt install -f  # Fix dependencies

# Step 3: Download model
sudo /usr/share/autowhisper/download-models.sh small

# Step 4: Enable
sudo systemctl enable --now autowhisper@$USER
```

**Total time:** ~3-4 minutes

---

## Before Publishing

### 1. Update Package Metadata

Edit [debian/control](debian/control):
```
Maintainer: Your Name <your.email@example.com>
Homepage: https://github.com/yourname/autowhisper
Vcs-Browser: https://github.com/yourname/autowhisper
```

Edit [debian/changelog](debian/changelog):
```
autowhisper (0.1.0-1) noble; urgency=medium

  * Initial release
  ...

 -- Your Name <your.email@example.com>  Tue, 28 Jan 2025 11:00:00 +0000
```

Edit [debian/copyright](debian/copyright):
```
Upstream-Contact: Your Name <your.email@example.com>
Source: https://github.com/yourname/autowhisper
```

### 2. Test Build

```bash
# Clean build
./build-deb.sh

# Verify package
dpkg -c ../autowhisper_*.deb | head -20
dpkg -I ../autowhisper_*.deb

# Check quality
lintian ../autowhisper_*.deb
```

### 3. Test Installation

```bash
# In clean container
lxc launch ubuntu:24.04 test
lxc file push ../autowhisper_*.deb test/tmp/
lxc exec test -- apt install -y /tmp/autowhisper_*.deb
lxc exec test -- systemctl status autowhisper@root
```

---

## Publishing Checklist

- [ ] Update `debian/control` with your details
- [ ] Update `debian/changelog` with your details
- [ ] Update `debian/copyright` with your info
- [ ] Add LICENSE file (already created: Apache 2.0)
- [ ] Test build: `./build-deb.sh`
- [ ] Test install in clean environment
- [ ] Create GitHub repository
- [ ] Tag release: `git tag v0.1.0`
- [ ] Push tags: `git push --tags`
- [ ] Choose distribution method (PPA/Releases/Custom)
- [ ] Update README with installation instructions

---

## Recommended Workflow

### For Open Source Project

1. **Create GitHub repo**
   ```bash
   gh repo create autowhisper --public --description "GPU-accelerated voice-to-text daemon"
   git remote add origin git@github.com:yourname/autowhisper.git
   git push -u origin main
   ```

2. **Setup PPA** (one-time, ~1 hour)
   - Create Launchpad account
   - Setup GPG key
   - Create PPA: `ppa:yourname/autowhisper`

3. **For each release:**
   ```bash
   # 1. Update version
   dch -v 0.2.0-1 "New features..."

   # 2. Build and test locally
   ./build-deb.sh
   sudo dpkg -i ../autowhisper_*.deb

   # 3. Build source package
   debuild -S -sa

   # 4. Upload to PPA
   dput ppa:yourname/autowhisper ../autowhisper_*_source.changes

   # 5. Create GitHub release
   gh release create v0.2.0 ../autowhisper_*_amd64.deb
   ```

### For Personal/Private Use

1. **Build locally**
   ```bash
   ./build-deb.sh
   ```

2. **Install on your machines**
   ```bash
   scp ../autowhisper_*.deb server1:/tmp/
   ssh server1 "sudo dpkg -i /tmp/autowhisper_*.deb"
   ```

---

## Updating the Package

### New Feature Release

```bash
# Update code
# ...

# Update version
dch -v 0.2.0-1 "New upstream release"
dch -a "Added streaming mode"
dch -a "Improved GPU performance"

# Build
./build-deb.sh

# Publish
dput ppa:yourname/autowhisper ../autowhisper_*_source.changes
gh release create v0.2.0 ../autowhisper_*_amd64.deb
```

### Bug Fix

```bash
# Fix bug
# ...

# Increment Debian revision
dch -v 0.1.0-2 "Fix systemd service permissions"

# Build & publish
./build-deb.sh
```

---

## Support Multiple Ubuntu Versions

Build for different Ubuntu releases:

```bash
# Ubuntu 24.04 (Noble)
dch -b -D noble -v 0.1.0-1noble "Build for Noble"
debuild -S -sa
dput ppa:yourname/autowhisper ../autowhisper_*noble*_source.changes

# Ubuntu 22.04 (Jammy)
dch -b -D jammy -v 0.1.0-1jammy "Build for Jammy"
debuild -S -sa
dput ppa:yourname/autowhisper ../autowhisper_*jammy*_source.changes
```

---

## Monitoring

### PPA Status

Check build status:
- https://launchpad.net/~yourname/+archive/ubuntu/autowhisper

### Download Stats

GitHub Releases shows:
- Download count per release
- Geographic distribution
- Traffic sources

---

## Summary

✅ **Complete Debian packaging** - Ready to distribute
✅ **Three distribution methods** - PPA, GitHub, Custom
✅ **Professional structure** - Follows Debian standards
✅ **Easy user installation** - `apt install autowhisper`
✅ **Automatic updates** - Via `apt upgrade` (PPA)
✅ **Documentation** - Comprehensive guides

**Next step:** Choose your distribution method and update the metadata with your details!

For detailed instructions, see [PACKAGING.md](PACKAGING.md).
