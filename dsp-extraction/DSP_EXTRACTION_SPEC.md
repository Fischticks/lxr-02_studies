# LXR-02 DSP Extraction Specification

**Status:** Source-audit specification; implementation has not started  
**Audit baseline:** `erica-synths/lxr-desktop-app` commit `dca0a44413931a12de65941355f06c07598a9be5`  
**Study repository:** `Fischticks/lxr-02_studies`  
**Integration destination:** `jamartinez4991/Tracker_LXR_02`  
**Last updated:** 2026-11-21

> This document records the audited behavior of the LXR-02 voice DSP and defines the intended extraction boundary for a Polyend Tracker CE port. It is not an implementation plan that authorizes changing the integration repository. The study and integration repositories remain separate.

## 1. Goals and non-goals

### Goals

- Extract the four LXR-02 voice architectures and their required local DSP.
- Preserve each voice's characteristic signal path rather than flattening all voices into one generic engine.
- Preserve original parameter ranges, conversions, defaults, modulation behavior, and trigger semantics where they are intentional and safe.
- Replace LXR hardware, sequencer, MIDI, mixer, and global-effect dependencies with explicit Tracker-facing interfaces.
- Identify numerical and memory-safety defects before porting, and make deliberate compatibility decisions for them.
- Establish a repeatable validation plan before implementation.

### Initial non-goals

The initial voice extraction does **not** include:
- LXR sequencer, pattern, automation, MIDI parser, front-panel UI, or preset manager.
- LXR audio routing, master mixer, global FX bus, or master effects.
- Hardware ADC fader, GPIO, DAC/I2S, DMA, HAL, or STM32-specific memory placement.
- User-sample oscillator playback, which is disabled in the audited active oscillator paths.
- A wholesale port of the LXR application or its UI.

Master effects may be audited and extracted separately if desired. They should remain outside the voice-local DSP boundary.

## 2. Source baseline, repository boundaries, and licensing

### Repositories

| Purpose | Repository |
|---|---|
| Source study, audit notes, experiments, validation harnesses | `Fischticks/lxr-02_studies` |
| Actual Tracker CE integration | `jamartinez4991/Tracker_LXR_02` |
| Official source baseline | `erica-synths/lxr-desktop-app` |

The audited official source revision is:

`dca0a44413931a12de65941355f06c07598a9be5`

Use this pinned revision as the source-of-truth baseline for implementation decisions. Re-check upstream changes separately; do not silently move the audit baseline.

**Repository separation is intentional.** The study fork is for understanding and documenting the source. The integration destination is for the later Tracker implementation. Do not automatically propagate changes, commits, or branches between them.

### Licensing

The source declares **GPL-3.0-only** and references additional permissions and terms in `NOTICE.md`. Preserve copyright and license notices. Before distributing a binary or source derivative, review the complete LICENSE and NOTICE terms, including trademark restrictions and the additional permissions. Do not assume that GPL-3.0-only alone describes every applicable condition.

## 3. Engine inventory and architecture

There are four distinct engine architectures and six total voice instances:

1. Three independent `DrumVoice` instances.
2. One `Snare`.
3. One `CymbalVoice`.
4. One shared `HiHat` voice handling both open and closed triggers.

The four architectures must remain distinct. Shared low-level DSP can be extracted into reusable modules, but engine-specific routing, envelope behavior, and trigger semantics must remain explicit.

### 3.1 DrumVoice (three instances)

**Signal path**
1. Generate modulator oscillator.
2. Generate main oscillator; in FM mode, the modulator buffer phase-modulates it.
3. Combine oscillators according to the oscillator mode.
4. Add transient signal (or use snap/offset behavior implemented elsewhere).
5. Process through the resonant filter.
6. Apply amplitude envelope.
7. Apply optional velocity gain.
8. Apply voice distortion.
9. Apply host/mixer volume and output gain.

**Oscillator modes**
- Mix mode: modulator and main oscillator are combined with complementary gains based on `fmModAmount`.
- FM mode: modulator buffer phase-modulates the main oscillator.
- FM depth is also scaled by the pitch envelope in the asynchronous update path.

These are separate synthesis modes, not interchangeable implementations of a single control.

**Trigger behavior**
- Retriggers the voice LFO.
- Updates the velocity modulation node.
- Conditionally resets oscillator phases according to envelope/transient-offset state.
- Sets base note on both oscillators.
- Triggers pitch, amplitude, transient, and snap envelopes.
- Resets filter state.
- Updates triggered pan.

Preserve waveform-specific phase initialization. Do not replace it with a generic phase reset.

