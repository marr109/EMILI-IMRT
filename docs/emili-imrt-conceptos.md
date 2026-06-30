# EMILI IMRT — Funcionamiento completo, solución óptima y parámetros

> Documento conceptual para entender el pipeline de optimización.

---

## 1. El problema: radioterapia IMRT con BAO

### ¿Qué estamos resolviendo?

Un paciente con cáncer de próstata necesita radioterapia. La máquina (LINAC) emite radiación desde un arco de 360° alrededor del paciente. El problema tiene dos partes:

1. **FMO (Fluence Map Optimization)**: dados K ángulos fijos, ¿qué intensidad de radiación disparar desde cada beamlet para maximizar dosis al tumor (PTV) y minimizar dosis a órganos sanos (OAR: vejiga, recto)?

2. **BAO (Beam Angle Optimization)**: ¿cuáles K ángulos elegir del conjunto disponible para que el FMO resultante dé el mejor plan posible?

```
                    ┌─────────────────────────┐
                    │     BAO (ángulos)        │
                    │  "¿desde dónde disparo?" │
                    └───────────┬─────────────┘
                                │ elige K=4 ángulos
                                ▼
                    ┌─────────────────────────┐
                    │     FMO (intensidades)   │
                    │  "¿con qué fuerza?"      │
                    └───────────┬─────────────┘
                                │ optimiza beamlets
                                ▼
                    ┌─────────────────────────┐
                    │     Plan de tratamiento  │
                    │  (dosis por voxel)       │
                    └─────────────────────────┘
```

### Anatomía del problema

```
Paciente → voxels (cubos de 2.5mm³):
  ├── PTV_68 (tumor, 500 voxels)     → queremos exactamente 68 Gy
  ├── Bladder (vejiga, 300 voxels)   → máximo 50 Gy
  └── Rectum (recto, 1764 voxels)    → máximo 50 Gy

Máquina → 36 ángulos (cada 10°):
  ├── Elegimos K=4 para el tratamiento
  └── Cada ángulo tiene 157 beamlets (pixeles de radiación)
```

---

## 2. La función objetivo del FMO

### ¿Qué mide?

El FMO resuelve: dadas las intensidades de beamlet `x_j` (para j=1..dimlets), minimizar:

```
f(x) = w_under · f_under(x)     ← penaliza subdosis en PTV
     + w_over  · f_over(x)      ← penaliza sobredosis en OAR
     + w_ptv_over · f_ptv(x)    ← penaliza sobredosis (hotspots) en PTV
```

### Componente por componente

| Término | Qué mide | Cuándo penaliza |
|---|---|---|
| `f_under` | Dosis en PTV por debajo de 68 Gy | Cada voxel del tumor que recibe <68 Gy |
| `f_over` | Dosis en OAR por encima de 50 Gy | Cada voxel de vejiga/recto que recibe >50 Gy |
| `f_ptv` | Dosis en PTV por encima de 107% Rx (72.76 Gy) | Hotspots: demasiada radiación en el tumor |

### ¿Qué significan los pesos?

```
w_under ALTO (1000)  → "No me dejes ni un voxel del tumor sin tratar"
w_under BAJO (1)     → "Acepto cierta subdosis en el tumor"

w_over ALTO (10)     → "Protegé los OAR a toda costa"
w_over BAJO (0.01)   → "Puedo tolerar bastante dosis en vejiga/recto"

w_ptv_over ALTO (50) → "No quiero hotspots en el PTV"
w_ptv_over BAJO (0.1)→ "Acepto picos de dosis en el tumor"
```

### El trade-off fundamental

Con solo K=4 ángulos, es **físicamente imposible** cumplir todos los constraints clínicos ideales. El FMO tiene que elegir qué sacrificar:

```
Ángulo que atraviesa vejiga → buena cobertura PTV, mala protección OAR
Ángulo que evita vejiga    → buena protección OAR, mala cobertura PTV
```

Los pesos `w_under`, `w_over`, `w_ptv_over` controlan **qué sacrificamos**.

---

## 3. ¿Qué es una solución óptima?

### Representación

Una solución es un conjunto ordenado de K=4 ángulos: `[130°, 190°, 220°, 90°]`

Cada solución tiene un valor de función objetivo `f*` (menor = mejor).

### El espacio de búsqueda

Con 36 ángulos disponibles y K=4: C(36,4) = **58,905 soluciones posibles**.

Evaluar todas con FMO exacto tomaría ~58,905 × 30s = **20 días**. Por eso usamos metaheurísticas.

### ¿Qué hace "buena" a una solución?

| Métrica | Ideal | Qué mide |
|---|---|---|
| **f*** | Mínimo posible | Objetivo FMO (menor = mejor) |
| **CI** | 1.0 | Conformidad: ¿la dosis se ajusta al tumor? |
| **HI** | 0 | Homogeneidad: ¿la dosis es uniforme en el tumor? |
| **V95%** | 100% | Cobertura: ¿qué % del tumor recibe ≥95% de la dosis? |
| **DVH constraints** | Todos OK | ¿Cumple los límites clínicos? |

---

## 4. Las metaheurísticas y sus parámetros

### 4.1 ILS (Iterated Local Search)

```
┌──────────────────────────────────────────┐
│ ILS = LS → perturbar → LS → perturbar → ... │
└──────────────────────────────────────────┘
```

