# PsyMelody v0.1.2 - Installation Guide
## Psytrance Melody Generator by EDEN

---

## Installation

### Using the Installer (Recommended)

1. Open `PsyMelody_v0.1.2.dmg`
2. Double-click `PsyMelody_Installer.pkg`
3. Follow the on-screen instructions
4. Click **Customize** to select which formats to install:
   - **VST3 Plugin** → `/Library/Audio/Plug-Ins/VST3/` (FL Studio, Ableton, Cubase, etc.)
   - **Audio Unit Plugin** → `/Library/Audio/Plug-Ins/Components/` (Logic Pro, GarageBand)
5. Click Install
6. Restart your DAW

### Uninstalling

Run `Uninstall_PsyMelody.command` included in the DMG.

### Manual Installation

Copy the plugin files to the following locations:

| Format | Copy To |
|--------|---------|
| **PsyMelody.vst3** | `/Library/Audio/Plug-Ins/VST3/` |
| **PsyMelody.component** | `/Library/Audio/Plug-Ins/Components/` |

---

## DAW Setup Guide

---

### FL Studio

#### Plugin Scan
1. Open FL Studio
2. Go to **Options** > **Manage plugins** (or press F10)
3. Click **"Find plugins"** button to scan
4. Wait for the scan to complete
5. PsyMelody should appear as **Synth** type

#### Adding PsyMelody
1. Open the **Channel Rack**
2. Click the **+** button at the bottom
3. Select **PsyMelody** from the plugin list

#### MIDI Routing (to hear sound through external synth)
1. Add PsyMelody to the Channel Rack
2. Add a synth (e.g., Serum, Vital, TB-303) to the Channel Rack
3. Open PsyMelody's channel settings (gear icon) > **MIDI** tab > Set **Port** output to **1**
4. Open the synth's channel settings > **MIDI** tab > Set **Port** input to **1**
5. Click **Generate** in PsyMelody, then press Play

#### Recommended Workflow
1. Generate a melody in PsyMelody
2. Click **Export MIDI** or **Quick Save**
3. Drag the `.mid` file into a Pattern's Piano Roll
4. Route the pattern to your preferred synth
5. Close PsyMelody to save CPU resources

---

### Ableton Live

#### Plugin Scan
1. Open Ableton Live
2. Go to **Preferences** > **Plug-Ins**
3. Ensure **Use VST3 Plug-In System Folder** is **On**
4. Click **Rescan** if PsyMelody doesn't appear

#### Adding PsyMelody
1. In the **Browser** on the left, go to **Plug-Ins** > **VST3**
2. Find **PsyMelody**
3. Drag it onto a **MIDI Track**

#### MIDI Routing
1. Create a MIDI track with PsyMelody
2. Create another MIDI track with your synth
3. On the synth track, set **MIDI From** to **PsyMelody**
4. Set Monitor to **In**
5. Arm the synth track

#### Recommended Workflow
1. Generate a melody in PsyMelody
2. Export as MIDI file
3. Drag `.mid` into an empty MIDI clip slot
4. Route to your preferred synth

---

### Logic Pro

> **Note:** Logic Pro uses **Audio Unit (AU)** format, not VST3.
> Make sure PsyMelody.component is installed.

#### Plugin Scan
1. Open Logic Pro
2. Go to **Logic Pro** > **Settings** > **Plug-In Manager**
3. Click **Reset & Rescan Selection** if PsyMelody doesn't appear
4. Ensure PsyMelody is checked (not disabled)

#### Adding PsyMelody
1. Create a new **Software Instrument** track
2. Click the **Instrument** slot
3. Select **AU Instruments** > **EDEN** > **PsyMelody**

#### MIDI Routing
1. PsyMelody will output MIDI on the same track
2. To route to another instrument:
   - Use the **Export MIDI** function
   - Drag the `.mid` file onto another track

#### Recommended Workflow
1. Generate a melody in PsyMelody
2. Export MIDI
3. Drag onto a track with your preferred synth
4. Remove PsyMelody from the project to save CPU

