# Roadmap post-entrega (viernes 2026-09-11) — metaheurísticas y comparaciones

Contexto: el informe que vence el viernes cubre resultados hasta ILS
(búsqueda local, `randorder`, vecindario circular, ILS). Todo lo de Tabu
Search, SA, rVNS y VNS quedó explícitamente fuera del alcance de esta
entrega (reunión con Leslie Pérez Cáceres, 2026-09-08/09) y se retoma la
semana siguiente. Este documento fija el orden acordado para retomarlo.

## Línea base ya establecida (no repetir, solo extender)

ILS con perturbación fija (`AngleShiftMultiPerturbation`, 3 pasos sin
repetición de ángulo, `tmaxiter 10`, `baoimprove rejectrepeated`), First y
Best, sobre **ambos catálogos**:

- `restricted` (grid5): ya validado, 15 semillas × {First, Best} × {n5, n10}.
- `unrestricted`: lote de 60 corridas (First/Best × n5/n10, 15 semillas c/u)
  lanzado 2026-09-10 20:17:59 vía `scripts/run_ils_batch.sh <step> <strategy>
  unrestricted`, en curso al momento de escribir esto.

Cualquier comparación futura de perturbación adaptativa, VNS o TS contra ILS
debe usar este mismo protocolo (mismas semillas, mismo `tmaxiter`, ambos
catálogos) para que la comparación sea justa.

## Roadmap acordado

1. **Vecindario en círculo dentro de ILS** (`nangshift <step> circular` en
   vez del orden fijo actual). Cambio mínimo, ya implementado y validado a
   nivel de búsqueda local pura — falta solo correrlo dentro de ILS y ver si
   la ganancia de costo observada ahí (Sección 6.4/6.5 del informe) se
   traslada a ILS. Mismo protocolo que la línea base de arriba, sobre los
   tres órdenes (fijo/random/circular) si el tiempo alcanza, o al menos
   fijo vs circular.

2. **Perturbación adaptativa** para los mismos 3 órdenes de escaneo: en vez
   de una magnitud/cantidad de ángulos fija por perturbación, escalar
   (aumentar ángulos movidos o magnitud) cuando la búsqueda se estanca
   varias rondas seguidas, y volver a bajarla al encontrar mejora. Motivación
   concreta (no solo teórica): el piloto de magnitud de perturbación
   (`experiments/ils/perturbation_magnitude_pilot/`, n=3 por brazo) ya
   mostró que 45° pierde frente a 15-30°, probablemente porque `tmaxiter 10`
   no alcanza para reconverger tras un salto tan grande — el mismo síntoma
   que la perturbación adaptativa está diseñada para evitar. Requiere
   calibración (umbral de estancamiento) vía irace antes de reportar como
   mejora validada.

   **IMPLEMENTADO (2026-09-14):** `AdaptiveAngleShiftPerturbation`
   (`imrt/imrt_bao.h`/`.cpp`), token CLI `prangshiftadaptive <step>
   <maxNumSteps> <stagnationThreshold>`. Mecanismo de auto-observación (sin
   hook nuevo en el framework): `perturb()` ya recibe la solución actual
   aceptada por el ILS en cada ronda, así que compara el objetivo de esta
   llamada contra el de la llamada anterior para inferir si hubo mejora.
   Arranca en nivel 0 (1 ángulo movido, magnitud aleatoria fija en
   `(step,2·step)`); tras `stagnationThreshold` llamadas seguidas sin
   mejora, sube un ángulo más por nivel (hasta `maxNumSteps`, mismo
   espíritu que `MultiScaleAngleShake`: k+1 movimientos para intensidad k);
   vuelve a nivel 0 apenas hay una mejora real. Verificado con smoke test
   real (progresión 1→1→1→1→1→2→2→3→3→4 en 10 ráfagas, confirmada fila por
   fila contra `trajectory.csv` vía `scripts/plot_ils_trajectory.py`).

   **Decisión de diseño (2026-09-14):** la magnitud NO escala con el nivel
   --queda fija en `(step,2·step)` en todos los niveles--, solo escala la
   cantidad de ángulos movidos. Se evaluó explícitamente la alternativa de
   escalar también la magnitud (interpolando entre `step` en nivel 0 y un
   tope `maxStep` en el nivel máximo, ancorado al mejor punto ya encontrado
   en el piloto de magnitud: 15°) y se decidió NO implementarla todavía,
   solo dejarla documentada como variante futura (`prangshiftadaptivemag
   <step> <maxStep> <maxNumSteps> <stagnationThreshold>`, token separado
   para poder correr ambas variantes en paralelo y aislar si la mejora viene
   de la cantidad, la magnitud, o la combinación). Implicaciones ya
   identificadas si se retoma: un hiperparámetro más que calibrar
   (`maxStep`), y la necesidad de correr la variante de un solo eje como
   control para no confundir el efecto de ambos ejes escalando juntos.

3. **SA, VNS, TS** — no como bloque único, orden por costo real de
   implementación pendiente:
   - **VNS**: el más barato. Reutiliza `GVNS` (ya en el framework) +
     `MultiScaleAngleShake` (ya implementado, token `bangshake`). Falta
     definir los vecindarios múltiples (±5, ±10, sumar ±15/±20) y correr el
     primer batch.
   - **SA**: segundo más barato. Algoritmo (`emili::SimulatedAnnealing`) ya
     implementado y con smoke test exitoso esta sesión. Falta correr el
     batch de 15 semillas (`scripts/run_sa_batch.sh`, ya escrito) usando
     terminación por **tiempo** (`ttime`, token nuevo agregado 2026-09-10 a
     `imrt_builder.cpp`, reutiliza `emili::TimedTermination` del framework
     genérico) en vez de `tmaxiter`, para no comparar contra ILS con
     presupuestos desiguales.
   - **TS**: el de mayor lift real. Lo que existe (`BestTabuSearch`/
     `FirstTabuSearch` sobre búsqueda local completa) es, según Leslie, un
     híbrido — no el tabú "clásico" que ella describe (vecino aleatorio por
     paso, sin barrer nunca el vecindario completo, "hay que soltar la
     búsqueda local"). Esto implica diseñar un algoritmo nuevo, no solo
     calibrar el existente con irace.

## Nota sobre `ttime`

Token CLI nuevo `ttime <segundos>` en `imrt_builder.cpp` (2026-09-10),
reutiliza `emili::TimedTermination` ya existente en el framework genérico
(wireada ahí bajo el token `time`, nunca antes expuesta para BAO). Compilado
limpio. Pendiente: smoke test real (el primer intento se interrumpió a
pedido del usuario para reordenar prioridades hacia el punto 1 de este
roadmap, no por falla del código) y batch real de ILS/SA con terminación por
tiempo.
