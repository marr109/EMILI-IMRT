# Análisis: orden de escaneo aleatorio (`randorder`) en First Improvement

## Contexto y motivación

`AngleShiftNeighborhood::computeStep()` recorría los slots de ángulos activos
siempre en orden fijo `0, 1, ..., K-1` (y dentro de cada slot, siempre `-step`
antes de `+step`). Para **First Improvement** esto introduce un sesgo
posicional real: como First se detiene en la primera mejora encontrada, el
slot 0 tiene sistemáticamente más chances de ser el primero en moverse en
cada ronda. Para **Best Improvement** este sesgo no aplica — Best evalúa el
vecindario completo (2×K candidatos) en toda ronda, así que el orden de
escaneo no puede cambiar el resultado final, solo afectaría desempates.

**Implementación** (`imrt/imrt_bao.h`, `imrt/imrt_bao.cpp`,
`imrt/imrt_builder.cpp`): nuevo flag `random_order_` en
`AngleShiftNeighborhood` (default `false`, preserva el comportamiento
histórico exacto — cero cambio de resultado en scripts existentes). En modo
`true`, `begin()` sortea una permutación de los slots vía Fisher-Yates
(mismo patrón que `AngleShiftMultiPerturbation`), usando el RNG global
sembrado por semilla (`emili::generateRandomNumber()`). Se re-sortea en
**cada ronda nueva**, no una sola vez por corrida. Token CLI: `randorder`
después del step (`nangshift <step> randorder`). Funciona igual dentro de
ILS sin wiring adicional, porque reutiliza la misma clase.

Durante la preparación de este experimento también se encontró y corrigió
un problema separado: `irandomk`/`ifirstk` habían perdido (desde el commit
de ILS del 1-sep) la capacidad de generar arranques "unrestricted" (360
ángulos posibles) — quedaron restringidos siempre a múltiplos de 5°. Se
agregó un token `unrestricted` para recuperar ese comportamiento sin romper
el default (mod-5) del que ya dependía el trabajo de ILS.

## Metodología

- **Instancia:** `instances/CERR_Prostate`, BAO con K=4 ángulos activos.
- **Condiciones:** {`restricted` (mod 5°), `unrestricted` (360°)} ×
  {First, Best} × {orden fijo (histórico), `randorder` (nuevo)}.
- **Semillas:** 15 por condición (mismas semillas → mismo punto de partida
  entre fijo y random, comparación pareada).
- **Carpetas:** históricas en `experiments/local_search/nangshift<step>/
  {restricted,unrestricted}/`, nuevas en `.../{restricted,unrestricted}-shift-rand/`
  — nunca se sobreescriben entre sí.
- **Métrica:** objetivo final real, tomado de la línea `Found solution` de
  cada `run.log` (**no** de la última fila del CSV — la última fila suele
  ser un vecino rechazado del round terminal, no la solución aceptada; este
  fue un error propio detectado y corregido durante el análisis).

## Estado de las corridas

| step | condición | First | Best |
|---|---|---|---|
| nangshift 5 | restricted | ✅ 15/15 | ✅ 15/15 |
| nangshift 5 | unrestricted | ✅ 15/15 | ✅ 15/15 |
| nangshift 10 | restricted | ✅ 15/15 | ✅ 15/15 |
| nangshift 10 | unrestricted | ✅ 15/15 | ✅ 15/15 |

