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

### Ganancia esperada

Si el solve queda reducido a la QP sin marshalling, el tiempo por solve debería caer
de ~9,5 s a un orden de 1-2 s. Serían entre 5x y 8x en todo el pipeline experimental.

No verificado. Es una estimación por proporción sobre el desglose medido, y el
número real depende de cuánto del tiempo de AMPL es marshalling y cuánto es Gurobi
resolviendo. Separar esas dos partes es el primer paso de cualquier prototipo.

## Prioridad

Alta en impacto, pero no bloqueante: los experimentos corren hoy. Conviene atacarlo
antes de lotes grandes, porque multiplica cuántas corridas caben en el mismo tiempo.
