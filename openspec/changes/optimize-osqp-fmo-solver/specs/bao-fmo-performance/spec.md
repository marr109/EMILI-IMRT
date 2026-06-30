# BAO FMO Performance Specification

## Purpose

Configures the OSQP FMO (Fluence Map Optimization) solver for tuning-speed operation, enabling irace metaheuristic runs on PROSTATE-sized BAO instances to complete in minutes instead of hours. This capability trades clinical-grade precision for solve speed during hyperparameter tuning; final clinical plans require a separate tight-tolerance re-solve.

## Requirements

### Requirement: OSQP Tuning-Mode Solver Settings

The FMO solver SHALL configure OSQP with `time_limit=2.0`, `max_iter=2000`, `eps_abs=1e-3`, `eps_rel=1e-3`, and `polishing=0`. The solver SHALL return the `1e30` sentinel objective when OSQP does not converge (`status_val <= 0`) or when setup fails, preserving the existing fallback behavior.

**Verification**: Manual — compile with `cmake --build build` and run EMILI on a PROSTATE instance; confirm a single FMO solve completes within the time limit.

#### Scenario: Solve completes within time limit

- GIVEN a PROSTATE BAO instance (36 angles, 5652 dimlets, 2564 boxets)
- WHEN the FMO solver is invoked with tuning-mode settings
- THEN OSQP returns a converged solution (`status_val > 0`) with a finite objective value
- AND the solve completes within 2.0 seconds

#### Scenario: OSQP hits time limit without convergence

- GIVEN a PROSTATE instance with tight dose constraints
- WHEN OSQP reaches the 2.0s `time_limit` before converging to `eps_abs`/`eps_rel`
- THEN the solver returns the best iterate found (status may indicate timeout)
- AND the objective is finite if `status_val > 0`, else the `1e30` sentinel is returned

#### Scenario: OSQP setup failure

- GIVEN an infeasible QP problem or corrupted input matrices
- WHEN `osqp_setup` returns a non-zero code
- THEN the solver returns `{zero_vector, 1e30}` and logs `[FMO] OSQP setup failed` to stderr

### Requirement: Release Build for Benchmarking

The project SHALL build with `-O3` optimization when configured with `-DO3_FLAGS=ON` in CMake. The Docker image used for irace runs SHALL be rebuilt with this release configuration so that benchmark and tuning runs use the optimized binary.

**Verification**: Manual — configure with `cmake -DO3_FLAGS=ON ..` then `cmake --build build`; confirm `-O3` appears in compile commands and the binary runs faster than the debug build.

#### Scenario: Release build produces optimized binary

- GIVEN the CMakeLists.txt supports `-DO3_FLAGS=ON`
- WHEN the project is configured with `-DO3_FLAGS=ON` and built
- THEN the resulting `emili` binary is compiled with `-O3`
- AND FMO solve throughput is measurably higher than the debug build

#### Scenario: Debug build still works but is slower

- GIVEN the project is configured without `-DO3_FLAGS=ON` (default/debug mode)
- WHEN the same PROSTATE instance is solved
- THEN the solver still produces correct results but takes significantly longer
- AND no compilation errors occur

### Requirement: Stderr Visibility in irace Target-Runners

All irace target-runner scripts SHALL NOT suppress stderr from the EMILI invocation. The `2>/dev/null` redirection SHALL be removed from `irace_prostate/target-runner-docker`, `irace_vns/target-runner`, and `irace_tabu/target-runner` so that OSQP error messages and diagnostics are visible in irace logs.

**Verification**: Manual — run a target-runner with a deliberately broken instance; confirm OSQP error text appears in stderr output.

#### Scenario: OSQP errors surface in irace logs

- GIVEN a target-runner script with `2>/dev/null` removed
- WHEN EMILI encounters an OSQP error during an irace run
- THEN the error message (e.g., `[FMO] OSQP setup failed`) appears in the irace log output
- AND the target-runner still parses stdout correctly for the objective value

#### Scenario: Stderr noise does not break stdout parsing

- GIVEN a target-runner script with stderr now visible
- WHEN OSQP produces verbose warnings but the solve succeeds
- THEN the `grep "Objective function value:"` still extracts the result from stdout
- AND the target-runner outputs `<objective> <elapsed>` on its last line as expected
