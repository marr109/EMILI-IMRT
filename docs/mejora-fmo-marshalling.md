# Punto de mejora: marshalling de la matriz de dosis a AMPL

Medición hecha en calafate el 2026-09-17 sobre `instances/CERR_Prostate`, 4 ángulos
activos, búsqueda local pura (`first irandomk locmin nangshift 5 randorder`,
semilla 7, 17 solves hasta el mínimo local).

No implementado. Este documento registra el diagnóstico para no repetir la medición.

## Dónde se va el tiempo

| Componente | Tiempo | Porcentaje |
|---|---|---|
| CPU del proceso padre (carga de ángulos + overhead del bucle) | 36 s | 18 % |
| Espera al subproceso AMPL (marshalling + solve) | ~161 s | 82 % |
| **Total de reloj** | **197 s** | |

Da ~9,5 s por solve del FMO.

Dos mediciones descartan explicaciones alternativas:

- **No es I/O de disco.** Repetir la corrida con la caché de página del SO ya
  caliente dio 3m17,5s contra 3m15,5s de la primera vez. Sin diferencia.
- **No es la carga perezosa de ángulos.** Esa carga (`cerr_instance.cpp:131`,
  `angle_loaded_[angle_idx]`) está dentro de los 36 s de CPU del padre, no en los 161 s.

## La causa

`imrt/imrt_fmo.cpp`, en cada llamada a `solveFmo`:

```cpp
ampl_->eval("reset data DIMLETS, PTV_DOSE, OAR_DOSE, d_ptv, d_oar;");
ampl_->getSet("DIMLETS").setValues(...);
ampl_->getSet("PTV_DOSE").setValues(ptv_tuples.data(), ptv_tuples.size());
ampl_->getSet("OAR_DOSE").setValues(oar_tuples.data(), oar_tuples.size());
ampl_->getParameter("d_ptv").setValues(...);
ampl_->getParameter("d_oar").setValues(...);
ampl_->solve();
```

La matriz de dosis dispersa de los ángulos activos se borra y se reenvía completa
en cada solve. Volumen medido con `verbose` (4 ángulos, 272 beamlets activos):

| Órgano | Entradas de dosis |
|---|---|
| PTVHD | 249.191 |
| PTVLD | 189.521 |
| BLADDER | 217.346 |
| RECTUM | ~200.000 |
| **Total por solve** | **~850.000 tuplas** |

Un movimiento del vecindario `nangshift` cambia **un** ángulo de los cuatro. El
75 % de esas tuplas es idéntico al envío anterior.

## Por qué está así

El comentario en el código lo documenta: actualizar los sets parcialmente hace que
AMPL levante `"invalid subscripts discarded"` como error duro —no advertencia—
cuando `d_ptv`/`d_oar` todavía tienen valores para índices que se están quitando.
Eso rompía todos los solves posteriores al primero. El `reset data` borra el set y
sus parámetros dependientes de forma atómica, sin paso intermedio de narrowing.

Es una solución correcta a un problema real. El costo es el que muestra la tabla.

## Mejora propuesta

Mantener la matriz de dosis del catálogo completo residente en AMPL, indexada por
ángulo, y que cada solve solo cambie **qué ángulos están activos** — 4 índices en
lugar de 850.000 tuplas.

Requiere modificar `ampl_gurobi/fmo.mod` además de `imrt/imrt_fmo.cpp`, porque el
modelo declarativo pasa a indexar la dosis por ángulo y a seleccionar el subconjunto
activo dentro del propio modelo.

### Estimación de memoria

~212.000 tuplas por ángulo (850.000 / 4).

| Catálogo | Ángulos | Tuplas residentes | Memoria estimada en AMPL |
|---|---|---|---|
| `restricted` (mod 5) | 72 | 15,3 M | ~1-2 GB |
| `unrestricted` | 360 | 76,3 M | ~4-8 GB |

calafate tiene 125 GB de RAM (116 disponibles), así que ambos entran. Verificar el
caso `unrestricted` en máquinas de desarrollo con menos memoria antes de asumirlo.

