# Cómo funciona la búsqueda local: First/Best Improvement + locmin

Explica, con código real y trazas de corridas reales, cómo se mueve la
búsqueda local en `imrt_bao.cpp` cuando se invoca `first`/`best ... nangshift
<step> ... locmin`. Pensado para entender el mecanismo, no solo el resultado
final — cada sección referencia el archivo y las líneas exactas, y usa datos
de corridas de `experiments/local_search/nangshift10/restricted/`.

## Resumen rápido

| Pieza | Qué hace | Dónde |
|---|---|---|
| `nangshift <step>` | Genera el vecindario: por cada ángulo activo, ±`step` grados (wraparound circular) | `AngleShiftNeighborhood`, `imrt_bao.cpp:328-451` |
| `first` | De cada ronda, toma el **primer** vecino que mejora y corta ahí | `FirstImprovementSearch::search`, `emilibase.cpp:775` |
| `best` | De cada ronda, recorre **todos** los vecinos y se queda con el mejor | `BestImprovementSearch::search`, `emilibase.cpp:627` |
| `locmin` | Termina cuando una ronda completa no logra mejorar el incumbente (empate incluido) | `LocalMinimaTermination::terminate`, `emilibase.cpp:1699` |
| `irandomk` | Elige la solución **inicial** — separado del vecindario, no confundir | `RandomKAnglesInit`, `imrt_bao.cpp:184-225` |

Punto que se presta a confusión: el `±5` de `irandomk`/`FirstKAnglesInit`
(filtro que restringe qué ángulos pueden ser el **punto de partida**, ver
[[project_ampl_gurobi]]) y el `±step` de `nangshift` (cuánto se mueve un
**vecino** desde el punto actual) son dos mecanismos totalmente
independientes. En los experimentos de esta carpeta, el filtro inicial es
múltiplo de 5° y el paso del vecindario es `nangshift 10` (±10°) — no son el
mismo número por coincidencia de diseño, hay que leerlos por separado.

## El vecindario: `nangshift`

`AngleShiftNeighborhood::size()` (imrt_bao.cpp:344-349) devuelve `2 * K` como
cota superior: por cada uno de los `K` ángulos activos, un candidato en
`-step` y otro en `+step`. Con `K=4` y `nangshift 10` → hasta 8 candidatos.

`computeStep()` (imrt_bao.cpp:383-444) los genera **uno a la vez, en orden
determinístico** — slot 0 (`-step`, `+step`), slot 1 (`-step`, `+step`), etc.
No hay azar acá:

```cpp
// imrt_bao.cpp:418-420
int new_pos = (cur_dir_ == 0)
    ? (pos - step_mod + n_angles_) % n_angles_
    : (pos + step_mod) % n_angles_;
int candidate = degree_order_[new_pos];
```

Si el candidato ya es un ángulo activo (choca con otro slot), se descarta sin
gastar evaluación (imrt_bao.cpp:426-430) — por eso el vecindario real suele
ser menor que `2K`. Ejemplo real, `best/seed08`, ronda 1 desde
`[25,140,150,215]`:

| Slot (ángulo activo) | `-step` | `+step` |
|---|---|---|
| 25 | 15 ✓ | 35 ✓ |
| 140 | ✗ choca con 150 | 150 ✗ choca (ya activo) |
| 150 | ✗ choca con 140 | 160 ✓ |
| 215 | 205 ✓ | 225 ✓ |

8 combinaciones posibles, 2 colisiones → 6 evaluaciones reales por ronda, no 8.

## First Improvement

```cpp
// emilibase.cpp:775-810 (recortado)
do {
    *bestSoFar = *incumbent;                              // ① foto antes de la ronda
    Neighborhood::NeighborhoodIterator iter = neighbh->begin(incumbent);
    for(; iter != end; ++iter) {
        if (incumbent->operator>(*ithSolution)) {          // ithSolution es MEJOR (objetivo menor)
            *incumbent = *ithSolution;
            break;                                          // ② corta apenas mejora — no mira el resto
        }
    }
} while(!termcriterion->terminate(bestSoFar, incumbent));  // ③ ver sección locmin
```

`operator>` compara para minimización: `incumbent > ithSolution` es cierto
cuando `ithSolution` tiene objetivo **menor** (mejor). Apenas eso pasa, corta
la ronda — no necesita ver el resto de los candidatos.

