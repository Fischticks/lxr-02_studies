# Oscillator / Wavetable Dependency Spike

**Status:** Static source audit complete; no compilation or source modification performed  
**Upstream repository:** `erica-synths/lxr-desktop-app`  
**Pinned revision:** `dca0a44413931a12de65941355f06c07598a9be5`  
**Scope:** `Oscillator.c/.h`, `wavetable.c/.h`, and directly included support headers

## Executive result

The oscillator/wavetable subsystem is a viable first host-harness target, but the current `Oscillator.c` translation unit is not a clean standalone DSP unit. It combines sample/block renderers with MIDI note conversion, sequencer constants, hardware RNG access, and ARM CMSIS intrinsics.

The least disruptive path is to preserve the oscillator algorithms and wavetable data while introducing narrow host boundaries for RNG and CMSIS operations, and separating MIDI note/frequency setup from sample rendering. The wavetable data itself is independent and can be compiled as a separate object.

This was a static dependency audit only. A CMake compile spike has not yet been run, so compile feasibility remains a hypothesis to validate in the next step.

## Files inspected

| File | Role | Observation |
|---|---|---|
| `Core/Src/Soundengine/DSPAudio/Oscillator.c` | Oscillator renderers and frequency/note setup | Mixed DSP and platform/application dependencies |
| `Core/Inc/Soundengine/DSPAudio/Oscillator.h` | `OscInfo`, waveform enum, public functions | Includes HAL, config, datatypes, wavetable headers |
| `Core/Src/Soundengine/DSPAudio/wavetable.c` | Sine and mipmapped waveform tables | Table data only; includes config for section attribute |
| `Core/Inc/Soundengine/DSPAudio/wavetable.h` | Table declarations and dimensions | Includes HAL and stdio unnecessarily for its declarations |
| `Core/Src/Soundengine/DSPAudio/random.c` | Hardware RNG wrapper | One active HAL call |
| `Core/Inc/Soundengine/DSPAudio/random.h` | RNG declaration | Exposes HAL RNG handle type |
| `Core/Inc/Soundengine/DSPAudio/Samples.h` | Legacy sample declarations | Currently empty apart from commented declaration |
| `Core/Inc/Soundengine/config.h` | Build-time sound-engine config | Defines sample rate, interpolation, memory attributes, DMA settings |
| `Core/Inc/Soundengine/datatypes.h` | Basic fixed-point bounds | No additional DSP dependencies |

## Dependency inventory

### Direct include graph

`Oscillator.c` includes:

- `Soundengine/DSPAudio/Oscillator.h`
- `Soundengine/DSPAudio/random.h`
- `Soundengine/DSPAudio/Samples.h` — currently inert
- `Soundengine/MIDI/MidiParser.h`
- `Soundengine/MIDI/MidiNoteNumbers.h`
- `Soundengine/Sequencer/sequencer.h`

The last three are pulled in for a very small number of note-setup symbols, but their headers bring in broad application-level dependencies:

- `MidiParser.h` includes HAL, `globals.h`, MIDI messages, and MIDI voice control.
- `sequencer.h` includes HAL, `globals.h`, sequence-step and parameter definitions.
- `MidiNoteNumbers.h` is a static frequency lookup table and is independently portable.

This is a strong reason to split note/frequency setup out of the render core, or replace those includes with a narrow portable note-utility interface.

### Active external symbols and platform facilities

| Symbol / facility | Used by | Host-harness treatment |
|---|---|---|
| `GetRngValue()` | `calcNoiseBlock`, legacy single-sample `calcNoise` | Inject deterministic scripted/seeded RNG; do not compile hardware `random.c` |
| `midiParser_calcDetune()` | `osc_setBaseNote`, `osc_recalcFreq` | Move detune calculation to portable note utility or provide exact compatible shim |
| `MidiNoteFrequencies[]` | `osc_setBaseNote`, `osc_recalcFreq` | Keep the lookup table; it has no runtime platform dependency |
| `SEQ_DEFAULT_NOTE` | `osc_setBaseNote`, `osc_recalcFreq` | Replace sequencer-header dependency with a narrow constant/config input (currently 60) |
| `__CLZ()` | Wavetable mipmap selection | Portable compiler builtin or a small compatible helper |
| `__QSUB16()` | PWM rendering | Portable saturating signed-16 subtraction helper; preserve ARM semantics |
| `APSR_Type`, `__get_APSR()` | Legacy single-sample `calcNoise` | Prefer removing this legacy path from the host target or rewriting its wrap detection portably |
| `HAL_RNG_GetRandomNumber()` | `random.c` | Avoid in host target; inject RNG through the existing `GetRngValue` boundary |

The sample playback functions are compiled but their bodies are disabled with `#if 0`; `Samples.h` currently contributes no declarations. The active default waveform set is the six built-in types: sine, triangle, saw, rectangle, noise, and PWM.

### Configuration and type assumptions

- `REAL_FS` is hard-coded to 44140 Hz in the audited config. The host harness must supply its configured sample rate rather than silently inheriting this value.
- `INTERPOLATE_OSC` and `INTERPOLATE_FM_OSC` are both enabled in the audited config.
- `IN_ITCM_RAM` and `INDTCM` are embedded memory-section attributes. They should become empty host definitions, without changing DSP logic.
- `OscInfo` uses fixed-width integer types and floats; its layout is straightforward to host-compile.
- `Oscillator.h` and `wavetable.h` include `stm32f7xx_hal.h` even though their public data declarations do not appear to require the full HAL. A minimal compatibility header can unblock the first compile, but reducing these header dependencies is preferable.
- `config.h` conditionally redefines `OUTPUT_DMA_SIZE` when `DMA_MODE_ACTIVE` is defined. In the firmware that macro is defined by `AudioCodecManager.h`; it is not intrinsic to oscillator DSP. The host target should not include audio-codec configuration merely to obtain oscillator constants.
- `wavetable.c` includes `config.h` only for the `INDTCM` attribute.