**Transient modes**
- Waveform 0: snap mode; affects oscillator pitch behavior through `SnapEg`.
- Waveform 1: offset mode; affects oscillator phase initialization.
- Waveforms 2–13: twelve transient samples.
- The transient block renderer itself emits silence for modes 0 and 1; their behavior is in the voice implementation.

### 3.2 Snare

Snare is not a DrumVoice with an oscillator removed. It has a dedicated noise generator, a tonal oscillator, a transient path, and unique mixing behavior.

**Signal path**
1. Generate stepped noise: a new random value is acquired at each noise phase wrap and held between wraps.
2. Filter the noise path with the resonant filter.
3. Generate the tonal oscillator into a separate buffer.
4. Generate transient into a separate buffer.
5. Blend filtered noise and tonal oscillator: `noise * mix + oscillator * (1 - mix)`.
6. Add transient independently after the blend.
7. Apply volume, optional velocity scaling, and host fader gain.
8. Apply Snare distortion.

The filter processes **noise only**. Tonal oscillator and transient bypass it.

**Noise behavior**
- Noise frequency controls the random-value update rate, not a conventional noise-filter cutoff.
- Raw noise frequency 0–127 maps linearly to 0–22 kHz.
- Noise phase is not reset by trigger.
- The noise oscillator does not currently track incoming note transposition, despite a source comment noting that it should.

**Trigger behavior**
- Retriggers LFO and velocity modulation node.
- Resets tonal oscillator phase with waveform-specific initialization.
- Applies offset-mode phase adjustment.
- Triggers pitch and amplitude envelopes.
- Stores velocity and sets tonal oscillator base note.
- Resets transient phase and triggers snap behavior.
- Updates equal-power pan.
- Does **not** reset filter state or noise oscillator phase.

### 3.3 CymbalVoice

Cymbal has one carrier oscillator and two independent modulation oscillators. Their signals are combined before modulating the carrier.

**Signal path**
1. Generate modulator 1 and scale by its gain.
2. Generate modulator 2 and scale by its gain.
3. Combine modulation buffers using saturating addition.
4. Phase-modulate the carrier using the combined buffer.
5. Filter the carrier.
6. Generate transient into the reused modulation buffer.
7. Add transient after the filter using saturating addition.
8. Apply amplitude envelope, optional velocity gain, volume, and host fader gain.
9. Apply Cymbal distortion.

**Critical behavior:** the shared FM renderer ignores the FM buffer when the carrier waveform is noise and instead calls the noise renderer. Therefore FM has no effect on a noise carrier. This is source behavior and should be preserved for compatibility unless a deliberate feature change is approved.

The snap envelope affects the carrier pitch multiplier. For a noise carrier, this changes the noise update rate even though FM buffer modulation is ignored.

**Trigger behavior**
- Retriggers LFO and velocity node.
- Resets carrier phase using waveform-specific rules and transient offset behavior.
- Resets both modulator phases.
- Sets base note on all three oscillators.
- Triggers amplitude, transient, and snap envelopes.
- Updates pan.
- Does not reset filter state.

### 3.4 HiHat

HiHat shares Cymbal's three-oscillator topology: one carrier, two modulators, filter, transient, amplitude envelope, snap behavior, and distortion.

**Signal path**
1. Generate and scale two modulation oscillators.
2. Combine modulation buffers with saturating addition.
3. Render carrier with FM.
4. Filter carrier.
5. Add transient after the filter.
6. Apply amplitude envelope, optional velocity gain, volume, and host fader gain.
7. Apply distortion.

**Open/closed behavior**
- One shared HiHat voice handles both trigger types.
- Caller supplies `isOpen`; the closed and open trigger paths select different decay values.
- Trigger copies `decayClosed` or `decayOpen` into the shared amplitude envelope before triggering it.
- A new open or closed trigger retriggers the same voice.
- `isOpen` is otherwise only used by commented-out gate-mode note-off code in the audited source.

**Difference from Cymbal:** HiHat passes a fixed carrier FM gain of 0.5 to the shared FM renderer; Cymbal passes 1.0. This is independent of the two user modulation gains.

**Trigger behavior**
- Retriggers LFO and velocity node.
- Applies waveform-specific carrier phase initialization and transient offset behavior.
- Resets both modulator phases and sets all three oscillator base notes.
- Selects open/closed decay.
- Triggers amplitude, transient, and snap envelopes.
- Updates pan.
- Does not reset filter state.

## 4. Shared DSP contracts

### 4.1 Oscillator and wavetable

**Core behavior**
- 32-bit phase accumulator; incremented once per rendered sample.
- Sine table: 4096 samples, with a 4097th guard entry.
- Saw, triangle, rectangle, and PWM tables: 1024 samples per mipmap level.
- Current configuration enables linear interpolation.
- Effective oscillator frequency is the product of base frequency, pitch multiplier, and modulation-node multiplier:
  `f_effective = f_base * pitchMod * modNodeValue`.
