# Comandos de los experimentos (modelo FMO por órgano)

Condiciones comunes: instancia `CERR_Prostate`, K = 4 ángulos, modelo por órgano, pesos 0,5 (vejiga) y 0,8 (recto),
prescripción 68 Gy, techo del PTV 1,07 × 68 con peso 0,5, solape PTV-OAR contado solo como PTV, **tope de intensidad 100**.
Todas las corridas usan 15 semillas y un hilo de Gurobi por proceso. Hay que dejar como máximo 48 procesos simultáneos en un
servidor de 48 núcleos físicos: con más, cada corrida se vuelve más lenta y las que terminan por reloj hacen menos evaluaciones.

## Entorno

```bash
export EMILI_FMO_MODEL=organs EMILI_W_OVER_ORGANS=0.5,0.8 EMILI_DMIN=68 EMILI_W_PTV_OVER=0.5
export EMILI_EXCLUDE_OVERLAP=1 EMILI_MAX_INTENSITY=100 EMILI_GUROBI_OPTIONS="threads=1"
# En Linux además: EMILI_AMPL_BIN_DIR y EMILI_GUROBI_BIN (ver docs/setup-calafate.md)
```

Los scripts de lote fijan estos valores por defecto; basta sobrescribir la variable para cambiar una condición.

## Una corrida

```bash
# Búsqueda local pura (termina en un óptimo local)
./build/emili instances/CERR_Prostate baoimrt 4 csv <traj.csv> <first|best> irandomk [unrestricted] locmin \
    nangshift <5|10> [circular|randorder] rnds <semilla>

# ILS (termina a los 18 minutos de reloj con -it 1080; -it va ANTES de rnds)
./build/emili instances/CERR_Prostate baoimrt 4 csv <traj.csv> ils <first|best> irandomk [unrestricted] locmin \
    nangshift <5|10> [circular|randorder] tmaxiter 1000000 <perturbación> baoimprove rejectrepeated -it 1080 rnds <semilla>

# SA (lineal o geométrico)
./build/emili ... sa irandomk [unrestricted] tmaxiter 1000000 nangshift <5|10> sa_metropolis 1300 80 11 -it 1080 rnds <semilla>
./build/emili ... sa irandomk [unrestricted] tmaxiter 1000000 nangshift <5|10> saacc 1300 80 0 1 0.977 -it 1080 rnds <semilla>

# VND (vecindarios en el orden dado; first improvement)
./build/emili instances/CERR_Prostate baoimrt 4 csv <traj.csv> vnd first irandomk locmin \
    nangshift 10 nangshift 5 [nangswap] rnds <semilla>
```

`[unrestricted]` se omite en el catálogo restringido (72 ángulos, múltiplos de 5°).

## Perturbaciones de ILS

| Variante | Tokens | Qué hace |
|---|---|---|
| base | `prangshift <paso> 3` | 3 ángulos, magnitud aleatoria en un rango |
| adaptativa | `prangshiftadaptive <paso> 4 2` | de 1 a 4 ángulos según el estancamiento |
| mag2 | `prangshiftmag 2 2 5 10` | 2 ángulos, ±5° o ±10° |
| mag2-stepwise | `prangshiftmag 2 2 5 10 stepwise` | igual, evaluando el FMO después de cada ángulo |
| adaptivemag-stepwise | `prangshiftadaptivemag 2 2 2 5 10 stepwise` | adaptativa de 1 a 2 ángulos, por pasos |

## Lotes completos

```bash
scripts/run_local_search_organs_all.sh 48 2      # 24 condiciones x 15 semillas (paso x catálogo x orden x estrategia)
scripts/run_sa_organs_all.sh 24 6                # SA lineal
scripts/run_sa_geometric_organs_all.sh 24 6      # SA geométrico
scripts/run_ils_organs_all.sh 48 3               # 5 perturbaciones x 2 pasos x 2 catálogos x (first con 3 órdenes + best fijo)
scripts/run_vnd_organs_all.sh 45                 # VND: 10-5, 10-swap y 10-5-swap
```

Argumentos: número de procesos en paralelo y cuántos trabajos se reparten las semillas de cada condición. Los scripts omiten las
semillas ya terminadas, así que se pueden relanzar. Salida en `experiments/new/<familia>/...` (no versionada).

## Análisis

```bash
python ampl_gurobi/dosimetry_batch.py experiments/new/<familia> --out experiments/new/<familia>/plans_dosimetry.csv --geud
python scripts/analyze_new_experiments.py        # tablas, figuras y experiments/new/ANALYSIS_NOTES.md
```

Notas de interpretación que conviene conocer:
- Con `best`, el orden de escaneo no cambia el resultado (verificado), así que solo se corre con orden fijo en ILS.
- En ILS, el plan final que devuelve el framework puede ser peor que el mejor plan evaluado (el corte por reloj cae a mitad de un barrido).
- Los resultados con tope 15000 y con tope 100 no son comparables entre sí.
