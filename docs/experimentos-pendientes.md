# Experimentos pendientes — estado consolidado (2026-09-15)

Este documento reemplaza como referencia rápida a
`docs/roadmap-metaheuristicas-post-informe.md` (que queda como bitácora
detallada, con el razonamiento y las decisiones de diseño de cada punto).
Acá solo el estado: qué está hecho, qué está corriendo ahora, qué falta.

## Hecho y validado

- **Vecindario circular** (`nangshift <step> circular`) a nivel de búsqueda
  local pura: 8 condiciones completas (First/Best × restricted/unrestricted
  × n5/n10), 15 semillas c/u. Resultado: no gana en calidad de forma
  consistente, sí reduce evaluaciones sistemáticamente (22-39% menos que
  fijo, en las 4 condiciones First). Integrado en el informe.
- **Vecindario circular dentro de ILS**: First en n=15 (4 condiciones), Best
  en n=15 (4 condiciones, completado 2026-09-11 en la madrugada). Fijado
  como línea base de referencia en n=10 para la presentación (decisión
  explícita del usuario, no seguir empujando a n=15 en ese contexto).
- **ILS unrestricted** (First/Best × n5/n10, orden fijo): 60/60 semillas
  completas. Cierra el hueco que faltaba (antes solo existía restricted).
- **Análisis de por qué `randorder` no supera al orden fijo de forma
  consistente**: escrito, integrado en el informe con el argumento de
  continuidad geométrica.
- **Piloto de magnitud de perturbación** (n=3 por brazo, prangshift
  15/30/45): 15-30° superan al baseline de 5°, 45° pierde. Documentado como
  hallazgo exploratorio (no concluyente), motivó el diseño de la
  perturbación adaptativa.
- **Token `ttime <segundos>`** (terminación por tiempo real, reutiliza
  `emili::TimedTermination` del framework genérico): implementado,
  compilando limpio.
- **`AdaptiveAngleShiftPerturbation`** (perturbación progresiva: arranca en
  1 ángulo, escala de a uno hasta `maxNumSteps` tras `stagnationThreshold`
  rondas sin mejora, vuelve a 1 con mejora real, magnitud fija en
  `(step,2·step)` en todos los niveles): implementado, verificado con smoke
  test real (progresión 1→1→1→1→1→2→2→3→3→4 confirmada fila por fila).
  Token CLI `prangshiftadaptive <step> <maxNumSteps> <stagnationThreshold>`.

## Corriendo ahora mismo

- **Batch de perturbación adaptativa** (`prangshiftadaptive 5/10 4 2`),
  First únicamente, 4 condiciones (n5/n10 × restricted/unrestricted), 15
  semillas c/u, `tmaxiter 10` (comparación válida contra First-fijo: misma
  estrategia interna, mismo presupuesto de iteraciones). Script:
  `scripts/run_ils_adaptive_batch.sh`. Salida en
  `experiments/ils/nangshift{5,10}/{restricted,unrestricted}-adaptive/data/first/`.
- **Smoke test de `ttime`** (90s de presupuesto, una sola semilla): validando
  que el criterio de tiempo corta bien antes de armar un batch real con él.

## Pendiente — próximos pasos, en orden sugerido

1. **Cerrar el batch de perturbación adaptativa (First)** ya lanzado, y
   comparar contra la línea base First-fija (misma condición, mismo
   `tmaxiter`) — comparación ya válida sin necesitar `ttime`.

2. **Confirmar `ttime` funciona** (smoke test en curso) y armar un batch
   real First-vs-Best por presupuesto de tiempo (no iteraciones) — esto es
   lo que hace válida por primera vez cualquier comparación First-vs-Best
   dentro de ILS (el problema metodológico que motivó `ttime` desde el
   principio: Best es más caro por iteración que First, así que
   `tmaxiter` igual para ambos no es un presupuesto real igual).

3. **Perturbación adaptativa con Best** (además de First), usando `ttime`
   para que la comparación Best-adaptativa vs Best-fija (y cualquier cruce
   contra First) sea justa desde el arranque, sin tener que rehacerla
   después.

4. **Variante de magnitud escalable** (`prangshiftadaptivemag`, documentada
   pero no implementada — ver `docs/roadmap-metaheuristicas-post-informe.md`
   sección 2): escalar también la magnitud del desplazamiento con el nivel
   (tope anclado en 15°, el mejor punto ya encontrado en el piloto), como
   variante separada para poder aislar el efecto de cantidad vs magnitud.
   Solo si el punto 1-3 ya mostró que vale la pena seguir invirtiendo en
   perturbación adaptativa.

5. **SA (recocido simulado)** — el más barato de implementar de los tres que
   quedan, porque el algoritmo YA está implementado
   (`emili::SimulatedAnnealing`, con smoke test exitoso de una sesión
   anterior). Falta:
   - Correr el batch real de 15 semillas (`scripts/run_sa_batch.sh`, ya
     escrito) usando `ttime` en vez de `tmaxiter` para terminación por
     tiempo real (recomendación explícita: no comparar SA contra ILS con
     presupuestos desiguales).
   - Sin calibración de temperatura inicial/alfa todavía — correr primero
     con los valores por defecto ya usados en el smoke test
     (`sa_metropolis 50000 1 250`) y evaluar si hace falta calibrar antes de
     sacar conclusiones.

6. **VNS (búsqueda de vecindario variable)** — reutiliza infraestructura ya
   existente en el framework (`GVNS`) + `MultiScaleAngleShake` (ya
   implementado, token `bangshake`, usado también en rVNS). Falta:
   - Definir los vecindarios múltiples a encadenar (ej. ±5°, ±10°, sumar
     ±15°/±20° como vecindarios adicionales).
   - Correr el primer batch de validación (15 semillas, mismo protocolo que
     el resto).

7. **TS (búsqueda tabú) — rediseño, no calibración**: lo que existe
   (`BestTabuSearch`/`FirstTabuSearch` sobre búsqueda local completa) es un
   híbrido, no coincide con la definición de tabú "clásico" (vecino
   aleatorio por paso, sin barrer nunca el vecindario completo — "soltar la
   búsqueda local"). Antes de invertir en calibración con irace del
   mecanismo actual, hay que decidir si:
   - (a) se implementa un algoritmo tabú nuevo, genuinamente vecino-a-vecino
     sin búsqueda local de por medio, o
   - (b) se documenta el mecanismo actual como una variante deliberada
     (tabú-sobre-descenso) y se reporta tal cual, con esa aclaración
     explícita.
   Esta decisión de diseño está abierta, no tomada todavía.

## Nota metodológica que aplica a todo lo de arriba

Cualquier batch nuevo debe usar el mismo protocolo ya establecido para que
sea comparable: 15 semillas independientes por condición, mismo punto de
partida por semilla entre variantes que se comparan entre sí (First vs Best,
fijo vs adaptativo, etc.), y métricas mínimas por corrida: objetivo final
(mín/mediana/promedio sobre las semillas), número de evaluaciones del FMO, y
tiempo de cómputo real. Antes de lanzar cualquier batch largo, verificar
`git diff --stat -- ampl_gurobi/fmo.mod` esté limpio (bloqueador intermitente
ya documentado).
