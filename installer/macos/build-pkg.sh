#!/bin/bash
# ============================================================
# Patina macOS Installer Builder
#
# Creates a .pkg installer from the Release build artifacts.
#
# Prerequisites:
#   cmake -B build -G Xcode
#   cmake --build build --config Release
#
# Usage:
#   chmod +x build-pkg.sh
#   ./build-pkg.sh
#
# Output:
#   ../output/Patina-1.0.0-macOS.pkg
# ============================================================

set -euo pipefail

VERSION="1.0.0"
IDENTIFIER="com.futureproofmusic.patina"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build/Patina_artefacts/Release"
OUTPUT_DIR="$SCRIPT_DIR/../output"
STAGING="$SCRIPT_DIR/.staging"

echo "=== Patina macOS Installer Builder ==="
echo "Version: $VERSION"
echo "Build root: $BUILD_ROOT"
echo ""

# ---- Verify build artifacts exist ----

VST3_PATH="$BUILD_ROOT/VST3/Patina.vst3"
AU_PATH="$BUILD_ROOT/AU/Patina.component"

MISSING=0

if [ ! -d "$VST3_PATH" ]; then
    echo "WARNING: VST3 not found at $VST3_PATH"
    MISSING=$((MISSING + 1))
fi

if [ ! -d "$AU_PATH" ]; then
    echo "WARNING: AU not found at $AU_PATH"
    MISSING=$((MISSING + 1))
fi

if [ $MISSING -eq 2 ]; then
    echo ""
    echo "ERROR: No build artifacts found. Build the plugin first:"
    echo "  cmake -B build -G Xcode"
    echo "  cmake --build build --config Release"
    exit 1
fi

# ---- Clean staging ----

rm -rf "$STAGING"
mkdir -p "$STAGING"
mkdir -p "$OUTPUT_DIR"

# ---- Stage: VST3 ----

if [ -d "$VST3_PATH" ]; then
    echo "Staging VST3..."
    VST3_STAGE="$STAGING/vst3"
    mkdir -p "$VST3_STAGE/Library/Audio/Plug-Ins/VST3"
    cp -R "$VST3_PATH" "$VST3_STAGE/Library/Audio/Plug-Ins/VST3/"

    pkgbuild \
        --root "$VST3_STAGE" \
        --identifier "${IDENTIFIER}.vst3" \
        --version "$VERSION" \
        --install-location "/" \
        "$STAGING/patina-vst3.pkg"

    echo "  -> patina-vst3.pkg"
fi

# ---- Stage: AU ----

if [ -d "$AU_PATH" ]; then
    echo "Staging AU..."
    AU_STAGE="$STAGING/au"
    mkdir -p "$AU_STAGE/Library/Audio/Plug-Ins/Components"
    cp -R "$AU_PATH" "$AU_STAGE/Library/Audio/Plug-Ins/Components/"

    pkgbuild \
        --root "$AU_STAGE" \
        --identifier "${IDENTIFIER}.au" \
        --version "$VERSION" \
        --install-location "/" \
        "$STAGING/patina-au.pkg"

    echo "  -> patina-au.pkg"
fi

# ---- Build combined installer ----

echo ""
echo "Building combined installer..."

# Collect available component packages
PKGS=()
for pkg in "$STAGING"/patina-*.pkg; do
    [ -f "$pkg" ] && PKGS+=("--package" "$pkg")
done

OUTPUT_FILE="$OUTPUT_DIR/Patina-${VERSION}-macOS.pkg"

productbuild \
    --distribution "$SCRIPT_DIR/distribution.xml" \
    --resources "$SCRIPT_DIR" \
    "${PKGS[@]}" \
    "$OUTPUT_FILE"

echo ""
echo "=== Done! ==="
echo "Installer: $OUTPUT_FILE"
echo "Size: $(du -h "$OUTPUT_FILE" | cut -f1)"
echo ""
echo "To sign (optional, requires Apple Developer ID):"
echo "  productsign --sign \"Developer ID Installer: Futureproof Music School\" \\"
echo "      \"$OUTPUT_FILE\" \"$OUTPUT_DIR/Patina-${VERSION}-macOS-signed.pkg\""

# ---- Cleanup staging ----

rm -rf "$STAGING"