- FM is phase modulation: incoming modulator samples offset the carrier's wavetable phase lookup. It does not directly change the carrier phase increment.
- Waveform-specific FM renderers exist because table sizes and lookup details differ.
- PWM uses phase-offset saw-table reads and saturating subtraction.
- Noise uses a phase accumulator and obtains a new random value on each wrap, holding it between wraps.
- Noise carrier rendering ignores the FM input buffer.

**Mipmap selector issue — high priority**
The audited frequency-to-table selection performs integer conversion of a frequency ratio before its leading-zero/logarithmic selection calculation. For positive frequencies below 440 Hz, the integer ratio becomes zero; the ARM leading-zero result then drives the selector to table index 4 after the subsequent integer conversions. This makes the lower mipmap levels unreachable for that frequency range and is a confirmed source-level defect candidate.

Do not blindly copy this selector. Validate table choice across the full intended note range and define a corrected selector. Keep a reference test that can reproduce the original behavior.

**Interpolation boundary issue — high priority**
The saw/triangle/rectangle tables are declared as `[11][1024]`, but interpolation reads the current sample and the next sample. At index 1023, the next read addresses index 1024, outside the current row. This crosses into the next row for most levels and reads beyond the array at the final level.

The Tracker implementation must use a valid cyclic guard sample or explicit wraparound to index zero. Do not preserve out-of-bounds behavior.

**User samples**
The user-sample oscillator rendering branches are disabled in the audited active code. Exclude sample playback from the initial extraction unless a later requirement explicitly brings it back.

### 4.2 Envelopes

Keep the three envelope implementations distinct.

**SlopeEg2 — amplitude envelope**
- Used by all four voice architectures.
- Advances by a fixed increment/decrement on each control update; parameters are not direct milliseconds.
- Attack and decay raw values are 0–127 and map nonlinearly to increments.
- Low raw values produce faster changes; high values produce slower changes.
- Curvature uses `a = 1.999 * (x - 0.5)` and `k = 2a / (1 - a)`; attack uses inverse curvature.
- Supports optional repeat behavior. Repeat uses attack increment as its repeated decay phase before transitioning to normal decay.
- In the current build, DrumVoice interpolates envelope gain across the block; Snare, Cymbal, and HiHat use the calculated envelope value across the block.

At raw attack/decay 127, the increment/decrement can become zero. This can produce an indefinitely held phase. Treat this as a defined endpoint or clamp it deliberately; do not accidentally create a stuck envelope.

**DecayEg — pitch envelope**
- Used by DrumVoice and Snare.
- One-way curved decay.
- Pitch depth maps as `A = (raw / 127)^2 * 32`, giving internal range 0–32.
- Pitch multiplier is `1 + envelope * A`.
- Raw slope 127 reaches a division-by-zero endpoint in the slope conversion and can propagate infinity/NaN into pitch calculations.

**Required correction:** define a finite maximum-slope curve or endpoint special case. Never allow NaN or infinity to reach oscillator frequency state. Add explicit tests at raw slope 126 and 127.

Raw decay 127 produces a zero decrement and therefore a non-decaying pitch envelope; decide whether to preserve as an intentional endpoint or impose a maximum duration.

**SnapEg — transient snap**
- Used by all four voice engines for transient waveform 0.
- Starts at 1 and returns `24 * value^2`.
- Subtracts `0.2 * time` per asynchronous/control update.
- Voice engines pass transient pitch as the time argument.
- Approximate update counts to completion: pitch 0.25 → 20 updates; pitch 1 → 5 updates; pitch 4 → 2 updates.

Its duration is control-rate dependent. Porting to a different block size or sample rate changes its real-time duration unless the update cadence or formula is adapted. Preserve or deliberately redesign this behavior and document the choice.

### 4.3 Resonant filter

- Nonlinear zero-delay-feedback state-variable filter, not a generic biquad.
- Two integrator state values `s1` and `s2`.
- Cutoff is shaped, multiplied by 0.45, and converted with `g = tan(pi * f)` (using the source approximation `fastTan`).
- Resonance uses `q = 1 - feedback`; very high resonance is clamped to a minimum q of 0.02.
- At `f >= 0.4499`, render path forces the damping parameter `R` to 1 for stability.
- Input uses `softClipTwo()`.
- Modes: low-pass, high-pass, band-pass, unity band-pass, notch, peak, naive two-pole, and off.
- Naive two-pole is a separate two-stage state update.
- Normal and cheap ZDF renderers exist.

**Cutoff range correction:** maximum normalized filter frequency is 0.45 cycles/sample. At the audited 44.14 kHz sample rate this corresponds to approximately 19.86 kHz, not 9.93 kHz.

