# Proposal: Subsample PROSTATE Instances for irace Tuning

## Intent

Current PROSTATE instances (36 angles, 5652 dimlets, 2564 boxets) are too slow for irace hyperparameter tuning — each metaheuristic run requires 128-672 OSQP FMO solves, causing irace to timeout. We need smaller instances that preserve the CORT format structure so irace can complete within its budget.

## Scope

### In Scope
- Python script `scripts/subsample_instance.py` to generate reduced instances from PROSTATE data
- 3-5 `PROSTATE_tiny_*` variant directories under `instances/` with different voxel random seeds
- Update irace `instances.txt` files to reference tiny instances for training
- Manual verification: EMILI loads and solves a tiny instance end-to-end

### Out of Scope
- C++ code changes (instance format is already data-driven)
- Hyperparameter generalization guarantees (handled by `validate-on-prostate.sh`)
- Changes to MEDIUM_test instances (already exist and work)

## Capabilities

### New Capabilities
- `instance-subsampling`: Script and workflow for generating reduced CORT instances from full PROSTATE data while preserving format invariants

### Modified Capabilities
- `irace-tuning-strategy`: Training instance pool changes from `MEDIUM_test` to `PROSTATE_tiny_*` variants (faster instances, same format)

## Approach

Standalone Python script that reads PROSTATE CORT files and writes reduced copies:
1. **Angles**: keep 8 equi-spaced angles (0,40,80,120,160,200,240,280) from 36
2. **Dimlets**: centered window of 24 per angle (total 192), filtered from 157 per angle
3. **Voxels**: stratified random sample per organ (~256 total: PTV ~150, Bladder ~50, Rectum ~56)
4. **Organs**: all 3 preserved (PTV_68, Bladder, Rectum)
5. Update `instance_config.txt` counts and VOILIST files accordingly

Generate 3-5 variants with different random seeds for irace training diversity.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `scripts/subsample_instance.py` | New | Standalone Python script for instance subsampling |
| `instances/PROSTATE_tiny_*/` | New | Generated tiny instance directories (3-5 variants) |
| `irace_prostate/instances.txt` | Modified | Point to tiny instances for training |
| `irace_vns/instances.txt` | Modified | Point to tiny instances for training |
| `irace_tabu/instances.txt` | Modified | Point to tiny instances for training |
| `openspec/specs/irace-tuning-strategy/spec.md` | Modified | Training instance requirement updated |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Tuned hyperparameters don't generalize to full PROSTATE | Medium | Existing `validate-on-prostate.sh` with Wilcoxon test validates on full instances |
| Dimlet centered window biases solution space | Low | Multiple variants with different voxel seeds; centered window preserves central beam geometry |
| Script produces invalid CORT format | Low | Manual EMILI load/solve verification before irace integration |

## Rollback Plan

1. Revert `instances.txt` files to point back at `MEDIUM_test` instances (git checkout)
2. Delete `instances/PROSTATE_tiny_*` directories
3. Remove `scripts/subsample_instance.py`
4. All changes are additive — no existing files are modified except `instances.txt` which tracks git

## Dependencies

- Python 3.6+ (standard library only: `os`, `shutil`, `random`, `math`)
- PROSTATE instance files must exist under `instances/PROSTATE_sampled*/`

## Success Criteria

- [ ] `scripts/subsample_instance.py` runs without errors on PROSTATE data
- [ ] At least 3 `PROSTATE_tiny_*` directories generated with valid CORT format
- [ ] EMILI loads and solves a tiny instance producing valid objective value
- [ ] irace completes a tuning run within its budget using tiny instances
- [ ] OSQP FMO solve time < 30ms per solve on tiny instances
