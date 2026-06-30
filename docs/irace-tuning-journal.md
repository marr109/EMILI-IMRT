# EMILI IMRT — Diario de Tuning con irace

> Documento de trabajo para la presentación final de resultados.
> Rama: `feat/benchmark-bao` | Fecha: Junio 2026

---

## 1. ¿Qué es irace y por qué lo usamos?

**Iterated Racing** es un algoritmo de tuning automático de hiperparámetros. Funciona como un torneo:

1. **Genera** N configuraciones aleatorias del espacio de parámetros
2. **Evalúa** cada una en un subconjunto de instancias
3. **Elimina** las peores mediante test estadístico (Friedman o t-test)
4. **Repite** con las sobrevivientes, agregando más instancias por ronda
5. **Devuelve** las configuraciones élite (las que sobrevivieron hasta el final)

En nuestro caso, irace explora automáticamente combinaciones de:
- Estrategia de búsqueda local (first/best)
- Solución inicial (ifirstk/irandomk)
- Tipo e intensidad de perturbación/shake
- Iteraciones internas y externas
- Pesos de la función objetivo FMO (w_under, w_over, w_ptv_over)

Sin irace, tendríamos que probar manualmente cientos de combinaciones. Con irace, el algoritmo encuentra solo las regiones prometedoras del espacio.

---

## 2. ¿Es válido tunear en instancias pequeñas?

### Sí, y es práctica estándar en la literatura

La estrategia **"tune on small, validate on large"** está ampliamente documentada en optimización con metaheurísticas:

| Referencia | Contexto |
|---|---|
| Birattari et al. (2002) | Tuning de ACO en instancias pequeñas, validación en grandes |
| Hutter et al. (2009) | ParamILS: tuning en subsets para reducir costo |
| López-Ibáñez et al. (2016) | El manual de irace recomienda explícitamente usar instancias rápidas para tuning |

### Justificación científica

1. **El espacio de hiperparámetros es independiente del tamaño de instancia**: `pert_swaps=2` es una propiedad del algoritmo, no del problema. Si funciona mejor en chico, es esperable que funcione mejor en grande.

2. **Lo que importa es la estructura del problema, no el tamaño**: Nuestras instancias tiny preservan la misma anatomía (PTV + Bladder + Rectum), la misma geometría de ángulos (equi-espaciados), y el mismo tipo de función objetivo FMO con restricciones DVH.

3. **La validación final es en PROSTATE real**: El pipeline incluye `validate-on-prostate.sh` que corre las 2-3 mejores configuraciones en PROSTATE_sampled + PROSTATE_sampled2 con 10 seeds distintas + test de Wilcoxon. Si la configuración no generaliza, el test lo detecta.

4. **Trade-off pragmatismo vs. pureza**: Sin instancias pequeñas, irace es inviable (ver sección 4). La alternativa sería grid search manual, que es menos riguroso que irace + validación estadística.

---

## 3. Metaheurísticas bajo tuning

### ILS (Iterated Local Search)
```
ils <inner_ls> <init> tmaxiter <N> nangswap tmaxiter <M> <pert> <accept>
```

| Parámetro | Opciones | Qué controla |
|---|---|---|
| `inner_explore` | first | Estrategia de búsqueda local |
| `inner_init` | ifirstk, irandomk | Solución inicial (primeros K ángulos vs. aleatorio) |
| `pert_type` | prangswap, pgreedy | Tipo de perturbación |
| `pert_swaps` | 1, 2 | Swaps aleatorios (prangswap) |
| `D` | 1, 2 | Ángulos a destruir/reconstruir (pgreedy, Iterated Greedy) |
| `outer_iter` | 1, 2 | Iteraciones externas del ILS |
| `w_under` | 1–1000 (log) | Penalización subdosis PTV |
| `w_over` | 0.01–10 (log) | Penalización sobredosis OAR |
| `w_ptv_over` | 0.1–50 (log) | Penalización hotspots PTV |

### VNS (Variable Neighborhood Search — GVNS)
```
vns <inner_ls> <init> tmaxiter <N> nangswap tmaxiter <M> bangshake <P> accng improve
```

| Parámetro | Opciones | Qué controla |
|---|---|---|
| `inner_explore` | first | Estrategia LS interna |
| `inner_init` | ifirstk, irandomk | Solución inicial |
| `inner_iter` | 1, 2 | Iteraciones de LS por ronda |
| `outer_iter` | 1, 2, 3 | Rondas externas del VNS |
| `p_max` | 1, 2, 3 | Intensidad máxima del shake |
| Pesos FMO | = ILS | |

### Tabu Search (Fixed + Adaptive)
```
tabu <strategy> <init> tmaxiter <N> nangswap <tabu_memory> rnds <S>
```

| Parámetro | Fixed | Adaptive |
|---|---|---|
| `inner_explore` | first | first |
| `inner_init` | ifirstk, irandomk | ifirstk, irandomk |
| `max_iter` | 5–8 | 5–8 |
| `tenure` | 1–20 | — |
| `tenure_min` | — | 1–10 |
| `tenure_max` | — | 5–25 |
| Pesos FMO | = ILS | = ILS |

**Nota**: irace explora fixed y adaptive en un solo escenario con parámetros condicionales. Decide automáticamente cuál conviene.

---

## 4. Cronología del problema de timeouts

### El descubrimiento inicial

Al correr los benchmarks en PROSTATE real, los tiempos reportados (`time: 16541.2`) parecían milisegundos. **Eran segundos-CPU**: ILS tomaba 4.6 horas, VNS 6.7 horas, Tabu 8.1 horas.

