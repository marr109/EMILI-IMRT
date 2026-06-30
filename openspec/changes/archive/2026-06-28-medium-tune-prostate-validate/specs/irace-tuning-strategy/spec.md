# irace-tuning-strategy Specification

## Purpose

Two-phase irace workflow for the EMILI BAO+FMO metaheuristics (ILS, VNS, Tabu):
hyperparameters are tuned on small, fast `MEDIUM_test` instances, then the winning
configurations are validated on clinical-scale `PROSTATE` instances with multi-seed
runs and statistical comparison against a pre-tuning baseline.

## Requirements

### Requirement: MEDIUM_test training instances

Each irace setup (`irace_prostate/`, `irace_vns/`, `irace_tabu/`) SHALL train on
`MEDIUM_test` instances rather than `PROSTATE`. Each setup's `instances.txt` SHALL
list at least 2 distinct `MEDIUM_test` instances drawn from the shared
`irace_all/instances/` directory. Instance paths in `instances.txt` SHALL NOT point
at `PROSTATE_*` files.

| Criterion | Value |
|-----------|-------|
| Min instances per setup | 2 |
| Source pool | `irace_all/instances/MEDIUM_test_K*` (K3, K4 available) |
| Forbidden entries | any path matching `PROSTATE_*` |

#### Scenario: All 3 setups reconfigured
- GIVEN the original `instances.txt` files reference `PROSTATE_sampled_K4` and `PROSTATE_sampled2_K4`
- WHEN the reconfiguration is applied
- THEN every setup's `instances.txt` lists at least 2 `MEDIUM_test_K*` entries and zero `PROSTATE_*` entries

#### Scenario: Insufficient instances
- GIVEN only one `MEDIUM_test` instance is available
- WHEN the setup is validated
- THEN it is rejected with a message stating at least 2 distinct `MEDIUM_test` instances are required

### Requirement: Reduced targetRunnerTimeout

All 3 scenario files SHALL set `targetRunnerTimeout = 120`. The previous value
(1500s, sized for `PROSTATE`) is invalid for `MEDIUM_test` runs that complete in seconds.

(Previously: 1500s per `PROSTATE` run.)

#### Scenario: Timeout reduced
- GIVEN `scenario.irace` files contain `targetRunnerTimeout = 1500`
- WHEN the change is applied
- THEN all 3 scenario files contain `targetRunnerTimeout = 120` and no occurrence of `1500` remains in that key

#### Scenario: Run exceeds 120s
- GIVEN a `MEDIUM_test` irace run
- WHEN a single configuration iteration takes longer than 120s
- THEN irace marks the run as timed out (no crash) and continues to the next configuration

### Requirement: Stderr visibility in target-runners

All 3 native target-runners SHALL invoke `emili` without `2>/dev/null` redirection
so OSQP and runtime warnings reach irace logs. irace parses only stdout's
`"Objective function value:"` line, so stderr noise SHALL NOT break result parsing.

#### Scenario: Stderr flows to logs
- GIVEN `irace_prostate/target-runner` invokes `$EMILI ... 2>/dev/null`
- WHEN the redirection is removed
- THEN the EMILI call has no `2>/dev/null` and stderr text appears interleaved in irace's log while the stdout result line is still parsed correctly

#### Scenario: OSQP emits warnings
- GIVEN an OSQP solve emits repeated solver warnings on stderr
- WHEN the target-runner captures the result
- THEN the parsed `Objective function value:` is non-empty and the run is scored normally regardless of stderr content

### Requirement: Validation script — basic flow

A script `scripts/validate-on-prostate.sh` SHALL accept an irace results directory
and an algorithm type (`ils`/`vns`/`tabu`) as inputs. It SHALL extract the configuration
with the lowest FMO objective from the results, run it on `PROSTATE_sampled` and
`PROSTATE_sampled2` with seeds 42-51 (10 seeds each), and run the BASELINE pre-irace
configuration on the same instances and seeds.

| Parameter | Purpose |
|-----------|---------|
| `--results-dir <path>` | irace output containing the best config |
| `--algorithm <ils\|vns\|tabu>` | metaheuristic to invoke |
| `--baseline <config>` | pre-irace baseline config file |

#### Scenario: Validate best config vs baseline
- GIVEN a completed irace run produces a best configuration
- WHEN `validate-on-prostate.sh --results-dir <r> --algorithm ils` is invoked
- THEN it runs 20 evaluations (2 instances × 10 seeds) for both best and baseline configs and emits a comparison table

#### Scenario: Missing results directory
- GIVEN the supplied results directory does not contain a valid config
- WHEN the script runs
- THEN it exits non-zero with a message identifying the missing config, and writes no comparison table

### Requirement: Validation comparison output

The validation script SHALL print a comparison table with one row per (config,
instance) pair containing: best/baseline label, mean objective `f*`, 95% confidence
interval (CI), half-width interval (HI), and mean `V95%` coverage. Values SHALL be
computed across the 10 seeds.

#### Scenario: Table columns present
- GIVEN a successful validation run
- WHEN the script finishes
- THEN the printed table has columns `config | instance | f* | CI | HI | V95%` and at least 4 data rows (best×2 + baseline×2)

#### Scenario: All seeds fail for one config
- GIVEN every seed for the best config fails on `PROSTATE_sampled2`
- WHEN the script computes statistics
- THEN that row reports `f* = N/A` and does not abort the rest of the table

### Requirement: Optional Wilcoxon signed-rank test

The validation script MAY compute a Wilcoxon signed-rank test comparing best vs
baseline objective vectors (10 paired seeds per instance) when invoked with
`--wilcoxon`. The output SHALL include the p-value and an explicit accept/reject
verdict at α = 0.05.

#### Scenario: Wilcoxon requested
- GIVEN `--wilcoxon` is passed and both configs produced 10 paired results
- WHEN the script runs
- THEN it prints a p-value and a verdict line (`reject H0` or `fail to reject H0`)

#### Scenario: Too few paired samples
- GIVEN fewer than 5 paired seeds completed for an instance
- WHEN `--wilcoxon` runs
- THEN it skips the test for that instance with a warning and continues with remaining instances

### Requirement: Portable scenario paths

All scenario files SHALL NOT hardcode macOS absolute paths
(`/Users/marrojasr/...`) or Docker paths (`/project/...`) for `targetRunner`,
`trainInstancesFile`, `parameterFile`, `logFile`, or `configurationsFile`. Paths
SHALL be relative to the scenario file's directory or resolved via an environment
variable override.

#### Scenario: No hardcoded user paths
- GIVEN the rewritten scenario files
- WHEN grepped for `/Users/` and `/project/`
- THEN zero matches are returned for those keys

#### Scenario: Environment override
- GIVEN `EMILI_IRACE_ROOT` is set to a checkout path
- WHEN the scenario is loaded by irace
- THEN all referenced files resolve under `EMILI_IRACE_ROOT` without manual edits