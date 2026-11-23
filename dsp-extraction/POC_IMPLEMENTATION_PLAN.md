# LXR-02 Tracker Proof of Concept — Implementation Plan

**Status:** Proposed implementation path, approved direction to pursue; detailed code work not yet started  
**Source baseline:** Erica Synths LXR desktop app `dca0a44413931a12de65941355f06c07598a9be5`  
**Tracker baseline:** Public `polyend/tracker-ce` repository and its Synth Plugin SDK  
**Integration destination:** `jamartinez4991/Tracker_LXR_02`

## 1. Objective

Build a working LXR-derived `.psyn` proof of concept quickly, then expand and correct it through listening, testing, and targeted engineering follow-up.

Do not make completion of the standalone oscillator host spike or the full reference harness a prerequisite. Retain their source-audit findings and use lightweight host tests, compiler diagnostics, and sanitizers as part of the actual extraction where practical.

The POC must establish the essential end-to-end path:

1. Build a standalone ARM `.psyn` using the public Tracker CE SDK.
2. Load it in Tracker CE.
3. Receive Tracker note events.
4. Trigger LXR-derived synthesis.
5. Expose a small, useful set of patch parameters.
6. Save and reload a patch.
7. Demonstrate that the engine can grow into a multi-part instrument without redesigning its host interface.

## 2. Recommended architecture

### 2.1 One Tracker host voice, multiple internal sound generators

Use a custom C++ class derived directly from `polysynth::InstrumentVoice`, exported through `PSYN_REGISTER_CPP_VOICE` in `psyn_cpp_bridge.h`.

Do not start with `CustomSynthVoice` as the primary base class. Its standard note-stack/play-mode behavior is useful for conventional synth voices, but this instrument needs custom internal part dispatch and aggregate activity/note reporting.

The plugin should initially advertise one host voice (default and maximum voice count both 1). That one host voice owns the internal LXR part state. This is the PERC-style architecture: Tracker allocates one instrument voice, while the plugin manages its own sound generators and simultaneous tails internally.

Implement the required `InstrumentVoice` lifecycle and query methods carefully. In particular, `isActive()` must remain true while any internal sound generator is producing a tail; note-query methods must consistently represent the plugin's internal note/trigger state.

### 2.2 Incremental internal engine scope

Build in two steps:

**POC 1 — Three DrumVoice parts**

- Instantiate three independent LXR `DrumVoice` states.
- Preserve their independent oscillator, envelope, transient, filter, and distortion state.
- Dispatch incoming notes to an internal part through a small Tracker-side note map.
- Allow multiple parts to ring simultaneously inside the one host voice.
- Use the audited embedded InitKit values for initial part defaults, while keeping constructor defaults and parameter defaults distinct.

**POC 2 — Complete LXR part set**

Add the remaining audited instances without changing the host-facing architecture:

- One Snare.
- One CymbalVoice.
- One shared HiHat instance supporting open and closed trigger variants.

The final internal topology is three DrumVoice instances, one Snare, one CymbalVoice, and one shared HiHat (six internal sound-generator instances across four architectures).

Do not merge the four LXR architectures into a generic voice implementation.

## 3. Trigger mapping

The original LXR sequencer/trigger infrastructure is excluded from the extraction. Tracker note events therefore need an explicit plugin-side routing policy.

For the first POC, use a simple, documented, replaceable MIDI-note map. A practical initial mapping may use three adjacent or conventional drum-note values for DrumVoice 1–3. Treat the actual note assignments as a Tracker adaptation, not as source-derived LXR behavior.

Keep routing isolated in one function/table so later work can add:
- Snare trigger.
- Cymbal trigger.
- HiHat closed trigger.
- HiHat open trigger.

Do not infer originating Tracker sequencer track identity from the SDK event: the audited SDK interface exposes MIDI note/event information, not a guaranteed Tracker track-source identifier. Pitch-based dispatch avoids depending on unavailable source identity.

Before implementing note-off behavior, inspect the original DrumVoice trigger and envelope call paths. Preserve one-shot percussion behavior unless the source establishes a required gate/release behavior.

## 4. Parameters and patch layout

For POC 1, expose only a compact set of musically useful DrumVoice controls. Prefer controls that demonstrate the LXR architecture and are already mapped in the source audit, for example:
- Main waveform / oscillator mode.
- Coarse or fine tuning.
- FM amount.
- Pitch-envelope amount or decay.
- Filter cutoff and resonance.
- Amplitude decay.
- Transient selection/amount where the source mapping is clear.