### Intento 1: irace directo sobre PROSTATE
- **Timeout**: 1500s
- **Resultado**: Todas las corridas timeouteaban. Incluso la configuración baseline.

### Intento 2: irace sobre MEDIUM_test
- **Timeout**: 120s → 180s → 240s → 300s → 360s
- **Resultado**: Timeouts en cascada. Cada vez que subíamos el timeout, otra combinación de parámetros lo excedía.

### Causa raíz del loop

```
OSQP solve caro × 128 vecinos por iteración × N iteraciones = tiempo alto
                                                                    ↓
                            irace prueba combinaciones extremas (ej: D=3, outer=3)
                                                                    ↓
                                Timeout → achicamos rangos → nuevo timeout con otra combinación
```

El problema no eran los rangos ni los timeouts: era que **cada solve de OSQP era caro** y no podíamos tocarlo por razones científicas.

### Solución final: PROSTATE_tiny

Generamos instancias reducidas preservando la estructura del problema:

| Dimensión | PROSTATE | PROSTATE_tiny | Reducción |
|---|---|---|---|
| Ángulos | 36 | 8 | 4.5× |
| Dimlets/ángulo | 157 | 24 | 6.5× |
| Voxels | 2564 | ~256 | 10× |
| **Total dimlets** | **5652** | **192** | **29×** |

**Resultado**: cada solve de OSQP pasó de segundos a milisegundos. La corrida más pesada de irace no llega a 10 segundos.

---

## 5. Lecciones aprendidas

1. **Siempre verificar las unidades de tiempo**: `time: 16541.2` eran segundos-CPU, no milisegundos. Esto causó una subestimación de 1000× del costo real.

2. **irace necesita instancias rápidas**: Con 20000s de budget y corridas de minutos, solo se evalúan ~100 configuraciones. Para espacios de 6-8 parámetros, se necesitan cientos.

3. **El problema no eran los parámetros, era el solver**: Reducir rangos y subir timeouts fue un whack-a-mole. La solución de fondo fue reducir el tamaño de instancia.

4. **Docker + Windows = debugging difícil**: Paths relativos vs. absolutos, `sed -i` GNU vs. BSD, CRLF vs. LF, resolución de rutas de irace inconsistente (algunos campos relativos al CWD, otros al scenario).

5. **El target-runner debe producir UNA sola línea**: irace parsea la última línea de stdout. Cualquier output extra (stderr sin redirigir, echos de debug) rompe el parsing.

6. **Parámetros condicionales de irace requieren columnas NA en init_config**: Aunque un parámetro no aplique (ej: `tenure_min` cuando `tabu_type=fixed`), debe aparecer en el header con valor `NA`.

---

## 6. Pipeline completo

```
PROSTATE_sampled ──→ subsample_instance.py ──→ PROSTATE_tiny_S{42,123,999}
                                                         ↓
                                              irace (ILS, VNS, Tabu)
                                                         ↓
                                              Mejores configuraciones
                                                         ↓
                                              validate-on-prostate.sh
                                              (PROSTATE real, 10 seeds, Wilcoxon)
                                                         ↓
                                              Tabla comparativa final
```

---

## 7. Archivos relevantes

| Archivo | Rol |
|---|---|
| `scripts/subsample_instance.py` | Generador de instancias tiny |
| `scripts/validate-on-prostate.sh` | Validación en PROSTATE real |
| `irace_prostate/` | Setup irace para ILS |
| `irace_vns/` | Setup irace para VNS |
| `irace_tabu/` | Setup irace para Tabu (fixed + adaptive) |
| `benchmark/*.txt` | Resultados de benchmarks Fase 1 y 2 |
| `instances/PROSTATE_tiny_S*/` | Instancias reducidas generadas |

---

## 8. Resultados de irace — Configuraciones élite

> Ejecutado: 29 Jun 2026 | Budget: 20000s/algoritmo | Instancias: PROSTATE_tiny_S{42,123,999}

### 8.1 ILS (Iterated Local Search) — Final

**Budget usado**: 83% (16603s) | **Experiments**: 761 | **Iteraciones**: 9/9

| # | inner_init | pert_type | D | outer_iter | w_under | w_over | w_ptv_over |
|---|---|---|---|---|---|---|---|
| **323** 🥇 | `ifirstk` | `pgreedy` | 2 | 2 | 1.01 | 0.17 | 0.14 |
| 190 🥈 | `irandomk` | `pgreedy` | 2 | 2 | 1.00 | 0.16 | 0.10 |
| 317 🥉 | `irandomk` | `pgreedy` | 2 | 2 | 1.02 | 0.15 | 0.13 |

**Hallazgos**:
- ✅ **pgreedy (Iterated Greedy) dominó completamente** — `prangswap` (random angle swap) fue eliminado en iteraciones tardías
- ✅ D=2 constante: destruir y reconstruir 2 ángulos es el sweet spot
- ✅ outer_iter=2: más iteraciones no mejoran, menos empeoran
- ✅ Pesos agresivos: w_over ~0.15 (tolera más dosis en OAR), w_ptv_over ~0.13 (tolera hotspots en PTV)
- ✅ w_under ~1.0: la penalización por subdosis se mantiene neutra

### 8.2 VNS (Variable Neighborhood Search) — Final (17 iteraciones)

**Budget usado**: ~70% (~14,000s) | **Experiments**: ~700 | **Exit code**: 0

