# ¿Cómo sabe irace que lo que encuentra es válido?

> Explicación del ciclo hipótesis → validación en el tuning de hiperparámetros.

---

## El flujo completo

```
┌─────────────────────────────────────────────────────────────────┐
│                    FASE 1: TUNING (irace)                       │
│                                                                 │
│  PROSTATE_tiny (8 ángulos, 192 dimlets)                        │
│       │                                                         │
│       ▼                                                         │
│  irace genera configs aleatorias                                │
│       │                                                         │
│       ├──→ #1: prangswap, tenure=5, w_over=0.50                │
│       │      target-runner → EMILI → FMO → f* = 65,000          │
│       │                                                         │
│       ├──→ #94: pgreedy D=2, w_over=0.17                       │
│       │      target-runner → EMILI → FMO → f* = 50,000          │
│       │                                                         │
│       └──→ ... (700+ configuraciones evaluadas)                  │
│                                                                 │
│  irace compara f* en tiny:                                      │
│       #94 (50k) < #1 (65k) → #1 eliminada                       │
│       #323 (45k) < #94 (50k) → #94 baja en ranking              │
│                                                                 │
│  Resultado: 5 configs élite  ────→  HIPÓTESIS                   │
│  "pgreedy, D=2, w_over=0.17 funciona mejor en tiny"             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ ¿Funciona en PROSTATE real?
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                 FASE 2: VALIDACIÓN (PROSTATE)                    │
│                                                                 │
│  PROSTATE_sampled (36 ángulos, 5652 dimlets, 2564 voxels)      │
│       │                                                         │
│       ▼                                                         │
│  Misma config #323 → EMILI → FMO → f* = 4,280                   │
│  Baseline manual → EMILI → FMO → f* = 8,086                     │
│                                                                 │
│  Resultado seed=42: f* bajó 47% ✅                               │
│                                                                 │
│  ¿Es ruido estadístico?                                         │
│       │                                                         │
│       ▼                                                         │
│  10 seeds (42-51):                                              │
│       f* = [4280, 4217, 4235, 4217, 4217, 4322, 4217,          │
│              4260, 4179, 4322]                                  │
│       μ = 4,235 ± 41                                            │
│                                                                 │
│  Wilcoxon signed-rank: p = 0.0059  ✅                            │
│                                                                 │
│  ¿Generaliza a otra anatomía?                                   │
│       │                                                         │
│       ▼                                                         │
│  PROSTATE_sampled2 (distinta distribución de voxels):           │
│       f* = 3,699 vs baseline 8,471  ✅                           │
│                                                                 │
│  CONCLUSIÓN: La hipótesis de irace se confirma.                  │
│  pgreedy D=2 con w_over=0.17 es significativamente mejor.       │
└─────────────────────────────────────────────────────────────────┘
```

---

## ¿Qué evalúa irace exactamente?

irace **no predice** cómo se comportará una configuración en PROSTATE real. Solo mide:

```
f*(config, PROSTATE_tiny)
```

Y compara configuraciones entre sí **dentro de tiny**:

```
Si f*(config_A, tiny) < f*(config_B, tiny) en varias instancias tiny
   → irace prefiere A sobre B
```

La **justificación** de por qué esto funciona es:

1. **Parámetros estructurales** (pgreedy vs prangswap, tenure, D, p_max) son propiedades del algoritmo, no de la instancia. Si pgreedy explota mejor el gradiente de FMO en tiny, también lo hará en PROSTATE.

2. **Parámetros de peso** (w_over, w_ptv) dependen parcialmente de la anatomía. La **dirección** (más agresivo = mejor) se preserva; el valor exacto puede ajustarse.

3. **La validación posterior** es la que confirma o rechaza la hipótesis. irace propone candidatos; los experimentos en PROSTATE real determinan si son válidos.

---

## ¿Podría fallar este enfoque?

**Sí.** Casos donde irace encontraría configuraciones inválidas:

| Escenario | Por qué fallaría |
|---|---|
| Tiny tiene anatomía muy distinta a PROSTATE | Los pesos óptimos en tiny no transfieren |
| El espacio de ángulos es muy chico (8 vs 36) | tenure=7 podría ser insuficiente en 36 ángulos |
| La reducción de dimlets cambia la convergencia de OSQP | first vs best podría decidirse distinto |

**Por eso existe la Fase 2.** Si la validación en PROSTATE mostrara que las configs de irace NO mejoran, sabríamos que el enfoque falló y necesitamos instancias más grandes para tuning.

**En nuestro caso, no falló.** Los 3 algoritmos mostraron mejora significativa (p < 0.01) y generalización en sampled2.

---

## El método científico aplicado

```
irace     →  HIPÓTESIS:  "pgreedy > prangswap en BAO"
10 seeds  →  EXPERIMENTO: medir f* en PROSTATE real
Wilcoxon  →  ANÁLISIS:   ¿la diferencia es significativa?
sampled2  →  REPLICACIÓN: ¿se reproduce en otra instancia?
Paper     →  CONCLUSIÓN:  "pgreedy supera a prangswap (p < 0.01)"
```

---

## Resumen visual

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   IRACE      │     │  VALIDACIÓN   │     │   PAPER      │
│   (tiny)     │ ──→ │  (PROSTATE)   │ ──→ │              │
│              │     │               │     │              │
│  Propone     │     │  Confirma o   │     │  Publica     │
│  candidatos  │     │  rechaza      │     │  evidencia   │
│              │     │               │     │              │
│  "Barato"    │     │  "Caro pero   │     │  "Riguroso"  │
│  700 evals   │     │   necesario"  │     │  p < 0.01    │
│  en 5.5h     │     │   54 runs     │     │              │
│              │     │   en 48h      │     │              │
└──────────────┘     └──────────────┘     └──────────────┘
```
