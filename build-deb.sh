#!/bin/bash
set -e

echo "========================================"
echo "Building AutoWhisper .deb Package"
echo "========================================"
echo ""

# Check dependencies
echo "Checking build dependencies..."
if ! command -v dpkg-buildpackage &> /dev/null; then
    echo "Installing build dependencies..."
    sudo apt install -y debhelper devscripts build-essential
fi

# Clean previous builds
echo "Cleaning previous builds..."
rm -rf debian/autowhisper
rm -f ../autowhisper_*.deb ../autowhisper_*.changes ../autowhisper_*.buildinfo

# Build package
echo "Building package..."
dpkg-buildpackage -us -uc -b

echo ""
echo "========================================"
echo "Build Complete!"
echo "========================================"
echo ""
echo "Package created:"
ls -lh ../autowhisper_*.deb
echo ""
echo "To install:"
echo "  sudo dpkg -i ../autowhisper_*.deb"
echo "  sudo apt install -f  # Fix any dependency issues"
echo ""
echo "Or test in clean environment:"
echo "  lxc launch ubuntu:24.04 test-autowhisper"
echo "  lxc file push ../autowhisper_*.deb test-autowhisper/tmp/"
echo "  lxc exec test-autowhisper -- apt install /tmp/autowhisper_*.deb"
echo ""
