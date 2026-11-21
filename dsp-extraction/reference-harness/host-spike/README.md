# Oscillator host compile spike

This is a deliberately small compile/link/runtime smoke target. It compiles the
**original upstream** `Oscillator.c` and `wavetable.c` directly from a separate
checkout; it does not vendor, patch, or copy those files.

## Required source checkout

Clone the upstream repository and check out the audited revision:

```sh
git clone https://github.com/erica-synths/lxr-desktop-app.git
git -C lxr-desktop-app checkout dca0a44413931a12de65941355f06c07598a9be5
```

## Build and run

From this directory:

```sh
cmake -S . -B build -DLXR_SOURCE_DIR=/absolute/path/to/lxr-desktop-app
cmake --build build
ctest --test-dir build --output-on-failure
```

Enable host memory/undefined-behavior sanitizers:

```sh
cmake -S . -B build-asan \
  -DLXR_SOURCE_DIR=/absolute/path/to/lxr-desktop-app \
  -DLXR_SPIKE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

The source revision is checked by CMake; a different commit is rejected.

## What the shims do

- Replace the broad HAL header with only the types/intrinsics referenced by this
  translation unit.
- Replace MIDI parser and sequencer headers with the two declarations/constants
  needed by oscillator note setup.
- Supply deterministic host RNG and a detune placeholder for linkability.
- Supply host definitions for embedded memory-section attributes and the
  oscillator sample-rate configuration.

The detune helper in `host_support.c` is explicitly provisional. Before any
reference audio comparison, replace it with the exact upstream detune formula
and test all 128 fine-tune values. This spike is intended first to establish
whether the upstream DSP translation units compile and link cleanly behind
narrow shims.

## Known expected follow-up

The default interpolation-enabled configuration is the first target. Then
configure a second build with both interpolation options disabled; source
inspection indicates those branches may reference an undeclared
`overtoneIndex`. Finally, add dedicated boundary tests for table index 1023
under ASan/UBSan. That test is expected to expose the wavetable row-boundary
read identified in `OSCILLATOR_DEPENDENCY_SPIKE.md`.

## Scope and limitations

This is not yet the full reference harness, does not render WAV fixtures, and
does not claim bit-exact validation. The deterministic RNG and temporary
detune shim exist only to prove the source boundary can be linked. The
production harness design remains in `REFERENCE_HARNESS_DESIGN.md`.