**Normal vs cheap renderer**
Cymbal and HiHat can select the cheap filter path based on global FX-bus reverb selection. This is a host optimization, not an intrinsic voice property. The cheap path omits normal-path NaN state cleanup and differs in some arithmetic. Use the normal ZDF renderer as the Tracker reference implementation for all voices. Consider the cheap path only after profiling and equivalence testing.

### 4.4 Transient generator

- Twelve signed 8-bit sample tables, each declared as 2205 samples.
- 20-bit fractional phase; sample index is `phase >> 20`.
- Pitch controls phase increment.
- Samples are scaled into 16-bit audio range and multiplied by transient volume.
- Active block renderer uses nearest-sample lookup; no interpolation.
- Trigger resets playback phase.
- Modes 0 and 1 are special snap/offset modes handled by voice code; block sample renderer emits silence for them.
- The legacy `transientVolumeTable` is not used by the active block renderer. Do not add it to the port without a deliberate design decision.
- Direct parameter selection supports waveform values 0–13. Float modulation maps through twelve entries (0–11), including snap and offset plus the first ten sample entries, and therefore does not reach the final two sample entries.

**Endpoint:** stop threshold is `2204 << 20`; the renderer accesses indices 0–2203 and does not render index 2204. Record this as original behavior; test whether it is intentional before changing it.

**Buffer clearing:** the early-return path clears `size / 2` 32-bit words, which assumes an even number of 16-bit samples. Tracker code must clear exactly the requested sample count, including odd-sized blocks.

### 4.5 LFO and modulation

- Six LFOs and six velocity modulation nodes.
- LFO waveforms: sine, triangle, saw up, saw down, rectangle, noise, exponential up, exponential down.
- Free-running frequency mapping: `f = 200 * (0.0078125 + 0.9921875*x)^3`, with normalized x.
- Sync choices: free, 4/1, 2/1, 1/1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16, 1/32.
- LFO is dispatched once per output block, not per sample.
- LFO output is converted from unipolar 0–1 to bipolar −1…+1, scaled by depth, added to the canonical normalized parameter, clamped to 0–1, and passed through the normal parameter conversion path.
- Triggering any voice checks all six LFOs; LFOs configured to retrigger from that voice are reset.
- LFO target/voice routing is global parameter-array/front-panel/preset plumbing, not a self-contained per-voice object.

**Mixer update order**
1. Restore modulation destinations to canonical parameter values.
2. Reapply velocity modulation nodes.
3. Dispatch six LFOs in voice order.
4. Recalculate filter frequencies.
5. Advance asynchronous envelope/frequency updates.
6. Render synchronous audio blocks.

**Collision behavior**
Modulation sources calculate from the canonical `activeParameters` value rather than accumulating on the value written by the preceding source. A later source overwrites an earlier source on a shared destination. LFO dispatch follows velocity modulation, so LFO writes can overwrite velocity writes. Later LFOs can overwrite earlier LFOs.

**Tracker recommendation:** replace this implicit global pointer plumbing with typed parameter descriptors and an explicit collision policy. Decide whether sources combine additively, use priority, or use another defined rule. Do not accidentally reproduce order-dependent overwrite behavior.

## 5. Parameter mapping reference

All ranges below are raw LXR parameter ranges unless stated otherwise. Most controls use 0–127. The parameter storage array is not a complete authoritative direct-control map: some pointers refer to modulation multipliers while normal MIDI handlers write tuning bytes or call dedicated setters. Port parameter conversion from the actual handler behavior, not from pointer names alone.

### 5.1 Common oscillator controls

| Control | Raw range | Internal behavior |
|---|---:|---|
| Waveform | 0–5 | Sine, triangle, saw, rectangle, noise, PWM |
| Coarse tuning | 0–127 | MIDI-note offset relative to incoming/base note |
| Fine tuning | 0–127 | Approximately ±50 cents |
| PWM | 0–127 | Normalized 0–1 |
| FM amount/gain | 0–127 | Normalized 0–1 |
| Modulator frequency | 0–127 | Note offset in the normal parameter handler |

Fine tuning multiplier is approximately `1 + (x - 0.5) * (2^(1/12) - 1)`, where x is normalized.

### 5.2 Common amplitude envelope

| Control | Raw range | Internal behavior |
|---|---:|---|
| Attack | 0–127 | Nonlinear per-update attack increment |
| Decay | 0–127 | Nonlinear per-update decay decrement |
| Slope | 0–127 | Curvature mapping |
| Repeat | 0–127 | Repeat behavior where the engine exposes it |

### 5.3 Pitch envelope (DrumVoice and Snare)

