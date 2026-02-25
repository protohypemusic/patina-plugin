# Patina

A lo-fi texture plugin by [Futureproof Music School](https://futureproofmusicschool.com).

6 processing modules, 12 presets, a randomize system, and a spectral bloom visualizer.

**Free download for everyone.**

## Modules

- **NOISE** -- Vinyl crackle, tape hiss, static
- **WOBBLE** -- Volume tremolo with adjustable rate
- **DISTORT** -- Continuous saturation (soft to hard clipping)
- **SPACE** -- Reverb with independent decay control
- **FLUX** -- Beat repeat / glitch across the full tempo range
- **FILTER** -- High/low cut shaping

## Download

Grab the latest release from the [Releases page](../../releases/latest).

| Platform | Format | Install Location |
|----------|--------|-----------------|
| Windows | VST3 | `C:\Program Files\Common Files\VST3\` |
| macOS | VST3 | `/Library/Audio/Plug-Ins/VST3/` |
| macOS | AU | `/Library/Audio/Plug-Ins/Components/` |

After installing, open your DAW, rescan plugins, and find **Patina** in your effects list.

## Building from Source

Requires CMake 3.22+ and a C++17 compiler (Visual Studio 2022 or Xcode 14+).

```bash
git clone --recursive https://github.com/FutureproofMusicSchool/patina-plugin.git
cd patina-plugin
cmake -B build
cmake --build build --config Release
```

See [installer/README.md](installer/README.md) for creating distributable installers.

## Releasing

Push a version tag to auto-build installers for both platforms:

```bash
git tag v1.0.0
git push origin v1.0.0
```

GitHub Actions builds the Windows .exe and macOS .pkg installers and publishes them as a Release.

## License

Free to use for personal and commercial music production. See the [license](installer/macos/license.html) for full terms.

Made by [Futureproof Music School](https://futureproofmusicschool.com).
