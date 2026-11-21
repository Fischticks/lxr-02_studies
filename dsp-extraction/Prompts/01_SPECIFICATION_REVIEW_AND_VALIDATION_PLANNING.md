# Prompt 01 — LXR-02 DSP Extraction: Specification Review and Validation Planning

We have completed the initial source audit of the Erica Synths LXR-02 desktop application and produced a comprehensive extraction specification.

**Specification:** `dsp-extraction/DSP_EXTRACTION_SPEC.md`  
**Repository:** `Fischticks/lxr-02_studies`  
**Branch:** `main`  
**Original specification commit:** `c2043b67c35f565b997da01966ecc9b102717388`

**Audited upstream source baseline:**

`dca0a44413931a12de65941355f06c07598a9be5`

Your task is to independently review the specification against the actual source code at that exact commit.

## Phase 1 — Source-to-specification audit

Read the entire `dsp-extraction/DSP_EXTRACTION_SPEC.md` and independently verify its technical claims against the pinned upstream source.

Pay particular attention to:

1. The four distinct voice architectures and their actual signal paths.
2. Trigger behavior, oscillator initialization, envelope behavior, and filter-state handling.
3. Parameter mappings, nonlinear conversions, and initialization values.
4. Shared DSP dependencies and their actual runtime behavior.
5. LFO and velocity modulation routing, including update order and destination collisions.
6. Host dependencies that must be replaced or separated during extraction.
7. Numerical edge cases, memory-safety concerns, and suspected source defects.
8. The distinction between intrinsic synthesis behavior and host/mixer optimizations.

Do not simply confirm the specification. Independently inspect the implementation and challenge its conclusions.

For every discrepancy, classify it as one of:

- **Incorrect:** The specification contradicts the source.
- **Incomplete:** The specification omits relevant behavior.
- **Ambiguous:** The source behavior or its interpretation is not sufficiently established.
- **Unverified:** The claim requires an experiment or runtime test.
- **Confirmed:** The specification accurately describes the source.

Include exact source file paths and relevant functions or code locations wherever possible.

## Phase 2 — Resolve critical technical uncertainties

Give special attention to these known concerns:

- Oscillator mipmap selection for frequencies below 440 Hz.
- Wavetable interpolation at index 1023 and potential out-of-bounds reads.
- `DecayEg` behavior at maximum pitch-envelope slope.
- `SlopeEg2` behavior at attack and decay endpoints.
- Transient playback endpoint behavior and odd-sized buffer clearing.
- Differences between normal and cheap ZDF filter implementations.
- Control-rate timing dependencies in `SnapEg`, envelopes, and LFOs.
- Multiple modulation sources targeting the same parameter.
- The actual role of `ParameterArray` pointers versus the normal MIDI parameter-update path.

Do not silently assume that a suspected defect is real or that a proposed correction is behaviorally safe. Establish what the source does, identify the consequences, and recommend a correction strategy where appropriate.

## Phase 3 — Produce a standalone DSP validation plan

Before beginning Tracker integration, recommend a standalone host-side reference/test harness that can exercise the original DSP independently of the Tracker firmware.

The harness should make it possible to:

- Instantiate and trigger each of the four voice architectures.
- Render deterministic audio blocks.
- Control sample rate and block size.
- Supply deterministic random values.
- Set parameters directly.
- Capture intermediate DSP signals where useful.
- Compare reference output against future extracted implementations.
- Test boundary conditions and numerical stability.

Specify the minimum test cases, expected observations, and acceptance criteria.

At minimum, cover:

- Oscillator waveforms, tuning, mipmap selection, and interpolation boundaries.
- DrumVoice mixing and FM modes.
- Snare noise/tonal blend and transient routing.
- Cymbal and HiHat dual-modulator FM behavior.
- HiHat open/closed decay selection.
- Envelope and transient edge cases.
- All resonant filter modes.
- LFO timing, retriggering, and modulation collisions.
- Multiple sample rates and block sizes.

Distinguish tests that can be performed immediately against the original source from tests that require a modified or instrumented version.

## Phase 4 — Recommend the implementation sequence

Based on your findings, propose a dependency-aware implementation sequence for eventual Tracker CE integration.

Keep the following principles:

- Preserve the four voice architectures separately.
- Extract reusable DSP components without unnecessarily rewriting their underlying behavior.
- Preserve the nonlinear ZDF filter architecture rather than substituting a generic biquad.
- Separate voice-local DSP from global mixer and master-FX behavior.
- Replace HAL-specific dependencies with portable interfaces.
- Replace LXR's global parameter/modulation plumbing with explicit Tracker-compatible parameter descriptors.
- Preserve original behavior unless a defect or host incompatibility justifies a documented change.
- Keep compatibility corrections distinct from intentional design improvements.
- Validate extracted DSP against the original reference before optimizing.

## Important project boundaries

This is an audit and preparation task, not an implementation task.

**Do not begin porting DSP code yet.**

Do not modify the Tracker integration repository:

`jamartinez4991/Tracker_LXR_02`

Do not copy files into it, create implementation branches, or begin Tracker CE integration.

All review notes, test plans, and any supporting documentation should remain in:

`Fischticks/lxr-02_studies`

Place new documents under `dsp-extraction/`; keep the repository root clean. Do not modify the existing specification in place until you have presented your findings and we have reviewed them.

## Deliverables

Please return:

1. **Specification audit report** — confirmed claims, discrepancies, omissions, and unresolved questions.
2. **Critical issues register** — suspected defects, severity, evidence, consequences, and recommended handling.
3. **Standalone validation plan** — test architecture, test cases, and acceptance criteria.
4. **Recommended implementation sequence** — dependency-aware phases with prerequisites and completion gates.
5. **Go/no-go recommendation** — whether the extraction specification is sufficiently reliable to serve as the implementation baseline, and what must be resolved first.

Be rigorous. The objective is to establish a trustworthy, testable DSP reference before any porting begins—not to rush toward implementation.