| Control | Raw range | Internal behavior |
|---|---:|---|
| Pitch decay | 0–127 | DecayEg decrement |
| Pitch amount | 0–127 | Squared mapping to 0–32 |
| Pitch slope | 0–127 | Curvature; raw 127 is unsafe in source |

Cymbal and HiHat do not have this separate pitch-decay envelope; their pitch movement is tied to SnapEg behavior.

### 5.4 Transient controls

| Control | Raw range | Internal behavior |
|---|---:|---|
| Waveform | 0–13 | Snap, offset, or one of twelve sample entries |
| Volume | 0–127 | Normalized 0–1 |
| Pitch | 0–127 | `0.25 + x / 33.9`, approximately 0.25–4.0 |

### 5.5 Filter controls

| Control | Raw range | Internal behavior |
|---|---:|---|
| Cutoff | 0–127 | Nonlinear shaping, then 0.45 maximum normalized frequency |
| Resonance | 0–127 | Feedback mapping; q clamped at high resonance |
| Type | 0–7 | Handler adds one: internal values 1–8 |
| Drive | 0–127 | `0.4 + 6*x^2`, approximately 0.4–6.4 |

Internal filter types: 1 LP, 2 HP, 3 BP, 4 unity BP, 5 notch, 6 peak, 7 naive two-pole, 8 off.

### 5.6 Distortion, volume, pan, velocity

| Control | Raw range | Internal behavior |
|---|---:|---|
| Voice distortion | 0–127 | Nonlinear shape mapping |
| Voice volume | 0–127 | Normalized 0–1 |
| Pan | 0–127 | Equal-power: L = sqrt((127-p)/127), R = sqrt(p/127) |
| Velocity amount | 0–127 | Normalized modulation depth |
| Velocity destination | Indexed | Selects modulation target |
| Velocity-to-volume | Boolean | Enables velocity gain |

Distortion mapping uses normalized x with `s = x - 1/128` and `k = 2*s/(1-s)`.

### 5.7 LFO controls

| Control | Raw range | Internal behavior |
|---|---:|---|
| Frequency | 0–127 | Cubic free-running frequency mapping |
| Amount | 0–127 | Modulation depth |
| Waveform | 0–7 | Eight LFO waveforms |
| Retrigger | 0–6 | Voice trigger source or none |
| Sync | 0–11 | Free through 1/32 divisions |
| Offset | 0–127 | Phase offset |
| Target | Indexed | Modulation destination |
| Voice | Indexed | Target voice selection |

## 6. Embedded InitKit values

These are decoded raw parameter values from the embedded `InitKit`, not generic DSP constructor defaults and not a claim about a separate factory preset bank.

### 6.1 DrumVoice InitKit

| Parameter | Drum 1 | Drum 2 | Drum 3 |
|---|---:|---:|---:|
| Main waveform | 0 | 0 | 0 |
| Coarse | 31 | 41 | 58 |
| Fine | 63 | 63 | 63 |
| Mod waveform | 0 | 0 | 0 |
| Filter cutoff | 127 | 127 | 127 |
| Filter resonance | 0 | 0 | 0 |
| Amp attack | 0 | 0 | 0 |
| Amp decay | 21 | 13 | 3 |
| Amp slope | 46 | 11 | 4 |
| Pitch decay | 26 | 93 | 7 |
| Pitch amount | 44 | 14 | 127 |
| Pitch slope | 22 | 10 | 14 |
| FM amount | 0 | 0 | 0 |
| FM frequency | 0 | 0 | 0 |
| Volume | 127 | 127 | 127 |
| Pan | 63 | 63 | 63 |
| Distortion | 63 | 63 | 63 |
| LFO frequency/amount | 0/0 | 0/0 | 0/0 |
| Filter drive/type raw | 0/0 | 0/0 | 0/0 |
| Transient volume/wave/pitch | 0/0/0 | 0/0/0 | 0/0/0 |
| PWM | 0 | 0 | 0 |

Velocity modulation settings are zero for all three voices.

### 6.2 Snare InitKit

| Parameter | Raw value |
|---|---:|
| Tonal waveform / coarse / fine | 1 / 45 / 64 |
| Noise frequency | 127 |
| Noise/oscillator mix | 61 |
| PWM | 0 |
| Amp attack / decay / repeat / slope | 0 / 10 / 0 / 10 |
| Pitch decay / slope / amount | 14 / 15 / 81 |
| Velocity destination / amount / volume enable | 0 / 0 / 0 |
| Transient waveform / volume / pitch | 0 / 0 / 0 |
| Filter cutoff / resonance / type / drive | 127 / 0 / 0 / 0 |
| LFO frequency / amount / waveform / retrigger / sync / offset | 0 / 0 / 0 / 0 / 0 / 0 |
| Volume / pan / distortion | 127 / 63 / 63 |

