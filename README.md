# SculptSampler

A four-track sculpting sampler / granular performance instrument, inspired by hardware like the Torso S-4. Currently shipped as a JUCE plugin prototype (AU / VST3 / Standalone), but the entire instrument engine is portable C++17 with **zero JUCE dependencies**, ready to move to embedded or standalone hardware later (Daisy Seed, Teensy, STM32, Raspberry Pi, custom Linux device).

## Architecture

```
Source/
  PluginProcessor.*     JUCE wrapper only: host bridge (audio/MIDI/params/state)
  PluginEditor.*        Temporary debug UI only (S-4-inspired panel layout: LCD
                        readouts, SELECT output, device row, MOD placeholder)
  ui/juce/              JUCE-only: InstrumentPanel (faux LCD), shared editor colours

  core/                 Portable: Engine, Clock, Transport, ParameterState,
                        ParameterIds, Scene, SceneManager
  audio/                Portable: Track, TrackEngine, MaterialSource, SampleBuffer,
                        SampleRecorder, TapePlayer, GranularEngine, GrainVoice,
                        GrainPool, FilterStage, ColorStage, SpaceStage, Mixer,
                        Envelope, OnePole
  modulation/           Portable: LFO, EnvelopeFollower, RandomModulator,
                        MacroControls, ModEngine (S-4-style per-track mod slots)
  hardware/             Portable: HardwareAbstraction interface + DummyHardware
  ui/                   Portable: ScreenModel + PageModel (abstract display state,
                        no drawing)
  util/                 Portable: Constants, MathUtils, SmoothedValue, RingBuffer,
                        Random
```

Per-track signal chain: **Material -> Granular -> Filter -> Color -> Space -> Mixer**

Key rules (see `.cursor/rules/`):

- JUCE is only the wrapper. No JUCE types in `core/`, `audio/`, `modulation/`, `hardware/`, `ui/`, `util/`.
- All parameters are normalized 0..1; normalized-to-real mapping lives only in `core/ParameterIds.h` (`sculpt::map`).
- Real-time safety: no allocation, locks, file I/O or logging in the audio path. Fixed-size grain pools and scratch buffers.

## Building

Requirements: CMake 3.22+, Xcode command line tools (macOS). JUCE is picked up from `~/Documents/JUCE` if present, otherwise fetched automatically (JUCE 8.0.9).

```bash
cmake -B build
cmake --build build --target SculptSampler_Standalone SculptSampler_AU -j8
```

Build outputs land in `build/SculptSampler_artefacts/`.

## First sounds

The plugin makes sound immediately: each track is filled with generated placeholder material (tone / wave / noise / pulse texture) and starts playing on launch. Use **TRK 1-4** to select a track, the device row (**MATERIAL** ... **SPACE**, **MIX**) to pick the edit page, the eight rotaries under the LCD for that page's parameters, and **MACRO** sliders for broad gestures. **SELECT** is the main output level. Scene buttons **A-D** (with **SAVE**) store and recall snapshots. MIDI notes 36-39 or 60-63 trigger/stop tracks 1-4.

**Your own audio:** Use **LOAD** or drag a WAV / AIFF / FLAC / OGG file onto the editor window; the file is imported into the **currently selected** track (up to about 8 seconds after resampling to the host sample rate). On the **MATERIAL** page, the **Input Capture** knob arms recording from the **plugin input** (enable an input in the standalone or DAW and turn the knob past halfway while the track is playing); the capture buffer wraps circularly over the same ~8 second material window. In the **standalone** app, open the audio device settings and choose an input if you want to capture from a microphone or interface. Longer capture notes appear in the help line's tooltip.