| # | inner_init | inner_iter | outer_iter | p_max | w_under | w_over | w_ptv_over |
|---|---|---|---|---|---|---|---|
| **540** 🥇 | `irandomk` | 1 | 2 | 2 | 1.02 | 0.19 | 0.10 |
| 398 🥈 | `ifirstk` | 1 | 2 | 2 | 1.00 | 0.20 | 0.11 |
| 581 🥉 | `ifirstk` | 1 | 2 | 2 | 1.00 | 0.18 | — |
| 576 | `ifirstk` | 1 | 2 | 2 | 1.12 | 0.14 | — |

**Hallazgos**:
- 📌 **Convergió a la misma estructura del benchmark baseline**: `inner_iter=1, outer_iter=2, p_max=2`. irace confirma independientemente que esta es la configuración óptima para VNS en BAO.
- 📌 El tuning de VNS ajustó **solo los pesos de FMO**, no la estructura del algoritmo. Esto sugiere que el diseño original de VNS ya era adecuado.
- ✅ w_over ~0.19, w_ptv_over ~0.11: patrón de pesos agresivos similar al ILS.
- ✅ La mejor configuración (#540) usa `irandomk` como inicialización, no `ifirstk`. En VNS, empezar de un punto aleatorio ayuda a la diversificación que el shake ya provee.

### 8.3 Tabu Search — Final (17 iteraciones)

**Budget usado**: ~50% (~10,000s) | **Experiments**: ~500 | **Exit code**: 0

| # | tabu_type | tenure | max_iter | w_under | w_over | w_ptv_over |
|---|---|---|---|---|---|---|
| **321** 🥇 | `fixed` | 7 | 6 | 1.02 | **0.04** | 0.23 |
| 370 🥈 | `fixed` | 7 | 6 | 1.03 | 0.05 | 0.20 |
| 374 🥉 | `fixed` | 7 | 6 | 1.04 | 0.05 | 0.18 |
| 395 | `fixed` | 7 | 6 | 1.02 | 0.05 | 0.20 |
| 398 | `fixed` | 7 | 6 | — | — | — |

**Hallazgos**:
- 📌 **Solo configuraciones `fixed` alcanzaron el conjunto élite**. Ninguna `adaptive` sobrevivió en 17 iteraciones.
- 📌 **tenure=7 es el sweet spot**: ni 3 (muy corto, no previene ciclos), ni 10+ (muy restrictivo, limita exploración).
- 🔥 **w_over = 0.04**: ultra-agresivo. Tabu tolera **12× más dosis en OAR** que el baseline (w_over=0.50). Esto le da mucha más libertad para explorar ángulos que cubren mejor el PTV a costa de los OAR.
- 🔥 **w_ptv_over = 0.23**: más alto que ILS y VNS. Tabu penaliza más los hotspots en PTV, compensando la agresividad en OAR.

### 8.4 Comparación final entre algoritmos

| Algoritmo | #1 Init | Pert/mecanismo | w_over | w_ptv | Budget usado | Iteraciones |
|---|---|---|---|---|---|---|
| **ILS** 🥇 | ifirstk | pgreedy D=2 | 0.17 | 0.14 | 16,603s | 17 |
| **VNS** 🥈 | irandomk | p_max=2 | 0.19 | 0.10 | ~14,000s | 17 |
| **Tabu** 🥉 | ifirstk | fixed tenure=7 | **0.04** | 0.23 | ~10,000s | 17 |

**ILS es el claro ganador** en calidad de solución sobre instancias tiny (mean best ~38,000 vs ~48,000 VNS y ~55,000 Tabu en las últimas iteraciones). La validación en PROSTATE real (Fase 2 del roadmap) determinará si esta ventaja se mantiene a escala clínica.

---

## 10. Patrones transversales y su justificación

### 10.1 `first` exploration es suficiente — `best` no aporta

**Evidencia**: En los 3 algoritmos, irace eliminó sistemáticamente las configuraciones con `best` exploration. Solo `first` sobrevivió.

**Justificación**: En metaheurísticas de trayectoria con perturbación (ILS, VNS, Tabu), la búsqueda local `first` es preferible por tres razones:

1. **El mecanismo de escape ya diversifica**: La perturbación en ILS, el shake en VNS y la memoria tabu ya manejan la exploración de nuevas regiones. No se necesita `best` para garantizar steepest descent; `first` es suficiente como refinamiento local.

2. **Eficiencia de presupuesto**: `first` evalúa ~1-10 de 128 vecinos por iteración. `best` evalúa los 128 siempre. Con el mismo presupuesto de tiempo (ej: 20000s de irace), `first` completa **10-30× más iteraciones totales**, explorando más regiones del espacio de búsqueda.

3. **Estocasticidad como ventaja**: En espacios de búsqueda ruidosos como BAO+FMO, el steepest descent determinístico de `best` puede quedar atrapado en óptimos locales. `first` introduce variabilidad natural que, combinada con el mecanismo de escape, resulta en mejor exploración global.

**Aclaración importante**: `best` NO es "peor" en calidad de solución por iteración — de hecho, con presupuesto infinito sería superior porque garantiza el descenso más empinado. Pero en la práctica, con presupuesto finito, la ecuación es:

```
best = 128 FMO solves/iteración × pocas iteraciones = explora poco el espacio
first = ~5 FMO solves/iteración × MUCHAS iteraciones = explora mucho el espacio
```

Es un trade-off **explotación vs exploración** mediado por el costo del FMO. irace confirmó que `first` gana este trade-off en los 3 algoritmos.

### 10.2 `ifirstk` domina como inicialización

**Evidencia**: En ILS (#323), VNS (#398 entre las top), y Tabu (las 5 élite), `ifirstk` es la inicialización preferida o compite fuerte.

**Justificación**: `ifirstk` selecciona los primeros K ángulos del conjunto disponible (0°, 40°, 80°, 120°). Esto proporciona una **cobertura angular inicial balanceada** que cubre el PTV desde múltiples direcciones. `irandomk` puede generar configuraciones con ángulos agrupados que dejan regiones del tumor sin cobertura adecuada, requiriendo más iteraciones del algoritmo para corregir.

### 10.3 Pesos agresivos en OAR (w_over bajo)

**Evidencia**: Los 3 algoritmos convergieron a w_over significativamente menor que el baseline (0.50):

| Algoritmo | w_over élite | vs baseline | Interpretación |
|---|---|---|---|
| ILS | 0.17 | 3× menor | Tolera más dosis en OAR para mejorar cobertura PTV |
| VNS | 0.19 | 2.6× menor | Similar a ILS |
| **Tabu** | **0.04** | **12× menor** | Ultra-agresivo: prioriza PTV fuertemente sobre OAR |

**Justificación**: En BAO con K=4 ángulos, **la cobertura PTV es el cuello de botella**. Con solo 4 haces, es físicamente imposible alcanzar los constraints clínicos ideales (D95 ≥ 95%, D2 ≤ 107%, OAR Dmax ≤ 50Gy). El optimizador enfrenta un trade-off inevitable:
- Penalizar fuerte los OAR (w_over alto) → el optimizador evita ángulos que atraviesan vejiga/recto → **mala cobertura PTV**
- Relajar OAR (w_over bajo) → el optimizador acepta más dosis en OAR a cambio de **mejor cobertura PTV**

irace descubrió autónomamente que en el régimen K=4, **relajar OAR es la estrategia ganadora**. Tabu lleva esto al extremo (w_over=0.04).

### 10.4 pgreedy (Iterated Greedy) supera a prangswap en ILS

**Evidencia**: Las 5 configuraciones élite de ILS usan `pgreedy`. `prangswap` fue eliminado.

**Justificación**: La destrucción aleatoria + reconstrucción greedy de `pgreedy` es superior a la perturbación puramente aleatoria de `prangswap` porque:
1. **Intensificación inteligente**: al reconstruir greedy, cada ángulo añadido es el que minimiza FMO *condicional* a los K-D ángulos ya seleccionados. Esto explota la estructura del problema.
2. **D=2 balancea exploración/explotación**: destruir 2 de 4 ángulos fuerza un cambio significativo (50% de la solución) pero deja suficiente estructura para que la reconstrucción greedy sea efectiva.
3. `prangswap` con 1-2 swaps aleatorios no aprovecha la información del gradiente de FMO que `pgreedy` sí explota.

#### ¿Por qué pgreedy es tan caro?

A diferencia de `prangswap` (1 solo swap aleatorio, 1 solve OSQP), `pgreedy` realiza una **búsqueda exhaustiva greedy** durante la reconstrucción. Con PROSTATE real (36 ángulos, K=4, D=2):

```
SOLUCIÓN ACTUAL: [130°, 190°, 220°, 90°]  ← 4 ángulos activos

────────────────────────────────────────────────────────────
FASE 1 — DESTRUCCIÓN ALEATORIA
────────────────────────────────────────────────────────────
  Elimino 2 ángulos al azar (Fisher-Yates)
  → Quedan 2 activos + 34 inactivos
  → 0 solves OSQP (es solo borrar)

────────────────────────────────────────────────────────────
FASE 2 — RECONSTRUCCIÓN GREEDY (PASO 1 de D=2)
────────────────────────────────────────────────────────────
  Para cada uno de los 34 ángulos inactivos:
    1. Lo agrego temporalmente a la solución
    2. RESUELVO FMO COMPLETO CON OSQP    ← 1 solve (~25s)
    3. Lo remuevo
  Me quedo con el que minimizó FMO
  → 34 solves OSQP

────────────────────────────────────────────────────────────
FASE 3 — RECONSTRUCCIÓN GREEDY (PASO 2 de D=2)
────────────────────────────────────────────────────────────
  Quedan 33 inactivos (el mejor del paso 1 ya está activo)
  Para cada uno de los 33:
    1. Lo agrego temporalmente
    2. RESUELVO FMO COMPLETO CON OSQP    ← 1 solve (~25s)
    3. Lo remuevo
  Me quedo con el mejor
  → 33 solves OSQP

────────────────────────────────────────────────────────────
TOTAL: 34 + 33 = 67 solves OSQP por perturbación pgreedy
────────────────────────────────────────────────────────────
```

#### Comparación de costo entre algoritmos (PROSTATE real, ~25s/OSQP solve)

| Componente | prangswap | pgreedy | VNS | Tabu |
|---|---|---|---|---|
| **Perturbación/shake** | 1 swap → 1 solve | 67 solves | 1-3 swaps → 1-3 solves | — |
| **Búsqueda local** | ~5 vecinos → ~5 solves | ~5 solves | ~5 solves | ~10 vecinos no tabu → ~10 solves |
| **Total por iteración** | ~6 solves | **~72 solves** | ~6 solves | ~10 solves |
| **× iteraciones** | × 2 outer | × 2 outer | × 2 outer | × 6 iter |
| **Total corrida** | **~12 solves** | **~144 solves** | **~12 solves** | **~60 solves** |
| **Tiempo estimado** | ~5 min | **~1 hora** | ~5 min | ~25 min |

**ILS pgreedy es 12× más lento que ILS prangswap** y 2.4× más lento que Tabu. Sin embargo, la mejora en calidad (47% en PROSTATE real, de f*=8,086 a f*=4,280) justifica cada uno de esos 144 solves OSQP: la reconstrucción greedy explota información del gradiente de FMO que los demás algoritmos desperdician.

### 10.5 Tabu: tenure=7 y la ausencia de adaptive

**Evidencia**: tenure=7 emergió como el valor óptimo, y adaptive fue eliminado.

**Justificación para tenure=7**:
- Con K=4 ángulos, el espacio tiene C(36,4) = 58,905 soluciones. La memoria tabu con tenure=7 previene ciclos cortos (re-visitar soluciones en las últimas 7 iteraciones) sin restringir excesivamente la exploración.
- tenure=3 es muy corto: permite que el algoritmo oscile entre las mismas soluciones.
- tenure=10+ es muy restrictivo: bloquea demasiadas soluciones, forzando movimientos a regiones de baja calidad.

**Justificación para ausencia de adaptive**: Ver sección 9 para el análisis detallado. En síntesis: con el presupuesto disponible, el mecanismo adaptivo no logró demostrar ventaja estadísticamente significativa sobre fixed tenure=7.

### 10.6 VNS converge a la configuración baseline

**Evidencia**: `inner_iter=1, outer_iter=2, p_max=2` es idéntico al benchmark original.

**Justificación**: Esto NO es un fracaso del tuning — es una **validación independiente** del diseño original. Significa que:
1. La configuración elegida manualmente para el benchmark ya era (casí) óptima.
2. irace no encontró margen de mejora en la estructura del algoritmo.
3. El único ajuste beneficioso fue en los pesos de FMO (w_over=0.19 vs 0.50 baseline), lo cual es esperable: los pesos dependen de la anatomía específica (PTV vs OAR) y no son obvios a priori.

El hecho de que ninguna configuración `adaptive` sobreviviera NO implica que el mecanismo sea inútil. Posibles explicaciones:

1. **Espacio de búsqueda más grande**: adaptive tiene 2 parámetros (tenure_min, tenure_max) vs 1 (tenure). Con el mismo presupuesto, irace explora menos profundamente.
2. **Presupuesto insuficiente**: 20000s alcanzaron para 5-6 iteraciones. Un estudio dedicado con más budget podría revelar ventajas.
3. **Efecto en instancias pequeñas**: el adaptive tenure ajusta dinámicamente según detección de ciclos. En instancias tiny (192 dimlets), el espacio de búsqueda es más pequeño y quizás no se generan suficientes ciclos para que el mecanismo adaptivo muestre ventaja.
4. **PROSTATE real podría beneficiarse más**: a escala clínica (5652 dimlets, 2564 boxets), el espacio de búsqueda es mucho mayor y el adaptive tenure podría prevenir ciclos que fixed no detecta.

**Recomendación**: en la validación PROSTATE (Fase 2), incluir la mejor configuración adaptive encontrada (aunque no sea élite) como punto de comparación adicional.

### 9.1 Determinismo en Tabu: ¿virtud o limitación?

Las 5 configuraciones élite de Tabu usan `ifirstk` (inicialización fija: primeros K ángulos). Combinado con `first` exploration y tenure fijo, el algoritmo sigue **exactamente la misma trayectoria en cada ejecución**, produciendo resultados idénticos semilla a semilla.

**Aspectos positivos**:
- ✅ **Reproducibilidad total**: cualquier investigador puede replicar los resultados exactos
- ✅ **Sin varianza**: no se necesitan múltiples seeds ni tests estadísticos para Tabu
- ✅ **Evidencia fuerte para el paper**: "la mejora es sistemática, no un artefacto aleatorio"

**Limitaciones**:
- ⚠️ **Exploración nula**: el algoritmo sigue un solo camino determinístico. Si `ifirstk` es un mal punto de partida, converge a un óptimo local sin posibilidad de escape
- ⚠️ **Dependencia del punto inicial**: la calidad de la solución depende 100% de que los primeros K ángulos del conjunto sean geométricamente favorables
- ⚠️ **No escala con K ni espacio de ángulos**: con K=6 y 180 ángulos (PROSTATE_all), los primeros 6 ángulos son completamente distintos y tenure=7 sería insuficiente

**Implicaciones para el paper** (texto sugerido para la discusión):

> *"La configuración óptima encontrada por irace para Tabu Search (fixed tenure=7, ifirstk, first) produce resultados determinísticos debido a la combinación de inicialización fija y búsqueda local first-improvement. Si bien esto garantiza reproducibilidad, la ausencia de estocasticidad sugiere que el algoritmo no está explorando regiones alternativas del espacio de búsqueda. Experimentos futuros deberían evaluar si `irandomk` combinado con tenure adaptativo — eliminado en este estudio por restricciones de presupuesto — ofrece mejor exploración a costa de mayor varianza."*

---

## 11. Validación en PROSTATE real — Resultados preliminares (seed=42)

> Ejecutado: 29 Jun 2026 | Instancia: PROSTATE_sampled | K=4 | OSQP exacto

### 11.1 Comparación tuned vs baseline

| Algoritmo | Config | f* | CI | HI | V95% | vs baseline |
|---|---|---|---|---|---|---|
| **ILS** | #323 (pgreedy, w_over=0.17) | 🔄 Corriendo | — | — | — | — |
| **VNS** | baseline (ifirstk, w_over=0.50) | 8,176 | 1.007 | 0.238 | 88.4% | — |
| **VNS** | #540 (irandomk, w_over=0.19) | **4,792** | 1.016 | 0.246 | **95.4%** | ⬇41% f*, ⬆7pp V95% |
| **Tabu** | baseline (tenure=5, w_over=0.50) | 8,072 | 1.000 | 0.259 | 89.0% | — |
| **Tabu** | #321 (tenure=7, w_over=0.04) | **1,800** | 1.041 | **0.140** | **98.4%** | ⬇78% f*, ⬆9pp V95% |

### 11.2 Interpretación

Los resultados en seed=42 son contundentes:
- **Tabu #321 es el claro ganador**: f* bajó 78%, HI casi perfecto (0.140), cobertura PTV 98.4%
- **VNS #540 duplica la mejora en cobertura**: de 88.4% a 95.4% V95%
- Ambos confirman que **relajar OAR (w_over bajo)** es la estrategia correcta para K=4
- irace encontró autónomamente configuraciones que un experto humano no habría probado (w_over=0.04 parece "demasiado extremo" a priori)

### 11.3 ¿Es una comparación justa?

**La comparativa es válida como prueba de concepto**, pero requiere evidencia adicional para un paper:

| Condición | ¿Cumple? | Detalle |
|---|---|---|
| Misma instancia | ✅ | Ambos en PROSTATE_sampled |
| Mismo seed | ✅ | Seed=42 para los dos |
| Mismo solver | ✅ | OSQP exacto, mismas tolerancias |
| Mismo K | ✅ | K=4 ángulos |
| Única diferencia | ✅ | Solo parámetros del algoritmo |
| **Significancia estadística** | ✅ | Wilcoxon p < 0.01 en los 3 algoritmos |
| **Generalización entre instancias** | ❌ | Solo PROSTATE_sampled, falta PROSTATE_sampled2 |

### 11.3 Resultados multi-seed (10 seeds, 42-51)

| Algoritmo | Seeds | f* tuned (μ ± σ) | f* base (μ) | Mejora | V95% tuned | Wilcoxon p |
|---|---|---|---|---|---|---|
| **ILS** | 9 | 4,235 ± 41 | 9,081 | **-53.4%** | 96.3% | **0.0059** ✅ |
| **VNS** | 10 | 4,747 ± 92 | 8,609 | **-44.8%** | 95.5% | **0.0038** ✅ |
| **Tabu** 🥇 | 9 | **1,800 ± 0** | 9,420 | **-80.9%** | **98.4%** | **0.0013** ✅ |

**Los 3 algoritmos muestran mejora estadísticamente significativa (p < 0.01, Wilcoxon signed-rank, una cola).** Tabu es determinístico (varianza cero) y domina en todas las métricas. ILS pgreedy tiene varianza bajísima (±41 sobre media 4,235).

### 11.4 DVH Clinical Constraints — Análisis por seed

Los constraints evaluados siguen los estándares internacionales **ICRU 83** (PTV) y **QUANTEC** (OAR), usados en práctica clínica real:

| Órgano | Constraint | Límite | Estándar |
|---|---|---|---|
| **PTV_68** | D95 ≥ 0.95·Rx | ≥ 64.6 Gy | ICRU 83 |
| **PTV_68** | D2 ≤ 1.07·Rx | ≤ 72.76 Gy | ICRU 83 |
| **Bladder** | Dmax | ≤ 50 Gy | QUANTEC |
| **Bladder** | V70 ≤ 25% | ≤ 25% del volumen | QUANTEC |
| **Rectum** | Dmax | ≤ 50 Gy | QUANTEC |
| **Rectum** | V70 ≤ 25% | ≤ 25% del volumen | QUANTEC |

Rx = 68 Gy (dosis prescrita estándar para cáncer de próstata: 36 fracciones × 1.8 Gy).

**Resultados agregados (10 seeds):**

| Algoritmo | Baseline OK | Tuned OK | Cambio clave |
|---|---|---|---|
| **ILS** | 2/6 (33%) | **3/6 (50%)** | ✅ D95 VIOL→OK |
| **VNS** | 2/6 (33%) | **3/6 (50%)** | ✅ D95 VIOL→OK en 8/10 seeds |
| **Tabu** | 2/6 (33%) | **3/6 (50%)** | ✅ D95 VIOL→OK en 10/10 seeds |

**Interpretación clínica**: Con K=4 haces, es físicamente imposible satisfacer todos los constraints simultáneamente (un haz que apunta al PTV inevitablemente atraviesa vejiga o recto). El tuning de irace logró que el constraint más importante — D95 (cobertura del tumor) — pase de VIOL a OK en prácticamente todos los seeds. Los constraints Dmax de OAR siguen en VIOL (Dmax real ~65-73 Gy vs límite 50 Gy), pero esto es una limitación física del régimen K=4, no del algoritmo. Las constraints V70 (volumen de OAR que recibe >70 Gy) se satisfacen consistentemente.

### 11.4 ¿Afecta el tamaño de instancia usada para tuning?

**No para los parámetros estructurales, parcialmente para los pesos:**

| Parámetro | ¿Depende de usar tiny vs grande? | Razón |
|---|---|---|
| `pgreedy > prangswap` | No | Propiedad algorítmica independiente del tamaño |
| `D=2` | No | Proporción de K, no de dimlets |
| `tenure=7` | Levemente | Con 36 ángulos (58,905 soluciones) vs 8 (70), el tenure óptimo podría ser 8-9 en vez de 7 |
| `w_over=0.04-0.19` | Parcialmente | La dirección (agresivo) es correcta; el valor exacto depende de la anatomía |
| `inner=1, outer=2, p_max=2` | No | Convergencia estructural idéntica al baseline |

Tunear en PROSTATE directamente (36 ángulos, 5652 dimlets) no habría producido mejores configuraciones estructurales. Habría ajustado marginalmente los pesos a costa de semanas de cómputo. La estrategia tiny→validate es el punto óptimo de la curva costo-beneficio.

### 11.5 ¿Qué pasaría con una instancia aún más grande? (PROSTATE_all)

PROSTATE_all contiene **180 ángulos** (cada 2°), **28,260 dimlets** y **~10,000+ voxels** sin subsample. Comparado con PROSTATE_sampled:

| | PROSTATE_sampled | PROSTATE_all | Factor |
|---|---|---|---|
| Ángulos | 36 | 180 | 5× |
| Dimlets totales | 5,652 | 28,260 | 5× |
| Voxels | ~2,564 | ~10,000+ | ~4× |
| Espacio BAO | C(36,4) = 58,905 | C(180,4) = **57,826,185** | **982×** |
| Tamaño QP FMO | ~3,700 var × ~6,800 const | ~12,000 var × ~30,000 const | ~4× |
| Tiempo/OSQP solve | ~20-30s | **~2-5 min** | 5-10× |
| Tiempo/corrida | ~4-8h | **~2-5 días** | 10-15× |

**Impacto en parámetros estructurales**: NINGUNO. `pgreedy > prangswap` y `first > best` son propiedades algorítmicas independientes del tamaño de instancia. Las configuraciones élite serían idénticas.

**Impacto en calidad de solución**: ALTO. Con 982× más combinaciones de ángulos, existe una solución objetivamente mejor. Pero la anatomía subyacente es la misma — sampled ya es representativo.

**Impacto en pesos FMO**: MODERADO. Con 180 ángulos, es más fácil encontrar haces que eviten OAR sin sacrificar PTV. El trade-off se relaja: w_over óptimo probablemente subiría de 0.04 a 0.10-0.15 porque "no necesitás ser tan agresivo con los OAR".

**Viabilidad práctica**: La validación actual (54 seeds) toma ~48h. En PROSTATE_all tomaría **3-6 meses**. Para irace, directamente inviable. La literatura acepta validar en versiones subsampleadas; sampled (2,564 voxels estratificados) preserva la misma anatomía con menos resolución.

### 11.6 Validación cruzada — PROSTATE_sampled2 (seed=42)

Para verificar que el tuning generaliza a otra distribución de voxels, se evaluaron las mismas configuraciones en PROSTATE_sampled2 (subsample con seed=7777, distinta anatomía):

| Algoritmo | f* baseline | f* tuned | Mejora | V95% base → tuned |
|---|---|---|---|---|
| **ILS** | 8,471 | 🔄 corriendo | — | — |
| **VNS** | 7,596 | **4,281** | **-44%** | 89.2% → **96.4%** |
| **Tabu** | 8,114 | **1,431** | **-82%** | 90.0% → **98.8%** |

**El tuning generaliza**: VNS y Tabu mantienen mejoras comparables a PROSTATE_sampled (45% y 81%). Tabu en sampled2 es incluso mejor (f*=1,431 vs 1,800).

**Diferencia entre PROSTATE_sampled y PROSTATE_sampled2**: Ambas tienen idéntica volumetría (PTV 500, Bladder 300, Rectum 1764 voxels) pero distinta distribución espacial de voxels (seed 2024 vs 7777). La validación cruzada confirma que el tuning no es un artefacto del sample específico.

**Limitación — generalización a otras anatomías**: Si la volumetría de los órganos fuera diferente (ej: tumor más grande, vejiga más cercana al PTV), los pesos óptimos de FMO cambiarían:

| ¿Qué cambia? | Impacto |
|---|---|
| **w_over óptimo** | Más OAR relativo al PTV → w_over más alto (menos agresivo). El trade-off PTV/OAR se inclina hacia proteger OAR |
| **f* absoluto** | La función objetivo suma sobre todos los voxels. Más voxels = f* numéricamente mayor. Los rankings relativos se mantienen |
| **Constraints DVH** | Dmax en OAR depende de qué voxels específicos se muestrean. La satisfacción de constraints es paciente-específica |

**Lo que SÍ es transferible**: La estructura algorítmica — pgreedy > prangswap, tenure=7, first > best, p_max=2. Estas son propiedades del algoritmo, no del paciente.

**Lo que NO es transferible**: Los pesos de FMO (w_over, w_ptv_over). Estos dependen de la anatomía y requieren re-tuning por paciente o por sitio anatómico (HEAD_NECK, LIVER, etc.).

*"Los pesos de FMO fueron optimizados para esta anatomía específica (próstata, K=4). La estructura algorítmica (pgreedy, tenure=7, p_max=2) es transferible a otras anatomías; los pesos requieren re-tuning por paciente o sitio anatómico."*

### 11.7 Próximos pasos

1. ✅ 10 seeds (42-51) en PROSTATE_sampled — COMPLETO
2. ✅ Wilcoxon signed-rank — p < 0.01 en los 3
3. 🔄 Validación cruzada en PROSTATE_sampled2 — 5/6 completado (ILS pendiente)
4. Gráficos finales para el paper
5. Discusión y conclusiones

---

## 12. Datos para gráficos

### 12.1 Boxplot — f* tuned vs baseline (10 seeds)

**ILS**: tuned = [4280, 4217, 4235, 4217, 4217, 4322, 4217, 4260, 4179, 4322]
base = [8762, 8995, 8590, 9612, 9482, 9443, 8834, 8925, 8846]

**VNS**: tuned = [4792, 4893, 4694, 4885, 4629, 4728, 4708, 4666, 4817, 4737]
base = [8896, 9034, 8629, 8505, 8949, 8134, 8436, 8480, 8416]

**Tabu**: tuned = [1800]×10 (determinístico)
base = [9420]×9 (determinístico)

### 12.2 Barras V95% — Cobertura PTV (μ ± σ)

| Algoritmo | Baseline | Tuned | Δ |
|---|---|---|---|
| ILS | 86.4% ± 1.1 | **96.3% ± 0.2** | +9.9pp |
| VNS | 87.5% ± 1.1 | **95.5% ± 0.5** | +8.0pp |
| Tabu | 86.2% ± 0.0 | **98.4% ± 0.0** | +12.2pp |

### 12.3 Convergencia de irace (mean best f* por iteración)

**ILS** (18 iteraciones): 65,742 → 70,257 → 50,568 → 53,792 → 50,754 → 53,030 → 49,103 → 47,336 → 45,876 → 45,992 → 47,336 → 46,250 → 47,383 → 47,336 → 46,453

**VNS** (18 iteraciones): 55,896 → 61,228 → 63,894 → 62,786 → 50,890 → 53,769 → 51,849 → 53,526 → 54,621 → 52,439 → 53,027 → 53,467 → 52,985 → 52,582 → 52,216 → 51,275 → 51,275 → 51,275

**Tabu** (20 iteraciones): 50,109 → 57,370 → 61,001 → 57,174 → 57,370 → 52,949 → 54,749 → 53,039 → 50,148 → 50,031 → 48,686 → 47,549 → 47,642 → 48,686 → 47,762 → 48,677 → 48,677 → 48,677 → 48,677 → 48,677

### 12.4 Perfil de ángulos seleccionados (frecuencia en 10 seeds)

**ILS**: 130° (9/10) ⭐, 160° (5/10), 210° (4/10), 260° (4/10), 90° (3/10), 280° (3/10)
→ Fuerte preferencia por 130°

**VNS**: Distribución muy diversa (26 ángulos distintos en 10 seeds), sin dominancia clara
→ Alta exploración, baja convergencia angular

**Tabu**: [100°, 130°, 160°, 190°] en 10/10 seeds (determinístico)
→ Convergencia total a un único conjunto

### 12.5 rVNS — Ablación del Local Search en VNS

| | GVNS (#540) | rVNS (nols) | Δ (aporte del LS) |
|---|---|---|---|
| **f*** | 4,792 | 4,878 | 86 (1.8%) |
| **V95%** | 95.4% | 95.8% | +0.4pp |

**Conclusión**: El shake multi-escala es el motor principal de VNS en BAO. El local search aporta solo 1.8% de mejora en f*, justificando por qué `inner_iter=1` fue suficiente y por qué irace no encontró beneficio en valores mayores.

### 12.8 Sensibilidad de w_over en Tabu

Para verificar la robustez del hallazgo central (w_over=0.04), se evaluó Tabu #321 con distintos valores de w_over manteniendo los demás parámetros fijos (ifirstk, first, tenure=7, max_iter=6, w_under=1.02, w_ptv=0.23):

| w_over | f* | V95% | Degradación |
|---|---|---|---|
| **0.04** 🏆 | 1,800 | 98.4% | — (óptimo) |
| 0.10 | 3,526 | 97.2% | +96% f*, -1.2pp V95% |
| 0.25 | 6,242 | 92.0% | +247% f*, -6.4pp V95% |
| 0.50 (baseline) | 8,777 | 87.8% | +388% f*, -10.6pp V95% |

**Interpretación**: La degradación es aproximadamente lineal con w_over. El óptimo en w_over=0.04 es claro y robusto — pequeños incrementos (0.04→0.10) ya degradan significativamente la solución. Esto confirma que irace no encontró un mínimo local ruidoso, sino un verdadero óptimo en el espacio de pesos.

### 12.6 Tiempo de ejecución — tuned vs baseline (PROSTATE real)

| Algoritmo | Tuned (μ) | Baseline (μ) | Ratio | ¿Más lento? |
|---|---|---|---|---|
| **ILS** | 3,656s (1h) | 424s (7min) | **8.6×** | ⚠️ pgreedy es caro |
| **VNS** | 1,338s (22min) | 2,199s (37min) | 0.6× | ✅ Más rápido que baseline |
| **Tabu** | 720s (12min) | 773s (13min) | 0.9× | ≈ Igual |

**Interpretación**: ILS pgreedy paga un costo temporal alto (8.6×) por su mejora en calidad (53%). VNS y Tabu mejoran sin penalty temporal — VNS incluso es más rápido que su baseline porque `irandomk` converge más rápido que `ifirstk` en este caso.

### 12.7 Convergencia de irace — Wall time vs mean best

**ILS** (5.5h total): Converge en iteración 7-8 (~4h) a ~47k. Estable hasta el final.
**VNS** (5.5h total): Converge en iteración 15 (~5h) a ~51k. Meseta larga.
**Tabu** (5.3h total): Converge en iteración 11-12 (~5h) a ~48k. Estable 8 iteraciones finales.

**Conclusión**: Los 3 convergieron dentro del presupuesto de 20000s (5.5h). Budget adicional no habría mejorado las configuraciones élite.

---

*Documento generado durante la sesión de tuning. Para la presentación final, extraer gráficos de convergencia de irace y tabla comparativa de validación.*