---

### Cubase / Nuendo

#### Plugin Scan
1. Open Cubase
2. Go to **Studio** > **VST Plug-in Manager**
3. Click **Rescan All**
4. PsyMelody should appear under **EDEN**

#### Adding PsyMelody
1. Create a new **Instrument Track**
2. Select **PsyMelody** from the VST Instruments list

#### MIDI Routing
1. Create a MIDI track
2. Set the MIDI track's output to PsyMelody
3. Create another instrument track with your synth
4. Route MIDI from PsyMelody to the synth

#### Recommended Workflow
1. Generate a melody in PsyMelody
2. Use **Export MIDI** to save
3. Import the `.mid` file into a MIDI/Instrument track
4. Assign to your preferred synth

---

### Studio One

#### Plugin Scan
1. Open Studio One
2. Go to **Studio One** > **Options** > **Locations** > **VST Plug-Ins**
3. Ensure `/Library/Audio/Plug-Ins/VST3` is in the list
4. Click **Rescan**

#### Adding PsyMelody
1. Open the **Browser** panel
2. Go to **Instruments** > **EDEN** > **PsyMelody**
3. Drag onto an empty track

---

### Bitwig Studio

#### Plugin Scan
1. Open Bitwig Studio
2. Go to **Settings** > **Plug-ins** > **Locations**
3. Ensure VST3 system path is included
4. Click **Rescan**

#### Adding PsyMelody
1. Click **+** to add a device
2. Search for **PsyMelody**
3. Add to an instrument track

---

## Preview Synth

PsyMelody includes a built-in preview synthesizer so you can hear melodies without an external synth:

1. Click the **▶ PREVIEW** toggle in the footer bar
2. Select an oscillator waveform from the **OSC** dropdown: **Saw** / **Square** / **Sine** / **Triangle**
3. Adjust volume with the **speaker icon slider**
4. Press Play in your DAW

> **Note:** The preview synth is for auditioning only.
> For production, export MIDI and use a dedicated synth.

---

## Troubleshooting

### Plugin doesn't appear after installation
- Restart your DAW
- Rescan plugins in your DAW's plugin manager
- Check that the file is in the correct folder:
  - VST3: `/Library/Audio/Plug-Ins/VST3/PsyMelody.vst3`
  - AU: `/Library/Audio/Plug-Ins/Components/PsyMelody.component`

### "Developer cannot be verified" warning on macOS
- Right-click the plugin file > **Open**
- Or: **System Settings** > **Privacy & Security** > Click **Open Anyway**

### Plugin shows as "error" in FL Studio
1. Close FL Studio
2. Delete the cached plugin info:
   ```
   ~/Documents/Image-Line/FL Studio/Presets/Plugin database/Installed/Generators/VST3/PsyMelody.nfo
   ~/Documents/Image-Line/FL Studio/Presets/Plugin database/Installed/Generators/VST3/PsyMelody.fst
   ```
3. Restart FL Studio and rescan

### No sound from PsyMelody
- PsyMelody generates MIDI, not audio
- You need to route MIDI to a synth (see DAW-specific instructions above)
- Or enable the built-in **Preview** synth

### MIDI export doesn't work
- Make sure you have generated a melody first (click **Generate**)
- Check that the export folder is writable

---

## System Requirements

- **OS:** macOS 10.15 (Catalina) or later
- **Architecture:** Apple Silicon or Intel (Universal Binary)
- **Formats:** VST3, Audio Unit
- **DAW:** Any VST3 or AU compatible DAW

---

## File Locations

| Item | Path |
|------|------|
| VST3 Plugin | `/Library/Audio/Plug-Ins/VST3/PsyMelody.vst3` |
| AU Plugin | `/Library/Audio/Plug-Ins/Components/PsyMelody.component` |
| User Presets | `~/Documents/EDEN/PsyMelody/Presets/` |

---

*PsyMelody v0.1.2 - Copyright (c) 2026 EDEN*