Traza real, `first/seed01` (10 filas de `trajectory_improvements.csv`, ver
`experiments/local_search/nangshift10/restricted/data/first/seed01/`):

- Ronda tras eval 19 (`45,105,145,285` f=96454.93): el **primer** candidato
  probado (eval 20, `35,105,145,285` f=96400.55) ya mejora → corta ahí, 1
  sola evaluación esa ronda.
- Ronda inicial (desde eval 1, f=101130.60): el primer candidato (eval 2,
  `25,115,165,285` f=101184.40) es *peor* → sigue probando; el segundo (eval
  3, f=101106.67) sí mejora → corta en 2 evaluaciones.
- Última ronda (evals 28-35, 8 evaluaciones): **ningún** candidato mejora →
  no hay dónde cortar, se agota el vecindario completo → dispara `locmin`.

La cantidad de evaluaciones por ronda de First es variable — depende
puramente del paisaje desde ese punto, no hay un número "esperable".

## Best Improvement

```cpp
// emilibase.cpp:627-661 (recortado)
do {
    *bestSoFar = *incumbent;
    Neighborhood::NeighborhoodIterator iter = neighbh->begin(bestSoFar);
    for(; iter != end; ++iter) {
        if (incumbent->operator>(*ithSolution)) {
            *incumbent = *ithSolution;      // sin break: sigue comparando contra TODOS
        }
    }
} while(!termcriterion->terminate(bestSoFar, incumbent));
```

Mismo esqueleto que First, sin el `break`: recorre el vecindario completo
cada ronda y se queda con el mejor de todos, recién ahí se mueve.

Traza real completa, `best/seed08` (`experiments/local_search/nangshift10/restricted/data/best/seed08/`, corrida terminada — `Found solution: angles=[15,140,150,225]deg f=92557.14`):

| Ronda | Incumbente entrando | Evals reales | Mejor de la ronda | ¿Mejora? |
|---|---|---:|---|---|
| 1 | `[25,140,150,215]` f=96492.79 | 6 (evals 2-7) | `[25,140,150,225]` f=93043.34 | Sí |
| 2 | `[25,140,150,225]` f=93043.34 | 6 (evals 8-13) | `[15,140,150,225]` f=92557.14 | Sí |
| 3 | `[15,140,150,225]` f=92557.14 | 6 (evals 14-19) | ninguno bate 92557.14 | No → óptimo local |

Tres rondas, 19 evaluaciones totales — no "8 vecinos y termina": cada ronda
que sí mejora arranca una ronda nueva alrededor del nuevo punto.

## `locmin`: cómo decide que llegó a un óptimo local

```cpp
// emilibase.cpp:1699-1710
bool LocalMinimaTermination::terminate(Solution* currentSolution, Solution* newSolution)
{
    if (newSolution == nullptr) return true;
    return currentSolution->operator<=(*newSolution);
}
```

Se llama como `terminate(bestSoFar, incumbent)` al final de cada ronda:
`bestSoFar` es la foto tomada **antes** de la ronda, `incumbent` es el
resultado **después**. Si `bestSoFar <= incumbent` (la ronda no logró bajar
el objetivo, ni siquiera igualarlo por debajo), devuelve `true` y el `while`
corta.

Dos detalles que importan:

- Es `<=`, no `<` — un **empate exacto** en una ronda también cuenta como
  óptimo local. No hace falta que todo sea estrictamente peor.
- No hay tope de iteraciones acá — a diferencia de un `tmaxiter` de ILS,
  `locmin` corre hasta un óptimo local real del vecindario dado, por eso el
  número de evaluaciones varía tanto entre semillas (13 a 82 en los datos de
  esta carpeta).

## Ver también

- `experiments/local_search/nangshift10/unrestricted/ANALYSIS.md` — comparación
  agregada First vs Best sobre 10 semillas, generación de ángulos sin
  restricción.
- `experiments/local_search/nangshift10/restricted/ANALYSIS.md` — misma
  comparación con el generador inicial restringido a múltiplos de 5°.
- `imrt/imrt_bao.cpp` — `RandomKAnglesInit`/`FirstKAnglesInit` (soluciones
  iniciales), `AngleShiftNeighborhood` (vecindario `nangshift`).
- `emilibase.cpp` — `FirstImprovementSearch`, `BestImprovementSearch`,
  `LocalMinimaTermination` (framework EMILI, no específico de IMRT).
