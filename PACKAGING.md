# Debian Package Creation

## Quick Release

```bash
./scripts/release.sh patch   # 0.1.0 → 0.1.1
./scripts/release.sh minor   # 0.1.0 → 0.2.0
./scripts/release.sh major   # 0.1.0 → 1.0.0
```

Or use the Claude Code skill: `/release`

## Local Testing with pbuilder

Test builds locally before uploading to Launchpad (simulates their build environment):

### One-time Setup

```bash
# Install pbuilder
sudo apt install pbuilder

# Create Noble base image with all repos
sudo pbuilder create --distribution noble --components "main universe multiverse"

# (Optional) Allow pbuilder without password
echo "$(whoami) ALL=(root) NOPASSWD: /usr/sbin/pbuilder" | sudo tee /etc/sudoers.d/pbuilder
sudo chmod 440 /etc/sudoers.d/pbuilder
```

### Test a Build

```bash
# Build source package
rm -rf /tmp/aw-build && mkdir /tmp/aw-build && cd /tmp/aw-build
git clone /path/to/autowhisper autowhisper-0.1.0
cd autowhisper-0.1.0 && rm -rf .git && cd ..
tar czf autowhisper_0.1.0.orig.tar.gz autowhisper-0.1.0
cd autowhisper-0.1.0
debuild -S -sa -d -k<YOUR_GPG_KEY>

# Test with pbuilder
cd ..
sudo pbuilder build --buildresult ./result autowhisper_*.dsc

# If successful, upload to PPA
dput ppa:primemanifold/autowhisper autowhisper_*_source.changes
```

## Manual Build

```bash
sudo apt install -y debhelper devscripts dh-python pybuild-plugin-pyproject
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../autowhisper_*.deb && sudo apt install -f
```

## Package Structure

```
/usr/bin/autowhisper                        # CLI entry point
/usr/lib/python3/dist-packages/autowhisper/ # Python package
/usr/lib/systemd/user/autowhisper.service   # systemd user service
/etc/autowhisper/config.toml                # Configuration
/usr/share/doc/autowhisper/                 # Documentation
```

## PPA Upload

```bash
# Build and sign source package
debuild -S -sa -k<YOUR_GPG_KEY>

# Upload to PPA
dput ppa:primemanifold/autowhisper ../autowhisper_*_source.changes
```

Users install with:
```bash
sudo add-apt-repository ppa:primemanifold/autowhisper
sudo apt update
sudo apt install autowhisper
```

## Verify Package

```bash
dpkg -c ../autowhisper_*.deb  # List contents
dpkg -I ../autowhisper_*.deb  # Show info
lintian ../autowhisper_*.deb  # Check quality
```
