# Delta for instance-subsampling

## ADDED Requirements

### Requirement: Angle subsampling

The script SHALL select N equi-spaced angles (default 8) from the source instance's
`Gantry*_D.txt` files. It SHALL copy only the Gantry files whose indices match the
selected angles and SHALL update the `angles` field in `instance_config.txt` to N.

| Parameter | Default | Constraint |
|-----------|---------|------------|
| N (`--angles`) | 8 | 1 ≤ N ≤ source angle count |
| Selection | equi-spaced | MUST be deterministic given source + N |

#### Scenario: Default 8 angles from 36
- GIVEN a PROSTATE instance with 36 angles
- WHEN the script runs with default `--angles 8`
- THEN exactly 8 `Gantry*_D.txt` files are copied and `instance_config.txt` has `angles = 8`

#### Scenario: N exceeds source angle count
- GIVEN the source instance has 36 angles
- WHEN `--angles 64` is passed
- THEN the script exits non-zero with a message stating N must be ≤ 36

### Requirement: Dimlet centered-window subsampling

The script SHALL select M dimlets per angle (default 24) using a centered window over
`local_beamlet_id`. It SHALL filter each copied `Gantry*_D.txt` to rows whose
`local_beamlet_id` falls in `[start, start+M-1]` where `start = floor((src - M)/2)`, and
SHALL set the `dimlets` field in `instance_config.txt` to N × M.

#### Scenario: Centered window of 24 from 157 rows
- GIVEN each source Gantry file has 157 rows
- WHEN the script runs with default `--dimlets 24`
- THEN each copied Gantry keeps rows with local_beamlet_id in [67, 90] and `dimlets = 192`

#### Scenario: Odd window biases left
- GIVEN a source Gantry file has 156 rows
- WHEN `--dimlets 24` is passed
- THEN `start = 66` (window biased left by one) and `dimlets = 192`

### Requirement: Voxel stratified random sampling

The script SHALL perform stratified random sampling per organ from VOILIST files,
preserving proportional representation. Default proportions: PTV_68 ~60%, Bladder ~20%,
Rectum ~20%. Total sample size SHALL be configurable via `--voxels` (default ~256).
Sampling SHALL be reproducible from `--seed`.

| Organ | Default proportion |
|-------|--------------------|
| PTV_68 | ~60% |
| Bladder | ~20% |
| Rectum | ~20% |

#### Scenario: Default ~256 voxels across 3 organs
- GIVEN a PROSTATE VOILIST with PTV_68, Bladder, Rectum rows
- WHEN the script runs with defaults and `--seed 42`
- THEN VOILIST files contain only the sampled rows (~154 PTV, ~51 Bladder, ~51 Rectum)

#### Scenario: Custom total voxel count
- GIVEN `--voxels 100 --seed 42` is passed
- WHEN the script runs
- THEN ~60 PTV, ~20 Bladder, ~20 Rectum voxels are selected reproducibly

### Requirement: CORT format invariant preservation

The subsampled instance SHALL satisfy `n_dimlets = n_angles × dimlets_per_angle` in
`instance_config.txt`. Global beamlet indexing per angle SHALL remain contiguous:
`local_beamlet_id` is renumbered 1..M within each angle and `global_beamlet_id` is
recomputed as `(angle_index × M) + local_beamlet_id`. All organs present in the source
SHALL be preserved in the output.

#### Scenario: Invariant holds after subsample
- GIVEN a subsampled instance is generated with N=8, M=24
- WHEN `instance_config.txt` is inspected
- THEN `dimlets = 192` (= 8×24) and each Gantry row's global_beamlet_id is contiguous per angle

#### Scenario: All organs preserved
- GIVEN the source has exactly 3 organs (PTV_68, Bladder, Rectum)
- WHEN the subsampled instance is generated
- THEN all 3 organs appear in the VOILIST and the organ count is unchanged

### Requirement: Reproducible variant generation

The script SHALL accept `--seed` to make voxel sampling reproducible. Each run SHALL
produce an instance directory named `PROSTATE_tiny_S{seed}` under `instances/`. Angle
and dimlet selections are deterministic (seed-independent); only voxel sampling varies
by seed.

| Output | Path |
|-------|------|
| Instance directory | `instances/PROSTATE_tiny_S{seed}/` |

#### Scenario: Same seed yields identical output
- GIVEN the script runs twice with `--seed 42` on the same source
- WHEN the two output directories are diffed
- THEN they are byte-identical (same voxels, same dimlets, same angles)

#### Scenario: Different seeds differ only in voxels
- GIVEN `--seed 42` and `--seed 7` are run on the same source
- WHEN the outputs are compared
- THEN angle and dimlet files match but the VOILIST voxel row sets differ between the two

### Requirement: EMILI-loadable output

Every generated instance SHALL be loadable by EMILI without errors and SHALL solve to a
valid objective value. Manual verification SHALL load and solve at least one generated
instance end-to-end before irace integration.

#### Scenario: EMILI loads and solves a tiny instance
- GIVEN a `PROSTATE_tiny_S42` instance generated with defaults
- WHEN `emili` is invoked on it with a metaheuristic
- THEN it loads without CORT-format errors and emits a finite `Objective function value:`

#### Scenario: Malformed config rejected early
- GIVEN `instance_config.txt` is hand-edited to break the `n_dimlets` invariant
- WHEN EMILI loads the instance
- THEN EMILI reports a format error (script-side invariant check should have prevented this)