# Patina

An all-in-one effect plugin by [Futureproof](https://futureproofmusicschool.com).

5 effect modules, presets, a randomize system, and a spectral bloom visualizer.

**Free download for everyone.**

## Modules

- **NOISE** -- Vinyl crackle, tape hiss, static (White / Pink / Crackle)
- **WOBBLE** -- Volume tremolo with tempo sync and sine-to-square shape control
- **DISTORT** -- Saturation from warm to aggressive (Soft / Hard / Diode / Fold / Crush)
- **RESONATOR** -- Comb, modal, or formant coloring with frequency and resonance control
- **SPACE** -- Reverb and delay (Plate / Room / Hall / Tonal)

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
git clone --recursive https://github.com/protohypemusic/patina-plugin.git
cd patina-plugin
cmake -B build
cmake --build build --config Release
```

See [installer/README.md](installer/README.md) for creating distributable installers.

## Releasing

Push a version tag to auto-build installers for both platforms:

```bash
git tag v1.1.0
git push origin v1.1.0
```

GitHub Actions builds the Windows .exe and macOS .pkg installers and publishes them as a Release.

## License

Free to use for personal and commercial music production. See the [license](installer/macos/license.html) for full terms.

Made by [Futureproof](https://futureproofmusicschool.com).