### 6.3 Cymbal InitKit

| Parameter | Raw value |
|---|---:|
| Carrier waveform / coarse / fine | 4 / 127 / 63 |
| Modulator waveforms 1 and 2 | 0 / 0 |
| Modulator frequencies 1 and 2 | 0 / 0 |
| Modulator gains 1 and 2 | 0 / 0 |
| PWM | 0 |
| Amp attack / decay / repeat / slope | 0 / 107 / 0 / 0 |
| Velocity destination / amount / volume enable | 0 / 0 / 0 |
| Transient waveform / volume / pitch | 0 / 0 / 0 |
| Filter cutoff / resonance / type / drive | 68 / 0 / 1 / 0 |
| LFO frequency / amount / waveform / retrigger / sync / offset | 0 / 0 / 0 / 0 / 0 / 0 |
| Volume / pan / distortion | 127 / 63 / 63 |

The carrier starts as noise. Its FM input is ignored by the shared noise renderer.

### 6.4 HiHat InitKit

| Parameter | Raw value |
|---|---:|
| Carrier waveform / coarse / fine | 0 / 50 / 63 |
| Modulator waveforms 1 and 2 | 0 / 0 |
| Modulator frequencies 1 and 2 | 56 / 127 |
| Modulator gains 1 and 2 | 127 / 127 |
| PWM | 0 |
| Amp attack / closed decay / open decay / slope | 0 / 5 / 17 / 6 |
| Transient waveform / volume / pitch | 0 / 0 / 0 |
| Filter cutoff / resonance / type / drive | 104 / 0 / 1 / 0 |
| LFO frequency / amount / waveform / retrigger / sync / offset | 0 / 0 / 0 / 0 / 0 / 0 |
| Velocity destination / amount / volume enable | 0 / 0 / 0 |
| Volume / pan / distortion | 127 / 63 / 63 |

Both modulators start at maximum gain. Open decay is longer than closed decay.

### 6.5 InitKit length caveat

The embedded array declares 254 bytes and begins with an eight-byte header. The preset initializer copies from index 8 up to, but not including, `END_OF_SOUND_PARAMETERS = 247`, copying 239 parameter bytes (indices 0–238). It does not copy indices 239–246. The array's remaining bytes should not be treated as authoritative active preset values without resolving this format discrepancy.

All voice-specific values tabulated above are within the copied parameter range.

## 7. Firmware defaults versus embedded preset values

Do not conflate DSP initialization defaults with embedded InitKit raw values.

Examples of audited DrumVoice constructor/DSP defaults include:
- Voice volume 0.8.
- FM amount 0.5.
- Transient volume and pitch 1.
- Transient waveform 0.
- Filter cutoff state 0.25, resonance state 0.9, drive state 0.315, LP type.
- Pitch envelope depth 0.5 and decay 0.01.
- Amplitude attack/decay 0.01.
- LFO sine waveform, frequency state 1, amount 0, sync off, retrigger off.
- Main waveform differs by drum voice; modulator waveform 1.
- Oscillator mix mode enabled when compiled with `ENABLE_MIX_OSC`.

Preset loading subsequently supplies parameter values. Treat constructor defaults, parameter defaults, and embedded InitKit values as separate layers.

## 8. Tracker porting boundary and host dependencies

### Keep inside the extracted DSP

- Four engine implementations and their voice-local state.
- Oscillator/wavetable generation.
- Envelope implementations.
- Resonant filter.
- Transient sample playback and special snap/offset behavior.
- Voice-local distortion.
- LFO and velocity modulation DSP primitives, adapted to Tracker parameter descriptors.
- Required interpolation, buffer, saturation, and random helpers.

### Replace or remove

| LXR dependency | Tracker treatment |
|---|---|
| STM32 HAL RNG | Portable, deterministic-testable RNG interface |
| `REAL_FS` | Actual Tracker host sample rate |
| `OUTPUT_DMA_SIZE` | Actual Tracker render block size |
| `seq_getBpm()` | Tracker tempo interface |
| `activeParameters` | Explicit Tracker parameter model |
| `MidiParser.c` | Typed parameter setters/converters |
| `ParameterArray.c` | Typed parameter descriptors; do not copy pointer table blindly |
| MIDI note-number table | Tracker note/frequency conversion or verified shared tuning table |
| `adc_getFaderVolume()` | Remove from voice DSP; use Tracker mixer only if required |
| FX bus global state | Keep master effects outside voice DSP |
| `mixer_audioRouting` | Tracker mixer/routing |
| LXR pan/output buffers | Tracker mixer |
| GPIO jack detection | Omit |
| Voice decimator | Omit unless explicitly required by Tracker design |
| Hardware fader and mixer smoothing | Tracker-level gain smoothing if required |
| CMSIS intrinsics (`__QADD16`, `__SSAT`, etc.) | Portable wrappers or equivalent host primitives |
| `INDTCM`, `IN_ITCM_RAM` | Remove |
| Sequencer trigger infrastructure | Tracker trigger interface |
| `USE_REVERB` filter switch | Remove; voice filter must not depend on master FX selection |

