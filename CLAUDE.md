# Hardware-Portable JUCE Sculpting Sampler Rules

This project is a JUCE plugin prototype for an eventual standalone hardware sample-sculpting / granular performance device.

The concept is inspired by modern hardware instruments like the Torso S-4:

* four parallel stereo tracks
* sample material per track
* granular processing
* loop/tape-style playback
* filter/color/space sound shaping
* modulation
* scenes
* macro performance controls
* hardware-first screen and control design

The most important rule:

JUCE is only the wrapper. The instrument engine must remain portable C++.

## Architecture Rules

1. The portable engine must not depend on JUCE.

   * No juce::AudioBuffer in core engine classes.
   * No juce::MidiBuffer in core engine classes.
   * No juce::String, juce::ValueTree, juce::File, juce::Component, or JUCE UI classes in the engine.
   * No APVTS inside the engine.
   * No JUCE graphics, timers, or message-thread assumptions in portable code.

2. PluginProcessor is only a bridge.
   It may:

   * receive host audio/MIDI
   * own the portable Engine
   * sync parameters from JUCE to the Engine
   * pass audio input/output buffers into the Engine
   * handle plugin state save/load
   * translate host automation into portable parameter values

   It must not:

   * contain DSP algorithms
   * contain sampler playback logic
   * contain granular playback logic
   * contain modulation logic
   * contain scene logic
   * contain track processing logic
   * contain hardware behavior

3. PluginEditor is only temporary UI.
   It must not:

   * contain DSP
   * contain granular logic
   * contain modulation behavior
   * own core state
   * perform parameter mapping beyond basic UI display
   * become the actual product UI architecture

4. All reusable instrument logic belongs in portable folders:

   * core/
   * audio/
   * modulation/
   * hardware/
   * util/
   * ui/ScreenModel only for abstract screen state, not JUCE drawing

5. All new features must answer these questions before implementation:

   * Can this compile without JUCE?
   * Could this run on embedded or standalone hardware later?
   * Is this separated from UI and host/plugin concerns?
   * Is this small enough to maintain?
   * Does this belong in an existing class, or should it be a new focused class?

## Product Direction Rules

This is not a normal keyboard synth.

Prioritize:

* audio capture
* sample material
* granular manipulation
* texture generation
* four-track layering
* real-time performance controls
* scene snapshots
* modulation
* tactile hardware workflow

Avoid drifting into:

* a full DAW
* a traditional subtractive synth
* a keyboard workstation
* a complex piano-roll sequencer
* a giant plugin UI
* a desktop-only sampler

## Track Architecture Rules

Each track should follow this conceptual chain:

Material → Granular → Filter → Color → Space → Mixer

Where:

* Material handles source audio, recorded audio, sample buffers, or placeholder generated material.
* Granular handles grain playback and texture generation.
* Filter handles basic tone shaping.
* Color handles saturation, drive, grit, or character.
* Space handles delay/reverb-style ambience.
* Mixer handles level, pan, mute, solo, and global output routing.

Keep these stages separated.

Do not merge everything into one giant Track class.

## Granular Engine Rules

The granular engine must be real-time safe.

* Use fixed-size grain pools.
* Avoid memory allocation during audio processing.
* Avoid locks during audio processing.
* Avoid file I/O during audio processing.
* Keep grain parameters clear and normalized.
* Keep the first granular implementation simple and stable before making it advanced.

Core grain parameters:

* position
* size
* density
* pitch
* spray/randomness
* texture/jitter
* spread
* mix

## Modulation Rules

Modulation must be portable C++.

Allowed first modulation sources:

* LFO
* envelope follower
* random modulator
* macro controls

The modulation matrix should:

* map modulation sources to ParameterId destinations
* use normalized values
* support modulation depth
* avoid allocations in the audio callback
* stay independent from JUCE/APVTS

## Scene Rules

Scenes are core product behavior, not plugin-only presets.

Scene data must be stored in portable C++ structures.

A scene should be able to store:

* track parameter values
* macro values
* selected track
* selected page
* performance state if appropriate

JUCE plugin state can serialize scenes later, but it must not define the scene architecture.

## File Size Rules

* Keep files under roughly 300 lines when possible.
* If a file grows too large, split it immediately.
* No mega-files.
* No giant PluginProcessor.
* No giant Engine class.
* No giant Track class.
* No dumping unrelated helpers into one file.

## Dependency Rules

* Portable engine code should use only the C++ standard library unless there is a strong reason.
* Avoid adding third-party dependencies to the engine.
* Any dependency must be easy to replace before hardware porting.
* JUCE-specific dependencies must stay in the JUCE wrapper layer.

## Parameter Rules

* Engine parameters use enum class ParameterId.
* Engine parameters use normalized 0–1 values where possible.
* Host/APVTS parameters are only a bridge.
* Mapping from normalized values to real values should live in one place.
* Avoid duplicate parameter conversion logic.

## Timing Rules

* Clock, transport, modulation timing, grain scheduling, loop playback, and track playback must live in portable C++.
* Host BPM and play state may be passed into the engine, but the timing logic should not depend on JUCE.
* Avoid UI-driven timing.

## Hardware Portability Rules

* Controls should be abstracted as encoders, buttons, LEDs, screen updates, audio inputs, and audio outputs.
* Do not assume mouse/keyboard/desktop interaction in the core engine.
* Any feature that would later map to a physical control should be designed through the hardware abstraction layer.
* Think in terms of physical pages, encoders, buttons, macros, scenes, and track selection.

## Real-Time Audio Rules

Inside the audio process path:

* no heap allocation
* no locks
* no file I/O
* no console logging
* no UI calls
* no plugin-host calls
* no dynamic object creation
* no large memory copies unless intentional and bounded

## Before Every Change

Before editing code, restate:

1. Which layer the change belongs to.
2. Whether the change is JUCE-specific or portable.
3. Which files will be touched.
4. How this keeps the granular/sampler hardware direction intact.
5. How you will keep files small and organized.

If a requested change violates these rules, suggest a better architecture before coding.