**Las 8 combinaciones completas.** Plots generados para las 8:
`plots/first_vs_best_comparison.png` + `plots/seed_comparisons/seedNN_comparison.png`
en cada carpeta `<condición>-shift-rand/`. Boxplot agregado (small multiples,
16 condiciones no entran en una sola paleta categórica sin ciclar colores —
ver `dataviz` skill, regla "assign categorical hues in fixed order, never
cycled"): `experiments/local_search/comparisons/boxplot_randorder_all_conditions/
boxplot_nangshift5.png` y `boxplot_nangshift10.png` (8 condiciones cada uno,
mismo orden/color entre los dos paneles).

## Resultados — objetivo final, mediana (15 semillas)

| condición | mediana fijo | mediana random |
|---|---:|---:|
| n5 restricted / First | 96156.6 | **95697.2** |
| n5 restricted / Best | 95900.6 | 95900.6 |
| n5 unrestricted / First | **94537.7** | 95123.8 |
| n5 unrestricted / Best | 94569.3 | 94569.3 |
| n10 restricted / First | **94834.7** | 95734.0 |
| n10 restricted / Best | 95798.7 | 95798.7 |
| n10 unrestricted / First | 95524.8 | **93348.3** |
| n10 unrestricted / Best | 94721.8 | 93822.3 |

Comparación pareada (misma semilla, mismo arranque, fijo vs. random):

| condición | n | random mejor | fijo mejor | empate |
|---|---:|---:|---:|---:|
| n5 restricted / First | 15 | 5 | 2 | 8 |
| n5 unrestricted / First | 15 | 2 | 5 | 8 |
| n10 restricted / First | 15 | 1 | 7 | 7 |
| n10 unrestricted / First | 15 | 3 | 3 | 9 |
| n5 restricted / Best | 15 | 0 | 0 | 15 |
| n5 unrestricted / Best | 15 | 0 | 0 | 15 |
| n10 restricted / Best | 15 | 0 | 0 | 15 |
| n10 unrestricted / Best | 15 | 1 | 0 | 14 |

## Interpretación

1. **Best es invariante ante el orden CASI siempre, pero no es una garantía
   absoluta — esto corrige una afirmación previa de este mismo documento.**
   En 3 de las 4 combinaciones Best sale exactamente igual en las 15
   semillas (fijo = random, dígito a dígito). Pero en `n10 unrestricted /
   Best`, la **semilla 2** diverge fuerte: fijo termina en 97584.1 (29
   evaluaciones), random en 92338.9 (101 evaluaciones) — más de 5000 puntos
   de diferencia. Se rastreó la trayectoria: la Ronda 1 converge idéntica en
   ambas (mismo conjunto de 8 candidatos, mismo mejor valor, misma base
   aceptada) — la divergencia aparece en una ronda posterior. Explicación
   más probable (consistente con la evidencia, no aislada al 100%): un
   empate real entre dos configuraciones de ángulos distintas con el mismo
   valor objetivo en esa ronda — cuál gana el empate depende del orden de
   visita (comparación estricta `>` en el código, gana el primero que
   iguala al mejor). Elegir una u otra abre un vecindario distinto de ahí
   en más (los vecinos dependen de los ángulos concretos, no del valor), y
   esa bifurcación desemboca en óptimos locales muy distintos evaluaciones
   después. **Conclusión corregida: Best es invariante ante el orden salvo
   que exista un empate real en alguna ronda — caso raro (1/60 corridas de
   Best en este dataset) pero no imposible.**

2. **First NO muestra un patrón catálogo-dependiente limpio y consistente
   entre steps — esto también corrige la hipótesis previa de este
   documento.** Con nangshift 5: random ayudaba en `restricted` (5-2) y
   perjudicaba en `unrestricted` (2-5). Con nangshift 10, el patrón es
   **distinto**: `restricted` ahora perjudica a random (1-7, dirección
   OPUESTA a n5) y `unrestricted` da empate en conteo de victorias (3-3)
   aunque la mediana favorece bastante a random (93348 vs 95525). La
   hipótesis original — "el efecto se invierte según catálogo, igual que
   con el step del vecindario" — **no se sostiene con los 4 datasets
   completos**. Lectura más honesta: el efecto de `randorder` en First
   parece depender de una combinación más compleja de catálogo Y step (o
   simplemente tener más variabilidad semilla-a-semilla de la que un
   patrón de 2 factores puede capturar), no de una regla simple de "ayuda
   acá, perjudica allá".

3. **En las 4 combinaciones de First, el empate exacto ronda el 50-60% de
   las semillas** (7-9 de 15) — confirma que el efecto de `randorder`,
   cuando existe, es una inclinación moderada sobre una base de muchos
   empates, nunca un cambio dominante. Ninguna combinación muestra una
   ventaja aplastante para ningún lado.

## Limitaciones

- **El dataset completo (8/8) no confirma una regla simple** — es la
  limitación más importante de este análisis. Con solo 2 valores de step
  (5 y 10) y 2 catálogos, no hay manera de aislar si el patrón depende del
  step, del catálogo, de su interacción, o es simplemente ruido de muestra
  con n=15. Ampliar a más steps (ej. 15, 20) podría ayudar a distinguir
  estas hipótesis, pero no se hizo en este análisis.
- **Costo de cómputo no uniforme**: combinaciones con `unrestricted` +
  `randorder` mostraron corridas individuales bastante más largas que el
  resto (una corrida de First tardó >5 min sin converger en una prueba
  preliminar temprana). No se investigó la causa exacta del costo extra.
- **Pregunta abierta, no probada:** ¿el efecto de `randorder` se amplifica o
  se atenúa dentro de un ILS? Hipótesis razonada (no verificada): en ILS
  cada búsqueda local interna arranca del resultado de la perturbación
  anterior, y el criterio de aceptación (`rejectrepeated`) depende de a qué
  óptimo local aterriza cada ronda — a diferencia de corridas de local
  search independientes, en ILS las diferencias de una iteración se
  propagan a las siguientes (dependencia secuencial, no muestreo i.i.d.),
  lo que podría amplificar el efecto en vez de promediarlo. Pendiente de
  probar como tercer experimento.

## Próximos pasos

1. Decidir si vale la pena ampliar a más valores de step para intentar
   separar el efecto de catálogo del efecto de step (punto 1 de
   limitaciones) — no es gratis en cómputo, evaluar costo/beneficio primero.
2. Evaluar correr el experimento equivalente dentro de ILS para probar (o
   descartar) la hipótesis de amplificación por dependencia secuencial.
3. Este documento queda como fuente de datos cruda para el informe — los
   números de las tablas de arriba están verificados contra `run.log`
   (línea `Found solution`), no contra la última fila del CSV.
