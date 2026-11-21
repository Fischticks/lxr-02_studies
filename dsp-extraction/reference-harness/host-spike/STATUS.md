# Host Compile Spike — Status

**Status:** Build target authored; execution blocked in the current assistant environment  
**Pinned upstream revision:** `dca0a44413931a12de65941355f06c07598a9be5`

The host spike now exists in `host-spike/`. It compiles the upstream oscillator and wavetable translation units directly from a separately checked-out source tree, verifies the checkout revision at CMake configure time, and supplies minimal host shims for HAL, MIDI parser, sequencer constants, RNG, and embedded memory attributes.

## Why there is no compile result yet

An attempt to clone the public upstream repository into the available execution container failed because the container cannot resolve `github.com`. The source files were inspected through the connected GitHub repository integration, but that integration does not provide a local checkout or compiler execution environment.

Accordingly:
- The CMake target and smoke test are committed, but **not yet compiled**.
- No claim is made that the shims are complete or that the target currently passes.
- The detune function is deliberately a link-only placeholder returning unity; it must be replaced with the exact pinned firmware implementation before note/frequency characterization.
- The host CMSIS compatibility helpers are intended only for the active code paths in this first spike and need semantic tests before reference rendering.

## Next execution

Run the commands in `README.md` on a machine with CMake, a C11 compiler, Git, and the pinned checkout. Fix only compile/link blockers first. Once the interpolation-enabled build passes, test the non-interpolated build separately and then run sanitizer boundary tests.

Do not copy upstream files into this repository or change the upstream checkout to make the spike pass. If a source seam is necessary, document it first and keep the reference baseline pristine.
