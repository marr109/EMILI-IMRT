# Apply Progress: Tune on MEDIUM_test, Validate on PROSTATE

**Change**: medium-tune-prostate-validate
**Mode**: Standard (no TDD — project has no test framework)
**Status**: All tasks complete

## Completed Tasks

- [x] 1.1 `irace_prostate/instances.txt` — replaced PROSTATE with MEDIUM_test instances
- [x] 1.2 `irace_vns/instances.txt` — same replacement
- [x] 1.3 `irace_tabu/instances.txt` — same replacement
- [x] 2.1 `irace_prostate/scenario.irace` — timeout 120, relative paths
- [x] 2.2 `irace_vns/scenario.irace` — timeout 120, relative paths
- [x] 2.3 `irace_tabu/scenario.irace` — timeout 120, relative paths
- [x] 3.1 `irace_prostate/target-runner` — removed 2>/dev/null, K=4, portable sed -i, cp instead of ln -s
- [x] 3.2 `irace_vns/target-runner` — removed 2>/dev/null, self-locating EMILI
- [x] 3.3 `irace_tabu/target-runner` — removed 2>/dev/null, self-locating EMILI
- [x] 4.1 Created `scripts/validate-on-prostate.sh`
- [x] 5.1 `cmake --build build` — EMILI compiles
- [x] 5.2 Target-runner smoke tests — all 3 produce correct output format
- [x] 5.3 Scenario file verification — all referenced files exist (irace package not installable in Docker for readScenario test)
- [x] 5.4 Validation script — syntax check passes, --help works, missing args handled, config extraction tested

## Files Changed

| File | Action | What Was Done |
|------|--------|---------------|
| `irace_prostate/instances.txt` | Modified | Replaced PROSTATE paths with `instances/MEDIUM_test` + `instances/MEDIUM_test_K4` |
| `irace_vns/instances.txt` | Modified | Same MEDIUM_test instance paths |
| `irace_tabu/instances.txt` | Modified | Same MEDIUM_test instance paths |
| `irace_prostate/scenario.irace` | Modified | targetRunnerTimeout 120; 5 paths changed to relative |
| `irace_vns/scenario.irace` | Modified | targetRunnerTimeout 120; 5 paths changed to relative |
| `irace_tabu/scenario.irace` | Modified | targetRunnerTimeout 120; 5 paths changed to relative |
| `irace_prostate/target-runner` | Modified | Removed 2>/dev/null; K=4 hardcoded; sed -i '' → sed -i; ln -s → cp; EMILI env var override |
| `irace_vns/target-runner` | Modified | Removed 2>/dev/null; EMILI changed from /project/build/emili to self-locating with env var override |
| `irace_tabu/target-runner` | Modified | Removed 2>/dev/null from both EMILI calls; EMILI changed from /project/build/emili to self-locating with env var override |
| `scripts/validate-on-prostate.sh` | Created | Full validation script: config extraction, 20 evaluations per config, comparison table, optional Wilcoxon test |

## Deviations from Design

1. **Instance paths**: Tasks specified `irace_all/instances/MEDIUM_test_K3` and `irace_all/instances/MEDIUM_test_K4`. These are broken symlinks on Windows/Docker (text files containing relative paths, not actual directories). Used `instances/MEDIUM_test` and `instances/MEDIUM_test_K4` instead — the actual directories that the symlinks pointed to. These work on all platforms.

2. **EMILI path in vns/tabu target-runners**: Tasks did not mention this, but the vns/tabu target-runners had `EMILI="/project/build/emili"` (hardcoded Docker path). Since scenario files now use relative paths (not /project/), changed to self-locating: `EMILI="${EMILI:-$(cd "$(dirname "$0")/.." && pwd)/build/emili}"`. This works both natively and in Docker (set EMILI env var for Docker override).

3. **ln -s → cp in prostate target-runner**: Tasks did not mention this. The prostate target-runner used `ln -s` to create symlinks to instance files in a temp directory. With relative instance paths, symlinks resolve relative to the temp directory (not the project root), breaking file loading. Changed to `cp` (matching vns/tabu target-runners) which works with relative paths.

4. **R config extraction robustness**: The irace Rdata file uses `allConfigurations` (not `configurations`) and may have empty `allElites` (incomplete runs). Updated the R snippet to handle both field names, fall back to `iterationElites` or first row, and filter meta columns (`.ID.`, `.PARENT.`, `Row.names`).

## Issues Found

1. **Tabu exceeds 120s timeout**: With default parameters (max_iter=10, tenure=5), Tabu takes 133s on MEDIUM_test_K4, exceeding the 120s targetRunnerTimeout. VNS takes 101s (within 120s but close). ILS takes 45s. The design's open question asked whether VNS exceeds 120s — answer: VNS is within 120s, but Tabu is not. Consider raising timeout for Tabu or reducing max_iter range in parameters.irace.

2. **irace R package not installable in Docker**: The `fs` dependency requires compilation, but the Docker image lacks gcc. Could not run `readScenario()` test (task 5.3). Verified all scenario-referenced files exist via filesystem checks instead. The validation script uses base R's `load()` (not the irace package), so it works without irace installed.

3. **CRLF line endings**: Files edited on Windows had CRLF endings, which broke bash scripts in Docker (e.g., `set -euo pipefail\r` → "invalid option name"). Fixed by converting all modified files to LF.

## Verification Results

| Check | Result |
|-------|--------|
| `grep -c PROSTATE instances.txt` = 0 | ✓ All 3 files |
| `grep MEDIUM_test instances.txt` = 2 per file | ✓ All 3 files |
| `grep targetRunnerTimeout scenario.irace` = 120 | ✓ All 3 files |
| `grep '/Users/\|/project/' scenario.irace` = 0 | ✓ All 3 files |
| `grep '2>/dev/null' target-runner` = 0 | ✓ All 3 files |
| `grep 'K=4' irace_prostate/target-runner` | ✓ Found (line 51) |
| `grep 'sed -i ..' irace_prostate/target-runner` = 0 | ✓ No macOS sed |
| `grep 'ln -s' irace_prostate/target-runner` = 0 | ✓ Uses cp |
| `bash -n scripts/validate-on-prostate.sh` | ✓ Syntax OK |
| `cmake --build build` | ✓ EMILI compiles |
| ILS smoke test (relative path) | ✓ "0.000000 45" |
| VNS smoke test (relative path) | ✓ "0.000000 101" |
| Tabu smoke test (relative path) | ✓ "0.000000 133" |
| Validation --help | ✓ Usage message, exit 0 |
| Validation missing args | ✓ Error message, exit 1 |
| R config extraction | ✓ Extracts config from irace.log |
| Baseline config parsing | ✓ All 3 algorithms parse correctly |

## Workload / PR Boundary

- Mode: single PR
- Current work unit: Unit 1 (all changes)
- Boundary: All 9 file modifications + 1 new script in a single PR
- Estimated review budget impact: ~200 changed lines (within 400-line budget)
