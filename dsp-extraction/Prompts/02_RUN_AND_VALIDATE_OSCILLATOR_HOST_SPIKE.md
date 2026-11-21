# Prompt 02 — Run and Validate the Oscillator Host Compile Spike

## Objective

Run the existing minimal host-side compile spike for the original LXR-02 oscillator implementation. Establish an actual, reproducible build result before expanding the reference harness.

This is a build-validation task, not a DSP port.

## Repository and files

Study repository: `Fischticks/lxr-02_studies`  
Branch: `main`

Existing spike directory:

`dsp-extraction/reference-harness/host-spike/`

Read all files in that directory, especially:

- `CMakeLists.txt`
- `README.md`
- `STATUS.md`
- `oscillator_smoke.c`
- `host_support.c`
- the files under `shims/`

The spike is designed to compile the original upstream `Oscillator.c` and `wavetable.c` directly from a separately checked-out upstream source tree.

Pinned upstream revision:

`dca0a44413931a12de65941355f06c07598a9be5`

## Required work

1. Obtain or use a separate checkout of the official upstream repository at the exact pinned revision.
2. Verify the checkout's HEAD SHA before configuring the spike. Do not silently use another revision.
3. Configure and build the host spike using the documented `LXR_SOURCE_DIR` mechanism.
4. Run the oscillator smoke test.
5. Record the exact compiler, compiler version, CMake version, platform, configure command, build command, and test command.
6. If the build fails, diagnose the first meaningful failure and continue iteratively until either:
   - the spike builds and its smoke test runs, or
   - a concrete blocker is established and documented.
7. Make only the smallest changes needed to the spike's own files under `dsp-extraction/reference-harness/host-spike/`. Do not change upstream source to make the host build pass.
8. Inspect compiler warnings. Distinguish harmless platform/configuration warnings from warnings that may indicate undefined behavior or incorrect shim semantics.
9. Verify that the build is genuinely compiling the pinned upstream source files, not copied or rewritten substitutes.
10. Review the provisional `midiParser_calcDetune()` stub. The current stub returns `1.0f` solely to satisfy linking. Do not use it to claim note/tuning correctness. Determine whether the spike can compile and run its current smoke test without invoking this stub; document the limitation. If exact upstream detune behavior is needed for the smoke test, implement only a faithful host-side shim based on the pinned source and cite its source location.

## Constraints

- Do not modify the official upstream repository or its source files.
- Do not modify `dsp-extraction/DSP_EXTRACTION_SPEC.md` during this task.
- Do not start the full harness or Tracker CE integration.
- Do not touch `jamartinez4991/Tracker_LXR_02`.
- Keep all new or changed files under `dsp-extraction/`.
- Preserve the pinned source revision check in CMake. Do not weaken or remove it to get a passing build.
- Do not describe the spike as passing unless you personally ran the build and test commands and observed success.

## Deliverables

Create:

`dsp-extraction/reference-harness/host-spike/HOST_SPIKE_BUILD_REPORT.md`

Include:

- Upstream checkout path and verified HEAD SHA.
- Toolchain and platform details.
- Exact commands executed.
- Configure, compile, link, and test outcomes.
- Relevant compiler warnings.
- Any errors encountered and the minimal fixes made.
- Confirmation that the original upstream `Oscillator.c` and `wavetable.c` were compiled directly.
- Smoke-test coverage and what it does not establish.
- Remaining limitations, especially the detune stub and deterministic RNG quality.
- A clear result: **PASS**, **PARTIAL**, or **BLOCKED**.

If code changes are necessary, keep them minimal and explain each one in the report. Commit the report and any justified spike-only fixes to the study repository, with all paths under `dsp-extraction/`.

## Completion gate

Stop after the report and spike validation. Do not proceed to broad DSP defect testing or full harness construction until the build result has been reviewed.
