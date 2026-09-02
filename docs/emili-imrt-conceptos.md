# EMILI IMRT — Funcionamiento general (FMO + BAO)

> Estado real del código en `feat/ampl-gurobi-solver`, verificado línea por
> línea contra `imrt/*.cpp`/`.h`. Reemplaza la versión anterior de este
> documento, que describía el dataset CORT/`PROSTATE_36ang` y el pipeline de
> tuning con irace — ambos retirados de esta rama (ver commit `59fac02`).

## 1. Panorama general

Dos subproblemas anidados sobre radioterapia IMRT, sobre la instancia
`instances/CERR_Prostate` (ver `docs/cerr-prostate-instance-analysis.md`):

- **FMO (Fluence Map Optimization)** — dado un conjunto fijo de K ángulos,
  ¿qué intensidad dispara cada beamlet? Es un QP convexo, resuelto exacto
  vía **AMPL + Gurobi** desde C++ (`imrt/imrt_fmo.cpp`, modelo compartido en
  `ampl_gurobi/fmo.mod`).
- **BAO (Beam Angle Optimization)** — ¿qué K ángulos elegir? Espacio
  combinatorio (`BaoProblem`, `imrt/imrt_bao.cpp`), resuelto con
  metaheurísticas de búsqueda local/trayectoria; cada evaluación de una
  solución candidata dispara un solve completo del FMO.

Dataset: 360 ángulos candidatos (1°/posición), ~23971 beamlets totales, 4
órganos (PTVHD, PTVLD, BLADDER, RECTUM). La mayoría de los experimentos usan
K=4 ángulos activos. Los valores clínicos Dmin/Dmax son **placeholder** (ver
§2) — no representan una prescripción real todavía.

## 2. Función objetivo del FMO

Formulación exacta que resuelve Gurobi (`ampl_gurobi/fmo.mod`, documentada
en `imrt/imrt_fmo.h:15-39`):

```
min   w_under · Σu²  +  w_over · Σv²   [+ w_ptv_over · Σw²]
s.t.  D_ptv·x + u  >=  Dmin        (subdosis PTV)
      D_oar·x − v  <=  Dmax        (sobredosis OAR)
      D_ptv·x − w  <=  1.07·Dmin   (sobredosis PTV / hotspot, si w_ptv_over>0)
      u, v, w >= 0
      0 <= x_j <= M
```

| Variable | Qué es |
|---|---|
| `x_j` | Intensidad del beamlet `j`, `0 <= x_j <= max_intensity` |
| `u_b` | Slack de subdosis del boxet PTV `b` (>0 si recibe menos que Dmin) |
| `v_b` | Slack de sobredosis del boxet OAR `b` (>0 si recibe más que Dmax) |
| `w_b` | Slack de hotspot PTV `b` — solo activo si `w_ptv_over>0`; hoy está en 0 (desactivado), la restricción queda no-binding por diseño |

**Versión evaluada por la búsqueda local clásica** (`ImrtProblem::calcObjectiveFunctionValue`,
`imrt/imrt.cpp:14-44`): misma fórmula en forma cerrada, para un `x` ya fijo
(no resuelve el QP, solo penaliza violaciones):

```
f(x) = w_under · Σ max(0, Dmin − dosis)²     [PTV]
     + w_over  · Σ max(0, dosis − Dmax)²     [OAR]
```

**Ejemplo:** Dmin=65 Gy, un boxet PTV con dosis=60 Gy → `under=5` → aporta
`w_under·25` a `f`.

### Valores actuales (placeholder, `imrt/cerr_instance.cpp:42-47`)

| Parámetro | Valor | Quién lo define en teoría |
|---|---:|---|
| Dmin (PTVHD, PTVLD) | 65.0 Gy | Médico (prescripción) — **placeholder, no real** |
| Dmax (BLADDER, RECTUM) | 50.0 Gy | Médico (tolerancia OAR) — **placeholder, no real** |
| w_under | 1.0 | Diseño del optimizador |
| w_over | 0.5 | Diseño del optimizador |
| w_ptv_over | 0.0 (desactivado) | Diseño del optimizador |
| max_intensity | 15000.0 | Diseño del optimizador |

El objetivo es muy sensible a Dmin/Dmax: ±20% lo mueve entre -97% y +170%
(ver `experiments/clinical_sensitivity_pilot/ANALYSIS.md`) — ningún valor
absoluto reportado hoy sirve para una conclusión clínica.

## 3. Espacio de búsqueda y qué se reporta

Con 360 ángulos y K=4: `C(360,4) ≈ 688 millones` de combinaciones — evaluar
todas con el QP exacto es inviable, de ahí las metaheurísticas.

Cada `trajectory.csv` reporta el objetivo agregado más el desglose por
órgano (`ptv_PTVHD_u2, ptv_PTVLD_u2, oar_BLADDER_v2, oar_RECTUM_v2`,
`FmoResult` en `imrt/imrt_fmo.h:48-53`). **No hay métricas DVH clínicas**
(CI, HI, V95%, etc.) — ese reporte (`reportPlan`) se eliminó junto con
`ImrtInstance`/CORT; el binario imprime explícitamente
`[report] Clinical DVH report unavailable`.

