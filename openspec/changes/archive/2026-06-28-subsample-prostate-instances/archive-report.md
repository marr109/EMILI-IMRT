# Archive Report: subsample-prostate-instances

**Archived**: 2026-06-28
**Source**: `openspec/changes/subsample-prostate-instances/`
**Archive**: `openspec/changes/archive/2026-06-28-subsample-prostate-instances/`

## Source Artifacts (Observation IDs)

| Artifact | Engram ID |
|----------|-----------|
| Proposal | #911 |
| Spec | #912 |
| Design | #913 |
| Tasks (apply complete) | #915 |
| Apply Progress | (filesystem: `apply-progress.md`) |
| Verify Report | inline (all pass) |

## Specs Synced

| Domain | Action | Details |
|--------|--------|---------|
| `instance-subsampling` | Created | New domain: angle subsampling, dimlet centered-window, voxel stratified sampling, CORT invariants, reproducibility, EMILI-loadable output |
| `irace-tuning-strategy` | Updated | 2 requirements modified: training instances (MEDIUM_test → PROSTATE_tiny, min 2→3), targetRunnerTimeout (120s→60s). 5 requirements preserved unchanged. Purpose text updated. |

## Archive Contents

- `proposal.md` ✅ — Intent, scope, approach, risk assessment
- `specs/instance-subsampling/spec.md` ✅ — Full spec for new instance-subsampling domain
- `specs/irace-tuning-strategy/spec.md` ✅ — Delta spec (MODIFIED requirements)
- `design.md` ✅ — Technical approach, architecture decisions, data flow, file changes
- `tasks.md` ✅ — 19/19 tasks complete (all checked)
- `apply-progress.md` ✅ — Full implementation record with deviations documented
- `archive-report.md` ✅ — This file

## Verification Status

- **EMILI load+solve**: All 3 tiny instances load correctly — `8/8 angles, 192 dimlets, 256 boxets, 3 organs`. Finite objectives: S42 f=69790.57, S123 f=94303.77, S999 f=58352.32. Solve times 2.3–4.5s (≪ 60s budget).
- **CORT invariants**: dimlets = angles × dimlets_per_angle = 8×24 = 192. All organs preserved. local_beamlet_id renumbered 0..23.
- **irace configs**: All 3 `instances.txt` have ≥3 PROSTATE_tiny entries, 0 MEDIUM_test/PROSTATE_sampled leftovers. All 3 `scenario.irace` have `targetRunnerTimeout = 60`.

## Deviations from Design (Archived Reference)

1. `--output` is full directory path (not base name + auto-suffix)
2. Seeds used: 42, 123, 999 (overrode tasks.md 42/7/123)
3. `dimlets` in config = per-angle value (24), not total (192) — consistent with EMILI CORT parser
4. Angle selection produced `[0, 40, 90, 130, 180, 220, 270, 310]` (consistent with spec formula, not design's example)
5. Gantry files filtered to sampled voxels (not just beamlets)
6. `--selftest` flag replaces bare `if __name__` assert blocks

## Source of Truth Updated

- `openspec/specs/instance-subsampling/spec.md` — New domain spec
- `openspec/specs/irace-tuning-strategy/spec.md` — Updated with modified requirements

## SDD Cycle Complete

The change `subsample-prostate-instances` has been fully planned, specified, designed, implemented, verified, and archived. Ready for the next change.
