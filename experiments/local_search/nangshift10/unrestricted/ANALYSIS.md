# First vs Best Improvement — análisis de búsqueda local (sin ILS)

## Objetivo

Comparar `First Improvement` contra `Best Improvement` como estrategias de
búsqueda local **puras**, sobre el vecindario de desplazamiento angular
(`AngleShiftNeighborhood`, step=10°), partiendo de soluciones iniciales
aleatorias. El objetivo es observar:

1. Qué tan buena es la solución final que encuentra cada estrategia.
2. Cuánto cuesta en evaluaciones FMO (llamadas a Gurobi) llegar ahí.
3. Cuánta variabilidad hay según el punto de partida.

## Por qué "sin ILS"

Esto es deliberado, no una simplificación. `First`/`Best` se invocan **sin** el
prefijo `ils` — es decir, sin loop externo de perturbación (`prangswap`) ni
criterio de aceptación (`improve`). Envolver la comparación en un ILS
mezclaría dos preguntas distintas: "¿qué tan buena es la búsqueda local en sí?"
vs. "¿qué tan bien rescata el ILS con reintentos?". Para aislar el efecto de
la estrategia de búsqueda local, se corre la búsqueda local sola, hasta su
propio óptimo local, y ahí termina.

Existe un experimento anterior (`experiments/_legacy/random_init_first_vs_best/`) que
sí usaba `ils ... tmaxiter 10 ... prangswap 1 improve` — **no es parte de este
análisis**, quedó como corrida exploratoria previa con una metodología distinta
(y con un tope arbitrario de iteraciones en vez de convergencia real).

## Metodología

- **Instancia:** `instances/CERR_Prostate` (real, formato CERR crudo, 360
  ángulos candidatos, sin restringir el alcance de la búsqueda).
- **K = 4** ángulos activos (`baoimrt 4`).
- **Solución inicial:** `irandomk` — K ángulos elegidos al azar del catálogo
  completo de 360.
- **Vecindario:** `nangshift 10` — por cada ángulo activo, prueba ±10°
  (con wraparound circular en 0°/359°). Tamaño de vecindario = 2K = 8
  candidatos por paso (menos si algún desplazamiento colisiona con otro
  ángulo ya activo — ver `imrt_bao.cpp`, chequeo de colisión).
- **Criterio de parada:** `locmin` (`LocalMinimaTermination`) — corre hasta
  encontrar un óptimo local real (ningún vecino del vecindario mejora), no un
  tope arbitrario de iteraciones.
- **Diseño pareado:** 10 semillas (`rnds 1`..`rnds 10`), **las mismas 10
  semillas usadas en ambos grupos** (First y Best) — así cualquier diferencia
  observada es atribuible a la estrategia, no al punto de partida al azar.
  Semillas *distintas entre sí* dentro de cada grupo (para observar
  variabilidad real).

Comando exacto por corrida:

```bash
./build/emili instances/CERR_Prostate baoimrt 4 csv <path>/trajectory.csv \
  <first|best> irandomk locmin nangshift 10 rnds <seed>
```

## Estructura de esta carpeta

```
local_search_first_vs_best/
├── ANALYSIS.md                 este archivo
├── data/
│   ├── first/seed01..10/       trajectory.csv, trajectory_improvements.csv
│   └── best/seed01..10/        (ídem)
└── plots/
    ├── first_vs_best_comparison.png    resumen: objetivo final + costo, las 10 semillas
    └── seed_comparisons/
        └── seed01..10_comparison.png   convergencia First vs Best superpuesta, por semilla
```

## Resultados

| seed | First: objetivo | First: evals | Best: objetivo | Best: evals | Δ (First − Best) |
|------|-----------------:|--------------:|-----------------:|-------------:|-------------------:|
| 01   | 98 768.82        | 16            | 95 596.90         | 89           | +3 171.92          |
| 02   | 89 882.39        | 48            | 97 584.14         | 29           | −7 701.75          |
| 03   | 96 954.64        | 27            | 96 954.64         | 41           | 0.00 (empate)       |
| 04   | 96 347.79        | 25            | 96 347.79         | 25           | 0.00 (empate)       |
| 05   | 92 953.87        | 13            | 92 953.87         | 25           | 0.00 (empate)       |
| 06   | 93 822.34        | 30            | 93 822.34         | 41           | 0.00 (empate)       |
| 07   | 96 895.07        | 27            | 95 575.04         | 49           | +1 320.03          |
| 08   | 97 788.99        | 32            | 91 718.01         | 57           | +6 070.98          |
| 09   | 95 524.84        | 21            | 91 131.79         | 73           | +4 393.05          |
| 10   | 96 409.08        | 25            | 97 485.35         | 41           | −1 076.27          |

*(objetivo FMO: menor es mejor — placeholder clínico, ver [[project_ampl_gurobi]] en memoria sobre `CLINICAL_CONFIG`)*

**Resumen:**
- Best encuentra mejor objetivo en 4/10 semillas, First en 2/10, empate exacto en 4/10.
- Costo promedio: First = 26.4 evaluaciones, Best = 47.0 evaluaciones (~1.8× más caro).
- Objetivo promedio: First = 95 534.8, Best = 94 917.0.

## Interpretación

Cuando First y Best difieren de verdad (6 de 10 semillas), Best tiende a
encontrar un óptimo local mejor — consistente con que evalúa el vecindario
completo en cada paso en vez de conformarse con la primera mejora. Pero paga
un costo real: en promedio ~1.8× más evaluaciones FMO (llamadas a Gurobi) por
corrida. Los 4 empates exactos ocurren cuando, desde ese punto de partida
random, la primera dirección que prueba First ya resulta ser también la mejor
de las 8 — ahí no hay diferencia que capturar.

Este es el tradeoff calidad-vs-costo que define la elección entre First y
Best como motor de búsqueda local dentro de una eventual ILS completa.

## Pendiente / próximos pasos

- Test estadístico pareado (Wilcoxon signed-rank) sobre las 10 diferencias,
  dado el diseño pareado — con n=10 es chico pero es lo que da la metodología.
- Repetir con otro `step` de `nangshift` (5°, 30°) para ver si el patrón se sostiene.
- Evaluar con más semillas si se necesita mayor poder estadístico.
