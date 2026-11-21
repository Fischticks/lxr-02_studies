# LXR-02 DSP Extraction — Claude Prompt Sequence

These prompts are stored in execution order. Run them individually and review each result before proceeding to the next.

| Prompt | File | Purpose | Gate |
|---|---|---|---|
| 01 | [Specification Review and Validation Planning](01_SPECIFICATION_REVIEW_AND_VALIDATION_PLANNING.md) | Independently audit the extraction specification and produce a validation plan. | Review findings and go/no-go recommendation. |
| 02 | [Run and Validate Oscillator Host Spike](02_RUN_AND_VALIDATE_OSCILLATOR_HOST_SPIKE.md) | Obtain a real build/test result for the existing oscillator host spike. | Review the reproducible build report. |
| 03 | [Targeted DSP Defect and Boundary Validation](03_TARGETED_DSP_DEFECT_AND_BOUNDARY_VALIDATION.md) | Experimentally resolve high-risk numerical, memory, timing, and modulation questions. | Review evidence and issues register. |
| 04 | [Build Standalone Reference Harness](04_BUILD_STANDALONE_REFERENCE_HARNESS.md) | Incrementally build the deterministic host-side reference harness. | Review coverage and reproducibility before any integration decision. |

## Execution rules

- Do not run all prompts as one uninterrupted task. Each prompt has a completion gate.
- Prompt 01 is the original specification-audit task, updated only to use the specification's current path under `dsp-extraction/`.
- Prompts 02–04 are follow-on tasks. Their prerequisites are intentional; do not skip a gate because a later task appears straightforward.
- Keep all artifacts and commits under `dsp-extraction/`. Do not add files at repository root.
- The official upstream reference source must remain pinned to commit `dca0a44413931a12de65941355f06c07598a9be5` and must not be modified.
- The Tracker integration repository `jamartinez4991/Tracker_LXR_02` is out of scope for every prompt in this sequence.
- These prompts authorize reference research and harness work only. They do not authorize DSP extraction, Tracker CE porting, or integration.