## Source-level findings to carry into characterization

### 1. Mipmap selection has a low-frequency discontinuity

`freqToMidiNote()` first converts `f / 440.f` to an integer before calling `fast_log2()`. For any positive frequency below 440 Hz, that integer is zero. On ARM, `__CLZ(0)` returns 32, so `31 - __CLZ(0)` becomes -1; the result is then returned through an unsigned 8-bit value. The subsequent arithmetic and narrowing in `freqToTableIndex()` produce table index 4 for this entire sub-440-Hz range.

Consequences:
- Mipmap rows 0–3 are not selected for ordinary positive frequencies below 440 Hz.
- The selector changes discontinuously at 440 Hz.
- This is a source-level defect candidate, not yet a measured audio result.

Add boundary tests around 0 Hz, the lowest intended note, 440 Hz, octave boundaries, and the highest supported frequency. Define the desired table-selection policy before changing it, since the table comments and actual index calculation should be reconciled.

### 2. Wavetable interpolation can read past a row

The saw, triangle, and rectangle tables are declared as `[11][1024]`. In interpolated rendering, code reads element `itg`, increments `itg`, and then reads the next element. When the initial index is 1023, the second read addresses element 1024, outside that row.

For rows 0–9 this may read the first value of the next row due to contiguous storage; for row 10 it reads beyond the complete table object. The same pattern appears in normal wavetable rendering and FM/PWM paths.

Required characterization:
- Exercise every mipmap row with phase indices 1022 and 1023.
- Run under AddressSanitizer/UndefinedBehaviorSanitizer in the host build.
- Compare any proposed wrap/guard-sample fix against the intended periodic interpolation result.
- Do not assume that adjacent-row memory makes the current behavior valid.

The sine table is different: it is declared with `TABLESIZE + 1` entries, providing an explicit guard sample for interpolation.

### 3. Non-interpolated branches contain an undeclared identifier

When interpolation is disabled, several branches reference `overtoneIndex`, although the active source shown in this translation unit defines a local `overtoneIndex` only inside `osc_calcWavetableFreq()`. The default audited configuration enables interpolation, so these branches are preprocessed out.

This means the current source likely does not compile with `INTERPOLATE_OSC=0` or `INTERPOLATE_FM_OSC=0` without correction. Add a build-matrix test for both flags; do not treat these options as valid until compiled.

### 4. RNG behavior is observable DSP state

`calcNoiseBlock()` advances the phase accumulator and fetches a new random value only when unsigned phase wrap is detected. Therefore deterministic noise characterization requires controlling:
- initial phase and phase increment,
- initial held output value,
- the exact RNG sequence,
- block length and render call boundaries.

The legacy single-sample `calcNoise()` uses APSR overflow state rather than the block renderer's explicit unsigned wrap comparison. It should not be assumed bit-equivalent to `calcNoiseBlock()`. Confirm whether any extracted voice calls the single-sample API before deciding whether it belongs in the reference harness.

### 5. PWM relies on ARM saturating arithmetic

PWM is generated by subtracting two saw-table reads using `__QSUB16()`. A host implementation must match the ARM intrinsic's signed 16-bit saturating subtraction semantics exactly. A normal C subtraction followed by narrowing is not a safe substitute.

## Recommended host-target boundary

A useful first target can be organized as:

1. **Portable table object:** compile `wavetable.c` with host definitions for the memory-section attribute and a minimal HAL include shim (or after removing unnecessary HAL includes).
2. **Oscillator render core:** preserve block renderers and `OscInfo`; provide deterministic RNG and portable CMSIS-equivalent helpers.
3. **Note/frequency utility:** isolate MIDI frequency lookup, fine-detune calculation, and default-note reference from the sequencer/MIDI parser headers.
4. **Legacy-only compatibility:** keep `calcNoise()` and sample playback outside the first characterization target unless call-site audit proves they are required.

A temporary compile-only shim can be used to prove the existing source compiles, but it should not become the long-term architecture if it drags `globals.h`, the sequencer, or the complete HAL into the host target.

## Dependency-only spike verdict

**Promising, with a small and well-defined seam.** The wavetable data can be compiled independently. The block oscillator renderers depend on a limited set of platform facilities, and most application coupling is concentrated in note setup. The most valuable next action is a minimal host compile target that compiles the pinned upstream oscillator and wavetable files without modifying the upstream checkout, followed by sanitizer-backed boundary tests.

### Next validation steps

1. Create a temporary host shim include directory for HAL/CMSIS types and section attributes.
2. Provide deterministic `GetRngValue()` and portable equivalents for the CMSIS intrinsics used by the active build.
3. Resolve or isolate the MIDI detune, note-frequency, and default-note dependencies without importing sequencer globals.
4. Compile `wavetable.c` and `Oscillator.c` with the audited interpolation flags first.
5. Repeat with interpolation flags disabled to expose the inactive-branch compile defect.
6. Run oscillator table-boundary and deterministic-noise tests under sanitizers.
7. Record whether the source can compile unchanged with shims, or whether a minimal extraction seam is required.

No files in the upstream repository or Tracker integration repository were modified during this audit.
