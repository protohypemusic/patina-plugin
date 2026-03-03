#!/bin/bash
# ================================================================
# Patina - macOS .pkg Installer Builder
#
# Usage:
#   1. Build Patina:
#      cmake -B build -G Xcode
#      cmake --build build --config Release
#   2. Run this script:
#      chmod +x installer/macos/build-pkg.sh
#      ./installer/macos/build-pkg.sh
#   3. Output: installer/output/Patina-1.0.0-macOS.pkg
# ================================================================

set -euo pipefail

VERSION="1.0.0"
IDENTIFIER="com.futureproofmusic.patina"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/Patina_artefacts/Release"
OUTPUT_DIR="$PROJECT_DIR/installer/output"
STAGING="$PROJECT_DIR/installer/macos/.staging"

echo "=== Patina macOS Installer Builder ==="
echo "Version: $VERSION"
echo ""

# ---- Verify build artifacts exist ----
VST3_PATH="$BUILD_DIR/VST3/Patina.vst3"
AU_PATH="$BUILD_DIR/AU/Patina.component"

FOUND_SOMETHING=false

if [ -d "$VST3_PATH" ]; then
    echo "[OK] Found VST3: $VST3_PATH"
    FOUND_SOMETHING=true
else
    echo "[--] VST3 not found (skipping)"
fi

if [ -d "$AU_PATH" ]; then
    echo "[OK] Found AU:   $AU_PATH"
    FOUND_SOMETHING=true
else
    echo "[--] AU not found (skipping)"
fi

if [ "$FOUND_SOMETHING" = false ]; then
    echo ""
    echo "ERROR: No build artifacts found."
    echo "Build first: cmake -B build -G Xcode && cmake --build build --config Release"
    exit 1
fi

echo ""

# ---- Clean staging area ----
rm -rf "$STAGING"
mkdir -p "$STAGING"
mkdir -p "$OUTPUT_DIR"

# ---- Build component packages ----
COMPONENT_PKGS=()

# VST3
if [ -d "$VST3_PATH" ]; then
    echo "Packaging VST3..."
    mkdir -p "$STAGING/vst3-root/Library/Audio/Plug-Ins/VST3"
    cp -R "$VST3_PATH" "$STAGING/vst3-root/Library/Audio/Plug-Ins/VST3/"

    pkgbuild \
        --root "$STAGING/vst3-root" \
        --identifier "$IDENTIFIER.vst3" \
        --version "$VERSION" \
        --install-location "/" \
        "$STAGING/patina-vst3.pkg"

    COMPONENT_PKGS+=("$STAGING/patina-vst3.pkg")
    echo "  -> patina-vst3.pkg"
fi

# AU
if [ -d "$AU_PATH" ]; then
    echo "Packaging AU..."
    mkdir -p "$STAGING/au-root/Library/Audio/Plug-Ins/Components"
    cp -R "$AU_PATH" "$STAGING/au-root/Library/Audio/Plug-Ins/Components/"

    pkgbuild \
        --root "$STAGING/au-root" \
        --identifier "$IDENTIFIER.au" \
        --version "$VERSION" \
        --install-location "/" \
        "$STAGING/patina-au.pkg"

    COMPONENT_PKGS+=("$STAGING/patina-au.pkg")
    echo "  -> patina-au.pkg"
fi

echo ""

# ---- Build distribution package ----
echo "Building distribution package..."

# Build the --package-path arguments
PKG_ARGS=""
for pkg in "${COMPONENT_PKGS[@]}"; do
    PKG_ARGS="$PKG_ARGS --package-path $(dirname "$pkg")"
done

productbuild \
    --distribution "$SCRIPT_DIR/distribution.xml" \
    --resources "$SCRIPT_DIR" \
    $PKG_ARGS \
    "$OUTPUT_DIR/Patina-$VERSION-macOS.pkg"

echo ""
echo "=== Done! ==="
echo "Installer: $OUTPUT_DIR/Patina-$VERSION-macOS.pkg"
echo ""

# ---- Clean up staging ----
rm -rf "$STAGING"

echo "To install: double-click the .pkg file"
echo "Note: macOS may show an 'unidentified developer' warning."
echo "      Right-click -> Open to bypass this."
