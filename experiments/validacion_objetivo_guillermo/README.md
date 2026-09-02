# Validación de función objetivo FMO — para Guillermo

Corrida de validación pedida en la reunión del 2026-08-13: confirmar que el
cálculo de la función objetivo del FMO coincide con el resultado de un solver
independiente, sobre una configuración de ángulos iniciales restringida a
múltiplos de 5° (para que el espacio de búsqueda alcanzable por el
vecindario de shift esté bien definido y sea comparable entre corridas).

## Corrección aplicada antes de esta corrida

La solución inicial aleatoria (`irandomk`) elegía K ángulos de un catálogo de
360 (0°–359°, sin restricción), lo que podía producir ángulos iniciales como
348° que no caen en la misma reja que el paso del vecindario de shift (10°).
Se corrigió `RandomKAnglesInit::generateEmptySolution()`
(`imrt/imrt_bao.cpp`) para muestrear solo de los 72 candidatos cuyo grado es
múltiplo de 5. Esta corrida ya incluye ese fix.

## Comando ejecutado

```bash
./build/emili instances/CERR_Prostate baoimrt 4 csv trajectory.csv \
  best irandomk locmin nangshift 10 rnds 1
```

- Instancia: `instances/CERR_Prostate` (export crudo de CERR, 360 ángulos
  candidatos, 4 órganos: PTVHD, PTVLD, BLADDER, RECTUM).
- K = 4 ángulos activos.
- Estrategia: Best Improvement, sin ILS (búsqueda local pura hasta óptimo
  local — `locmin`).
- Vecindario: `nangshift 10` (desplazamiento angular ±10°, wraparound
  circular en 0°/359°).
- Semilla: 1.

## Resultado

| | Ángulos activos (deg) | Objetivo |
|---|---|---|
| Solución inicial | 35, 115, 165, 285 | 101 130.596154 |
| Solución final    | 45, 135, 165, 275 | **96 582.522366** |

42 evaluaciones FMO (llamadas a Gurobi) hasta el óptimo local. Todas las
configuraciones de ángulos visitadas durante la corrida (columna
`angles_deg` en `trajectory.csv`) son múltiplos de 5.

## Función objetivo (`fmo_mod.txt`, adjunto en esta carpeta)

Dado un conjunto fijo de K ángulos activos:

```
min   w_under · Σ u_b²  +  w_over · Σ v_b²  +  w_ptv_over · Σ w_b²

s.t.  D_ptv · x + u  ≥  Dmin        (piso de dosis PTV; u absorbe el déficit)
      D_oar · x − v  ≤  Dmax        (techo de dosis OAR; v absorbe el exceso)
      D_ptv · x − w  ≤  1.07·Dmin   (techo de sobredosis PTV — ver nota)
      u, v, w  ≥  0
      0  ≤  x_j  ≤  max_intensity
```

- `x` = intensidad de cada beamlet activo.
- `u` = slack de subdosis por vóxel PTV (uno por vóxel de PTVHD/PTVLD).
- `v` = slack de sobredosis por vóxel OAR (uno por vóxel de BLADDER/RECTUM).
- `w` = slack de sobredosis PTV (hot-spot). El bound `1.07·Dmin` sí se manda
  al modelo, pero es no-binding en la práctica: como `w_ptv_over = 0`, `w` no
  cuesta nada en el objetivo y solo tiene cota inferior (`w ≥ 0`), así que el
  solver puede agrandarlo lo necesario para cumplir la restricción sin pagar
  costo — no limita a `x` de forma efectiva. Es una restricción opcional
  (cap de hot-spot dentro del propio PTV) deshabilitada por diseño desde la
  formulación original en OSQP, no una decisión nueva de esta rama.

Resuelto vía AMPL + Gurobi (Gurobi 13.0.2) desde C++, un QP separable
convexo por cada evaluación de una configuración de K ángulos.

## Cómo reproducir el resultado con AMPL directo (`emili_validation.dat`)

No hace falta reimplementar nada ni parsear los archivos crudos de CERR: el
archivo `emili_validation.dat` (adjunto en esta carpeta) trae ya cargados
todos los parámetros y conjuntos que `fmo.mod` necesita para esta
configuración exacta de 4 ángulos (45°, 135°, 165°, 275° — la solución final
de la tabla de arriba), generados directo desde el mismo código C++ que
resolvió la corrida (`scripts/export_fmo_dat.cpp`, no una reimplementación
aparte). Con una sesión `ampl` de terminal alcanza:

```ampl
model fmo_mod.txt;
data emili_validation.dat;
option solver gurobi;   # o la ruta absoluta al binario de Gurobi
solve;
display fmo_objective;
```

Ya lo corrí yo mismo antes de mandarlo: da `fmo_objective = 96582.52237`,
igual (con diferencia de redondeo del `display`) al `96582.522366` de la
tabla de resultados. Si a vos te da ese mismo número, el cálculo del
objetivo está confirmado — no queda nada más que validar en esta parte.

**Nota de tamaño:** el archivo pesa ~31MB porque incluye la matriz de dosis
dispersa completa (430 612 entradas PTV + 364 238 OAR) para los 262 beamlets
activos de estos 4 ángulos — es dato real de la instancia, no se puede
resumir sin perder fidelidad.

## Parámetros usados

⚠️ **Son placeholders de validación mecánica del pipeline, no valores
clínicos reales del paciente.** No existe en el dataset `CERR_Prostate`
ningún `instance_config.txt` ni plan de tratamiento que documente la
prescripción real — por eso no se pueden sacar conclusiones clínicas de
estos números todavía, solo validar que el cálculo del objetivo es correcto.

| Parámetro | Valor |
|---|---|
| Dmin PTVHD | 65.0 Gy |
| Dmin PTVLD | 65.0 Gy |
| Dmax BLADDER | 50.0 Gy |
| Dmax RECTUM | 50.0 Gy |
| Dmax PTV | 1.07 × Dmin (por vóxel; no penalizado — ver nota arriba) |
| w_under | 1.0 |
| w_over | 0.5 |
| w_ptv_over | 0.0 (desactivado) |
| max_intensity | 15000.0 |

## Archivos en esta carpeta

- `trajectory.csv` — las 42 evaluaciones FMO de la corrida completa (eval,
  ángulos activos, objetivo, si vino de caché).
- `fmo_mod.txt` — modelo AMPL exacto usado para cada evaluación (contenido
  idéntico a `ampl_gurobi/fmo.mod`, la única fuente de verdad del objetivo y
  las restricciones — renombrado a `.txt` para que se abra sin problemas
  fuera del repo).
- `emili_validation.dat` — datos AMPL completos (parámetros + matriz de
  dosis dispersa) para reproducir la solución final sin pasar por nuestro
  pipeline C++. Ver sección "Cómo reproducir..." arriba.
- `parametros.txt` — los mismos valores clínicos/pesos en texto plano, para
  referencia rápida sin abrir el `.dat`.
- `README.md` — este archivo.
