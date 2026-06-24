# EMILI IMRT — Beam Angle & Fluence Map Optimization

<p align="center">
  <pre>
   ______ __  __ _____ _      _____ 
  |  ____|  \/  |_   _| |    |_   _|
  | |__  | \  / | | | | |      | |  
  |  __| | |\/| | | | | |      | |  
  | |____| |  | |_| |_| |____ _| |_ 
  |______|_|  |_|_____|______|_____|
  </pre>
</p>

**EMILI** (*Easily Modifiable Iterated Local search Implementation*) is an extensible C++ metaheuristic framework for combinatorial optimization, extended here with an **IMRT treatment planning module** that solves the coupled **Beam Angle Optimization (BAO)** and **Fluence Map Optimization (FMO)** problem using OSQP and iterated local search.

---

## Table of Contents

- [Architecture](#architecture)
- [IMRT Problem Formulation](#imrt-problem-formulation)
- [Quick Start](#quick-start)
- [Command-Line Usage](#command-line-usage)
- [Hyperparameter Tuning (irace)](#hyperparameter-tuning-irace)
- [Clinical Report (ICRU-83)](#clinical-report-icru-83)
- [Algorithm Competition](#algorithm-competition)
- [Project Structure](#project-structure)
- [Build Options](#build-options)
- [Contributing](#contributing)
- [License & Credits](#license--credits)

---

## Architecture

The codebase follows a **framework + extension** pattern. The core EMILI library provides abstract metaheuristic building blocks; the `imrt/` directory extends them for radiation therapy.

```
┌──────────────────────────────────────────────────┐
│  FRAMEWORK CORE  (emilibase.h/.cpp)              │
│  Problem · Solution · InitialSolution            │
│  Neighborhood · Perturbation · Acceptance        │
│  Termination · LocalSearch · TabuMemory          │
├──────────────────────────────────────────────────┤
│  PARSER / BUILDER  (generalParser.h/.cpp)        │
│  CLI grammar → algorithm composition at runtime  │
├──────────────────────────────────────────────────┤
│  IMRT MODULE  (imrt/)                            │
│  ImrtInstance · ImrtProblem · ImrtFmoSolver      │
│  BaoProblem · AngleSwapNeighborhood              │
│  RandomAnglesPerturbation · GreedyAngles...      │
└──────────────────────────────────────────────────┘
```

### Metaheuristics available out of the box

| Algorithm | Token | Internal LS | Special components |
|---|---|---|---|
| Iterated Local Search | `ils` | Best/First improvement | `prangswap`, `improve` |
| Variable Neighborhood Search | `vns` | Best/First improvement | `bangshake`, `accng` |
| Tabu Search | `tabu` | Best/First improvement | `TBao_fixed`, `TBao_adaptive` |
| Iterated Greedy | `ils` + `pgreedy` | Best/First improvement | destruction-reconstruction |
| Simulated Annealing | `ils nols` + `saacc` | Empty (Metropolis accept) | `saacc s e β i α` |

*All algorithms are composed at runtime via CLI tokens — no recompilation needed.*

---

## IMRT Problem Formulation

### Clinical context

Intensity-Modulated Radiation Therapy delivers conformal dose to a Planning Target Volume (PTV) while sparing Organs at Risk (OARs). The problem has two coupled sub-problems:

1. **BAO (Beam Angle Optimization)** — select *K* gantry angles from *N* candidates
2. **FMO (Fluence Map Optimization)** — compute optimal beamlet intensities for the chosen angles

### Quadratic penalty objective (FMO)

The FMO sub-problem is solved **exactly** via OSQP:

```
min  w_under · Σ || u_b ||²  +  w_over · Σ || v_b ||²  +  w_ptv_over · Σ || w_b ||²
s.t. D_ptv·x + u ≥ Dmin            (PTV underdose slack)
     D_oar·x  − v ≤ Dmax           (OAR overdose slack)
     D_ptv·x  − w ≤ 1.07·Dmin      (PTV hot-spot constraint)
     u, v, w ≥ 0
     0 ≤ x_j ≤ max_intensity
```

| Weight | Controls | Clinical effect of increasing |
|---|---|---|
| `w_under` | PTV underdose penalty | ↑ forces V95% coverage |
| `w_over` | OAR overdose penalty | ↑ protects healthy organs |
| `w_ptv_over` | PTV hot-spot penalty | ↓ reduces D2 / improves homogeneity |

### Sparse dose matrix

Dose deposition data is stored in the **CORT format** — one VOIList per organ plus per-angle Dij files (`Gantry{g}_Couch{c}_D.txt`) with global voxel indices. The FMO solver assembles a **compact QP** using only the *K* active angles' beamlets, keeping the problem size O(K) regardless of candidate pool size.

---

## Quick Start

### Prerequisites

- C++11 compiler (gcc ≥ 4.7 or Clang)
- CMake ≥ 2.8
- [OSQP](https://osqp.org/) (`brew install osqp` on macOS)

### Build

```bash
mkdir build && cd build
cmake .. -DWITH_OSQP=ON
make -j$(nproc)
```

The binary is `build/emili`.

---

## Command-Line Usage

```bash
./build/emili <instance_dir> <problem> <algorithm_description> [options]
```

### BAO+FMO (K=4 angles, ILS with first improvement)

```bash
./build/emili instances/PROSTATE_36ang baoimrt 4 \
    ils first irandomk tmaxiter 1 nangswap \
    tmaxiter 3 prangswap 2 improve \
    rnds 42
```

### FMO-only (all angles, single-beamlet-shift neighborhood)

```bash
./build/emili instances/TINY_test imrt 36 \
    bi izero nshift 0.5 tmaxiter 100 \
    rnds 42
```

### Algorithm tokens quick reference

| Category | Token | Meaning |
|---|---|---|
| Problem | `imrt <dir> [nactive K] [verbose]` | FMO on all angles |
| Problem | `baoimrt <K> <dir> [verbose]` | BAO+FMO |
| Init | `izero` / `iuniform X` / `irandom M` | FMO initial intensities |
| Init | `ifirstk` / `irandomk` | BAO initial angle set |
| Neighborhood | `nshift Δ` / `nswap` | FMO beamlet moves |
| Neighborhood | `nangswap` | BAO angle swap |
| Perturbation | `prangswap P` / `pgreedy D` | BAO perturbations |
| Acceptance | `improve` / `saacc s e β i α` | Accept criterion |
| Termination | `tmaxiter N` / `tfeasible` | Stop condition |

---

## Hyperparameter Tuning (irace)

The `irace_*/` directories contain [irace](https://iridia.ulb.ac.be/irace/) scenarios for automatic algorithm configuration.

### Available scenarios

| Scenario | Instances | Algorithms | Parameters | Budget |
|---|---|---|---|---|
| `irace_tiny` | 1 tiny | ILS | 6 + 3 weights | ~2 min |
| `irace_prostate` | 2 sampled | ILS | 4 + 3 weights | ~1.5 h |
| `irace_prostate_all` | 2 sampled | ILS vs rVNS | varies + 3 weights | ~1.5 h |
| `irace_all` | 2 medium | ILS/Tabu/VNS/Greedy/SA | varies + 3 weights | ~2 h |

### Tunable FMO weights (all scenarios)

The target-runner **dynamically creates a temporary instance** with the irace-sampled weight values via symlinks (no 75 MB copies):

```bash
# In parameters.irace:
w_under     "--w_under "     r,log  (1.0, 1000.0)   # PTV underdose
w_over      "--w_over "      r,log  (0.01, 10.0)    # OAR overdose
w_ptv_over  "--w_ptv_over "  r,log  (0.1, 50.0)     # PTV hot-spot
```

### Running a tuning session

```bash
# Quick functional test
irace --scenario irace_tiny/scenario.irace

# Full ILS calibration on prostate
irace --scenario irace_prostate/scenario.irace

# Algorithm competition
irace --scenario irace_all/scenario.irace
```

---

## Clinical Report (ICRU-83)

Every BAO+FMO run outputs a clinical plan summary:

```
================================================================
  IMRT FMO RESULT   K=4 beams
================================================================
Selected angles (deg) : [ 160, 220, 270, 290 ]
FMO objective f*      : 231180.90
Prescription dose Rx  : 68.00 Gy

----------------------------------------------------------------
  Per-organ dose statistics  (Gy)
----------------------------------------------------------------
Structure    Voxels    Dmin   Dmean    Dmax     D95      D5      D2
----------------------------------------------------------------
PTV_68         6770   54.28   67.53   79.16   60.77   73.83   74.92
Bladder       11596    0.13   35.29   70.02       -   63.10   64.21
Rectum         1764    0.04   38.55   67.51       -   62.20   64.70

----------------------------------------------------------------
  DVH constraints  (clinical goals)
----------------------------------------------------------------
Structure  Metric          Goal        Achieved      Status
----------------------------------------------------------------
PTV_68     D95 >= 0.95*Rx  64.60       60.77         VIOL
PTV_68     D2  <= 1.07*Rx  72.76       74.92         VIOL
Bladder    Dmax <= limit   50.00       70.02         VIOL

----------------------------------------------------------------
  Plan quality indices
----------------------------------------------------------------
Conformity Index    CI (V_Rx / V_PTV)    = 1.001
Homogeneity Index   HI (D2 - D98)/D_Rx   = 0.229
PTV coverage        V95% of Rx           = 71.85 %
```

A **DVH CSV** is also generated (`dvh.csv`) for plotting with `plot_bao_results.py`.

### Plan quality metrics

| Metric | Definition | Target |
|---|---|---|
| **V95%** | % PTV volume receiving ≥ 95% Rx | ≥ 95% |
| **CI** | Conformity Index: V_Rx / V_PTV | → 1.0 |
| **HI** | Homogeneity Index: (D2 − D98) / Rx | → 0 |
| **D95** | Dose to 95% of PTV | ≥ 0.95·Rx |
| **D2** | Near-max dose (2% volume) | ≤ 1.07·Rx |

---

## Algorithm Competition

`run_comparison.sh` benchmarks multiple algorithms with identical seeds:

```bash
./run_comparison.sh
```

Output per algorithm in `comparison_results/`:

| Algorithm | Log file | Key components |
|---|---|---|
| ILS | `ILS.log` | ILS + random init + 2-swap perturbation |
| Tabu | `Tabu.log` | Tabu with BAO angle-set memory |
| VNS | `VNS.log` | rVNS with multi-scale shake |
| SA | `SA.log` | ILS + Metropolis cooling |
| Greedy | `Greedy.log` | ILS + destruction-reconstruction |

Visualize convergence with:

```bash
python3 plot_bao_results.py results_36ang_k3.csv results_36ang_k4.csv
```

Generates 4 figures in `plots/`: convergence curves, comparative stats, angle frequency heatmaps, and pair correlation matrices.

---

## Project Structure

```
emili_imrt/
├── main.cpp                   # Entry point
├── emilibase.h/.cpp           # Core framework (Problem, Solution, LS, ILS, VNS, Tabu...)
├── generalParser.h/.cpp       # CLI token parser + Builder pattern
├── setup.h                    # Compile-time flags
├── CMakeLists.txt             # Build system
│
├── imrt/                      # IMRT extension module
│   ├── imrt_instance.h/.cpp   # Sparse dose matrix, CORT format loader
│   ├── imrt.h/.cpp            # FMO: ImrtProblem, beamlet neighborhoods
│   ├── imrt_fmo.h/.cpp        # OSQP-exact FMO solver
│   ├── imrt_bao.h/.cpp        # BAO: angle neighborhoods, perturbations
│   └── imrt_builder.h/.cpp    # CLI builder for IMRT tokens
│
├── grámaticas/                # XML grammar definitions per algorithm
├── irace_*/                   # irace tuning scenarios
├── plot_bao_results.py        # Convergence & DVH visualization
├── run_comparison.sh          # Multi-algorithm benchmark runner
└── generate_doc.py            # Documentation generator
```

---

## Build Options

| CMake flag | Default | Description |
|---|---|---|
| `WITH_OSQP` | OFF | Enable BAO+FMO (requires OSQP library) |
| `DEBUG_FLAGS` | ON | Debug symbols (`-g`) |
| `O3_FLAGS` | OFF | `-O3` optimization |
| `IRACE_OPTIMISED_FLAGS` | OFF | irace-tuned GCC flags |
| `OPT_OPTIMISED_FLAGS` | OFF | OpenTuner-tuned GCC flags |
| `WITH_STATS` | ON | Solution statistics output |
| `USE_NEW_MAIN` | ON | Component-based algorithm loading |
| `PORTABLE` | OFF | Static linking |

---

## Contributing

This repository follows a **PR-based workflow** managed by Gentle AI. All changes go through:

1. **Feature branch** off `develop`
2. **Code changes** with conventional commits
3. **Pull Request** with automated review
4. **Merge to `develop`** after approval

### Conventional commit prefixes

- `feat:` — new feature
- `fix:` — bug fix
- `perf:` — performance improvement
- `refactor:` — code restructuring
- `docs:` — documentation
- `ci:` — CI/irace configuration

### Instance data

Clinical instance files (Dij matrices, VOILists) are **not tracked** in this repository due to size. Contact the maintainer for access to training instances.

---

## License & Credits

- **EMILI framework**: Federico Pagnozzi (BSD 2-Clause) — `federico.pagnozzi@ulb.ac.be`
- **IMRT extension** (BAO+FMO, OSQP integration, irace tuning): Marco Rojas
- **OSQP**: Bartolomeo Stellato, Goran Banjac et al. (Apache 2.0)

```
EMILI IMRT — BAO & FMO optimization for radiation therapy planning
Copyright (c) 2014 Federico Pagnozzi. All rights reserved.
This file is distributed under the BSD 2-Clause License. See LICENSE.txt
```
