# Design: Tune on MEDIUM_test, Validate on PROSTATE

## Technical Approach

Reconfigure the three existing irace setups (`irace_prostate/`, `irace_vns/`, `irace_tabu/`) to train on fast `MEDIUM_test` instances instead of `PROSTATE`. Keep OSQP exact. Lower the per-run timeout to 120s, expose stderr, make scenario paths portable, and add a validation script that runs the winning configuration and a baseline on `PROSTATE_sampled` / `PROSTATE_sampled2` with seeds 42-51.

## Architecture Decisions

| Decision | Options | Tradeoffs | Choice |
|----------|---------|-----------|--------|
| MEDIUM_test instances | A) `MEDIUM_test_K3` + `MEDIUM_test_K4` symlinks<br>B) Create `MEDIUM_test_K5` | A gives 2 distinct instances now, reuses existing data. B adds diversity but is out of scope. | **A** — use both symlinks under `irace_all/instances/`. |
| K value | A) Extract K from instance name<br>B) Hardcode K=4 | A would run K=3 on `MEDIUM_test_K3`, diverging from VNS/Tabu and from PROSTATE validation. B unifies all setups. | **B** — hardcode K=4; both directories support it. |
| Reconfiguration | A) Modify existing dirs in-place<br>B) Create `irace_*_medium/` copies | A is smaller, reuses parameter/init files, matches the proposal. B preserves old setup but duplicates files. | **A** — modify `irace_prostate/`, `irace_vns/`, `irace_tabu/` in place. |
| Timeout | A) 120s as specified<br>B) Tune per algorithm | A satisfies the spec and is ample for ILS/Tabu. B is safer for VNS but violates the spec. | **A** — `targetRunnerTimeout = 120`; watch VNS during verification. |
| Scenario paths | A) Relative to project root<br>B) Absolute paths<br>C) Env-var driven | A works across macOS/Linux/Docker when irace is run from project root. B breaks on every machine. C is ideal but irace does not expand env vars. | **A** — relative paths in `scenario.irace`; target-runners self-locate and accept `EMILI` override. Keep `scenario-docker.irace` with `/project/` paths. |
| Best-config extraction | A) Parse `irace.log` Rdata with R<br>B) Parse `irace_output.txt` | A is robust and documented. B is fragile. | **A** — embed an R snippet in the validation script. |

## Data Flow

```
scenario.irace ──▶ irace target-runner ──▶ irace.log (Rdata)
instances.txt          MEDIUM_test K=4           │
parameters.irace                                 ▼
                                          validate-on-prostate.sh
                                            - extract best config
                                            - run baseline
                                            - compute stats
                                                   │
                                                   ▼
                                          comparison table + Wilcoxon
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `irace_prostate/instances.txt` | Modify | Replace PROSTATE paths with `irace_all/instances/MEDIUM_test_K3` and `MEDIUM_test_K4`. |
| `irace_vns/instances.txt` | Modify | Same MEDIUM_test instance paths. |
| `irace_tabu/instances.txt` | Modify | Same MEDIUM_test instance paths. |
| `irace_prostate/scenario.irace` | Modify | `targetRunnerTimeout = 120`; replace `/Users/...` absolute paths with relative paths. |
| `irace_vns/scenario.irace` | Modify | `targetRunnerTimeout = 120`; replace `/project/...` absolute paths with relative paths. |
| `irace_tabu/scenario.irace` | Modify | `targetRunnerTimeout = 120`; replace `/project/...` absolute paths with relative paths. |
| `irace_prostate/target-runner` | Modify | Remove `2>/dev/null`; hardcode K=4. |
| `irace_vns/target-runner` | Modify | Remove `2>/dev/null`. |
| `irace_tabu/target-runner` | Modify | Remove `2>/dev/null`. |
| `scripts/validate-on-prostate.sh` | Create | Extract best config from `irace.log`, run best + baseline on PROSTATE (seeds 42-51), print comparison table, optional Wilcoxon. |

## Interfaces / Contracts

### Target-runner
- Input: `CONFIG_ID INSTANCE_ID SEED INSTANCE <param flags>`
- Output: `"<objective> <elapsed_seconds>"` on stdout
- stderr no longer suppressed; parsing uses only the `Objective function value:` line.

### Scenario files
- Native `scenario.irace` uses paths relative to the project root; invoke irace from the project root.
- `EMILI` environment variable overrides the EMILI binary location in target-runners.

### Validation script
```bash
scripts/validate-on-prostate.sh \
  --results-dir irace_prostate/results \
  --algorithm ils \
  --baseline irace_prostate/init_config.txt \
  [--wilcoxon]
```
- Reads `results/irace.log`, extracts the final elite config with R.
- Runs 20 evaluations per config (2 instances × 10 seeds).
- Prints a table with columns `config | instance | f* | CI | HI | V95%`.
- With `--wilcoxon`, runs a paired Wilcoxon signed-rank test per instance.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Build | EMILI compiles | `cmake --build build` |
| Integration | Target-runner returns `"cost time"` on MEDIUM_test | One manual invocation per algorithm |
| Integration | Scenario loads without path errors | `R -e 'irace::readScenario(...)'` |
| E2E | Validation script produces the expected table | Run against a completed or synthetic `irace.log` |

## Migration / Rollout

No migration required. Rollback: restore PROSTATE `instances.txt`, restore `targetRunnerTimeout = 1500`, re-add `2>/dev/null` if needed, and delete the validation script.

## Open Questions

- [ ] Does VNS ever exceed 120s on MEDIUM_test? Verify during apply/verify; raise timeout only if empirical timeouts occur.
- [ ] Should `scenario-docker.irace` also switch to MEDIUM_test? Proposal scope limits changes to native scenarios; Docker scenarios stay as-is for now.
