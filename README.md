# StepShaper

A step-sequenced motion-shaping effect for Logic Pro (Audio Unit), also built as VST3 and Standalone.

It has seven FX modules. Each module has its own 32-step sequencer, and every step has its own drawable envelope shape:

| Module | What the sequence moves | Extra controls |
|---|---|---|
| **Volume** | Gain: trance gates, sidechain pumping, tremolo, stutter | n/a |
| **Filter** | Cutoff, swept exponentially between Min and Max | Type (LP / HP / BP / Notch), Min, Max, Resonance |
| **Distortion** | Drive amount | Type (Soft / Hard / Fold / Crush), Drive, Mix |
| **Phaser** | 6-stage phaser frequency | Min, Max, Feedback, Mix |
| **Flanger** | Flanger delay time | Range, Feedback (+/−), Mix |
| **Pitch** | Modulated delay, giving tape-stop, warble and wow-style pitch bends | Range, Mix |
| **Delay** | Send level into a tempo-synced delay (echo throws) | Time, Feedback, Mix |

Signal chain: Distortion → Filter → Phaser → Flanger → Pitch → Delay → Volume, then a global dry/wet Mix and Output.

## The sequencer

* **Steps** (1–32) and **Rate** (1/1 to 1/32, including triplets and dotted values) are set separately for each module. Different lengths on different modules give you polyrhythms.
* **Depth** sets how far the sequence moves the module. At 0 the module sits at its maximum.
* **Smooth** is the glide between values. Low settings give snappy gates and high settings give fluid curves.
* **Swing** (global) runs from 50 to 75%.
* The sequencer locks to Logic's transport. When the transport is stopped it free-runs at the project tempo, so you can audition on live input.

### Editing steps
* **Drag** on the grid to draw step levels.
* **Alt-click** a step to toggle it fully on or off.
* **Cmd-drag** to stamp the shape selected in the Shape Library onto every step you drag across.
* **Right-click** a step for its menu: choose its shape, apply a shape to all steps, set its level, or set the loop end.
* The **Shape panel** (top right) shows the selected step's envelope. Draw directly on it to make a custom curve.
* **Shape Library**: click to apply a shape to the selected step, shift-click to apply it to all steps.
* **Copy Step / Paste / Paste All** copy one step, including its curve, to other steps.
* **Random / Clear / Invert / << / >>** work on the whole lane. The arrows rotate the steps.

## Presets
* **137 factory presets** in 11 categories: Init, Sidechain, Trance Gate, Filter, Distortion, Phaser & Flanger, Pitch, Delay Throws, Tremolo & Stutter, Polyrhythm and Motion (multi-module).
* They're in the preset menu at the top of the plugin and in Logic's own preset menu.
* **Save** stores your own presets in `~/Library/StepShaper/Presets/`.
* **Save Shape / Load Shape** handle a single step envelope (`.ssshape`).
* **Save Module / Load Module** handle one module's sequence and settings (`.ssmodule`). A module preset can be loaded into a different module. For example, a filter pattern can drive the distortion.

## Building for Logic Pro (macOS)

You need:
* **Xcode** from the Mac App Store. After installing it, open it once so it can finish setup.
* **CMake**: `brew install cmake` if you use Homebrew, or the installer from cmake.org.

Then, from this folder:

```bash
cmake -B build -G Xcode
```

```bash
cmake --build build --config Release
```

The first configure downloads JUCE 8.0.4 from GitHub, so it needs an internet connection. The build produces a universal binary that runs on both Apple Silicon and Intel Macs.

When the build finishes, the plugin is copied automatically to:
* `~/Library/Audio/Plug-Ins/Components/StepShaper.component` (AU, which is what Logic uses)
* `~/Library/Audio/Plug-Ins/VST3/StepShaper.vst3`

A Standalone app is also built in `build/StepShaper_artefacts/Release/Standalone/`.

**Getting it into Logic:**
1. Optionally, check that the AU validates:
   ```bash
   auval -v aufx Ssh1 Stpa
   ```
2. Open Logic Pro. If the plugin doesn't appear, go to **Logic Pro → Settings → Plug-in Manager** and click **Reset & Rescan Selection**.
3. On an audio or instrument track, open an Audio FX slot and choose **Audio Units → StepShaper Audio → StepShaper**.
4. Press play. The sequencer follows Logic's tempo and position.

If you already have a JUCE checkout, you can point the build at it instead of downloading:

```bash
cmake -B build -G Xcode -DJUCE_DIR=/path/to/JUCE
```

## Tests (optional)

`-DSTEPSHAPER_BUILD_TESTS=ON` builds `StepShaperTests`. It runs every factory preset through the audio engine with a simulated transport and checks the output for NaN or infinite values, runaway levels and silence. It also checks gate timing, save/restore of state and custom curves, and module mini-presets, and saves editor screenshots.
