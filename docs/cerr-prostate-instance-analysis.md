# Análisis de la instancia `instances/CERR_Prostate`

Todos los números de este documento están verificados directamente contra los
archivos crudos (no son estimaciones ni vienen de memoria) — el método de
verificación de cada sección está documentado junto al hallazgo, porque en el
camino apareció un bug de tooling que produjo un dato incorrecto (ver §4) y
vale la pena dejar registrado cómo se detectó.

## 1. Qué es

Export crudo del sistema CERR para un paciente de próstata — **no** es el
formato CORT/`instance_config.txt` que usaban las instancias `PROSTATE_*`. Sin
ningún paso de conversión: se lee tal cual la dejó CERR (ver
`docs/ampl-gurobi-integration.md` §3 para el porqué de esa decisión).

| | |
|---|---|
| Tamaño en disco | 11 GB |
| Archivos | 2173 |
| Ángulos candidatos | 360 (0°–359°, resolución 1°) |
| Beamlets totales | 23 971 |
| Órganos | PTVHD, PTVLD (target), BLADDER, RECTUM (OAR) |

## 2. Ángulos y beamlets — NO es una cantidad fija por ángulo

A diferencia de CORT (que asumía `n_dimlets_per_angle` constante), acá cada
ángulo tiene una cantidad distinta de beamlets, leída directo de
`beamletIndex.txt` (360 filas: `angle_idx global_start global_end`,
1-based/inclusive/acumulativo).

```
min=58   max=72   media=66.59   mediana=67   desvío=3.77
```

Distribución completa:

| beamlets | # ángulos | | beamlets | # ángulos |
|---|---|---|---|---|
| 58 | 15 | | 67 | 20 |
| 59 | 17 | | 68 | 21 |
| 60 | 2  | | 69 | 55 |
| 61 | 7  | | 70 | 56 |
| 62 | 7  | | 71 | 34 |
| 63 | 13 | | 72 | 11 |
| 64 | 49 | |
| 65 | 25 | |
| 66 | 28 | |

Es exactamente por esto que se construyó `CerrFmoSource` en vez de reusar
`ImrtInstance` — forzar un "beamlets por ángulo" fijo hubiera distorsionado la
geometría real del beam.

## 3. Órganos — vóxeles oficiales

| Órgano | Vóxeles | Rol |
|---|---:|---|
| PTVHD | 2 518 | target, dosis alta |
| PTVLD | 2 595 | target, dosis baja |
| BLADDER | 3 639 | OAR |
| RECTUM | 1 894 | OAR |

### Qué significan PTVHD y PTVLD

**PTV** = *Planning Target Volume* — el volumen objetivo de radioterapia. No es
el tumor en sí; es el volumen clínico a tratar más un margen de seguridad para
compensar errores de posicionamiento, movimiento del paciente, etc.

**HD** = *High Dose*, **LD** = *Low Dose*.

Que existan PTVHD y PTVLD como estructuras separadas indica un esquema de
tratamiento **SIB** (*Simultaneous Integrated Boost*) — dos volúmenes objetivo
tratados a la vez, cada uno con su propia dosis prescrita:

- **PTVHD**: el volumen "boost" — típicamente la próstata en sí (o el tumor
  macroscópico), que recibe la dosis más alta.
- **PTVLD**: un volumen más amplio alrededor — típicamente vesículas
  seminales, ganglios pélvicos u otras estructuras de menor riesgo, que
  recibe una dosis prescrita menor.

Esto es exactamente lo que explica el overlap de 944 vóxeles entre ambos
(§4): en un esquema SIB, el volumen de alta dosis está anidado dentro (o se
superpone con) el de baja dosis — es la misma región anatómica de próstata
con dos niveles de prescripción simultáneos, no dos estructuras separadas.

Definidos por `<ORGANO>.txt` — una lista plana de IDs de vóxel global (0-based,
CT grid completo), sin header ni comentarios. El orden de línea define el
índice local del boxet para ese órgano (así lo usa `CerrFmoSource::loadVoxelList`).

## 4. Overlap entre órganos — un hallazgo corregido en el camino

### El error original

Al principio de esta investigación se afirmó (y quedó escrito en memoria y en
código) que PTVHD y PTVLD **no se superponen**, verificado con:

```bash
comm -12 <(sort -n PTVHD.txt) <(sort -n PTVLD.txt) | wc -l   # dio 0
```

Eso es **incorrecto** — es un bug de tooling, no un hecho de los datos.
`sort -n` ordena *numéricamente*, pero `comm` compara líneas *como texto*
(colación lexicográfica). Con IDs de 7 y 8 dígitos mezclados, el orden
numérico y el orden lexicográfico no coinciden (`"9047303"` es lexicográficamente
mayor que `"11151599"` aunque numéricamente sea menor), así que un archivo
ordenado numéricamente **no** está ordenado en el sentido que `comm` necesita
— y `comm` no valida esto, asume que sí y camina ambos archivos en paralelo
comparando mal, sin ningún error visible.

### La verificación correcta

```python
a = set(int(l) for l in open("PTVHD.txt"))
b = set(int(l) for l in open("PTVLD.txt"))
len(a & b)   # 944
```

Confirmado además con `grep` directo: el ID `11151599` aparece literalmente en
ambos archivos (línea 585 de `PTVHD.txt`, línea 1485 de `PTVLD.txt`).

### El resultado real — matriz de overlap completa

| | PTVHD | PTVLD | BLADDER | RECTUM |
|---|---:|---:|---:|---:|
| **PTVHD** | — | 944 | 238 | 174 |
| **PTVLD** | | — | 204 | 295 |
| **BLADDER** | | | — | **0** |
| **RECTUM** | | | | — |

