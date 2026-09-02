# Random init: grid5 vs unrestricted — análisis comparativo

## Objetivo

Medir el efecto de restringir el catálogo de ángulos candidatos de
`irandomk` (solución inicial de `RandomKAnglesInit`) a múltiplos de 5° sobre
el resultado de la búsqueda local First/Best Improvement, manteniendo todo
lo demás idéntico al experimento ya existente
[`experiments/local_search/nangshift10/unrestricted`](../../unrestricted/ANALYSIS.md).

La única variable que cambia entre las dos condiciones es el rango de
ángulos que `irandomk` puede elegir como punto de partida:

- **unrestricted**: cualquiera de los 360 ángulos del catálogo (0°–359°).
- **grid5**: sólo ángulos cuyo grado es múltiplo de 5 (`bao_.angleDegree(i) %
  5 == 0`), fix aplicado en `RandomKAnglesInit::generateEmptySolution` /
  `FirstKAnglesInit::generateEmptySolution` (`imrt/imrt_bao.cpp`).

Todo lo demás — instancia, K, vecindario, step del vecindario, criterio de
parada, semillas — es exactamente lo mismo que en el experimento base.

## Metodología

- **Instancia:** `instances/CERR_Prostate`.
- **K = 4** ángulos activos (`baoimrt 4`).
- **Solución inicial:** `irandomk`, condición **grid5** (múltiplos de 5°)
  para este experimento nuevo. La condición **unrestricted** ya está
  corrida y documentada en `experiments/local_search/nangshift10/unrestricted/` — sus
  números se citan tal cual, no se re-derivan.
- **Vecindario:** `nangshift 10` (idéntico en ambas condiciones).
- **Criterio de parada:** `locmin` (óptimo local real, sin ILS).
- **Diseño pareado:** mismas 10 semillas (`rnds 1`..`rnds 10`) en ambos
  grupos (First/Best) de este experimento, mismas semillas numeradas que en
  el experimento unrestricted (para poder cruzar resultados semilla-a-semilla
  entre condiciones).

Comando exacto por corrida:

```bash
./build/emili instances/CERR_Prostate baoimrt 4 csv <path>/trajectory.csv \
  <first|best> irandomk locmin nangshift 10 rnds <seed>
```

### Nota importante: posible desajuste 5° vs step=10°

El comentario en el código que justifica restringir el catálogo inicial a
múltiplos de 5° habla de "la reja de 5° que usa el vecindario de shift". Sin
embargo, el `nangshift` efectivamente usado tanto en este experimento como
en el experimento unrestricted es **10**, no 5. Es decir: el punto de
partida queda alineado a una reja de 5°, pero el vecindario que explora la
búsqueda local se mueve de a 10° — dos rejas distintas conviviendo en la
misma corrida.

Esto **no se resuelve en este documento**: se deja como pregunta abierta
para quien revise este análisis. No está claro si el criterio correcto es
(a) alinear el catálogo inicial al mismo step que usa el vecindario (grid10
en vez de grid5), (b) el grid5 es intencional porque describe la reja "nativa"
del catálogo de instancia y el step del vecindario es una elección
independiente, o (c) el mismatch es simplemente un bug de redacción en el
comentario del código y no en el comportamiento.

## Estructura de la carpeta

Reorganizado — los datos de la condición grid5 viven junto a las demás
condiciones de búsqueda local; esta carpeta sólo contiene la comparación
cruzada:

```
experiments/local_search/nangshift10/
├── unrestricted/                        (comparado acá, datos propios)
├── restricted/                          (llamado "grid5" en el análisis de abajo)
│   ├── data/
│   │   ├── first/seed01..15/            run.log, trajectory.csv,
│   │   │                                 trajectory_improvements.csv,
│   │   │                                 convergence.png, iterations.png
│   │   └── best/seed01..15/             (ídem)
│   └── plots/seed_comparisons/
│       └── seed01..15_comparison.png    First vs Best superpuesto, condición grid5
└── comparisons/unrestricted_vs_grid5/
    ├── ANALYSIS.md                      este archivo
    └── plots/
        └── grid5_vs_unrestricted_comparison.png   comparación clave: 4 condiciones
                                                     (unrestricted/grid5 × First/Best)
```

