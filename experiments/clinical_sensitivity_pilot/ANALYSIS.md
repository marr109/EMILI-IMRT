# Piloto de sensibilidad al placeholder clínico (Dmin/Dmax)

## Motivación

`CerrFmoSource` (`imrt/cerr_instance.cpp:42-47`) usa valores de Dmin/Dmax
placeholder porque no existe documentado el plan clínico real de
`CERR_Prostate` (ver `docs/cerr-prostate-instance-analysis.md`). Antes de
seguir sacando conclusiones algorítmicas (First vs Best, ILS, etc.) sobre
estos datos, medimos qué tan sensible es el objetivo del FMO a ese
placeholder — si mover el valor un 10-20% mueve el objetivo poco, el
placeholder importa poco para conclusiones *relativas* entre algoritmos; si
lo mueve mucho, cualquier valor absoluto reportado hoy no dice nada del plan
real.

## Método

Una sola semilla (`rnds 1`), First Improvement puro (sin ILS), mismo comando
documentado en `experiments/local_search/nangshift10/unrestricted/ANALYSIS.md`:

```bash
./build/emili instances/CERR_Prostate baoimrt 4 csv <path>/trajectory.csv \
  first irandomk locmin nangshift 10 rnds 1
```

Se parcheó temporalmente `dmin_ptvhd`/`dmin_ptvld` o `dmax_bladder`/
`dmax_rectum` en `imrt/cerr_instance.cpp:42-43`, se recompiló, se corrió, y
se revirtió antes de la siguiente variante (los 4 valores en HEAD quedan
intactos — ver git log, no hay diff pendiente).

## Resultados

| Variante | Dmin (Gy) | Dmax (Gy) | Objetivo final | Δ vs. baseline |
|---|---:|---:|---:|---:|
| Dmin -20% | 52.0 | 50.0 | 2872.02 | -97% |
| Dmin -10% | 58.5 | 50.0 | 31535.85 | -68% |
| **baseline** | **65.0** | **50.0** | **98770.06** | — |
| Dmax -10% | 65.0 | 45.0 | 169563.98 | +72% |
| Dmax -20% | 65.0 | 40.0 | 266319.44 | +170% |

## Lectura

El objetivo (`f = w_under·Σu² + w_over·Σv²`) es una suma de cuadrados de
violación — por diseño, extremadamente sensible al umbral: mover el límite
mueve la magnitud de cada violación individual, y el cuadrado amplifica esa
diferencia. Dmin pesa más que Dmax con la misma magnitud relativa (un -20%
en Dmin casi anula el objetivo; un -20% en Dmax casi lo triplica) —
consistente con `w_under=1.0 > w_over=0.5` en `CLINICAL_CONFIG`.

**Conclusión:** el placeholder actual (65/50 Gy) no es un detalle
cosmético. Los valores absolutos de objetivo reportados en todos los
experimentos de esta rama (`experiments/local_search/`, `experiments/ils/`)
son artefactos del placeholder, no del plan clínico real — no usar esos
números para ninguna afirmación clínica. El *ranking relativo* entre
configuraciones algorítmicas (First vs Best, grid5 vs unrestricted, etc.)
corridas bajo el mismo placeholder probablemente se mantiene, pero eso no
se verificó acá — sería el siguiente paso si hiciera falta (ej. repetir el
piloto First-vs-Best-idéntica-semilla bajo un Dmin/Dmax distinto y ver si el
ranking se invierte).

## Pendiente

- Bloqueante real: reemplazar el placeholder cuando aparezca el plan
  clínico documentado de `CERR_Prostate`.
- No se probó sensibilidad a `w_under`/`w_over` — son decisión de diseño
  del optimizador, no prescripción médica, y se dejaron fuera de este
  piloto a propósito (ver discusión en la sesión).