El overlap PTVHD↔PTVLD (944 vóxeles, ~37% del PTVHD) es clínicamente
razonable — ver §3 para el porqué (esquema SIB, alta dosis anidada dentro de
baja dosis). Los overlaps PTV↔OAR (174–295 vóxeles) también tienen sentido
anatómico: el recto y la vejiga están pegados a la próstata, así que un
margen de PTV inevitablemente roza tejido de esos órganos. Solo
BLADDER↔RECTUM da 0 — anatómicamente separados por la próstata.

**Implicación para la función objetivo:** un vóxel que está en PTVHD y en
BLADDER a la vez (238 casos) participa en el modelo **dos veces** — una fila
en el bloque PTV (penalizada si hay subdosis) y otra fila en el bloque OAR
(penalizada si hay sobredosis) — con su propio slack en cada caso. `fmo.mod`
no necesita ni asume exclusión mutua entre bloques; cada organo-vóxel es una
fila independiente del QP, así que esto no rompe nada, pero es información
clínica real que vale la pena tener presente al interpretar resultados.

## 5. Los archivos por ángulo contienen MÁS vóxeles que la lista oficial del órgano

Otro hallazgo no obvio: `<ORGANO>_<angle>.txt` no está restringido a los
vóxeles oficiales de ese órgano — trae dosis para una región mucho más amplia
(probablemente la grilla de cálculo de dosis completa de ese haz), y es
**`CerrFmoSource::ensureAngleLoaded`** el que filtra, vía
`voxel_to_local.find(voxel_id)`, quedándose solo con las filas que matchean un
vóxel oficial del órgano.

Verificado en el ángulo 0:

| Órgano | Filas totales en el archivo | Filas que matchean un vóxel oficial | % descartado |
|---|---:|---:|---:|
| PTVHD | 304 941 | 56 770 | 81.4% |
| PTVLD | 351 289 | 39 316 | 88.8% |
| BLADDER | 336 747 | 47 510 | 85.9% |
| RECTUM | 380 965 | 43 556 | 88.6% |

O sea: por cada archivo `<ORGANO>_<angle>.txt`, entre el 81% y el 89% de las
filas se descartan al cargar porque no corresponden a ningún vóxel de la lista
oficial de ese órgano. Esto es esperable y el loader ya lo maneja bien — se
documenta acá porque si alguien lee estos archivos directamente (por ejemplo
para hacer una cuenta rápida de sparsity) y no aplica el mismo filtro, va a
sacar conclusiones de densidad completamente erróneas (fue exactamente el
primer error de cálculo en este análisis, corregido antes de escribir la
tabla de abajo).

## 6. Densidad real de la matriz de dosis (post-filtro)

Densidad = filas válidas / (vóxeles oficiales × beamlets del ángulo), ángulo 0
(70 beamlets):

| Órgano | Filas válidas | Celdas densas posibles | Densidad |
|---|---:|---:|---:|
| PTVHD | 56 770 | 176 260 | 32.2% |
| PTVLD | 39 316 | 181 650 | 21.6% |
| BLADDER | 47 510 | 254 730 | 18.7% |
| RECTUM | 43 556 | 132 580 | 32.9% |

Ni remotamente densa (como se espera de dosis-influencia — un beamlet solo
deposita dosis apreciable en una fracción de los vóxeles a lo largo de su
trayectoria), pero tampoco extremadamente rala. Confirma que el diseño de
`fmo.mod` con sets dispersos (`PTV_DOSE`/`OAR_DOSE` como sets de tuplas, no
matrices densas) es la elección correcta.

Sin duplicados: dentro de un mismo archivo, cada par (vóxel, beamlet_local) es
único (verificado con `sort | uniq -c` sobre `PTVHD_0.txt` — 304 941 filas,
304 941 pares únicos).

## 7. Rango de valores de dosis (`dose_rate`)

Muestreado sobre 4 ángulos (0°, 90°, 180°, 270°):

| Órgano | mínimo | máximo | media |
|---|---:|---:|---:|
| PTVHD | 1.11e-03 | 1.50e+00 | 6.03e-02 |
| PTVLD | 1.45e-03 | 1.50e+00 | 7.18e-02 |
| BLADDER | 7.55e-04 | 1.97e+00 | 6.40e-02 |
| RECTUM | 1.14e-03 | 1.58e+00 | 5.58e-02 |

Unidades: Gy por unidad de intensidad de beamlet (consistente con
`max_intensity = 15000` en `CLINICAL_CONFIG` — intensidades altas para
compensar `dose_rate` chico, mismo patrón que ya se documentó para
`PROSTATE_36ang` en `MANUAL.md`).

## 8. Resumen de gotchas para quien trabaje con esta instancia después

1. **`comm` sobre archivos con `sort -n` da resultados silenciosamente
   incorrectos** si los IDs tienen distinta cantidad de dígitos. Para
   intersección de conjuntos de enteros, usar `sort` lexicográfico (sin `-n`)
   con `comm`, o directamente sets en Python/AMPL — no mezclar orden numérico
   con herramientas que comparan texto.
2. **Los archivos `<ORGANO>_<angle>.txt` traen ~85% de filas que no
   pertenecen a ese órgano** — hay que filtrar contra `<ORGANO>.txt` antes de
   sacar cualquier estadística de densidad o conteo.
3. **PTVHD y PTVLD se superponen (944 vóxeles)** — no asumir bloques
   mutuamente excluyentes al razonar sobre el objetivo o al debuggear
   resultados de dosis.
4. Los valores clínicos (Dmin/Dmax/pesos) siguen siendo placeholder — ver
   `docs/ampl-gurobi-integration.md` §4.
