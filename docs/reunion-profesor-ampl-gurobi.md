# Reunión con el profesor — AMPL + Gurobi, instancia nueva, dudas abiertas

Documento de trabajo para la reunión. Resume qué se implementó, y sobre todo
lista los puntos que necesitan una decisión o corrección de tu parte —
a nivel de modelo, de instancia, y de metodología de análisis.

Documentación técnica completa (referencia, no hace falta leerla entera antes
de la reunión):
- `docs/ampl-gurobi-integration.md` — cómo se integró AMPL/Gurobi, el `.mod`, la función objetivo.
- `docs/cerr-prostate-instance-analysis.md` — análisis completo de la instancia nueva.
- `experiments/local_search/nangshift10/unrestricted/ANALYSIS.md` — comparación First vs Best.

---

## 1. Resumen — qué se hizo

1. **Se reemplazó OSQP por AMPL + Gurobi** como solver del FMO, manteniendo
   exactamente la misma función objetivo (penalización cuadrática — no se
   tocó la formulación, solo el backend de resolución).
2. **Se cambió de instancia**: de los datasets CORT (`PROSTATE_*`) a
   `instances/CERR_Prostate` — un export crudo de CERR, sin convertir, con
   estructura distinta (360 ángulos a 1° de resolución, beamlets variables
   por ángulo, dos PTV).
3. Como consecuencia, **se eliminó todo el soporte CORT/`ImrtInstance`** del
   código — decisión tomada en conjunto, el proyecto ahora trabaja
   exclusivamente sobre CERR.
4. Se corrió un **análisis de búsqueda local (First vs Best Improvement)**,
   sin ILS, con 10 semillas pareadas, sobre esta instancia real.
5. **Se agregó el diagnóstico de dimensiones por órgano** que el profesor
   pidió en la reunión pasada — ver §2.1.

---

## 2. AMPL + Gurobi — cómo quedó funcionando

- Se encontró la **API C++ oficial de AMPL** vendorizada dentro del paquete
  pip `amplpy`, se extrajo a `third_party/ampl_cppapi/` (1.1MB, sí va a git) y
  se linkea directo al binario — no hay ningún subprocess armado a mano.
- `ImrtFmoSolver` mantiene una sesión AMPL persistente (se abre una vez, se
  reutiliza en cada `solve()` — importante porque BAO puede llamar `solve()`
  miles de veces por corrida).
- Gurobi confirmado en uso: **versión 13.0.2**, invocado como driver externo
  de AMPL con ruta absoluta (no depende de `PATH`).
- La licencia AMPL es de tipo *short-term lease* (renueva cada 60-300s contra
  el servidor de AMPL) — se cortó varias veces durante corridas largas por
  problemas de DNS/red, hubo que armar reintento automático en los scripts de
  experimento.

### 2.1 Diagnóstico de dimensiones por órgano (implementado)

En la reunión pasada quedó pendiente: mostrar por pantalla, antes de cada
llamada al solver, las dimensiones de las matrices por órgano — para
confirmar que cada solve realmente usa datos frescos de esa configuración y
no está repitiendo la misma matriz por error. Ya está en `ImrtFmoSolver`
(activado con el flag `verbose`), ejemplo real de una corrida:

```
[FMO] beamlets_activos=280
    PTV PTVHD: boxets=2518 entradas_dosis=223653
    PTV PTVLD: boxets=2595 entradas_dosis=155365
    OAR BLADDER: boxets=3639 entradas_dosis=190564
    OAR RECTUM: boxets=1894 entradas_dosis=166737
Gurobi 13.0.2: optimal solution; objective 139186.2068
```

Los `boxets` quedan fijos (son propiedad del órgano, no de la configuración
de ángulos). Lo que varía de verdad entre soluciones son las
`entradas_dosis` — confirmado comparando dos configuraciones distintas de la
misma corrida (238873/164099/223602/146604 en una vs 223653/155365/190564/166737
en otra) — la matriz cambia con cada configuración, no se está repitiendo.

Traza completa de una corrida (13 evaluaciones) + explicación de cómo leer
cada número y qué habría sido señal de bug: `docs/fmo-verbose-diagnostic.md`.

## 3. La instancia nueva — lo importante

- `CERR_Prostate`: 11GB, 2173 archivos, 360 ángulos, 23971 beamlets totales,
  4 órganos (PTVHD 2518 vóxeles, PTVLD 2595, BLADDER 3639, RECTUM 1894).
- **Beamlets por ángulo NO es constante** (rango 58-72) — por eso se construyó
  un loader C++ nuevo (`CerrFmoSource`) en vez de reusar el viejo, que asumía
  cantidad fija.
