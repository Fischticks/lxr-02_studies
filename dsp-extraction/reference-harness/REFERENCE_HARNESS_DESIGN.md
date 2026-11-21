# LXR-02 Standalone DSP Reference Harness — Architecture

**Status:** Design proposal; no harness implementation has started  
**Study repository:** `Fischticks/lxr-02_studies`  
**Source baseline:** `erica-synths/lxr-desktop-app` commit `dca0a44413931a12de65941355f06c07598a9be5`  
**Related specification:** [`DSP_EXTRACTION_SPEC.md`](../DSP_EXTRACTION_SPEC.md)

## 1. Purpose

Build a host-side test environment that can render and inspect the original LXR-02 voice DSP independently of the desktop application's UI, sequencer, audio device, and STM32 hardware.

The harness is intended to provide a repeatable behavioral reference for the later Tracker CE extraction. It must distinguish:

- **Original reference behavior:** behavior at the pinned upstream source revision, including known defects where they can safely be observed.
- **Corrected reference behavior:** an instrumented or minimally patched reference used to test an explicitly approved correction.
- **Tracker port behavior:** the independently extracted implementation.

The harness is not a replacement application, a plugin, or a Tracker integration layer.

## 2. Core design decisions

1. **C-first, host-native build.** The audited DSP is C. Use a C11 build for the reference layer; use CTest for orchestration. Avoid introducing C++ into the DSP boundary.
2. **Do not vendor or silently fork upstream source.** The harness should build against a separately checked-out LXR source tree selected with `LXR_SOURCE_DIR`. Record and verify the exact source commit at configure/test time.
3. **Keep reference and port targets separate.** Build distinct libraries/targets with separate include paths, compile definitions, and adapters. Never link both implementations into one namespace if their symbols collide; use separate test executables or explicit symbol renaming.
4. **Adapt dependencies at the boundary.** Provide host shims for HAL RNG, CMSIS intrinsics, memory-placement attributes, tempo, sample rate, and block configuration. Do not spread host conditionals through DSP algorithms.
5. **Make randomness deterministic.** The reference RNG adapter must support a fixed seed and a recorded sequence mode. The production Tracker RNG can be different, but tests must be reproducible.
6. **Make timing explicit.** Every render/test configuration specifies sample rate, block size, tempo, and control-update cadence. Do not assume the original 64-sample block or 44,140 Hz configuration.
7. **Capture both audio and state.** WAV output is useful, but regression checks should also inspect selected intermediate buffers, envelope values, oscillator phase, filter state, and trigger/reset behavior.
8. **Separate characterization from correctness.** A test that records what upstream does is not automatically an assertion that the behavior should be retained.

## 3. Proposed repository layout

```text
dsp-extraction/
├── DSP_EXTRACTION_SPEC.md
├── reference-harness/
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── cmake/
│   │   ├── VerifyLxrRevision.cmake
│   │   └── HostCapabilities.cmake
│   ├── adapters/
│   │   ├── hal_rng/
│   │   ├── cmsis/
│   │   ├── platform/
│   │   └── host_config/
│   ├── include/
│   │   ├── harness/
│   │   └── reference/
│   ├── src/
│   │   ├── harness/
│   │   └── reference/
│   ├── tests/
│   │   ├── unit/
│   │   ├── voices/
│   │   ├── regression/
│   │   └── fixtures/
│   ├── tools/
│   │   ├── render_voice/
│   │   └── inspect_wav/
│   └── output/                 # generated; ignored by Git
├── validation/
└── research/
```

This is a proposed layout, not a claim that these directories already exist. Generated WAVs, traces, build products, and temporary renders should remain untracked unless a small fixture is intentionally committed.

## 4. Build topology

Use CMake with these conceptual targets:

| Target | Purpose |
|---|---|
| `lxr_reference` | Original selected LXR DSP sources plus host adapters |
| `lxr_reference_instrumented` | Optional reference build with trace hooks or approved diagnostic patches |
| `tracker_lxr_port` | Future extracted implementation; not part of the first harness milestone |
| `harness_support` | WAV writer, deterministic RNG, assertions, trace capture, test utilities |
| `test_oscillator`, etc. | Focused test executables grouped by DSP component |
| `render_voice` | Command-line renderer for deterministic voice renders |

The harness should accept an explicit source path, for example:

```sh
cmake -S . -B build -DLXR_SOURCE_DIR=/path/to/lxr-desktop-app
cmake --build build
ctest --test-dir build --output-on-failure
```

At configure time, inspect the source checkout's Git revision and fail if it is not the pinned baseline, unless an explicit override is provided for a separately labelled experiment. Print the resolved revision in test logs and render metadata.

### Source selection

Start with the smallest viable source set, then add dependencies only when the linker/compiler demonstrates they are required. Expected groups include:

- Four voice engine source files.
- Oscillator and wavetable implementation.
- Envelope implementations.
- Resonant filter.
- Transient generator and transient tables.
- Distortion and buffer helpers.
- LFO and modulation primitives only when the selected test requires them.

