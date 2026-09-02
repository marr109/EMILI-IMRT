# Perturbación de ILS a múltiplos de 5° — qué implica el cambio

Documento para decidir, no para ejecutar todavía. Junta cómo funciona la
perturbación hoy, qué problema encontramos, las dos formas de rediseñarla,
y — el punto que pediste analizar en profundidad — qué tan grave es la
opción de simplemente reescribir los ángulos en los CSV ya generados sin
volver a resolver el FMO, con números reales, no una estimación a ojo.

## 1. Cómo funciona la perturbación hoy

`AngleShiftMultiPerturbation::perturb()` (`imrt/imrt_bao.cpp`), invocada por
`prangshift <step> <numSteps>`:

```cpp
int base_step = step_;                         // 10 o 5, según la corrida
int span = base_step - 1;
int step_mod = base_step + 1 + (rand() % span); // magnitud final
```

Para `step=10`: magnitud sorteada en **[11, 19]**. Para `step=5`: en **[6, 9]**.
Ninguno de esos rangos contiene múltiplos de 5 — es matemáticamente
imposible que el ángulo resultante caiga en la reja de 5°, salvo por la
única coincidencia de `step=10` → magnitud=15 (que sí es múltiplo de 5, pero
es 1 solo valor de 9 posibles, ~11% de las veces).

**Razón original del diseño:** que la magnitud nunca sea exactamente
`step` (ni un múltiplo de `step`) para no quedar atrapado en la misma clase
módulo `step` que ya cubre el vecindario de búsqueda local — el mismo
fenómeno de partición mod-10 que documentamos para `nangshift 10` (ver
[[project_leslie_localsearch_review]]).

## 2. El problema que encontramos

Como la magnitud no es múltiplo de 5, el ángulo perturbado sale de la reja
de 5° de la condición grid5. Y no es solo esa fila: la búsqueda local que
sigue después de la perturbación se mueve ±step **desde ese punto ya fuera
de la reja**, así que arrastra la contaminación por toda esa ronda, no
solo el paso de perturbación.

Medido sobre los datos reales de First (30 semillas, 15 nangshift10 + 15
nangshift5, todas con `perturbations.csv` disponible):

| condición | valores distintos que aparecen como destino de perturbación (de 72 posibles) | valores distintos en TODO el archivo | de esos, % que NO es múltiplo de 5 |
|---|---:|---:|---:|
| nangshift10 | 26.1 (rango 22-29) | 104.3 (rango 77-137) | **74%** |
| nangshift5 | 24.3 (rango 18-28) | 90.1 (rango 71-101) | **75%** |

El segundo número (104.3, 90.1) ya supera el máximo teórico de 72 — prueba
directa de que la contaminación no se limita al momento de perturbar.

## 3. Dos formas de rediseñar la perturbación

### Opción A — magnitud aleatoria, múltiplo de 5, con exclusión según el step

```
magnitud = 5 × k
k sorteado de {2, ..., K_max}
si step == 10: excluir k pares (evita múltiplos de 10 — sigue escapando la partición mod-10)
si step == 5:  sin exclusión (no existe partición que escapar con step=5;
                ya lo confirmamos en esta misma conversación —
                nangshift 5 conecta las 72 posiciones sin partirlas)
```

Con `K_max=20`, por ejemplo: `nangshift10` → {15,25,35,45,55,65,75,85,95}
(9 opciones); `nangshift5` → cualquier múltiplo de 5 entre 10 y 100 (19
opciones). `K_max` es un parámetro libre, no hay razón para que el pool sea
chico — el límite natural sería ~175° (mitad del círculo).

### Opción B — trackear ángulos no explorados

En vez de calcular magnitud+dirección, mantener el conjunto de ángulos (de
los 72 múltiplos de 5) que ya se usaron en algún momento de la corrida, y
sortear la perturbación solo entre los que faltan.

**Definición de "ya explorado" — importa cuál se use:**
- *Solo destino de perturbación* (definición A de la tabla arriba): pool se
  gasta lento, ~26/72 usados en promedio tras una corrida completa — sostenible,
  siempre quedan ~46 sin tocar.
- *Cualquier ángulo que aparece en cualquier fila* (definición B): ya vimos que
  supera 72 — el concepto de "pool" ni siquiera está bien definido bajo esta
  definición con los datos actuales (aunque una vez arreglada la contaminación,
  esta definición volvería a tener sentido, sería más chica).