- **Hallazgo relevante**: PTVHD y PTVLD se superponen en 944 vóxeles — es
  esperable clínicamente (esquema SIB, boost anidado), pero significa que un
  vóxel compartido entra dos veces al modelo (una fila en el bloque PTVHD,
  otra en PTVLD, cada una con su propio slack). Ver duda 4.a más abajo.
- **Los valores clínicos (Dmin/Dmax/pesos) son placeholder** — no hay ningún
  `instance_config.txt` ni plan de tratamiento real de este paciente en el
  repo. Usé: Dmin PTVHD=PTVLD=65 Gy, Dmax BLADDER=RECTUM=50 Gy, w_under=1.0,
  w_over=0.5, w_ptv_over=0 (desactivado), max_intensity=15000.

---

## 4. Dudas / puntos a resolver

### 4.a — Nivel de modelo

1. **¿El overlap PTVHD/PTVLD (944 vóxeles) debería tratarse distinto en el
   modelo?** Hoy cada vóxel compartido se penaliza dos veces (una vez como
   parte de PTVHD, otra como parte de PTVLD) si queda por debajo de su
   respectivo Dmin. ¿Es el comportamiento clínicamente correcto (reforzar la
   exigencia en la zona de boost), o debería el vóxel compartido contarse una
   sola vez, con el Dmin más exigente de los dos?
2. **`w_ptv_over` está en 0 (desactivado)** — ¿corresponde activarlo para esta
   instancia? La restricción de hot-spot (`D_ptv·x - w ≤ 1.07·Dmin`) y la
   variable `w` ya están en el modelo, solo hace falta un peso > 0.
3. Confirmar que la función objetivo (penalización cuadrática, sin gEUD/IRLS)
   sigue siendo la elección definitiva ahora que cambiamos de instancia — no
   se tocó nada acá, pero vale la pena la confirmación explícita dado el
   cambio de dataset.

### 4.b — Nivel de instancia

1. **Necesito los valores clínicos reales** (Dmin PTVHD, Dmin PTVLD, Dmax
   BLADDER, Dmax RECTUM, y los pesos) si existe el plan de tratamiento
   original de este paciente — los placeholder actuales no tienen respaldo
   clínico, solo sirven para validar que el pipeline funciona mecánicamente.
2. La eliminación completa de CORT es una decisión ya tomada, pero la dejo
   explícita para confirmarla formalmente: ya no hay forma de cargar
   `PROSTATE_36ang` ni ningún dataset CORT en esta rama.

### 4.c — Manejo de valores cacheados (esto es lo que más quiero discutir)

Cuando la búsqueda local revisita un conjunto de ángulos que ya evaluó antes
(pasa seguido — por ejemplo, deshacer el último movimiento), el sistema
**reusa el resultado en vez de llamar a Gurobi de nuevo** (columna `cached` en
los CSV de trayectoria). Esto está bien para no gastar cómputo de más, pero
genera una pregunta de **cómo reportar el costo** de cada estrategia:

Con los datos ya corridos (10 semillas × First + Best, análisis de búsqueda
local):

| | evaluaciones totales (con cacheadas) | evaluaciones reales (solo Gurobi) | % cacheado |
|---|---:|---:|---:|
| First | 26.4 promedio | 23.2 promedio | ~12% |
| Best | 47.0 promedio | 40.1 promedio | ~15% |
| **Total combinado** | 734 filas | 633 llamadas reales | **13.8%** |

**La pregunta concreta:** el `ANALYSIS.md` que ya armamos reporta "evaluaciones"
como el total de filas del CSV (incluye las cacheadas). Pero una fila cacheada
es esencialmente gratis (un lookup, no un solve de Gurobi) — así que ese
número **sobreestima el costo computacional real** en ~14%. ¿Cuál de las dos
métricas es la que te interesa para el análisis?

- **Mantener el total (con cacheadas)**: mide "cuántos pasos dio la búsqueda",
  útil si lo que importa es la trayectoria/cantidad de movimientos explorados.
- **Reportar solo las reales**: mide "cuánto costó en tiempo de cómputo
  real", más apropiado si la comparación First-vs-Best es sobre eficiencia
  computacional.

Puedo recalcular el `ANALYSIS.md` y los gráficos con cualquiera de las dos
convenciones (o ambas, una al lado de la otra) — quería confirmarlo antes de
tocar los resultados ya presentados.

---

## 5. Próximos pasos propuestos (pendiente de tu visto bueno)

1. Ajustar el modelo/instancia según lo que se resuelva en 4.a y 4.b.
2. Recalcular el análisis First vs Best con la convención de costo que se
   defina en 4.c.
3. Test estadístico pareado (Wilcoxon) sobre las 10 diferencias del
   experimento actual, una vez cerrada la metodología.
4. Evaluar si conviene repetir con otro `step` de vecindario (5°, 30°) para
   ver si el patrón First-vs-Best se sostiene.