**Funcionamiento**:
1. Partir de una solución inicial (K ángulos)
2. **Búsqueda local**: probar vecinos (cambiar un ángulo por otro), quedarse con el primero que mejore (`first`) o el mejor de todos (`best`)
3. **Perturbar**: modificar la solución para escapar de óptimos locales
4. Repetir desde (2)

**Parámetros**:

| Parámetro | Qué controla | Valores |
|---|---|---|
| `inner_explore` | Estrategia de búsqueda local | `first` (primer vecino que mejora) vs `best` (el mejor de 128 vecinos) |
| `inner_init` | Solución inicial | `ifirstk` (primeros K ángulos) vs `irandomk` (K aleatorios) |
| `pert_type` | Tipo de perturbación | `prangswap` (swap aleatorio) vs `pgreedy` (destruir D y reconstruir greedy) |
| `pert_swaps` | Intensidad de prangswap | 1-2 ángulos swapeados al azar |
| `D` | Intensidad de pgreedy | 1-3 ángulos destruidos y reconstruidos greedy |
| `outer_iter` | Ciclos de LS + perturbación | 1-3 |

### 4.2 VNS (Variable Neighborhood Search)

```
┌──────────────────────────────────────────────────┐
│ VNS = shake(k) → LS → ¿mejoró? → k=0 : k++       │
└──────────────────────────────────────────────────┘
```

**Funcionamiento**:
1. Partir de una solución inicial
2. **Shake**: perturbar con intensidad k (k=0: 1 swap, k=1: 2 swaps, k=2: 3 swaps)
3. **Búsqueda local**: refinar
4. Si mejoró → volver a k=0 (vecindario chico). Si no → k++ (vecindario más grande)
5. Repetir

**Parámetros**:

| Parámetro | Qué controla | Valores |
|---|---|---|
| `inner_explore` | Estrategia LS | `first` vs `best` |
| `inner_init` | Solución inicial | `ifirstk` vs `irandomk` |
| `inner_iter` | Iteraciones de LS por ronda | 1-2 |
| `outer_iter` | Rondas externas (reinicios) | 1-3 |
| `p_max` | Máxima intensidad de shake | 1-3 (k=0 hasta p_max-1) |

### 4.3 Tabu Search

```
┌──────────────────────────────────────────────────────┐
│ Tabu = LS + memoria: "no vuelvas a estas soluciones"  │
└──────────────────────────────────────────────────────┘
```

**Funcionamiento**:
1. Partir de solución inicial
2. **Explorar vecindario**: probar todos los vecinos no prohibidos por la memoria tabu
3. Elegir el mejor (aunque empeore)
4. Agregar la solución a la memoria tabu (prohibida por `tenure` iteraciones)
5. Repetir

**Parámetros**:

| Parámetro | Qué controla | Fixed | Adaptive |
|---|---|---|---|
| `inner_explore` | Estrategia | `first` | `first` |
| `inner_init` | Solución inicial | `ifirstk` / `irandomk` | `ifirstk` / `irandomk` |
| `max_iter` | Iteraciones totales | 5-8 | 5-8 |
| `tenure` | Cuántas iteraciones una solución está prohibida | 1-20 | — |
| `tenure_min` | Tenure mínimo (adaptive) | — | 1-10 |
| `tenure_max` | Tenure máximo (adaptive) | — | 5-25 |

**Fixed vs Adaptive**:
- **Fixed**: tenure constante. Si tenure=7, una solución queda prohibida por 7 iteraciones.
- **Adaptive**: tenure dinámico. Si se detecta un ciclo (re-visitar solución), tenure sube. Si no hay ciclos por 2×tenure iteraciones, tenure baja.

---

## 5. El pipeline completo de optimización

```
                      ┌──────────────────┐
                      │  PROSTATE_sampled │  (36 ángulos, 5652 dimlets, 2564 voxels)
                      └────────┬─────────┘
                               │ subsample_instance.py
                               ▼
                      ┌──────────────────┐
                      │  PROSTATE_tiny   │  (8 ángulos, 192 dimlets, 256 voxels)
                      └────────┬─────────┘
                               │ irace × 3
                               ▼
            ┌──────────────────┼──────────────────┐
            ▼                  ▼                  ▼
      ┌──────────┐      ┌──────────┐      ┌──────────┐
      │ ILS #323 │      │ VNS #540 │      │ Tabu #321│
      │ pgreedy  │      │ irandomk │      │ tenure=7 │
      │ D=2      │      │ p_max=2  │      │ w_over=  │
      │ w_over=  │      │ w_over=  │      │   0.04   │
      │   0.17   │      │   0.19   │      │          │
      └────┬─────┘      └────┬─────┘      └────┬─────┘
           │                 │                 │
           └─────────────────┼─────────────────┘
                             │ validación en PROSTATE real
                             ▼
                      ┌──────────────────┐
                      │  Tabla comparativa│
                      │  + Wilcoxon test  │
                      └──────────────────┘
```

### ¿Por qué tunear en tiny y validar en real?

1. **Costo**: una corrida en PROSTATE real tarda 4-8 horas. irace necesita cientos de corridas.
2. **Estructura**: los hiperparámetros (tenure, D, p_max) dependen de la estructura del algoritmo, no del tamaño de la instancia.
3. **Validación**: el test de Wilcoxon confirma si las mejoras son reales o ruido estadístico.