**Ventaja de B sobre el enfoque de magnitud (Opción A):** no depende de
ninguna matemática de módulos, es directo — "no repitas lo que ya viste".
**Desventaja:** necesita estado persistente por corrida (un `std::set<int>`
que vive durante toda la corrida de ILS, no por-llamada como ahora), y hay
que decidir si el pool se resetea alguna vez o si eventualmente se agota
(con 72 ángulos y ~26 usados en 10 iteraciones, a corridas más largas sí se
agotaría).

## 4. Qué pasa con los datos que ya tenemos — el análisis en profundidad que pediste

### 4.1 Cuánto cambia realmente el objetivo por un error de redondeo de 1-4°

No lo estimé — lo medí. Corrí un vecindario fino (`nangshiftmulti`, pasos de
1° a 3°) sobre un ángulo real de la instancia (semilla de prueba, ángulo base
10°, los otros 3 ángulos fijos en 170°/270°/310°) y medí el cambio real de
objetivo vía el solver FMO real, no una aproximación:

| ángulo | Δ respecto al ángulo base (10°) | objetivo | Δ objetivo |
|---:|---:|---:|---:|
| 10° | 0° (base) | 96 708.30 | — |
| 9° | −1° | 97 044.91 | +336.62 |
| 11° | +1° | 96 855.73 | +147.43 |
| 8° | −2° | 96 976.997 | +268.70 |
| 12° | +2° | 96 963.373 | +255.08 |
| 7° | −3° | 97 047.298 | +339.00 |

**Orden de magnitud: ~150-340 unidades de objetivo por cada 1-3° de error en
UN ángulo**, sobre un objetivo total de ~96 700 (≈0.15%-0.35% relativo por
ángulo). No es una función lineal limpia respecto al ángulo (no hay razón
para que lo sea, depende de la geometría de dosis de esta configuración
puntual), pero el orden de magnitud es consistente en las 5 mediciones.

### 4.2 Por qué esto no es "cosmético" para reescribir sin resolver

Un solo paso de perturbación mueve **3 ángulos simultáneamente**, cada uno
con un error de redondeo de hasta ~2° (la magnitud actual cae en [11,19] o
[6,9]; redondear al múltiplo de 5 más cercano nunca pide más de 2-3° de
corrección). Si los errores de los 3 ángulos apuntan en la misma dirección
(no hay garantía de que se cancelen), el error acumulado en el objetivo de
esa fila podría ser del orden de **3× lo medido para un solo ángulo — varios
cientos de unidades, potencialmente ~0.5%-1% del objetivo total** solo para
la fila de perturbación. Y como el 74-75% de TODAS las filas del archivo
están afectadas (no solo la de perturbación, por el arrastre de la ronda
completa), reescribir sin resolver dejaría **la mayoría del archivo con un
objetivo que no corresponde al ángulo mostrado** — no un error raro y chico
en un puñado de filas, sino el estado por defecto de gran parte del dataset.

### 4.3 Costo real de la alternativa — volver a correr todo

Tiempo medido por semilla (promedio real de esta sesión, excluyendo cortes
por sueño de la máquina o vencimiento de licencia):

| condición | tiempo promedio/semilla |
|---|---:|
| First | ~28 min |
| Best | ~33 min |

Si hubiera que rehacer las 60 corridas de ILS que tenemos entre manos (15
First + 15 Best, × 2 steps): **≈30 horas de cómputo bruto**, sin contar
cortes (que ya sabemos que van a pasar — sueño de la máquina, licencia de
AMPL, etc., cada uno agregando horas reales adicionales sobre el cómputo
puro).

## 5. La decisión, resumida

| | Reescribir sin resolver | Volver a correr |
|---|---|---|
| Costo | Minutos (es solo texto) | ~30h de cómputo bruto + los cortes que ya venimos viendo |
| Precisión | El objetivo de ~74-75% de las filas queda desalineado con el ángulo mostrado, por ~0.15-1% por fila afectada | Exacto, cada fila corresponde a un solve real |
| Honestidad de los datos | Los números "parecen" grid5 pero no lo son — invisible sin este análisis | Sin ambigüedad |

El error por fila (0.15%-1%) es chico en términos absolutos, pero no es
despreciable frente a las diferencias que venimos reportando entre
condiciones en este proyecto (por ejemplo, la brecha nangshift10 vs
nangshift5 en el boxplot de First fue de ~530 unidades sobre ~90 000, es
decir ~0.6% — del mismo orden que el error de redondeo que introduciría
reescribir sin resolver). Si el error de "hacer trampa" es comparable en
magnitud a las diferencias que estamos tratando de medir, reescribir sin
resolver podría estar generando ruido del mismo tamaño que la señal que
buscamos — ese es el riesgo concreto, no una objeción de principios.