### Audio/control scheduling

The original mixer performs modulation dispatch and asynchronous updates once per output block, then renders voice blocks. Tracker's actual block size and sample rate may differ. The port must explicitly decide which operations are:
- Per-sample audio-rate.
- Per-block/control-rate.
- Trigger-time state changes.
- Host/mixer operations.

Do not assume original 64-sample block timing. The source configuration uses 64 samples with `USE_BIG_SAMPLE_BUFFER`, but a conditional DMA configuration can redefine the block size to 16. The source sample rate is 44,140 Hz. The Tracker host values must be queried from the target build/runtime.

## 9. Known issues and deliberate correction policy

| ID | Finding | Classification | Required action |
|---|---|---|---|
| DSP-001 | Oscillator mipmap selector maps positive frequencies below 440 Hz to table index 4 | Confirmed defect candidate | Correct selector; retain regression test against original |
| DSP-002 | Interpolating 1024-entry tables read index 1024 at row boundary | Memory-safety defect | Add guard/wrap logic; test all mipmap rows |
| DSP-003 | DecayEg slope raw 127 divides by zero and can yield NaN | Numerical defect | Define finite endpoint; test 126/127 |
| DSP-004 | Transient stop threshold prevents rendering declared sample index 2204 | Unresolved endpoint behavior | Preserve initially or correct only with explicit decision |
| DSP-005 | Transient early clear assumes even sample count | Buffer correctness issue | Clear exact requested sample count |
| DSP-006 | SlopeEg2 raw 127 yields zero attack/decay increment | Endpoint behavior | Define intended maximum-time/stuck-phase behavior |
| DSP-007 | SnapEg duration depends on control update cadence | Port timing hazard | Recalibrate or preserve explicitly for Tracker block cadence |
| DSP-008 | Cheap filter path omits normal-path NaN cleanup and differs arithmetically | Optimization risk | Use normal path as reference; benchmark later |
| DSP-009 | Modulation collisions overwrite by dispatch order | Implicit behavior | Define explicit Tracker collision policy |
| DSP-010 | ParameterArray pointers do not fully represent normal parameter writes | Architectural mismatch | Rebuild typed parameter descriptors from actual setter behavior |
| DSP-011 | InitKit declared size and copy length differ | Format ambiguity | Do not interpret uncopied tail as active parameter data |
| DSP-012 | Cymbal/HiHat filter algorithm selection depends on global reverb state | Host coupling | Remove global FX dependency from voice implementation |

**Policy:** classify each port behavior as one of:
- **Preserve:** intentional and safe original synthesis behavior.
- **Adapt:** original behavior depends on LXR hardware, timing, or host architecture.
- **Correct deliberately:** confirmed numerical or memory-safety defect, with a documented compatibility decision and regression test.

Never silently “fix” a sound-shaping behavior. Conversely, never preserve undefined memory access or NaN propagation merely for bit-level compatibility.

## 10. Validation plan and acceptance gates

### Gate A — oscillator correctness
- Test table selection across MIDI notes 0–120 and relevant frequency extremes.
- Verify expected mipmap level and harmonic content.
- Test interpolation at indices 1022 and 1023 for every table level.
- Test phase wrap and interpolation continuity.
- Test FM phase offsets for sine, saw, triangle, rectangle, and PWM.
- Verify that noise carrier ignores FM buffer, as in the source.
- Verify fine tuning at endpoints and midpoint.

### Gate B — envelopes and trigger behavior
- Test SlopeEg2 attack, decay, repeat, and slope endpoints.
- Test DecayEg slope 126 and 127; assert all state and outputs remain finite.
- Test pitch depth 0, midpoint, and 127.
- Test SnapEg at transient pitches 0.25, 1, and 4 under each supported block cadence.
- Verify each voice's exact trigger reset behavior, including filter/noise state distinctions.

### Gate C — transient playback
- Test pitches 0.25, 1, and 4.
- Verify exact sample index progression, volume scaling, endpoint, and retrigger.
- Test odd and even render block sizes.
- Verify modes 0 and 1 are handled by voice logic and sample modes use the transient table.

