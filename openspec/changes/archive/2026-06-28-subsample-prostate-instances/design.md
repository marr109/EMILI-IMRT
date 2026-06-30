# Design: Subsample PROSTATE Instances for irace Tuning

## Technical Approach

Add a standalone Python 3 script `scripts/subsample_instance.py` that reads a full PROSTATE CORT instance and writes a reduced, EMILI-loadable instance. The script subsamples angles (equi-spaced selection), dimlets (centered window), and voxels (stratified random sampling per organ), then updates `instance_config.txt` and the irace training pool. All work is done with the Python standard library so it runs inside the existing Docker image without new dependencies.

## Architecture Decisions

| Decision | Options | Tradeoffs | Choice |
|----------|---------|-----------|--------|
| Angle selection | Equi-spaced indices vs. equi-spaced angle values | Index selection is simpler but may skip clinically relevant angles; value selection preserves the proposal's set `{0,40,80,120,160,200,240,280}` | Sort source angles, then pick values at indices `floor(i * src_angles / N)` |
| Dimlet index basis | 0-based (actual CORT files) vs. 1-based (spec wording) | 1-based would break EMILI's CORT parser, which passes `local_beamlet` directly to `globalDimletIndex` | Use 0-based `[start, start+M)` where `start = floor((src-M)/2)`; spec scenario `[67,90]` is interpreted as the 1-based human-readable equivalent of file IDs `[66,89]` |
| Output naming | `{output}` vs. `{output}_S{seed}` | Plain name hides reproducibility; seeded name matches spec and avoids collisions | Directory created as `instances/{output}_S{seed}/`; `--output` is the base name |
| Atomic generation | Write in place vs. temp dir + rename | In-place leaves partial output on crash; rename guarantees all-or-nothing | Build in `tempfile.mkdtemp(dir=instances/)`, then `os.rename` on success |
| Voxel allocation | Exact proportions vs. rounding guard | Exact rounding can overshoot/undershoot total by ±2 | Floor counts, then assign leftover voxels to organs with largest fractional remainders |
| irace timeout | 60 s vs. keep existing 180/240/360 s | 60 s matches tiny-instance budget; existing values were for MEDIUM_test/PROSTATE | Set `targetRunnerTimeout = 60` in all three `scenario.irace` files |

## Data Flow

```
CLI args
   │
   ▼
Parse source config ──► validate constraints (N≤src_angles, M≤src_dimlets, etc.)
   │
   ▼
Select angles ──► filter Gantry*_D.txt rows by local_beamlet_id window
   │                              │
   ▼                              ▼
Update config            Renumber local_beamlet_id 0..M-1 per angle
   │                              │
   ▼                              ▼
Stratified voxel sample ◄── read VOILIST files
   │
   ▼
Write to temp dir ──► rename to instances/{output}_S{seed}/
   │
   ▼
Update irace instances.txt + scenario.irace
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `scripts/subsample_instance.py` | Create | Standalone stdlib-only generator |
| `instances/PROSTATE_tiny_S{seed}/` | Create | 3–5 generated tiny instances |
| `irace_prostate/instances.txt` | Modify | Point to ≥3 `PROSTATE_tiny_S*` instances |
| `irace_vns/instances.txt` | Modify | Same |
| `irace_tabu/instances.txt` | Modify | Same |
| `irace_prostate/scenario.irace` | Modify | `targetRunnerTimeout = 60`, update header comment |
| `irace_vns/scenario.irace` | Modify | `targetRunnerTimeout = 60`, update header comment |
| `irace_tabu/scenario.irace` | Modify | `targetRunnerTimeout = 60`, update header comment |

## Interfaces / Contracts

CLI:

```
python scripts/subsample_instance.py \
  --source PROSTATE_sampled \
  --output PROSTATE_tiny \
  --angles 8 \
  --dimlets 24 \
  --voxels 256 \
  --seed 42
```

Produces `instances/PROSTATE_tiny_S42/` containing:

- `instance_config.txt` with `angles 8 0 40 80 120 160 200 240 280` and `dimlets 192`
- One `Gantry{angle}_Couch0_D.txt` per selected angle (0-based filtered `local_beamlet_id`)
- `PTV_68_VOILIST.txt`, `Bladder_VOILIST.txt`, `Rectum_VOILIST.txt` (sorted sampled IDs)

Key invariants enforced by the script:

- `dimlets` in config equals `angles × dimlets_per_angle`.
- Every kept `local_beamlet_id` is in `[0, M)` and every output Gantry file has at most `M` distinct IDs.
- All source organs are preserved in the output.
- Same `--source` + `--seed` yields byte-identical output.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | Dimlet window math, angle selection, voxel allocation | Python `assert` smoke tests inside the script under `if __name__ == "__main__":` guard (no external test runner) |
| Integration | Script produces loadable instance | Run script on `PROSTATE_sampled`, then run `build/emili` on `PROSTATE_tiny_S42` and check for finite objective |
| E2E | irace completes within budget | Run one irace setup against tiny instances and confirm no timeouts |

## Migration / Rollout

No code migration is required; the change is purely additive except for irace text files. Rollback is `git checkout` on the three `instances.txt`/`scenario.irace` files plus deleting `instances/PROSTATE_tiny_S*` and `scripts/subsample_instance.py`.

## Open Questions

- Should the script accept `--force` to overwrite an existing `PROSTATE_tiny_S{seed}` directory, or fail fast? (Recommendation: fail fast; remove `--force` to keep operations predictable.)
