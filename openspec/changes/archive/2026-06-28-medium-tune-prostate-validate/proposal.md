# Proposal: Tune on MEDIUM_test, Validate on PROSTATE

## Intent

All 3 irace setups (ILS, VNS, Tabu) are configured to tune on PROSTATE instances (36 angles, 5652 dimlets, 2564 boxets). A single metaheuristic run takes 4-8 hours due to expensive OSQP FMO solves. With a 20000s budget and 1500s timeout, almost all runs time out, making irace impractical.

The PROSTATE slowness is a feature of problem size, not a bug. For tuning hyperparameters, we can use MEDIUM_test instances (12 angles, 480 dimlets, 270 boxets) where runs complete in seconds. We then validate only the 2-3 winning configurations on PROSTATE.

This approach keeps OSQP exact — no tolerance changes, no approximations.

## Scope

### In Scope
- Reconfigure `irace_prostate/`, `irace_vns/`, `irace_tabu/` to use MEDIUM_test instances instead of PROSTATE
- Reduce `targetRunnerTimeout` from 1500s to 120s in all 3 scenario files
- Remove `2>/dev/null` from all 3 target-runners (stderr visibility)
- Create validation script: takes winning config from each irace run, tests on PROSTATE with seeds 42-51
- Ensure at least 2 MEDIUM_test instances exist per irace setup for irace training

### Out of Scope
- Changing OSQP solver settings (covered by `optimize-osqp-fmo-solver`)
- Creating new MEDIUM_test instance data (reuse existing `irace_all/instances/MEDIUM_test_K*`)
- Modifying irace parameter spaces (parameters.irace files stay as-is)
- Docker target-runner changes (only native target-runners)

## Capabilities

### New Capabilities
- `irace-tuning-strategy`: Two-phase irace workflow — tune hyperparameters on small instances (MEDIUM_test), validate winners on clinical-scale instances (PROSTATE) with statistical testing

### Modified Capabilities
- None

## Approach

1. **Instance reconfiguration**: Point `instances.txt` in each irace directory to MEDIUM_test instances. Reuse `irace_all/instances/MEDIUM_test_K3` and `MEDIUM_test_K4` (or create seed variants if needed for diversity).

2. **Timeout reduction**: Set `targetRunnerTimeout = 120` in all 3 scenario files. MEDIUM_test runs complete in seconds; 120s provides ample margin.

3. **Stderr visibility**: Remove `2>/dev/null` from `irace_prostate/target-runner`, `irace_vns/target-runner`, and `irace_tabu/target-runner`.

4. **Validation script**: Create `scripts/validate-on-prostate.sh` that:
   - Accepts a best config file (from irace output) and algorithm type (ils/vns/tabu)
   - Runs the config on PROSTATE instances with seeds 42-51
   - Reports mean/std objective values and run times
   - Supports Wilcoxon comparison against baseline config

5. **Path updates**: Update all absolute paths in scenario files from macOS paths (`/Users/marrojasr/...`) and Docker paths (`/project/...`) to use relative or configurable paths.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `irace_prostate/instances.txt` | Modified | PROSTATE → MEDIUM_test instances |
| `irace_prostate/scenario.irace` | Modified | targetRunnerTimeout 1500→120, paths updated |
| `irace_prostate/target-runner` | Modified | Remove `2>/dev/null` |
| `irace_vns/instances.txt` | Modified | PROSTATE → MEDIUM_test instances |
| `irace_vns/scenario.irace` | Modified | targetRunnerTimeout 1500→120, paths updated |
| `irace_vns/target-runner` | Modified | Remove `2>/dev/null` |
| `irace_tabu/instances.txt` | Modified | PROSTATE → MEDIUM_test instances |
| `irace_tabu/scenario.irace` | Modified | targetRunnerTimeout 1500→120, paths updated |
| `irace_tabu/target-runner` | Modified | Remove `2>/dev/null` |
| `scripts/validate-on-prostate.sh` | New | Validation script for PROSTATE testing |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| MEDIUM_test configs don't generalize to PROSTATE | Medium | Validation with 10 seeds + Wilcoxon test confirms or rejects; if rejected, fallback to PROSTATE tuning with longer budget |
| MEDIUM_test has only 2 instances (limited training diversity) | Low | Use K=3 and K=4 variants; irace handles 2 instances fine with t-test |
| Path changes break existing Docker workflows | Low | Keep Docker paths in separate scenario files (scenario-docker.irace) |

## Rollback Plan

1. Restore original `instances.txt` files pointing to PROSTATE instances
2. Restore `targetRunnerTimeout = 1500` in all scenario files
3. Re-add `2>/dev/null` to target-runners if stderr noise breaks irace parsing
4. Delete `scripts/validate-on-prostate.sh` if not needed

## Dependencies

- MEDIUM_test instances already exist in `irace_all/instances/`
- irace installed for parameter tuning
- PROSTATE instances available for validation step

## Success Criteria

- [ ] All 3 irace setups complete within their configured budget using MEDIUM_test instances
- [ ] No irace runs timeout (targetRunnerTimeout = 120s is sufficient)
- [ ] Validation script runs winning configs on PROSTATE with 10 seeds and reports results
- [ ] OSQP errors visible in irace logs (stderr no longer suppressed)
- [ ] At least one winning config per algorithm achieves competitive objective on PROSTATE