(agrupado bajo `nangshift10/` porque próximamente se corre la misma comparación con `nangshift5/`, ver [[project_leslie_localsearch_review]])

Nota: al momento de escribir este documento las tablas de abajo siguen
citando sólo 10 semillas (seed01-10); seed11-15 ya están corridas en ambas
condiciones pero las tablas/promedios de este análisis todavía no se
actualizaron — pendiente.

## Resultados

Objetivo FMO: menor es mejor (placeholder clínico, ver `CLINICAL_CONFIG` en
[[project_ampl_gurobi]]). Los valores **unrestricted** están citados
verbatim de la tabla de resultados en
`experiments/local_search/nangshift10/unrestricted/ANALYSIS.md`. Δ = grid5 − unrestricted.

### First Improvement

| seed | unrestricted: obj | unrestricted: evals | grid5: obj | grid5: evals | Δ obj | Δ evals |
|------|-------------------:|----------------------:|-------------:|---------------:|-------------:|----------:|
| 01   | 98 768.82          | 16                     | 95 350.03    | 35              | −3 418.79    | +19       |
| 02   | 89 882.39          | 48                     | 94 794.24    | 45              | +4 911.85    | −3        |
| 03   | 96 954.64          | 27                     | 96 910.91    | 27              | −43.73       | 0         |
| 04   | 96 347.79          | 25                     | 95 377.57    | 47              | −970.22      | +22       |
| 05   | 92 953.87          | 13                     | 95 733.99    | 9               | +2 780.12    | −4        |
| 06   | 93 822.34          | 30                     | 94 466.84    | 41              | +644.50      | +11       |
| 07   | 96 895.07          | 27                     | 93 506.09    | 35              | −3 388.98    | +8        |
| 08   | 97 788.99          | 32                     | 92 557.14    | 14              | −5 231.85    | −18       |
| 09   | 95 524.84          | 21                     | 94 470.21    | 38              | −1 054.63    | +17       |
| 10   | 96 409.08          | 25                     | 91 471.44    | 49              | −4 937.64    | +24       |

Promedio: unrestricted obj = 95 534.8, evals = 26.4. grid5 obj = 94 463.85,
evals = 34.0.

### Best Improvement

| seed | unrestricted: obj | unrestricted: evals | grid5: obj | grid5: evals | Δ obj | Δ evals |
|------|-------------------:|----------------------:|-------------:|---------------:|-------------:|----------:|
| 01   | 95 596.90          | 89                     | 96 582.52    | 41              | +985.62      | −48       |
| 02   | 97 584.14          | 29                     | 102 734.80   | 33              | +5 150.66    | +4        |
| 03   | 96 954.64          | 41                     | 95 798.68    | 57              | −1 155.96    | +16       |
| 04   | 96 347.79          | 25                     | 96 860.39    | 31              | +512.60      | +6        |
| 05   | 92 953.87          | 25                     | 95 733.99    | 9               | +2 780.12    | −16       |
| 06   | 93 822.34          | 41                     | 94 466.84    | 41              | +644.50      | 0         |
| 07   | 95 575.04          | 49                     | 93 506.09    | 81              | −2 068.95    | +32       |
| 08   | 91 718.01          | 57                     | 92 557.14    | 19              | +839.13      | −38       |
| 09   | 91 131.79          | 73                     | 94 470.21    | 57              | +3 338.42    | −16       |
| 10   | 97 485.35          | 41                     | 95 136.77    | 41              | −2 348.58    | 0         |

Promedio: unrestricted obj = 94 917.0, evals = 47.0. grid5 obj = 95 784.74,
evals = 41.0.

### Empates exactos (First = Best) en la condición grid5

5 de 10 semillas convergen al mismo óptimo local desde First y Best
(vs. 4/10 en unrestricted):

