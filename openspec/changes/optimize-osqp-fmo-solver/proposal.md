# Proposal: Optimize OSQP FMO Solver

## Intent

PROSTATE-sized BAO instances (36 angles, 5652 dimlets, 2564 boxets) take 4-8 hours per ILS metaheuristic run because each FMO solve uses extreme precision settings (`eps=1e-6`, `max_iter=10000`, `polishing=1`) with no time limit, and the binary is compiled in debug mode. This makes irace parameter tuning impractical.

## Scope

### In Scope
- Cap OSQP solve time (`time_limit=2.0s`) and loosen convergence tolerances in `imrt/imrt_fmo.cpp`
- Build with `-O3` optimization by default for benchmarking/irace runs
- Expose stderr in irace target-runner scripts to surface OSQP errors

### Out of Scope
- Warm-starting OSQP across FMO calls (requires refactor of QP structure reuse)
- Changing the metaheuristic algorithm itself (ILS, VNS, Tabu)
- Final clinical-quality plan generation (use tight eps for that separately)

## Capabilities

### New Capabilities
- `bao-fmo-performance`: FMO solver configuration for tuning-speed vs clinical-quality tradeoff

### Modified Capabilities
- None

## Approach

1. **OSQP settings** in `imrt/imrt_fmo.cpp:265-272`:
   - `time_limit = 2.0` — hard cap per solve
   - `max_iter = 2000` — down from 10000
   - `eps_abs = 1e-3`, `eps_rel = 1e-3` — down from `1e-6`
   - `polishing = 0` — skip extra solve pass

2. **Build optimization**: Use `-DO3_FLAGS=ON` in CMake for irace/benchmark builds (already supported at line 14, 26-28)

3. **Target-runner stderr**: Remove `2>/dev/null` from `irace_prostate/target-runner`, `irace_all/target-runner`, and `irace_prostate/target-runner-docker`

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `imrt/imrt_fmo.cpp` | Modified | OSQP settings: time_limit, max_iter, eps_abs, eps_rel, polishing |
| `CMakeLists.txt` | Unchanged | `-DO3_FLAGS=ON` already supported; no code change needed |
| `irace_prostate/target-runner` | Modified | Remove `2>/dev/null` from emili invocation |
| `irace_all/target-runner` | Modified | Remove `2>/dev/null` from emili invocation |
| `irace_prostate/target-runner-docker` | Modified | Remove `2>/dev/null` from docker exec invocations |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Looser eps changes objective values | Medium | Acceptable for tuning; document that final plans need tight eps re-solve |
| `time_limit` returns suboptimal iterate | Low | Fine for metaheuristic ranking; best iterate still valid |
| Build flag confusion (debug vs release) | Low | Document build commands; add Makefile target or script |

## Rollback Plan

1. Revert `imrt/imrt_fmo.cpp` OSQP settings to original values (`eps=1e-6`, `max_iter=10000`, `polishing=1`, remove `time_limit`)
2. Re-add `2>/dev/null` to target-runner scripts if stderr noise breaks irace parsing
3. Build with `-DDEBUG_FLAGS=ON` instead of `-DO3_FLAGS=ON`

## Dependencies

- OSQP library installed and discoverable by CMake (`WITH_OSQP=ON`)
- irace installed for parameter tuning

## Success Criteria

- [ ] PROSTATE ILS run completes in < 30 minutes (was 4-8 hours)
- [ ] irace completes within its configured budget
- [ ] OSQP errors visible in irace logs (stderr no longer suppressed)
- [ ] No regression in solution feasibility (objective may differ slightly but constraints satisfied)
