# Estructura de archivos AMPL — tu comentario sobre `.mod`/`.dat` por BAC

## Lo que dijiste

Por cada BAC (cada configuración de K ángulos) deberíamos necesitar más o
menos los mismos archivos: un `.mod` con el modelo, un `.dat` con los
valores de los parámetros, y un `.dat` por cada órgano (OAR y PTV).

## Contraste con cómo quedó funcionando

| Archivo que describiste | ¿Existe así en el pipeline real? |
|---|---|
| `.mod` (modelo) | **Sí, exacto.** `ampl_gurobi/fmo.mod` es la única fuente de verdad, se lee **una sola vez** en el constructor de `ImrtFmoSolver` — no se relee por cada BAC. |
| `.dat` de parámetros | **No.** No se escribe a disco en la corrida real. |
| `.dat` por órgano (OAR/PTV) | **No.** Tampoco se escribe a disco en la corrida real. |

**Por qué no hay `.dat` en producción:** BAO llama `solve()` potencialmente
miles de veces por corrida (una vez por cada configuración de ángulos que
evalúa la búsqueda). Escribir y volver a leer un `.dat` en cada una de esas
llamadas sería carísimo en I/O. En vez de eso, los parámetros y las matrices
de dosis por órgano se empujan **directo a AMPL vía la API C++**, en
memoria, sin pasar por disco:

```cpp
ampl_->getParameter("d_ptv").setValues(ptv_tuples.data(), ...);
ampl_->getParameter("d_oar").setValues(oar_tuples.data(), ...);
```

(`imrt/imrt_fmo.cpp`, dentro de `ImrtFmoSolver::solve()`.)

## Lo único que sí generó un `.dat`

El export que armamos para que pudieras reproducir un solve a mano, sin
pasar por nuestro C++: `experiments/validacion_objetivo_guillermo/emili_validation.dat`
(generado por `scripts/export_fmo_dat.cpp`).

Hoy empaqueta **todo en un solo archivo** — parámetros + las 4 matrices de
órganos juntas, ~31MB — no separado por órgano como describiste.

## Pregunta concreta para la reunión

¿Tu comentario era sobre:

1. **La arquitectura del pipeline en producción** — en cuyo caso la
   respuesta es la de arriba: vive en memoria vía API C++, no en archivos,
   por costo de I/O dado el volumen de llamadas a `solve()`.
2. **Cómo debería verse el `.dat` de reproducción** que te mandamos — en
   cuyo caso puedo partirlo en un `.dat` de parámetros + un `.dat` por
   órgano. Encaja natural, además: los datos crudos de CERR ya vienen
   organizados por órgano (`<ORGANO>.txt` + `<ORGANO>_<ángulo>.txt`), así
   que la separación que pedís ya existe en el origen, solo no se refleja
   en el export final.

Si es la opción 2, lo ajusto en `scripts/export_fmo_dat.cpp` antes de la
próxima corrida de validación.
