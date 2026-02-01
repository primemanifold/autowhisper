# Debian Package Creation

## Quick Build

```bash
sudo apt install -y debhelper devscripts build-essential python3 python3-pip python3-venv
./build-deb.sh
sudo dpkg -i ../autowhisper_*.deb && sudo apt install -f
```

## Package Structure

```
/usr/bin/autowhisper                      # Entry point
/opt/autowhisper/                         # Application + venv
/etc/autowhisper/config.toml              # Configuration
/lib/systemd/system/autowhisper@.service  # systemd service
/usr/share/autowhisper/download-models.sh # Model downloader
```

## Distribution Options

### Option 1: GitHub Releases (Easiest)

```bash
./build-deb.sh
gh release create v0.1.0 ../autowhisper_*.deb --title "AutoWhisper v0.1.0"
```

Users install with:
```bash
wget https://github.com/yourname/autowhisper/releases/latest/download/autowhisper_*.deb
sudo dpkg -i autowhisper_*.deb && sudo apt install -f
```

### Option 2: Launchpad PPA

1. Create account at https://launchpad.net
2. Setup GPG key: `gpg --full-generate-key && gpg --send-keys YOUR_KEY_ID`
3. Create PPA at https://launchpad.net/~/+activate-ppa
4. Upload:
```bash
debuild -S -sa
dput ppa:yourname/autowhisper ../autowhisper_*_source.changes
```

Users install with:
```bash
sudo add-apt-repository ppa:yourname/autowhisper
sudo apt install autowhisper
```

## Updating

```bash
dch -v 0.2.0-1 "New upstream release"
./build-deb.sh
```

## Verify Package

```bash
dpkg -c ../autowhisper_*.deb  # List contents
dpkg -I ../autowhisper_*.deb  # Show info
lintian ../autowhisper_*.deb  # Check quality
```
