# Patina -- Installer Build Guide

How to create distributable installers for Windows and macOS.

---

## Automated Releases (Recommended)

Push a version tag and GitHub Actions builds both installers automatically:

```bash
git tag v1.0.0
git push origin v1.0.0
```

This triggers `.github/workflows/release.yml` which:
1. Builds Windows VST3 + compiles Inno Setup installer
2. Builds macOS VST3 + AU + creates .pkg installer
3. Publishes both as a GitHub Release with download links

You can also trigger manually from the Actions tab using "Run workflow."

---

## Manual Build: Windows (.exe installer)

### Prerequisites

1. **Visual Studio 2022** (or Build Tools for VS 2022)
2. **CMake 3.22+**
3. **Inno Setup 6** -- [Download free](https://jrsoftware.org/isdl.php)

### Steps

```bash
# 1. Build the plugin (from project root)
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# 2. Compile the installer
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\windows\patina-installer.iss

# 3. Output
#    installer/output/Patina-1.0.0-Windows.exe
```

### What the Installer Does

- Copies `Patina.vst3` bundle to `C:\Program Files\Common Files\VST3\`
- Creates an uninstaller

---

## Manual Build: macOS (.pkg installer)

### Prerequisites

1. **Xcode 14+** (includes command-line tools)
2. **CMake 3.22+** -- `brew install cmake`

### Steps

```bash
# 1. Build the plugin (from project root)
cmake -B build -G Xcode
cmake --build build --config Release

# 2. Create the installer
chmod +x installer/macos/build-pkg.sh
./installer/macos/build-pkg.sh

# 3. Output
#    installer/output/Patina-1.0.0-macOS.pkg
```

### What the Installer Does

- Copies `Patina.vst3` to `/Library/Audio/Plug-Ins/VST3/`
- Copies `Patina.component` (AU) to `/Library/Audio/Plug-Ins/Components/`
- Shows branded welcome, license, and completion pages

Users choose which components to install (VST3, AU, or both).

### Code Signing (Optional)

Without signing, macOS Gatekeeper shows an "unidentified developer" warning. Users bypass it by right-clicking the .pkg and choosing Open.

To sign (requires [Apple Developer ID](https://developer.apple.com/programs/), $99/year):

```bash
productsign \
    --sign "Developer ID Installer: Futureproof Music School" \
    installer/output/Patina-1.0.0-macOS.pkg \
    installer/output/Patina-1.0.0-macOS-signed.pkg
```

---

## Version Bumps

When releasing a new version, update the version number in:

| File | What to Change |
|------|---------------|
| `CMakeLists.txt` | `project(Patina VERSION X.Y.Z)` |
| `installer/windows/patina-installer.iss` | `#define MyAppVersion "X.Y.Z"` |
| `installer/macos/build-pkg.sh` | `VERSION="X.Y.Z"` |
| `installer/macos/distribution.xml` | `version="X.Y.Z"` in all `pkg-ref` tags |

---

## Output Files

After building, distributable files appear in `installer/output/`:

| File | Platform | Size (approx) |
|------|----------|---------------|
| `Patina-1.0.0-Windows.exe` | Windows 10/11 (64-bit) | ~4 MB |
| `Patina-1.0.0-macOS.pkg` | macOS 12+ (Intel + Apple Silicon) | ~8 MB |

With GitHub Actions, these are automatically attached to the GitHub Release.

---

## Folder Structure

```
patina-plugin/
├── .github/
│   └── workflows/
│       └── release.yml             # CI/CD: auto-build on version tag
├── installer/
│   ├── windows/
│   │   └── patina-installer.iss    # Inno Setup script
│   ├── macos/
│   │   ├── build-pkg.sh            # Build script
│   │   ├── distribution.xml        # Component definitions
│   │   ├── welcome.html            # Welcome page (branded)
│   │   ├── license.html            # License agreement
│   │   └── conclusion.html         # Post-install page
│   ├── output/                     # Generated installers (gitignored)
│   └── README.md                   # This file
```