| seed | ángulos finales      | objetivo   | First evals | Best evals |
|------|-----------------------|-----------:|------------:|-----------:|
| 05   | [40,180,270,310]      | 95 733.99  | 9           | 9          |
| 06   | [15,145,225,280]      | 94 466.84  | 41          | 41         |
| 07   | [105,225,315,325]     | 93 506.09  | 35          | 81         |
| 08   | [15,140,150,225]      | 92 557.14  | 14          | 19         |
| 09   | [60,105,270,325]      | 94 470.21  | 38          | 57         |

seed07 es el caso más interesante de este grupo: mismo óptimo local exacto,
pero Best tarda más del doble de evaluaciones (81 vs 35) en llegar ahí — un
recordatorio de que "empate en calidad" no implica "empate en costo".

## Interpretación

**El patrón First-vs-Best se invierte respecto al baseline unrestricted.**
En unrestricted, Best encontraba mejor objetivo en 4/10 semillas contra 2/10
de First (con 4 empates), y el objetivo promedio de Best (94 917.0) superaba
al de First (95 534.8). En grid5 ocurre lo contrario: First gana en 4/10
semillas (01, 02, 04, 10) — incluyendo diferencias grandes de +4 000 a
+5 000 en el objetivo —, Best gana en sólo 1/10 (seed 03), y hay 5/10
empates. El objetivo promedio de First (94 463.85) ahora es mejor que el de
Best (95 784.74).

No hay, a priori, una explicación mecánica obvia para esta inversión dentro
del alcance de este experimento — podría ser ruido estadístico con n=10
(estas diferencias grandes en seeds 01/02/10 pesan mucho en el promedio), o
podría estar relacionado con el desajuste reja-5°/step-10° descripto arriba
(un punto de partida más "grueso" cambia la topología de vecinos disponibles
de forma distinta para First que para Best). Esto queda como pregunta
abierta, igual que el desajuste de grillas — no se resuelve acá.

**El costo relativo se mantiene en la misma dirección pero se achica.** Best
sigue siendo, en promedio, más caro que First (41.0 vs 34.0 evaluaciones en
grid5, un ratio ~1.2×), pero la brecha es mucho menor que en unrestricted
(47.0 vs 26.4, ratio ~1.8×). Esto es consistente con más empates: cuando
First y Best llegan al mismo óptimo, la diferencia de costo tiende a
comprimirse (aunque seed07 muestra que no siempre — ahí el empate en calidad
convive con una diferencia de costo de más del doble).

**La tasa de empates sube.** 5/10 en grid5 contra 4/10 en unrestricted. Con
un catálogo inicial más restringido (72 ángulos posibles en vez de 360),
hay menos puntos de partida distintos disponibles combinatoriamente, lo cual
podría — otra vez, especulativamente — aumentar la probabilidad de que
First y Best terminen convergiendo a la misma cuenca de atracción. No se
verificó esta hipótesis con un análisis formal; se deja anotada para futuro
trabajo.

## Pendiente / próximos pasos

- Resolver la pregunta abierta sobre el desajuste reja-5°/step-10°: decidir
  si el catálogo inicial debería alinearse al mismo step que el vecindario
  (grid10) o si la reja de 5° es una elección independiente y válida por sí
  misma.
- Investigar si la inversión First-vs-Best observada acá es un efecto real
  del catálogo restringido o ruido de n=10 — repetir con más semillas o un
  test estadístico pareado (Wilcoxon signed-rank) sobre las diferencias
  grid5 vs unrestricted, no sólo sobre First vs Best dentro de cada
  condición.
- Confirmar formalmente si la tasa de empates más alta en grid5 es
  atribuible al tamaño reducido del catálogo inicial (72 vs 360 ángulos) o
  es coincidencia con n=10.
- Repetir con otros valores de `nangshift` (5° para que coincida
  exactamente con la reja del catálogo inicial, o 30°) para separar el
  efecto del catálogo inicial del efecto del step del vecindario.
