# Claude Task — LXR-02 POC 1: Three DrumVoice Parts as a Tracker .psyn

## Objective

Implement the first working LXR-02 proof of concept as a standalone Tracker CE `.psyn` plugin. The POC should use one Tracker host voice to manage three independent internal LXR `DrumVoice` instances.

This task intentionally bypasses the standalone oscillator host compile spike and full reference harness. Validate the actual extracted/plugin code as you build it. Do not spend time constructing an intermediate test framework.

## Authoritative documents and source baselines

Read before coding:

- `dsp-extraction/DSP_EXTRACTION_SPEC.md`
- `dsp-extraction/POC_IMPLEMENTATION_PLAN.md`
- The source audit sections for DrumVoice, shared DSP, parameter mappings, InitKit values, host dependencies, and known defects.
- Public Tracker CE Synth Plugin SDK at the pinned revision currently used for this task:
  `c1713de8a63c743504d1b96fc56e6de4e24c8177`
- SDK files:
  - `synth-plugin-sdk/README.md`
  - `synth-plugin-sdk/include/psyn/psyn_cpp_bridge.h`
  - `synth/include/PolySynth/sdk/InstrumentVoice.h`
  - `synth/include/PolySynth/sdk/InstrumentType.h`
  - `synth/include/PolySynth/sdk/SynthVoiceSDK.h`
  - `synth-plugin-sdk/engines/chip.cpp`
  - `docs/custom_synth_sdk.md`

Original LXR source baseline:

`erica-synths/lxr-desktop-app` commit `dca0a44413931a12de65941355f06c07598a9be5`

## Repository boundaries

- Implement in `jamartinez4991/Tracker_LXR_02`.
- Use public `polyend/tracker-ce` as the read-only build/architecture baseline.
- Keep research and test notes in `Fischticks/lxr-02_studies`.
- Do not modify `Fischticks/tracker_ce_CleanEdits`.
- Do not modify the official Tracker CE repository.
- Do not merge any pull request.
- Preserve LXR copyright/license notices. Review `LICENSE` and `NOTICE.md`; do not distribute binaries until applicable terms are understood.

If the integration repository is still empty, initialize only the minimum project structure needed for this POC. Do not copy the full Tracker CE repository into it. Build against a separate checkout/worktree of public Tracker CE and link/use the official SDK as intended.

## Required architecture

### One host voice, three internal parts

Implement a custom C++ voice derived directly from `polysynth::InstrumentVoice`, registered with `PSYN_REGISTER_CPP_VOICE`.

Do not use `CustomSynthVoice` as the main base class for this POC. We need custom internal note routing and aggregate state reporting.

Configure the plugin for one host voice (default voices 1, maximum voices 1). The one host voice owns three independent internal `DrumVoice` instances.

The three internal instances must have independent:
- Oscillator phases and state.
- Pitch and amplitude envelope state.
- Transient and snap state.
- Filter state.
- Distortion state.
- Trigger/retrigger state.

All three may have overlapping tails and must be mixed into the plugin output correctly.

### Trigger routing

Create one isolated, documented MIDI-note-to-part routing table/function. Use a simple provisional mapping for the three DrumVoice parts; clearly label it as a Tracker adaptation, not original LXR behavior.

Do not assume the SDK supplies Tracker sequencer-track identity. Route from note/event data actually exposed by `MidiEvent`.

Inspect the original DrumVoice trigger path before implementing note-off behavior. Preserve the source's one-shot envelope behavior unless the source proves note-off is required.

### InstrumentVoice lifecycle

Implement all required virtual methods from the exact SDK header:
- `onMidiEvent`
- `onParamChange`
- `isActive`
- `handlesNote`
- `handlesAnyNote`
- `getLastNote`
- `getCurrentNoteVelocity`

Also implement `reset()` if required for clean voice reuse.

Aggregate activity across all three internal parts. A tail on any part means the host voice remains active. Ensure note queries and note release handling are consistent with the chosen one-shot model.

Do not opt into idle sleep until `isActive()` is demonstrably correct. Never claim a voice is inactive while any internal generator can still emit a tail.

