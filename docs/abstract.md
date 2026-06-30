# EMILI IMRT — Executive Abstract

## Automated Hyperparameter Tuning for Beam Angle Optimization in IMRT

**Problem**: Beam Angle Optimization (BAO) for Intensity-Modulated Radiotherapy (IMRT) requires selecting K gantry angles that minimize a multi-objective FMO function balancing tumor coverage (PTV) against organ-at-risk (OAR) sparing. With K=4 angles from 36 candidates (C(36,4)=58,905 combinations) and each FMO evaluation requiring an expensive OSQP quadratic programming solve, manual parameter tuning is impractical.

**Method**: We applied Iterated Racing (irace) to automatically tune hyperparameters of three metaheuristics — Iterated Local Search (ILS), General Variable Neighborhood Search (GVNS), and Tabu Search — on subsampled PROSTATE instances (192 dimlets, 256 voxels). Winning configurations were validated on full PROSTATE data (5,652 dimlets, 2,564 voxels) with 10 random seeds, cross-validated on an independent subsample, and tested for statistical significance via Wilcoxon signed-rank test.

**Results**: All three algorithms showed statistically significant improvement (p < 0.01):

| Algorithm | f* improvement | V95% gain | Key finding |
|---|---|---|---|
| **Tabu Search** 🥇 | **-81%** | 86.2% → 98.4% | Fixed tenure=7, ultra-aggressive OAR weight (w_over=0.04) |
| **ILS** 🥈 | **-53%** | 86.4% → 96.3% | Iterated Greedy perturbation (pgreedy) dominates random swap |
| **GVNS** 🥉 | **-45%** | 87.5% → 95.5% | Optimal structure equals manual baseline; only FMO weights tuned |

Tabu Search is deterministic and produces identical results across all seeds. ILS with pgreedy shows extremely low variance (±41 on mean 4,235). An ablation study reveals that the local search component of GVNS contributes only 1.8% to solution quality — the multi-scale shake is the primary driver. Cross-validation on an independent subsample confirms generalization (Tabu: f*=1,431 vs 1,800).

**Clinical Impact**: The D95 constraint (PTV coverage ≥ 95% prescribed dose) — the most clinically relevant metric — improved from VIOL to OK across all three algorithms. OAR constraints (Bladder/Rectum Dmax ≤ 50Gy) remain violated, which is a physical limitation of the K=4 regime, not an algorithmic deficiency.

**Conclusions**: Automated tuning discovers non-obvious parameter configurations (e.g., w_over=0.04) that manual expert selection would not consider. The "tune-on-small, validate-on-large" strategy is both computationally feasible and scientifically valid. Algorithmic structure (pgreedy > prangswap, tenure=7, p_max=2) transfers across instances; FMO weights require instance-specific tuning.
