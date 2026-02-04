# Debian Package Testing Guide

This document describes how to test the autowhisper Debian package before releasing to Launchpad PPA.

## Prerequisites

```bash
# Install pbuilder if not already installed
sudo apt install pbuilder

# Create a noble chroot (one-time setup)
sudo pbuilder create --distribution noble
```

## Testing Process

### Step 1: Clean Build Artifacts

```bash
cd /home/isura/autowhisper
rm -rf debian/autowhisper debian/.debhelper debian/*.debhelper \
       debian/files debian/autowhisper.substvars debian/debhelper-build-stamp \
       .pybuild build
```

### Step 2: Build Source Package

```bash
# Create orig tarball (if needed)
git archive --format=tar.gz --prefix=autowhisper-$(dpkg-parsechangelog -S Version | cut -d- -f1)/ \
    HEAD -o ../autowhisper_$(dpkg-parsechangelog -S Version | cut -d- -f1).orig.tar.gz

# Build source package (unsigned for testing)
debuild -S -d --no-sign
```

**Check source tarball size** - should be < 1MB:
```bash
ls -lh ../autowhisper_*.tar.xz
```

If tarball is > 100MB, check `debian/source/options` for missing exclusions.

### Step 3: Test Build in Clean Chroot (pbuilder)

This is the most important test - it verifies the package builds in a clean environment:

```bash
sudo pbuilder build ../autowhisper_*.dsc
```

**Expected output:**
- All build dependencies resolve
- Package builds successfully
- Output: `dpkg-deb: building package 'autowhisper' in '../autowhisper_*_all.deb'`

**Built package location:** `/var/cache/pbuilder/result/`

### Step 4: Run Lintian

Check for packaging issues:

```bash
lintian /var/cache/pbuilder/result/autowhisper_*_all.deb
```

**Acceptable warnings:**
- `maintainer-script-calls-systemctl` - Expected (we stop service on remove)
- `no-manual-page` - Nice to have, not critical
- `copyright-not-using-common-license-for-apache2` - Minor formatting issue

**Errors that must be fixed:**
- Missing dependencies
- Syntax errors in maintainer scripts
- File permission issues

### Step 5: Test Dependency Resolution

Verify the package can be installed:

```bash
sudo apt-get install --dry-run /var/cache/pbuilder/result/autowhisper_*_all.deb
```

**Expected:** Should show packages to install, no errors.

### Step 6: Test Local Installation (Optional)

For full integration testing:

```bash
# Install the package
sudo apt install /var/cache/pbuilder/result/autowhisper_*_all.deb

# Verify venv was created
ls -la /opt/autowhisper/venv/

# Verify service can start
systemctl --user start autowhisper
systemctl --user status autowhisper

# Verify CLI works
autowhisper doctor

# Uninstall
sudo apt remove autowhisper

# Purge (removes venv and config)
sudo apt purge autowhisper
```

## Quick Test Commands

One-liner for quick testing:

```bash
# Full test: clean, build source, pbuilder, lintian
rm -rf .pybuild build debian/autowhisper && \
debuild -S -d --no-sign && \
sudo pbuilder build ../autowhisper_*.dsc && \
lintian /var/cache/pbuilder/result/autowhisper_*_all.deb
```

## Common Issues

### Source tarball too large (> 100MB)

Check `debian/source/options` for missing exclusions:
```
tar-ignore=.hotkey-venv
tar-ignore=venv
tar-ignore=*.deb
tar-ignore=__pycache__
```

### Build fails in pbuilder

1. Check Build-Depends in `debian/control`
2. Run locally first: `dpkg-buildpackage -us -uc -b`
3. Check build logs in pbuilder output

### postinst fails

Test the script manually:
```bash
sudo bash -x debian/postinst configure
```

### Dependency resolution fails

Check `debian/control` Depends field:
- Are package names correct for Ubuntu noble?
- Use `apt-cache policy <package>` to verify availability

## Release Checklist

Before uploading to Launchpad:

- [ ] Source tarball < 1MB
- [ ] pbuilder build succeeds
- [ ] Lintian shows no errors (warnings OK)
- [ ] Dependency resolution works (dry-run)
- [ ] Changelog version is correct
- [ ] requirements.lock is up to date

## Uploading to Launchpad

After all tests pass:

```bash
# Sign the source package
debuild -S -sa

# Upload to PPA
dput ppa:YOUR-PPA-NAME ../autowhisper_*_source.changes
```