Do not compile the complete desktop application, sequencer, UI, MIDI parser, preset manager, or master FX bus just to obtain the voice DSP.

Some original source files mix DSP with platform or application dependencies. If a file cannot be compiled cleanly with boundary shims, document the dependency and use the smallest possible adapter or source split. Do not create broad fake implementations that conceal missing behavior.

## 5. Host adapter contracts

### 5.1 Configuration

A test render configuration should contain at least:

- Sample rate in Hz.
- Audio block size in samples.
- Tempo in BPM.
- Control-update cadence, expressed explicitly (per block or at a specified interval).
- Deterministic RNG seed or scripted random sequence.
- Output channel count and sample format.

Use the actual Tracker target values when known. Until then, test at 44.1, 48, and 96 kHz, and block sizes 16, 32, 64, and 128 where the source can support them.

### 5.2 Random number generation

Provide a narrow host RNG adapter with:

- Seeded pseudo-random mode for repeatable renders.
- Scripted-value mode for exact unit tests.
- Optional call counter and trace.
- Explicit integer width and range semantics matching the original HAL call.

The adapter must not accidentally change the distribution or scaling expected by the source. Record the RNG mode and seed in every render manifest.

### 5.3 Platform and arithmetic shims

Provide tested host equivalents for only the intrinsics actually used by the selected DSP sources, such as saturating arithmetic and fixed-point helpers. Add unit tests for signed limits, overflow, and rounding. Define memory-placement attributes as empty host annotations.

Avoid assuming that a convenient host intrinsic has identical overflow, saturation, or rounding semantics to the ARM implementation.

### 5.4 Tempo and timing

Supply tempo through a deterministic harness configuration. Keep audio sample time and control-update time distinct. A control-rate update must occur at the same point in the render sequence as the reference behavior being characterized.

## 6. Runtime interface

The harness should expose a small, test-oriented interface independent of the original UI:

1. Create/reset a selected engine instance.
2. Set a complete parameter snapshot or named parameter values.
3. Send a trigger with note, velocity, and engine-specific trigger data (including HiHat open/closed).
4. Advance control-rate updates at an explicitly configured cadence.
5. Render exactly N audio samples, including partial final blocks where supported by the adapter.
6. Read selected state and intermediate buffers through optional instrumentation.
7. Reset or destroy the instance.

Do not design this API around LXR MIDI CC numbers. Tests should use named, typed parameters or a source-accurate parameter adapter so that the harness remains understandable and the later Tracker mapping stays separate.

For each engine, preserve its actual architecture:
- DrumVoice: three independent instances; mixing and FM modes.
- Snare: separate noise, tonal, and transient paths.
- Cymbal: carrier plus two modulators; preserve noise-carrier FM bypass.
- HiHat: one shared voice with open/closed decay selection.

## 7. Output and observability

### Audio output

Write mono or stereo floating-point WAV files (32-bit float preferred for diagnostic renders). If a standard PCM WAV is also needed for listening, generate it as a secondary export with explicit clipping/conversion.

Each render should have a small machine-readable manifest containing:

- Source revision.
- Harness build identifier and compiler.
- Engine and voice instance.
- Parameter snapshot.
- Note, velocity, trigger sequence.
- Sample rate, block size, tempo, control cadence.
- RNG mode and seed.
- Whether the render is original, instrumented, corrected-reference, or port output.

### State and intermediate traces

Support optional capture points rather than always logging every sample. Candidate taps:

- Oscillator output and phase accumulator.
- Modulator buffers and combined FM buffer.
- Noise output.
- Transient output and playback phase.
- Filter input, output, and state variables.
- Envelope values at each control update.
- Pre-envelope, post-envelope, and post-distortion voice signal.
- Trigger/reset events and parameter changes.

Instrumentation must be disabled in normal timing/performance measurements. A trace should identify the exact sample or control-update index for every value.

## 8. Test strategy

Use three layers:

### Layer A — unit characterization

Test shared DSP primitives in isolation: oscillator lookup, tuning, envelopes, transient playback, filter equations, distortion, saturating helpers, and RNG adapter.

### Layer B — voice behavior

Test each engine's signal path and trigger semantics with controlled parameters and deterministic randomness. Assert both audio properties and state transitions.

### Layer C — cross-implementation regression

Once the Tracker port exists, render identical parameter/trigger/configuration sequences through the reference and port. Compare:

- Exact equality where arithmetic and behavior are intentionally identical.
- Absolute/relative numerical tolerance where floating-point ordering differs.
- Signal metrics (peak, RMS, DC offset, duration, spectral bands) where deliberate corrections alter samples.
- State traces for envelope, phase, filter, and trigger behavior.

Never rely on a single whole-file waveform hash as the only acceptance test. A hash is useful for exact deterministic fixtures, but it does not explain a failure and is brittle across compilers or intentional fixes.

## 9. Initial test inventory and acceptance criteria

