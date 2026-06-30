# Tasks: Subsample PROSTATE Instances for irace Tuning

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~180–220 (script ~150, irace configs ~30, generated instances not counted) |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Delivery strategy | ask-on-risk |
| Chain strategy | not needed |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Python script + 3 tiny instances + irace config updates | PR 1 (single) | All in one PR; no test framework needed |

## Phase 1: Script Implementation

- [x] 1.1 Create `scripts/subsample_instance.py` — argparse CLI with `--source`, `--output`, `--angles`, `--dimlets`, `--voxels`, `--seed`
- [x] 1.2 Implement angle selection: read `instance_config.txt`, identify source angles, select N equi-spaced (deterministic index formula `floor(i * src_angles / N)`)
- [x] 1.3 Implement dimlet centered-window: compute `start = floor((src - M) / 2)`, filter Gantry rows to `[start, start+M)`, renumber `local_beamlet_id` 0..M-1
- [x] 1.4 Implement stratified voxel sampling: read VOILIST per organ, allocate proportionally (~60/20/20), use `random.sample` with seed, floor counts + distribute leftovers by largest fractional remainder
- [x] 1.5 Implement atomic write: build in `tempfile.mkdtemp(dir=instances/)`, write all files, `os.rename` on success
- [x] 1.6 Add smoke-test `assert` blocks under `if __name__ == "__main__":` for dimlet math, angle selection, voxel allocation

## Phase 2: Instance Generation

- [x] 2.1 Run `python scripts/subsample_instance.py --source PROSTATE_sampled --output PROSTATE_tiny --seed 42` → produces `instances/PROSTATE_tiny_S42/`
- [x] 2.2 Run same script with `--seed 7` → produces `instances/PROSTATE_tiny_S7/`  (orchestrator override: seeds are 42 / 123 / 999; see apply-progress §Deviations)
- [x] 2.3 Run same script with `--seed 123` → produces `instances/PROSTATE_tiny_S123/`
- [x] 2.4 Verify each directory has `instance_config.txt`, 8 `Gantry*_D.txt`, 3 `*_VOILIST.txt`

## Phase 3: irace Configuration Updates

- [x] 3.1 Update `irace_prostate/instances.txt` — replace `MEDIUM_test` entries with ≥3 `instances/PROSTATE_tiny_S*/instance_config.txt` paths (3 distinct seeds)
- [x] 3.2 Update `irace_vns/instances.txt` — same pattern
- [x] 3.3 Update `irace_tabu/instances.txt` — same pattern
- [x] 3.4 Update `irace_prostate/scenario.irace` — set `targetRunnerTimeout = 60`, update header comment noting tiny instances
- [x] 3.5 Update `irace_vns/scenario.irace` — same
- [x] 3.6 Update `irace_tabu/scenario.irace` — same

## Phase 4: Verification

- [x] 4.1 Manual: run `build/emili --instance instances/PROSTATE_tiny_S42/instance_config.txt` and confirm finite `Objective function value:` output
- [x] 4.2 Manual: confirm `targetRunnerTimeout = 60` present in all 3 `scenario.irace` files (no leftover 120/1500)
- [x] 4.3 Manual: confirm all 3 `instances.txt` files contain ≥3 `PROSTATE_tiny_S` entries and zero `MEDIUM_test` / `PROSTATE_sampled` entries
- [x] 4.4 Manual: confirm `instances/PROSTATE_tiny_S42/` dimlets=192, angles=8 in config, and each Gantry file has renumbered `local_beamlet_id` 0..23