### Gate D — filter behavior
- Compare normal and cheap ZDF output and state evolution.
- Exercise all eight filter types.
- Test cutoff near zero, maximum, and the 0.4499 stability threshold.
- Test resonance near the clamp region.
- Test filter state after trigger for each voice.
- Verify maximum cutoff interpretation at the host sample rate.

### Gate E — modulation
- Test each LFO waveform and sync division.
- Test free-running frequency endpoints.
- Test phase offset and retrigger source behavior.
- Test multiple velocity/LFO sources targeting one parameter.
- Verify the chosen Tracker collision policy is deterministic and documented.
- Verify modulation of continuous versus discrete parameters is safe.

### Gate F — host integration
- Test actual Tracker sample rates (at least 44.1, 48, and 96 kHz if supported).
- Test actual host block sizes; include 16, 32, 64, and 128 in the standalone harness where practical.
- Verify tempo-synchronized LFO behavior.
- Verify no HAL, GPIO, DMA, LXR mixer, or FX-bus dependency remains in voice DSP.
- Run sanitizers or equivalent bounds checks in the host test harness.
- Test deterministic rendering with a seeded RNG.

### Acceptance criteria

The extraction is ready for integration only when:
1. All four architectures are independently represented.
2. Parameter conversions and defaults are traceable to source behavior.
3. All known critical defects have explicit correction decisions and regression tests.
4. Render timing is correct for the Tracker host sample rate and block size.
5. Voice DSP builds without LXR hardware, sequencer, MIDI parser, or mixer dependencies.
6. License and NOTICE requirements have been reviewed for the intended distribution.

## 11. Suggested implementation sequence

1. Build a host-side reference harness around the audited DSP and capture representative output/state fixtures.
2. Resolve oscillator mipmap and interpolation defects.
3. Port shared oscillator, wavetable, and random interfaces.
4. Port envelope implementations and numerical endpoint handling.
5. Port the ZDF filter and test all modes.
6. Port transient generator and sample tables.
7. Port voice-local distortion and required helpers.
8. Port DrumVoice, then Snare, then Cymbal, then HiHat, preserving architecture-specific behavior.
9. Introduce typed Tracker parameter descriptors and explicit modulation policy.
10. Integrate with Tracker trigger, tempo, sample-rate, and block interfaces.
11. Validate sound and behavior against reference fixtures.
12. Only then consider optimization, including the cheap filter path.

## 12. Primary source files

The following official source files form the core audit set at the pinned revision:

**Voice engines**
- `Core/Src/Soundengine/DSPAudio/DrumVoice.c`
- `Core/Inc/Soundengine/DSPAudio/DrumVoice.h`
- `Core/Src/Soundengine/DSPAudio/Snare.c`
- `Core/Inc/Soundengine/DSPAudio/Snare.h`
- `Core/Src/Soundengine/DSPAudio/CymbalVoice.c`
- `Core/Inc/Soundengine/DSPAudio/CymbalVoice.h`
- `Core/Src/Soundengine/DSPAudio/HiHat.c`
- `Core/Inc/Soundengine/DSPAudio/HiHat.h`

**Shared DSP**
- `Core/Src/Soundengine/DSPAudio/Oscillator.c`
- `Core/Src/Soundengine/DSPAudio/wavetable.c`
- `Core/Inc/Soundengine/DSPAudio/wavetable.h`
- `Core/Src/Soundengine/DSPAudio/ResonantFilter.c`
- `Core/Src/Soundengine/DSPAudio/SlopeEg2.c`
- `Core/Src/Soundengine/DSPAudio/Decay.c`
- `Core/Src/Soundengine/DSPAudio/snapEg.c`
- `Core/Src/Soundengine/DSPAudio/transientGenerator.c`
- `Core/Src/Soundengine/DSPAudio/distortion.c`
- `Core/Src/Soundengine/DSPAudio/lfo.c`
- `Core/Src/Soundengine/DSPAudio/modulationNode.c`
- `Core/Src/Soundengine/DSPAudio/BufferTools.c`
- `Core/Src/Soundengine/DSPAudio/random.c`

**Host, parameter, and configuration**
- `Core/Src/Soundengine/DSPAudio/mixer.c`
- `Core/Src/Soundengine/MIDI/MidiParser.c`
- `Core/Src/Soundengine/MIDI/ParameterArray.c`
- `Core/Inc/Parameters.h`
- `Core/Inc/Soundengine/config.h`
- `Core/Src/Preset/initKit.c`
- `Core/Inc/Preset/InitKit.h`
- `Core/Src/menu/Cc2Text.c`
- `Core/Src/Preset/presetManager.c`

---

**Document status:** This specification captures the source-audit findings and porting decisions known at the pinned baseline. Any unresolved item is explicitly marked; do not infer that an unverified behavior has been settled. Update this document as follow-up source checks and reference tests resolve those items.