| Area | Minimum tests | Acceptance |
|---|---|---|
| Oscillator | All waveforms; tuning endpoints/midpoint; phase wrap; mipmap selection; interpolation at indices 1022/1023 | No invalid reads; expected table selection documented; phase/output continuity checked |
| FM | Supported tonal carrier waveforms; modulation depth extremes; noise carrier | Phase modulation matches source; noise carrier ignores FM input |
| DrumVoice | Mix mode and FM mode; transient modes; filter and envelope trigger behavior | Correct signal-path ordering and independent instance state |
| Snare | Noise rate; mix extremes; transient bypass; note trigger | Noise is stepped and free-running; only noise is filtered |
| Cymbal | Two modulators; saturating combine; transient bypass; noise carrier | Correct combined modulation and noise-carrier bypass |
| HiHat | Open then closed; closed then open; retrigger while ringing | Shared voice state and selected decay are correct |
| Envelopes | SlopeEg2 attack/decay/repeat; DecayEg slopes 126/127; SnapEg at transient pitch .25/1/4 | Outputs/state finite where required; endpoint policy explicitly asserted |
| Transients | Pitches .25/1/4; trigger/retrigger; end threshold; odd/even block sizes | Correct progression and bounds-safe clearing |
| Filter | Eight modes; cutoff/resonance extremes; normal vs cheap path | Stable state; mode outputs characterized; normal path is baseline |
| Modulation | LFO waveforms/sync/retrigger; velocity/LFO destination collision | Dispatch order and collision policy reproducible |
| Host config | 44.1/48/96 kHz; 16/32/64/128 samples | No hidden dependence on original compile-time rate/block assumptions |

### Defect-specific test policy

For each known defect, keep two distinct tests where possible:

- A **characterization test** documenting what the pinned source does.
- A **safety/correctness test** asserting the required behavior of the corrected reference or Tracker port.

Do not execute undefined out-of-bounds reads as a routine regression test under normal conditions. Reproduce the boundary index with AddressSanitizer or a narrowly controlled diagnostic build, and assert that the corrected implementation remains in bounds.

## 10. Recommended implementation milestones

### Milestone 0 — environment verification

- Check out the exact upstream revision locally.
- Verify compiler, CMake, and sanitizer availability.
- Inventory selected DSP files' direct includes and external symbols.
- Produce a dependency report before writing adapters.

**Exit gate:** the minimal source/dependency list and required shims are known.

### Milestone 1 — build a primitive reference target

- Compile the oscillator/wavetable and envelope/filter/transient primitives incrementally.
- Add deterministic RNG and platform shims.
- Add unit tests and sanitizer configuration.

**Exit gate:** selected primitives compile and their isolated tests run without the desktop application.

### Milestone 2 — add voice render adapters

- Bring up one engine at a time.
- Add parameter injection, trigger, control update, and block render.
- Capture deterministic WAVs and state traces.

**Exit gate:** all four engines can be instantiated and rendered independently.

### Milestone 3 — freeze reference fixtures

- Create a small, reviewed set of parameter snapshots and trigger sequences.
- Record manifests and expected state/audio metrics.
- Label fixtures as characterization or corrected-reference.

**Exit gate:** repeat runs with the same build/configuration/seed are deterministic.

### Milestone 4 — connect the future port

- Add the Tracker extraction as a separate target only after it exists.
- Run identical test vectors through both implementations.
- Report exact, tolerance-based, and intentionally divergent results separately.

**Exit gate:** every divergence is either fixed or documented and approved.

## 11. Risks and decisions to settle before implementation

1. **Source build coupling:** some source files combine DSP and application dependencies. The exact minimal compile set is not yet proven.
2. **Compile-time configuration:** macros may change DSP state layout or behavior. Record the selected macro set and avoid incompatible configurations in one binary.
3. **Global state:** original code may rely on globals and singleton buffers. The harness must not imply independent instances until that is verified.
4. **Symbol collisions:** reference and port may export similar names. Use separate executables initially.
5. **Control-rate semantics:** matching the original call order is essential; a superficially equivalent per-sample update can change sound.
6. **Floating-point reproducibility:** compiler flags, fused operations, and architecture can change samples. Record compiler and flags; do not demand bit-exact output across unrelated targets without evidence.
7. **Defect characterization:** undefined behavior must be documented safely, not depended on.
8. **Licensing:** retain source notices and review GPL-3.0-only plus NOTICE conditions before distributing derived source or binaries.

## 12. First practical action

Before writing the harness, perform a dependency-only spike:

1. Check out the pinned upstream revision.
2. Enumerate direct includes and external symbols for each candidate DSP source.
3. Identify which files compile unchanged with host shims.
4. Identify global state and compile-time configuration assumptions.
5. Produce a minimal CMake target for one isolated primitive (preferably oscillator/wavetable) without modifying the upstream checkout.

This spike will reveal whether the proposed adapter approach is practical and prevent us from designing a large harness around an unverified build assumption.

**Current status:** Architecture and test strategy proposed. No harness code, fixtures, or Tracker integration has been created.