Do not attempt to expose every LXR parameter in the first iteration. Do not invent conversion curves: use the audited source handlers and parameter mappings. Keep raw LXR values, internal DSP values, SDK patch values, and displayed values clearly distinguished.

Use stable parameter IDs and fixed parameter storage. Keep the layout within the SDK's supported section count. A future section per internal part is preferable to dynamic changes in parameter count or index.

Patch serialization must preserve all parameters that are part of the POC. Test Init Patch, save, reload, and parameter persistence on device.

## 5. DSP extraction strategy

Use the pinned LXR source audit to identify the smallest dependency closure for DrumVoice. Extract only what is required by the three DrumVoice instances; do not port Snare/Cymbal/HiHat-only code until POC 2.

Likely shared dependencies include oscillator/wavetable, random source, amplitude and pitch envelopes, snap/transient handling, resonant filter, buffer/saturation helpers, and distortion. Confirm actual compile-time and call dependencies from the pinned source rather than assuming this list is complete.

Keep LXR host dependencies out of the plugin:
- No LXR sequencer, MIDI parser, preset manager, or global parameter array.
- No LXR mixer, master FX bus, fader, GPIO, DMA, HAL, or STM32 memory placement.
- No audio-thread allocation, blocking file I/O, or dynamic container growth.

Replace hardware RNG with a small deterministic-testable plugin-local RNG interface. Use the actual Tracker sample rate and render block size. Preserve the nonlinear ZDF filter architecture.

## 6. Known source issues to handle deliberately

The source audit already identifies these high-priority risks:

- Oscillator mipmap selection for frequencies below 440 Hz.
- Wavetable interpolation at index 1023.
- DecayEg maximum-slope non-finite behavior.
- SlopeEg2 maximum attack/decay endpoint behavior.
- Transient endpoint and odd-sized buffer clearing.
- SnapEg control-rate dependence.
- LXR global modulation and parameter-pointer plumbing.

For the POC:
- Do not spend time building a complete reference framework first.
- Do not preserve undefined memory access or NaN propagation.
- Apply only small, evidence-backed safety corrections needed for stable operation.
- Record each deliberate divergence from the pinned source.
- Defer broader modulation redesign, LFO routing UI, and sonic refinements until the basic instrument works.

## 7. Build and validation checkpoints

### Checkpoint A — SDK shell

A minimal plugin builds and packages as `.psyn`, loads in Tracker, appears in the synth selector, and produces a controlled test signal.

### Checkpoint B — First LXR sound

One DrumVoice renders through the Tracker plugin interface, responds to a note trigger, and exposes a minimal patch parameter set.

### Checkpoint C — Three internal DrumVoice parts

Three independent DrumVoice instances can be triggered by their mapped notes and sustain overlapping tails while the plugin still uses one Tracker host voice.

### Checkpoint D — Patch persistence and stability

Init Patch, save/reload, parameter updates, retriggering, All Notes Off/reset behavior, and idle/sleep reporting are verified. No allocation or blocking calls occur in real-time callbacks.

### Checkpoint E — POC listening pass

Test on actual Tracker hardware. Record sound quality, parameter behavior, CPU/resource behavior, and any audible differences or defects. Do not call this a complete LXR port.

## 8. Later expansion

After the three-DrumVoice POC works:

1. Add Snare, preserving its noise-only filter path and distinct trigger behavior.
2. Add Cymbal, preserving its two independent modulators and carrier/filter/transient ordering.
3. Add the shared HiHat, preserving open/closed decay selection and its distinct carrier FM gain.
4. Expand patch layout and trigger mapping.
5. Revisit LFO/velocity modulation and define explicit Tracker modulation collision behavior.
6. Build broader host-side characterization tests around the actual extracted code.
7. Profile and optimize only after the complete sound set is functionally correct.

## 9. Repository and licensing constraints

- Public `polyend/tracker-ce` remains the architectural/build baseline and is read-only.
- Work on actual integration only in `jamartinez4991/Tracker_LXR_02`.
- Keep source research and validation artifacts in `Fischticks/lxr-02_studies`.
- Keep feature documentation in `Fischticks/ChatGPT_Prototyping`.
- Do not modify the user's personal Tracker firmware repository as part of this project.
- Preserve upstream GPL-3.0-only notices and review the complete `LICENSE` and `NOTICE.md` before distributing source or binaries.

## 10. Explicitly deferred

The following are not POC prerequisites:
- Standalone oscillator compile spike.
- Full standalone reference harness.
- Exhaustive independent review of every audited DSP path.
- Full LFO/modulation matrix and UI.
- Exact sonic parity across all four architectures.
- Optimization of the cheap filter path.

These remain follow-up work, not forgotten requirements.