## DSP scope

Extract only the dependency closure required by the three DrumVoice instances from the pinned LXR source. Verify actual includes, called functions, compile-time flags, and shared state. Do not bring in LXR application infrastructure.

Expected shared components may include:
- Oscillator and wavetable.
- Portable random source.
- SlopeEg2 amplitude envelope.
- Decay pitch envelope.
- SnapEg and transient generator.
- Resonant ZDF filter.
- Buffer/saturation helpers.
- Voice-local distortion.
- Any LFO/velocity component actually required by the audited DrumVoice path.

Do not assume this list is complete; establish it from source.

Remove or replace:
- STM32 HAL and hardware RNG.
- CMSIS intrinsics with tested equivalents where needed.
- LXR MIDI parser and sequencer dependencies.
- Global `activeParameters` / `ParameterArray` plumbing.
- LXR mixer, fader, pan routing, and master FX dependencies.
- DMA, GPIO, and embedded memory-section attributes.

Use Tracker's actual sample rate and output buffer length. Do not hard-code the original 44,140 Hz or 64-sample block size.

No heap allocation, container growth, blocking I/O, or file access in audio callbacks.

## Parameter and patch UI

Start with a compact parameter set per DrumVoice part. Use source-audited parameter conversions and InitKit values. A sensible first set includes waveform/mode, tuning, FM amount, pitch-envelope control, filter cutoff/resonance, amplitude decay, and transient amount/selection where the source mapping is unambiguous.

Do not expose every LXR parameter yet. Do not invent parameter curves. Keep parameter IDs stable and patch storage fixed. Use no more than the SDK-supported number of editor sections.

Ensure Init Patch, parameter updates, save, and reload work.

## Defect handling

The source audit identifies:
- Mipmap selection issue below 440 Hz.
- Wavetable interpolation at index 1023.
- DecayEg maximum-slope non-finite endpoint.
- SlopeEg2 maximum attack/decay endpoint.
- Transient endpoint and odd-sized clearing.
- SnapEg control-rate dependence.

Do not blindly copy unsafe behavior. Fix only issues necessary for stable POC operation, keep corrections small, and record each divergence with source location and rationale. Add focused tests for any correction. Do not start broad DSP optimization or sonic redesign.

## Build and validation

Build the `.psyn` using the SDK toolchain and packaging flow. Verify:
1. Plugin builds and packages.
2. Plugin loads and appears in Tracker's synth selector.
3. A note triggers each internal DrumVoice part.
4. Multiple parts can sound simultaneously and tails remain intact.
5. The compact parameter set changes sound as expected.
6. Init Patch and patch save/reload work.
7. Retriggering and All Notes Off/reset do not leave stuck or ghost voices.
8. No audio callback performs allocation or blocking work.
9. Basic CPU/resource use is acceptable for a proof of concept.

Run host tests or sanitizers where available, but do not let creation of a separate host spike block implementation. Hardware validation is required before calling the POC demonstrated.

## Deliverables

In the integration repository:
- Minimal buildable `.psyn` project.
- Extracted/ported DrumVoice DSP and required shared components.
- Patch metadata and compact editor layout.
- Build/deployment instructions.
- Tests for trigger routing, aggregate activity, parameter conversion, and any corrected boundary behavior.

In the study repository, add:
- `dsp-extraction/POC1_IMPLEMENTATION_REPORT.md`
- A source-lineage table for each extracted LXR file.
- A list of deliberate divergences/corrections.
- Build, test, packaging, and hardware results with exact evidence.
- Known limitations and next-step recommendation.

In the companion documentation repository, update the LXR feature status and roadmap when the POC reaches a verified milestone.

## Stop conditions

Stop and report rather than guessing if:
- The SDK's direct InstrumentVoice registration/ABI path is incompatible with the chosen architecture.
- A required LXR dependency cannot be separated without importing broad application state.
- Parameter mappings cannot be traced to the audited source.
- License/NOTICE terms create a distribution blocker.
- The plugin loads but its note routing or activity lifecycle cannot be made reliable.

Do not claim completion based on compilation alone. Do not merge anything.
