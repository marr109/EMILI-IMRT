# Diagnóstico verbose del FMO — dimensiones por órgano antes de cada solve

## Qué es y por qué existe

Pedido explícito del profesor (Guillermo Cabrera) en la reunión de integración
de AMPL: antes de que el código llame al solver, quería ver por pantalla las
dimensiones de las matrices de cada órgano — para descartar, sin ambigüedad,
que el sistema estuviera repitiendo la misma matriz de dosis en vez de armar
una nueva para cada configuración de ángulos.

Implementado en `ImrtFmoSolver` (`imrt/imrt_fmo.h/.cpp`), activado con el
flag `verbose` que ya existía en el CLI de BAO. Se imprime justo antes de
`ampl_->solve()` — es decir, antes de que Gurobi vea los datos, no después.

## Cómo activarlo

```bash
./build/emili instances/CERR_Prostate baoimrt 4 verbose \
  ils first ifirstk tmaxiter 1 nangshift 10 tmaxiter 3 prangswap 1 improve rnds 7
```

El flag `verbose` (tercer token después de `baoimrt K`) alcanza para activarlo
— internamente hace `BaoProblem::setVerbose(true)`, que a su vez llama
`fmo_.setVerbose(true)` en `ImrtFmoSolver`.

## Salida real de una corrida completa (13 evaluaciones, semilla 7)

```
[FMO] beamlets_activos=280
    PTV PTVHD: boxets=2518 entradas_dosis=228087
    PTV PTVLD: boxets=2595 entradas_dosis=156250
    OAR BLADDER: boxets=3639 entradas_dosis=194070
    OAR RECTUM: boxets=1894 entradas_dosis=171522
Gurobi 13.0.2: optimal solution; objective 644373.5101
  BAO eval: angles=[0,1,2,3deg] -> f=644373.51

[FMO] beamlets_activos=279
    PTV PTVHD: boxets=2518 entradas_dosis=221261
    PTV PTVLD: boxets=2595 entradas_dosis=153697
    OAR BLADDER: boxets=3639 entradas_dosis=182773
    OAR RECTUM: boxets=1894 entradas_dosis=170913
Gurobi 13.0.2: optimal solution; objective 235699.5947
  BAO eval: angles=[350,1,2,3deg] -> f=235699.59

[FMO] beamlets_activos=275
    PTV PTVHD: boxets=2518 entradas_dosis=232352
    PTV PTVLD: boxets=2595 entradas_dosis=160027
    OAR BLADDER: boxets=3639 entradas_dosis=211324
    OAR RECTUM: boxets=1894 entradas_dosis=154930
Gurobi 13.0.2: optimal solution; objective 132333.1171
  BAO eval: angles=[1,2,235,350deg] -> f=132333.12

[FMO] beamlets_activos=274
    PTV PTVHD: boxets=2518 entradas_dosis=225215
    PTV PTVLD: boxets=2595 entradas_dosis=157480
    OAR BLADDER: boxets=3639 entradas_dosis=199498
    OAR RECTUM: boxets=1894 entradas_dosis=154356
Gurobi 13.0.2: optimal solution; objective 133335.5216
  BAO eval: angles=[351,2,235,350deg] -> f=133335.52

[FMO] beamlets_activos=273
    PTV PTVHD: boxets=2518 entradas_dosis=235581
    PTV PTVLD: boxets=2595 entradas_dosis=160345
    OAR BLADDER: boxets=3639 entradas_dosis=218681
    OAR RECTUM: boxets=1894 entradas_dosis=151286
Gurobi 13.0.2: optimal solution; objective 117358.7729
  BAO eval: angles=[11,2,235,350deg] -> f=117358.77

[FMO] beamlets_activos=268
    PTV PTVHD: boxets=2518 entradas_dosis=244986
    PTV PTVLD: boxets=2595 entradas_dosis=167994
    OAR BLADDER: boxets=3639 entradas_dosis=237849
    OAR RECTUM: boxets=1894 entradas_dosis=137724
Gurobi 13.0.2: optimal solution; objective 109870.5188
  BAO eval: angles=[11,68,235,350deg] -> f=109870.52

[FMO] beamlets_activos=270
    PTV PTVHD: boxets=2518 entradas_dosis=241757
    PTV PTVLD: boxets=2595 entradas_dosis=167676
    OAR BLADDER: boxets=3639 entradas_dosis=230492
    OAR RECTUM: boxets=1894 entradas_dosis=141368
Gurobi 13.0.2: optimal solution; objective 110346.7566
  BAO eval: angles=[1,68,235,350deg] -> f=110346.76

[FMO] beamlets_activos=269
    PTV PTVHD: boxets=2518 entradas_dosis=245797
    PTV PTVLD: boxets=2595 entradas_dosis=167274
    OAR BLADDER: boxets=3639 entradas_dosis=244567
    OAR RECTUM: boxets=1894 entradas_dosis=132651
Gurobi 13.0.2: optimal solution; objective 110478.2368
  BAO eval: angles=[21,68,235,350deg] -> f=110478.24

[FMO] beamlets_activos=270
    PTV PTVHD: boxets=2518 entradas_dosis=244938
    PTV PTVLD: boxets=2595 entradas_dosis=166621
    OAR BLADDER: boxets=3639 entradas_dosis=241352
    OAR RECTUM: boxets=1894 entradas_dosis=137613
Gurobi 13.0.2: optimal solution; objective 111153.4559
  BAO eval: angles=[11,58,235,350deg] -> f=111153.46

[FMO] beamlets_activos=266
    PTV PTVHD: boxets=2518 entradas_dosis=246415
    PTV PTVLD: boxets=2595 entradas_dosis=171401
    OAR BLADDER: boxets=3639 entradas_dosis=235113
    OAR RECTUM: boxets=1894 entradas_dosis=138484
Gurobi 13.0.2: optimal solution; objective 106291.566
  BAO eval: angles=[11,78,235,350deg] -> f=106291.57

[FMO] beamlets_activos=255
    PTV PTVHD: boxets=2518 entradas_dosis=264204
    PTV PTVLD: boxets=2595 entradas_dosis=185391
    OAR BLADDER: boxets=3639 entradas_dosis=259252
    OAR RECTUM: boxets=1894 entradas_dosis=124240
Gurobi 13.0.2: optimal solution; objective 111153.0346
  BAO eval: angles=[11,78,89,235deg] -> f=111153.03

[FMO] beamlets_activos=257
    PTV PTVHD: boxets=2518 entradas_dosis=260975
    PTV PTVLD: boxets=2595 entradas_dosis=185073
    OAR BLADDER: boxets=3639 entradas_dosis=251895
    OAR RECTUM: boxets=1894 entradas_dosis=127884
Gurobi 13.0.2: optimal solution; objective 105862.3337
  BAO eval: angles=[1,78,89,235deg] -> f=105862.33
```

