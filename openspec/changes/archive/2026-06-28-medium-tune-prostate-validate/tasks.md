# Tasks: Tune on MEDIUM_test, Validate on PROSTATE

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~130–170 (9 files modified, 1 new script) |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Reconfigure all 3 irace setups + create validation script | PR 1 (to feat/benchmark-bao) | Single atomic PR; all changes are independent file edits plus one new file |

## Phase 1: Instance Reconfiguration

- [x] 1.1 `irace_prostate/instances.txt` — replace 2 PROSTATE lines with `irace_all/instances/MEDIUM_test_K3` and `irace_all/instances/MEDIUM_test_K4` (relative paths)
- [x] 1.2 `irace_vns/instances.txt` — same replacement with MEDIUM_test instances
- [x] 1.3 `irace_tabu/instances.txt` — same replacement with MEDIUM_test instances

**Verify**: `grep -c MEDIUM_test irace_prostate/instances.txt irace_vns/instances.txt irace_tabu/instances.txt` → each shows 2; `grep PROSTATE irace_*/*.txt` → no matches.

## Phase 2: Scenario File Updates

- [x] 2.1 `irace_prostate/scenario.irace` — change `targetRunnerTimeout = 1500` → `120`; replace 5 absolute `/Users/marrojasr/...` paths with relative paths (`target-runner`, `irace_prostate/instances.txt`, `parameters.irace`, `results/irace.log`, `init_config.txt`)
- [x] 2.2 `irace_vns/scenario.irace` — change `targetRunnerTimeout = 1500` → `120`; replace 5 `/project/...` paths with relative paths (`irace_vns/target-runner`, `irace_vns/instances.txt`, `parameters.irace`, `results/irace.log`, `init_config.txt`)
- [x] 2.3 `irace_tabu/scenario.irace` — same changes as 2.2 for Tabu

**Verify**: `grep targetRunnerTimeout irace_prostate/scenario.irace irace_vns/scenario.irace irace_tabu/scenario.irace` → all show `120`; `grep -E '/Users/|/project/' irace_prostate/scenario.irace irace_vns/scenario.irace irace_tabu/scenario.irace` → no matches on those keys.

## Phase 3: Target-Runner Fixes

- [x] 3.1 `irace_prostate/target-runner` — remove `2>/dev/null` from EMILI call (line 82); replace K-extraction block (lines 52–54) with `K=4`; replace `sed -i ''` with portable `sed -i` (3 occurrences, lines 72–74)
- [x] 3.2 `irace_vns/target-runner` — remove `2>/dev/null` from EMILI call (line 75); no K change needed (already `K=4`)
- [x] 3.3 `irace_tabu/target-runner` — remove `2>/dev/null` from both EMILI calls (lines 84 and 94); no K change needed (already `K=4`)

**Verify**: `grep '2>/dev/null' irace_prostate/target-runner irace_vns/target-runner irace_tabu/target-runner` → no matches; `grep 'K=4' irace_prostate/target-runner` → 1 match replacing the extraction block.

## Phase 4: Validation Script

- [x] 4.1 Create `scripts/validate-on-prostate.sh` — implements the full contract from the spec:
  - Accepts `--results-dir`, `--algorithm`, `--baseline`, and optional `--wilcoxon`
  - Extracts best config from `irace.log` Rdata via embedded R snippet
  - Runs 20 evaluations per config (2 PROSTATE instances × 10 seeds 42–51)
  - Prints table with columns: `config | instance | f* | CI | HI | V95%`
  - With `--wilcoxon`: paired Wilcoxon signed-rank test per instance, p-value + accept/reject at α=0.05
  - Graceful degradation: `f* = N/A` for all-fail rows; skips Wilcoxon when <5 paired seeds complete

**Verify**: Script passes `bash -n scripts/validate-on-prostate.sh` (syntax check); `--help` or missing args produces a usage message and exits non-zero.

## Phase 5: Verification

- [x] 5.1 `cmake --build build` — confirm EMILI compiles (required by target-runner)
- [x] 5.2 One manual target-runner smoke test per algorithm on MEDIUM_test: `irace_prostate/target-runner 1 <instance> 42 irace_all/instances/MEDIUM_test_K4` → output format `"<float> <int>"` (no stderr suppression confirms OSQP warnings visible)
- [x] 5.3 `R -e 'irace::readScenario("irace_prostate/scenario.irace")'` → loads without path errors
- [x] 5.4 `scripts/validate-on-prostate.sh --results-dir irace_prostate/results --algorithm ils --baseline irace_prostate/init_config.txt` → table rows appear (can use synthetic irace.log for dry-run)