## 4. Generación de soluciones iniciales

| Token CLI | Clase | Problema | Qué hace |
|---|---|---|---|
| `ifirstk` | `FirstKAnglesInit` | BAO | Los K ángulos de menor grado, **restringido a múltiplos de 5°** (`imrt_bao.cpp:167-192`) |
| `irandomk` | `RandomKAnglesInit` | BAO | K ángulos al azar (Fisher-Yates parcial), mismo filtro mod-5 (`imrt_bao.cpp:204-226`) |
| `izero` | `ZeroInitialSolution` | FMO clásico | Todos los dimlets en 0 |
| `iuniform <x>` | `UniformInitialSolution` | FMO clásico | Todos los dimlets en intensidad fija `x` |
| `irandom <max>` | `RandomInitialSolution` | FMO clásico | Dimlets ~ U[0, max] |

El filtro mod-5 en BAO es incondicional en el código actual — no hay
variante "sin restricción" ya armable desde CLI (motivo y consecuencias en
[[project_leslie_localsearch_review]]).

## 5. Generadores de vecinos

| Token | Clase | Problema | Movimiento |
|---|---|---|---|
| `nangswap` | `AngleSwapNeighborhood` | BAO | Cambia un ángulo activo por uno inactivo |
| `nangshift <step>` | `AngleShiftNeighborhood` | BAO | Cada ángulo activo ±`step` grados (wraparound), determinístico |
| `nangshiftmulti <n> <s1..sn>` | `AngleMultiShiftNeighborhood` | BAO | Vecindario "inclusivo": acumula ±sᵢ para cada step de la lista |
| `nshift <delta>` | `SingleBeamletShift` | FMO clásico | Un dimlet ±`delta`, clamp a `[0, max_intensity]` |
| `nswap` | `BeamletSwap` | FMO clásico | Swap de intensidades entre dos dimlets — cada dimlet tiene su propia columna dispersa de influencia de dosis, así que sí cambia el objetivo |

## 6. Perturbaciones (para ILS y metaheurísticas de trayectoria)

| Token | Clase | Problema | Qué hace |
|---|---|---|---|
| `prangswap <p>` | `RandomAnglesPerturbation` | BAO | Reemplaza `p` ángulos activos al azar |
| `pgreedy <D>` | `GreedyAnglesPerturbation` | BAO | Destruye `D` ángulos y reconstruye greedy (Iterated Greedy) |
| `prangshift <step> <numSteps>` | `AngleShiftMultiPerturbation` | BAO | Desplaza `numSteps` ángulos activos DISTINTOS (sin repetir slot), magnitud aleatoria en `(step, 2·step)` — ver fix de trampa módulo-step en [[project_leslie_localsearch_review]] |
| `prandom <k> <max>` | `RandomBeamletPerturbation` | FMO clásico | Reasigna `k` dimlets a U[0, max] |

## 7. Metaheurísticas — estado real

No todo lo que está *wireado* en el parser CLI fue probado en un
experimento real. Distinguir eso es el punto de esta tabla:

| Metaheurística | Estado | Sintaxis |
|---|---|---|
| **Búsqueda local pura** (First/Best + `locmin`) | ✅ Usada extensivamente (15 semillas × 4 condiciones) | `<first\|best> irandomk locmin nangshift <step> rnds <seed>` |
| **ILS** | ✅ Usada extensivamente (lotes de 15 semillas, nangshift 5 y 10) | `ils <first\|best> irandomk locmin nangshift <step> tmaxiter <n> prangshift <step> <numSteps> baoimprove rejectrepeated rnds <seed>` |
| **VNS (rVNS shake)** | ⚠️ Wireada (`MultiScaleAngleShake`, token `bangshake <p_max>`, `imrt_builder.cpp:433-448`) — **sin ningún experimento corrido todavía** | `bangshake <p_max>` |
| **Tabu Search** | ⚠️ Wireada (`BaoTabuMemory`/`AdaptiveBaoTabuMemory`, tokens `TBao_fixed <tenure>` / `TBao_adaptive <min> <max>`, `imrt_builder.cpp:410-423`) — **sin ningún experimento corrido todavía** | `TBao_fixed <tenure>` / `TBao_adaptive <min> <max>` |
| Simulated Annealing / Metrópolis | Infraestructura genérica de EMILI (no específica de IMRT); no se verificó en esta pasada si ya es instanciable para BAO sin trabajo extra | — |

## 8. Dónde está cada cosa

- Objetivo/constraints FMO: `ampl_gurobi/fmo.mod`, `imrt/imrt_fmo.h`, `imrt/imrt_fmo.cpp`
- Reader de instancia CERR: `imrt/cerr_instance.h/.cpp`
- Problema BAO (ángulos): `imrt/imrt_bao.h/.cpp`
- Problema FMO clásico (beamlets): `imrt/imrt.h/.cpp`
- Parser CLI (tokens de este documento): `imrt/imrt_builder.h/.cpp`
- Mecánica First/Best/`locmin` con trazas reales: `docs/local-search-first-best-locmin.md`
- Sensibilidad al placeholder clínico: `experiments/clinical_sensitivity_pilot/ANALYSIS.md`