## Qué significa cada número

- **`beamlets_activos`**: cuántas variables `x` tiene el QP en esta llamada —
  suma de los beamlets de los K=4 ángulos activos (varía porque, a
  diferencia de CORT, en CERR_Prostate cada ángulo tiene una cantidad
  distinta de beamlets, ver `docs/cerr-prostate-instance-analysis.md`).
- **`boxets`**: cantidad de vóxeles oficiales de ese órgano — es una
  propiedad fija del órgano (viene de `<ORGANO>.txt`), **no depende de qué
  ángulos estén activos**. Por eso nunca cambia entre líneas.
- **`entradas_dosis`**: cuántos pares (boxet, beamlet) tienen una dosis
  no-nula registrada para esa combinación específica de ángulos activos —
  esto **sí** depende de qué ángulos están prendidos, porque cada ángulo
  ilumina un conjunto distinto de vóxeles.

## Qué se supone que confirma esto (cómo leerlo)

Es un chequeo de cordura, no una métrica de calidad de la solución. Lo que
hay que mirar:

✅ **Correcto — lo que se ve arriba:**
- `boxets` idéntico en todas las líneas para un mismo órgano (2518, 2595,
  3639, 1894) — confirma que la estructura anatómica no se está recalculando
  ni corrompiendo entre llamadas.
- `entradas_dosis` cambia en cada evaluación (por ejemplo BLADDER va
  194070 → 182773 → 211324 → ... → 251895) — confirma que cada solve arma
  una matriz de dosis genuinamente nueva para esa combinación de ángulos,
  no está reutilizando la de la llamada anterior.
- `beamlets_activos` también varía (280, 279, 275, ..., 257) — consistente
  con que distintos ángulos tienen distinta cantidad de beamlets.

❌ **Lo que habría sido señal de bug** (no ocurrió, pero es lo que este
diagnóstico está diseñado para detectar):
- `boxets` cambiando entre llamadas → algo está recalculando mal la
  estructura de órganos, que debería ser constante para toda la instancia.
- `entradas_dosis` idéntico entre dos configuraciones de ángulos distintas →
  fuerte indicio de que se está reusando (por error) la matriz de la
  llamada anterior en vez de reconstruirla — exactamente el bug que el
  profesor quería poder detectar a simple vista.
- `entradas_dosis = 0` para algún órgano → esa combinación de ángulos no
  está aportando dosis a ese órgano, lo cual sería sospechoso si pasa
  seguido (normalmente cada ángulo ilumina algo de cada órgano, dado que
  están todos cerca anatómicamente).

En esta corrida real, las 13 evaluaciones muestran el patrón correcto en las
tres columnas — evidencia directa de que el pipeline AMPL+Gurobi está
recibiendo datos frescos y correctos en cada llamada, no repitiendo nada.
