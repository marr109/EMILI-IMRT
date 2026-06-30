# Apply Progress: Subsample PROSTATE Instances for irace Tuning

**Change**: `subsample-prostate-instances`
**Mode**: Standard (no TDD — no test framework configured per `openspec/config.yaml`)
**Status**: All 19 tasks complete (Phases 1–4). Ready for verify.

## Completed Tasks

### Phase 1: Script Implementation
- [x] 1.1 `scripts/subsample_instance.py` argparse CLI (`--source`, `--output`, `--angles`, `--dimlets`, `--voxels`, `--seed`, `--selftest`)
- [x] 1.2 Angle selection — `floor(i * src_angles / N)` over numerically-sorted source angles
- [x] 1.3 Dimlet centered-window — `start = (src - M) // 2`, filter `[start, start+M)`, renumber `local_beamlet_id` 0..M-1
- [x] 1.4 Stratified voxel sampling — 60/20/20 floor + largest fractional remainder (cap-and-redistribute), `random.Random(seed).sample` per organ in config order
- [x] 1.5 Atomic write — `tempfile.mkdtemp(dir=output_parent)` + `os.rename`; fail fast if output exists
- [x] 1.6 Built-in smoke tests via `--selftest` (asserts for angle math, dimlet window, voxel allocation, reproducibility, total-dimlets invariant)

### Phase 2: Instance Generation
- [x] 2.1 `instances/PROSTATE_tiny_S42/` generated
- [x] 2.2 `instances/PROSTATE_tiny_S123/` generated (orchestrator seeds: 42/123/999 — see Deviations)
- [x] 2.3 `instances/PROSTATE_tiny_S999/` generated
- [x] 2.4 Verified: each dir has `instance_config.txt` + 8 `Gantry*_D.txt` + 3 `*_VOILIST.txt`

### Phase 3: irace Configuration Updates
- [x] 3.1 `irace_prostate/instances.txt` → 3 tiny entries
- [x] 3.2 `irace_vns/instances.txt` → 3 tiny entries
- [x] 3.3 `irace_tabu/instances.txt` → 3 tiny entries
- [x] 3.4 `irace_prostate/scenario.irace` → `targetRunnerTimeout = 60`, header notes tiny instances
- [x] 3.5 `irace_vns/scenario.irace` → same
- [x] 3.6 `irace_tabu/scenario.irace` → same

### Phase 4: Verification
- [x] 4.1 EMILI load+solve (Docker `emili-build`): all 3 instances load — `8/8 angles, 192 dimlets, 256 boxets, 3 organs`; finite objectives (S42 f=69790.57, S123 f=94303.77, S999 f=58352.32); solves in 2.3–4.5 s (well under 60 s budget)
- [x] 4.2 `targetRunnerTimeout = 60` present in all 3 `scenario.irace` (no 180/240/360 leftovers)
- [x] 4.3 All 3 `instances.txt`: 3 `PROSTATE_tiny_S*` entries, 0 `MEDIUM_test`/`PROSTATE_sampled` leftovers
- [x] 4.4 Config: `angles 8 <values>`, `dimlets 24` (per-angle); EMILI reports total `192 dimlets`; renumbered `local_beamlet_id` ∈ {0..23} (24 distinct values, max 23)

## Files Changed

| File | Action | What Was Done |
|------|--------|---------------|
| `scripts/subsample_instance.py` | Created | Stdlib-only subsampling tool (CLI + selftest) |
| `instances/PROSTATE_tiny_S42/` | Created | Generated tiny instance (seed 42) |
| `instances/PROSTATE_tiny_S123/` | Created | Generated tiny instance (seed 123) |
| `instances/PROSTATE_tiny_S999/` | Created | Generated tiny instance (seed 999) |
| `irace_prostate/instances.txt` | Modified | Point to 3 tiny instances (directory paths) |
| `irace_vns/instances.txt` | Modified | Same |
| `irace_tabu/instances.txt` | Modified | Same |
| `irace_prostate/scenario.irace` | Modified | `targetRunnerTimeout = 60`, header updated |
| `irace_vns/scenario.irace` | Modified | Same |
| `irace_tabu/scenario.irace` | Modified | Same |

