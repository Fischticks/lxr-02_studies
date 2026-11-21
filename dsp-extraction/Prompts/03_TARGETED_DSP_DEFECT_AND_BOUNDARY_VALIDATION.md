# Prompt 03 — Targeted DSP Defect and Boundary Validation

## Objective

Use the pinned original LXR-02 source and the now-validated oscillator host spike as the starting point for a focused investigation of the highest-risk numerical and memory-safety concerns identified in the extraction specification.

This task must establish which suspected issues are real, characterize their consequences, and distinguish source defects from test-harness artifacts. It is not permission to rewrite the DSP.

## Prerequisites

Before beginning:

1. Read `dsp-extraction/DSP_EXTRACTION_SPEC.md`.
2. Read the oscillator dependency spike report:
   `dsp-extraction/reference-harness/OSCILLATOR_DEPENDENCY_SPIKE.md`
3. Read the host spike design and current status:
   `dsp-extraction/reference-harness/REFERENCE_HARNESS_DESIGN.md`
   `dsp-extraction/reference-harness/host-spike/STATUS.md`
4. Read `HOST_SPIKE_BUILD_REPORT.md` and verify that Prompt 02 established a real build/test result.
5. Use only upstream revision `dca0a44413931a12de65941355f06c07598a9be5`.

If Prompt 02 did not reach a reproducible build, stop and report what prerequisite remains unresolved rather than pretending to run tests.

## Investigations

### A. Oscillator mipmap selection

- Trace the exact frequency-to-mipmap calculation and integer conversions.
- Evaluate representative frequencies below, at, and above 440 Hz, including very low positive values and boundary values.
- Establish the actual selected table rows.
- Determine whether the suspected integer truncation causes lower mipmap rows to be skipped or otherwise produces incorrect selection.
- Recommend a correction strategy, but do not change the upstream reference implementation.

### B. Wavetable interpolation boundaries

- Inspect all interpolated waveform table layouts and indexing expressions.
- Exercise indices near 0, 1022, and 1023, including the largest possible phase-derived index.
- Use AddressSanitizer and UndefinedBehaviorSanitizer where available.
- Determine whether index 1024 reads beyond a row, beyond the complete allocation, or into an adjacent row.
- Test the final mipmap row separately.
- Distinguish a logically incorrect cross-row read from a true allocation-level out-of-bounds access.
- Record whether behavior changes with compiler, optimization level, or sanitizer.

### C. Envelope numerical endpoints

Investigate:

- `DecayEg` at the maximum slope parameter and adjacent values.
- `SlopeEg2` attack/decay at minimum and maximum parameter values.
- Zero increments, divisions by zero, infinities, NaNs, stuck envelope phases, and unexpected completion behavior.
- Whether the problematic values are reachable through normal parameter mappings and initialization, or only through direct invalid inputs.

### D. Transient buffer boundaries

- Trace the transient playback phase threshold and final sample index.
- Determine whether the final stored sample is intentionally unreachable or accidentally omitted.
- Test clearing for both even and odd sample counts.
- Establish whether the final 16-bit sample remains stale for odd-sized buffers.
- Verify the exact buffer units (samples, bytes, or 32-bit words) at each operation.

### E. Filter implementation differences

- Compare normal and cheap ZDF filter code path by path.
- Identify arithmetic differences, state updates, nonlinearities, clipping, NaN handling, and mode behavior.
- Establish when each path is selected by the original host application.
- Do not assume the cheap path is equivalent to the normal path.
- Recommend which path should serve as the extraction reference and what compatibility implications follow.

### F. Control-rate and modulation behavior

- Trace actual invocation cadence for `SnapEg`, amplitude/pitch envelopes, and LFO dispatch.
- Determine how their time behavior changes with sample rate and block size.
- Verify update order and destination collision semantics for velocity and LFO sources.
- Confirm whether later writes overwrite earlier modulation values or whether values accumulate.
- Verify the role of `ParameterArray` pointers relative to the canonical active parameter and MIDI update paths.

## Test discipline

- Keep the original pinned source immutable.
- Prefer external test drivers, host-side shims, compile-time instrumentation, or temporary patches that are not applied to the upstream checkout.
- If a temporary patch is necessary, keep it outside the upstream checkout, document it, and make it mechanically reviewable.
- Run sanitizer tests with debug-friendly settings and, where useful, optimized settings.
- Record exact test inputs, commands, observed outputs, and reproducibility details.
- Do not infer a pass from a successful compile alone.
- Do not change production behavior or propose a fix as though it were already validated.

## Deliverables

Create:

1. `dsp-extraction/reference-harness/DSP_BOUNDARY_VALIDATION_REPORT.md`
   - One section per investigation.
   - Exact source locations and relevant expressions.
   - Test method and reproducible commands.
   - Observed results and evidence.
   - Classification: confirmed defect, confirmed behavior, not reproduced, or unresolved.
   - Severity and practical impact.
   - Recommended handling: preserve, compatibility correction, or defer pending more evidence.

2. `dsp-extraction/reference-harness/DSP_ISSUES_REGISTER.md`
   - Concise issue table with ID, component, severity, evidence, consequence, reachability, and recommended disposition.
   - Link each issue to the detailed report section.

3. Any small standalone test drivers or scripts needed to reproduce the findings, stored under:
   `dsp-extraction/reference-harness/tests/`

Do not modify the original extraction specification in place. Do not port code to Tracker.

## Completion gate

Stop after the evidence-backed report and reproducible tests are committed. Identify which findings materially affect the extraction baseline and which can safely wait. Await review before expanding the harness or changing the specification.