Las estimaciones de bytes por tupla son conservadoras y no medidas; hay que
confirmarlas con un prototipo antes de comprometerse.

### Ganancia medida

Actualización 2026-09-17: se instrumentó el solve por fases (`verbose`) y la
estimación original de 5x-8x que figuraba acá **era incorrecta**. Asumía que todo
el tiempo de AMPL era marshalling; más de la mitad es Gurobi resolviendo.

Desglose real, promedio sobre solves en régimen (calafate, load ~46):

| Fase | Tiempo | Share |
|---|---|---|
| `armado` — construir las tuplas en C++ | ~1.600 ms | 14 % |
| `envio` — transferirlas a AMPL | ~3.400 ms | 29 % |
| `gurobi` — resolver la QP | ~6.200 ms | 52 % |
| `lectura` — leer resultados | ~600 ms | 5 % |
| **Total** | **~11.800 ms** | |

Gurobi resuelve en 47-49 iteraciones de barrier, lo cual es sano: no hay patología
en el solver. Probar `threads=4` contra `threads=16` no movió la aguja (5,9 s vs
6,0 s), así que el paralelismo tampoco es el limitante.

**Techo real de la mejora: 1,7x.** Eliminando `armado` + `envio` por completo, el
solve bajaría de ~11,8 s a ~6,8 s. El 52 % de Gurobi no lo toca ninguna
reestructuración del modelo.

Traducido a presupuesto de tiempo: una corrida con `-it 60` pasaría de ~5 solves a
~8-9 solves.

### Nivel 1 (cachear tuplas en C++): probado y descartado

Se implementó y se midió. **No sirve.**

| Intento | Ganancia medida |
|---|---|
| Saltear el conteo por órgano (`organOf`) fuera de `verbose` | 1,1 % |
| Cachear los bloques de tuplas por ángulo | 0,4 % |

Ambos dentro del ruido entre corridas. El caché era **correcto** —objetivo idéntico
al baseline, `f=94433.73`— pero costaba ~340 MB de RSS y un pimpl más un hash map,
a cambio de nada medible. Se revirtió.

Razón del fracaso: el caché evita *construir* las ~850k `ampl::Tuple`, pero después
tiene que *copiarlas* del bloque cacheado al vector que recibe `setValues`. `Tuple`
posee sus `Variant`, así que copiar cuesta prácticamente lo mismo que construir.

**Conclusión: `armado` + `envio` (43 % del tiempo) no son alcanzables mientras la
API `setValues` de AMPL esté en el camino caliente.** Materializar el arreglo de
tuplas y transferirlo es el precio de entrada de esa API, no un descuido del código.

También se descartaron por medición:

- **Threads de Gurobi**: `threads=4` vs `threads=16` → 5,9 s vs 6,0 s.
- **Tolerancia de barrier**: `barconvtol` de 1e-8 a 1e-4 recorta 47 → 42 iteraciones
  y el tiempo de fase no se mueve. Con `outlev=1` se ve por qué: barrier resuelve en
  3,46 s, pero la fase mide ~5,2 s. Los ~1,7 s restantes son AMPL escribiendo un
  `.nl` de 1,27M no-ceros y leyendo el `.sol` de vuelta, en cada solve.

### Lo único que queda con ganancia real

Llamar a la API C++ de Gurobi directamente elimina tres costos de una vez:
`envio` (3,0 s), la interfaz de archivos `.nl`/`.sol` (1,7 s) y casi toda la
`lectura` (0,6 s). Son ~5,3 s de ~11,5 s, o sea **~2x**, y además habilita warm
start: solves consecutivos difieren en un ángulo y hoy el `reset data` destruye
esa información.

El costo es que `fmo.mod` deja de ser el motor. Una opción intermedia es mantenerlo
como especificación de referencia —el documento legible que se discute— y validar
contra él una implementación en Gurobi C++.

Decisión pendiente del equipo. No es una decisión técnica: es sobre qué rol cumple
el modelo declarativo en el proyecto.

## Prioridad

Alta en impacto, pero no bloqueante: los experimentos corren hoy. Conviene atacarlo
antes de lotes grandes, porque multiplica cuántas corridas caben en el mismo tiempo.