## Deviations from Design

1. **`--output` semantics**: design said "`--output` is the base name; directory created as `instances/{output}_S{seed}/`". Implemented `--output` as the **full output directory path** (no auto `_S{seed}` suffix). This matches the orchestrator's literal CLI (`--output instances/PROSTATE_tiny_S42 --seed 42`) and the verification expectations (`/project/instances/PROSTATE_tiny_S42`). The `{seed}` is encoded by the caller in the directory name, not auto-appended.

2. **Seeds**: tasks.md listed seeds 42/7/123; design/proposal mentioned multiple generic variants. Orchestrator prompt specified seeds **42 / 123 / 999** (overriding the literal task wording). Implemented per the orchestrator prompt.

3. **`instance_config.txt` `dimlets` field value**: the spec scenario states `dimlets = 192` (= N×M). The EMILI CORT parser (`imrt_instance.cpp:156-157`) reads the `dimlets` field as **per-angle** (`n_dimlets_per_angle`) and computes `n_dimlets = n_angles * n_dimlets_per_angle` internally (line 48). The source config writes `dimlets 157` (per-angle) for 36 angles → total 5652. To match EMILI's semantics and the source convention, the script writes `dimlets 24` (per-angle M). The **total** n_dimlets = 192 is verified via EMILI's load log ("192 dimlets") and the invariant holds. The spec's "`dimlets = 192`" wording describes the total, not the config field value. Task 4.4 ("dimlets=192, angles=8 in config") is satisfied: EMILI reports 192 total; config has `angles 8 ...` and `dimlets 24`. The per-angle field value 24 is consistent with EMILI's parser.

4. **Angle selection result**: design's interface example listed `angles 8 0 40 80 120 160 200 240 280` (stride-4). The task/design formula `floor(i * src_angles / N)` over numerically-sorted source 36 angles yields **`[0, 40, 90, 130, 180, 220, 270, 310]`** (indices 0,4,9,13,18,22,27,31). Followed the explicit formula in task 1.2; the design's example output used a different (inconsistent) index stride.

5. **Gantry file filtering**: design did not explicitly state filtering Gantry rows to sampled voxels. Implemented filtering to keep only rows whose `global_voxel_id` ∈ union of sampled organ voxels (the actual tiny-instance requirement — otherwise Gantry files stay at full-grid size and the instance isn't tiny). EMILI only loads rows whose voxel is in a VOIList anyway, so this is both correct and produces the intended tiny files (~1700–2400 rows per angle vs source ~93700).

6. **`--selftest` flag**: task 1.6 said "assert blocks under `if __name__ == "__main__":`". Implemented as a `--selftest` subcommand to avoid running asserts on every normal CLI invocation. `python scripts/subsample_instance.py --selftest` exercises the dimlet/angle/voxel/reproducibility math.

## Voxel allocation detail (default `--voxels 256`, props 60/20/20)
- raw = `[153.6, 51.2, 51.2]`, floors = `[153, 51, 51]` → sum 255, leftover 1
- largest fractional remainder = PTV (0.6) → +1 → **`[154, 51, 51]`** = 256 total
- Matches spec scenario "~154 PTV, ~51 Bladder, ~51 Rectum".
- Prompt's rough "PTV ~150, Bladder ~50, Rectum ~56" was an approximation; spec scenario figures are authoritative and matched exactly.

## Invariant checks verified
- Same seed → byte-identical output (MD5-checked across instance_config, Gantry, VOILISTs)
- Different seeds → identical config (angles/dimlets), identical Gantry filename set, identical beamlet window; differing VOILIST voxel row sets
- `local_beamlet_id` renumbered to `[0, 24)` (24 distinct, max 23)
- EMILI CORT load: "8/8 angles, 192 dimlets, 256 boxets, 3 organs" — no format errors
- Finite objective on all 3 seeds; solve times 2.3–4.5 s (≪ 60 s timeout)

## Issues Found

None. All EMILI load/solve runs produced finite objectives without CORT-format errors.

## Remaining Tasks

None — all 19 tasks complete. Ready for the verify phase.