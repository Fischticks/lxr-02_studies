# Prompt 04 — Build the Standalone LXR-02 DSP Reference Harness

## Objective

Build out the standalone host-side reference harness in the study repository so the original LXR-02 DSP can be rendered, controlled, measured, and used as a trustworthy oracle for a future extracted implementation.

This is still a reference-harness project. It is not Tracker CE integration and must not become a port.

## Prerequisite review

Before implementation, read:

- `dsp-extraction/DSP_EXTRACTION_SPEC.md`
- `dsp-extraction/reference-harness/REFERENCE_HARNESS_DESIGN.md`
- `dsp-extraction/reference-harness/OSCILLATOR_DEPENDENCY_SPIKE.md`
- `dsp-extraction/reference-harness/host-spike/HOST_SPIKE_BUILD_REPORT.md`
- `dsp-extraction/reference-harness/DSP_BOUNDARY_VALIDATION_REPORT.md`
- `dsp-extraction/reference-harness/DSP_ISSUES_REGISTER.md`

Also review the outcomes of Prompts 01–03 and any decisions made during human review.

If any prerequisite report is missing, or the host spike has not achieved a reproducible build, do not begin implementation. Report the blocker.

If the audit identifies unresolved architectural decisions that would change the harness design, present those decisions and wait rather than guessing.

## Architecture requirements

The harness must:

- Compile the original DSP source from a separate checkout pinned to:
  `dca0a44413931a12de65941355f06c07598a9be5`
- Never vendor or silently copy upstream source into the study repository.
- Keep the original reference target separate from any future extracted/port target.
- Use portable host shims for hardware-specific services.
- Make random-number generation deterministic and injectable.
- Allow explicit sample-rate and block-size configuration.
- Expose controlled parameter setup and voice triggering.
- Render repeatable audio blocks.
- Capture final output and selected intermediate signals.
- Write documented audio artifacts (for example WAV) and machine-readable render manifests.
- Support automated unit, characterization, regression, and sanitizer tests.
- Make test runs reproducible from documented commands.

Retain the four distinct synthesis architectures:

1. Three independent DrumVoice instances.
2. Snare.
3. Cymbal.
4. HiHat, with open/closed decay selection on its shared voice.

Do not merge them into one generic voice implementation.

## Implementation sequence

Work incrementally, with a build/test checkpoint after each stage.

### Stage 1 — Harness foundation

- Stabilize CMake configuration and pinned-source validation.
- Separate host compatibility shims from test drivers.
- Establish deterministic RNG injection and document its algorithm/seed behavior.
- Replace provisional stubs with faithful host equivalents wherever required for the test scope.
- Add a basic command-line test runner and clear failure reporting.
- Add a smoke test that proves the reference source is compiled and executed.

### Stage 2 — Oscillator characterization

- Expand coverage to every supported oscillator waveform.
- Exercise tuning and phase increment calculations.
- Add mipmap-selection tests and interpolation-boundary tests.
- Preserve tests for known defects so later compatibility fixes cannot silently alter behavior.
- Add sanitizer configurations and verify that they actually detect a deliberately introduced test-only fault before relying on them.

### Stage 3 — Shared DSP characterization

Add independent tests for:

- Resonant ZDF filter modes and state behavior.
- Amplitude and pitch envelopes.
- Snap envelope.
- Transient playback.
- Distortion and relevant lookup/interpolation helpers.
- LFO and modulation-node behavior.
- Deterministic random-dependent paths.

### Stage 4 — Voice-level characterization

Add controlled render/trace tests for:

- DrumVoice mixing mode and FM mode.
- Snare noise/tonal balance and transient bypass.
- Cymbal carrier with two independent modulators.
- HiHat carrier with two modulators and open/closed decay selection.
- Trigger retriggering, phase resets, filter-state handling, velocity, and pan where those behaviors are within the isolated DSP boundary.

### Stage 5 — Reference artifacts and regression format

- Define a stable render manifest containing source revision, harness revision, compiler/build configuration, sample rate, block size, RNG seed/script, parameter values, trigger events, and output file hashes.
- Define deterministic comparison metrics (exact sample equality where appropriate, otherwise documented tolerances and signal metrics).
- Store only small, intentional fixtures in the repository; avoid committing large generated WAV collections without justification.
- Document how a future independent port target will consume the same test vectors and compare against the reference.

## Test coverage and acceptance criteria

At minimum, support:

- Multiple sample rates and block sizes.
- Deterministic repeated renders.
- Every oscillator waveform and boundary conditions.
- All four voice architectures and their distinctive signal paths.
- All resonant filter modes.
- Envelope/transient endpoints and numerical stability.
- LFO timing, retriggering, and modulation collisions.
- Sanitizer-enabled test runs.
- Machine-readable render metadata and reproducible audio output.

A stage is complete only when its tests run and the results are recorded. A successful build alone is not sufficient evidence of DSP correctness.

## Constraints and repository boundaries

- Work only in `Fischticks/lxr-02_studies`, branch `main`, under `dsp-extraction/`.
- Do not modify the Tracker integration repository `jamartinez4991/Tracker_LXR_02`.
- Do not create Tracker implementation branches or copy files into the integration repository.
- Do not modify the official upstream source checkout.
- Keep reference-source changes, if any are absolutely unavoidable for instrumentation, out of the pinned checkout and clearly separated as external test patches.
- Preserve original behavior; do not silently repair suspected DSP defects during harness construction.
- Record every shim and its behavioral assumptions.
- Keep host/mixer optimizations distinct from intrinsic voice DSP.
- Do not optimize the DSP or begin the Tracker port as part of this task.

## Deliverables

Create or update, as appropriate:

- Harness source and build configuration under `dsp-extraction/reference-harness/`.
- Test drivers and test data under `dsp-extraction/reference-harness/tests/`.
- `HARNESS_USER_GUIDE.md` with setup, build, run, and troubleshooting instructions.
- `HARNESS_COVERAGE_MATRIX.md` mapping specification requirements to tests and evidence.
- `HARNESS_ARCHITECTURE.md` describing targets, shims, data flow, determinism, artifacts, and extension points.
- A concise progress report identifying completed stages, known limitations, and next gates.

## Completion gate

Do not declare the reference harness complete until all required voice architectures can be instantiated and rendered deterministically, the core shared DSP has characterization coverage, and the test artifacts are reproducible.

Stop before any Tracker CE integration. Present the completed harness, test results, coverage gaps, and a recommendation for the next human-approved step.
