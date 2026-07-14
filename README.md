# Requiem

*A cathedral in a box — cinematic convolution reverb for orchestral and choral space.*

[![CI](https://github.com/metal-up-your-ass/requiem/actions/workflows/ci.yml/badge.svg)](https://github.com/metal-up-your-ass/requiem/actions/workflows/ci.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

> **Work in progress.** Requiem is pre-1.0 and under active development. There are no built binaries or releases yet — building from source is currently the only way to run it. Expect breaking changes until v1.0.0 ships (see [Roadmap](#roadmap)).

<!-- ==BEGIN BODY== (plugin engineer: replace this block with What it is / Features / Signal flow / Roadmap) -->
## What it is

Requiem is a cinematic **convolution reverb** built for orchestral and choral space in symphonic metal productions - cathedral, hall, and chamber-style tails for strings, choir, and everything that needs to bloom into a wide, dark stereo image behind the wall of guitars. Rather than shipping (and licensing) an IR sample library, its impulse response is generated **procedurally**, off the audio thread, from two controls - Decay and Damping - or you can load your own WAV/AIFF impulse response to override it entirely.

## Features

- `juce::dsp::Convolution`-based stereo reverb engine, IR generated from **Decay** (reverb time) and **Damping** (HF cutoff of the tail) - no bundled sample library, no licensing to manage.
- **Pre-Delay** (0-250 ms) separates the direct sound from the tail's onset - useful for keeping palm-muted rhythm parts tight while strings/choir bloom behind them.
- **Width** control (0-200%) reshapes the wet signal's stereo image via mid/side scaling, independent of the dry signal.
- Optional **user impulse response** override (Load IR.../Clear IR in the editor) - drop in your own captured space, persisted in the plugin's saved state.
- Latency-compensated Dry/Wet **Mix** and post-mix **Output** trim.
- AU / VST3 / Standalone, built on JUCE 8.

## Signal flow

```
input -> Pre-Delay -> Convolution (procedural or user IR) -> Width (M/S, wet only) -> Dry/Wet Mix (latency-compensated) -> Output -> output
```

Decay and Damping don't touch the audio-thread signal path directly - they drive a background, message-thread-only impulse-response regeneration step (see [`docs/architecture.md`](docs/architecture.md) for the full explanation of why, and how it stays real-time safe).

## Roadmap

Tracked as GitHub milestones and issues (`gh issue list`, `gh api repos/metal-up-your-ass/requiem/milestones`). Planned beyond v0.1: early-reflection modelling ahead of the diffuse tail, a custom vector-drawn GUI (current editor is a functional slider layout), and presets for common cathedral/hall/chamber spaces.
<!-- ==END BODY== -->

## Installation

No pre-built binaries are published yet (see the work-in-progress notice above). Once releases begin, installation will follow the standard plugin locations:

**macOS**

| Format | Path |
|---|---|
| AU (Component) | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |

If Logic Pro doesn't pick up the plugin after installing, force a rescan by resetting the AU cache:

```sh
killall -9 AudioComponentRegistrar
auval -a
```

**Windows**

| Format | Path |
|---|---|
| VST3 | `C:\Program Files\Common Files\VST3\` |

## Building from source

Requires JUCE 8.0.14, C++20, and CMake ≥ 3.24. See [`docs/building.md`](docs/building.md) for full prerequisites and step-by-step build/test commands for macOS and Windows.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

Requiem is licensed under the [GNU Affero General Public License v3.0](LICENSE) (AGPLv3).

This project uses [JUCE](https://juce.com) 8, whose open-source tier is licensed under AGPLv3 (as of JUCE 8; JUCE 7 and earlier used GPLv3), which is why this project is AGPLv3 rather than GPLv3. See [`docs/adr/0002-agplv3-licensing.md`](docs/adr/0002-agplv3-licensing.md) for the full reasoning.

VST is a registered trademark of Steinberg Media Technologies GmbH.

Requiem is an independent open-source project and is not affiliated with, endorsed by, or sponsored by any plugin manufacturer.
