# PsyMelody

**Psytrance MIDI Generator Plugin (VST3 / AU) by EDEN**

PsyMelody is a MIDI generator plugin for Psytrance music production. It generates melodies, basslines, and chord voicings based on scales, chord progressions, and rhythm patterns characteristic of Psytrance subgenres.

![PsyMelody Screenshot](docs/images/PsyMelody.png)

## Download

**[Download the latest release](https://github.com/efirio-git/PsyMelody/releases/latest)**

## Features

- **Melody / Bassline / Chord Voicing** generation
- **Subgenres**: Goa, Full-On, Dark Psy, Progressive Psy
- **Seed lock** — reproduce the exact same phrase, tweak one parameter at a time
- **Humanize / Swing** knobs for natural timing and shuffle feel
- **Partial regeneration** — new pitches (keep rhythm) or new rhythm (keep pitches)
- **Euclidean rhythm mode** driven by the Density knob
- **Built-in piano roll** editor with undo/redo and velocity / pan / pitch-bend lanes
- **Drag & drop MIDI export** straight into your DAW, plus file export / import
- **Preview synth** (Saw / Square / Sine / Triangle)
- **Preset system** with save/load
- **DAW BPM sync**, English / Japanese UI with tooltips on every control

## System Requirements

### macOS
- macOS 10.15 (Catalina) or later
- Apple Silicon or Intel (Universal Binary)
- VST3 or AU compatible DAW

### Windows
- Windows 10 / 11, 64-bit
- A VST3 compatible DAW (FL Studio, Ableton Live, Cubase, Studio One, Bitwig, REAPER, ...)

## Installation

### macOS

1. Open the downloaded `PsyMelody_vX.Y.Z_Mac.dmg`
2. Run `PsyMelody_Installer.pkg`
3. Click **Customize** to select VST3 and/or AU
4. Restart your DAW and rescan plugins

### Windows

1. Download `PsyMelody_vX.Y.Z_Win.zip`
2. Right-click the zip and choose **Extract All**
3. In the extracted folder, double-click `Install_PsyMelody.bat`
4. Allow administrator permission when Windows asks
5. Restart your DAW and rescan plugins

The Windows build is VST3 only (no AU, no Standalone) and is not code-signed, so Windows may show
"The publisher could not be verified" or SmartScreen's "Windows protected your PC" when you run the
installer — click **Run** (or **More info** > **Run anyway**) to continue. You can verify the
download against the SHA256 published on the [release page](https://github.com/efirio-git/PsyMelody/releases/latest).

See [Installation Guide](docs/Installation_Guide.md) for DAW-specific setup instructions.
Full feature documentation lives in the [specification](docs/SPEC.md) (Japanese).

## Building from Source

### Requirements
- CMake 3.22+
- C++17 compiler
- [JUCE Framework](https://juce.com/)

### Build (macOS)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Build outputs are in `build/PsyMelody_artefacts/Release/`.

### Build (Windows)

Requires Visual Studio 2022 Build Tools (with the Windows SDK) in addition to CMake.

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Build outputs are in `build\PsyMelody_artefacts\Release\`. JUCE drops AU automatically on Windows.

## License

This project is licensed under the [GNU Affero General Public License v3.0](LICENSE).

PsyMelody is built with the [JUCE Framework](https://juce.com/) under the AGPLv3.

## Author

Developed by **EDEN**
