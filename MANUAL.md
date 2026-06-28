# Manual de uso — EMILI IMRT BAO

## Índice
1. [Arquitectura del sistema](#1-arquitectura)
2. [Compilación](#2-compilación)
3. [Formato de instancias](#3-formato-de-instancias)
4. [Ejecutar EMILI manualmente](#4-ejecutar-emili-manualmente)
5. [Interpretar los resultados](#5-interpretar-resultados)
6. [Ejecutar irace](#6-ejecutar-irace)
7. [Referencia rápida de parámetros](#7-referencia-rápida)
8. [Estado actual del proyecto](#8-estado-actual)

---

## 1. Arquitectura

El sistema resuelve el problema IMRT en dos niveles anidados:

```
┌─────────────────────────────────────────────────────┐
│  BAO — Beam Angle Optimization                      │
│  ¿Qué K ángulos de gantry usar?                     │
│  Algoritmo: ILS con vecindario de intercambio       │
│  Espacio: C(N,K) combinaciones discretas            │
│                                                     │
│  Para evaluar cada combinación → llama al FMO ↓    │
└─────────────────────────────────────────────────────┘
                        ↓ 1 llamada OSQP por evaluación
┌─────────────────────────────────────────────────────┐
│  FMO — Fluence Map Optimization                     │
│  Dados esos ángulos, ¿qué intensidad tiene cada    │
│  beamlet para maximizar dosis en PTV y             │
│  minimizar dosis en OARs?                           │
│  Algoritmo: OSQP (solver QP convexo exacto)        │
│  Espacio: R^(K×dimlets) continuo y convexo         │
└─────────────────────────────────────────────────────┘
```

**Archivos clave:**
- `imrt/imrt_bao.cpp` — lógica BAO (ILS, vecindarios de ángulos)
- `imrt/imrt_fmo.cpp` — lógica FMO (ensamblado QP, llamada OSQP)
- `imrt/imrt_instance.cpp` — carga de instancias CORT
- `emilibase.cpp` — framework ILS genérico

---

## 2. Compilación

```bash
cd /Users/marrojasr/Documents/code/emili_imrt
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..
```

El binario queda en `build/emili`. Verificar con:
```bash
./build/emili --help 2>&1 | head -5
```

---

## 3. Formato de instancias

Cada instancia es un **directorio** con esta estructura:

```
instancia/
├── instance_config.txt       ← configuración principal
├── PTV_68_VOILIST.txt        ← IDs de vóxeles del tumor
├── Bladder_VOILIST.txt       ← IDs de vóxeles de vejiga
├── Rectum_VOILIST.txt        ← IDs de vóxeles de recto
├── Gantry0_Couch0_D.txt      ← matriz de dosis, ángulo 0°
├── Gantry10_Couch0_D.txt     ← matriz de dosis, ángulo 10°
└── ...                       ← un archivo por ángulo candidato
```

### instance_config.txt

```
angles    36  0 10 20 30 ... 350    # N ángulos candidatos + lista
dimlets   157                        # beamlets por ángulo
ptv       PTV_68  68.0  -10.0        # nombre, dosis prescrita (Gy), a_gEUD
oar       Bladder 50.0 0.25 45.0 10.0  # nombre, Dmax, V_frac, Dlim, a_gEUD
oar       Rectum  50.0 0.25 45.0 10.0
voilist   PTV_68   PTV_68_VOILIST.txt
voilist   Bladder  Bladder_VOILIST.txt
voilist   Rectum   Rectum_VOILIST.txt
w_under   1.0       # peso penalización subdosis PTV
w_over    0.5       # peso penalización sobredosis OAR
w_ptv_over 0.5      # peso penalización sobredosis PTV (>1.07×Rx)
max_intensity 15000.0  # intensidad máxima por beamlet
```

### Matrices de dosis (GantryX_Couch0_D.txt)

Formato sparse: cada fila es `voxel_id  beamlet_id  dose_rate`
```
# Gantry 0 Couch 0 — global_voxel_id local_beamlet_id dose_rate (0-based)
1436007  0  4.211503e-06
1436008  0  4.656950e-06
...
```

### Valores de max_intensity por tipo de instancia

| Instancia | max_intensity | Por qué |
|-----------|--------------|---------|
| MEDIUM_test (sintética) | 10.0 | Dose rates ~0.2, escala correcta |
| PROSTATE_36ang (real CORT) | 15000.0 | Dose rates ~4×10⁻⁶, necesita escala alta |
| PROSTATE_sampled* (real) | 15000.0 | Misma fuente que PROSTATE_36ang |

> **Crítico**: `max_intensity` demasiado bajo → OSQP entrega dosis ~0 Gy → f ≈ 31,000,000 → resultados sin sentido clínico.

---

## 4. Ejecutar EMILI manualmente

### Sintaxis completa BAO

```bash
./build/emili INSTANCIA baoimrt K \
    ils EXPLORE INIT tmaxiter INNER_ITER nangswap \
    tmaxiter OUTER_ITER prangswap PERT_SWAPS ACCEPT \
    rnds SEED
```

### Parámetros

| Parámetro | Opciones | Descripción |
|-----------|----------|-------------|
| `INSTANCIA` | path al directorio | Ruta a la instancia |
| `K` | entero (1–N) | Número de ángulos activos |
| `EXPLORE` | `best` / `first` | Best improvement (evalúa todos los vecinos) o First improvement (para en el primer que mejora) |
| `INIT` | `ifirstk` / `irandomk` | Solución inicial: primeros K ángulos / K ángulos aleatorios |
| `INNER_ITER` | entero ≥ 1 | Iteraciones del LS interno. Nota: valores 1 y 2 son equivalentes (dan 1 scan efectivo) |
| `OUTER_ITER` | entero ≥ 1 | Número de reinicios del ILS (perturba + LS interno) |
| `PERT_SWAPS` | 1 / 2 | Ángulos intercambiados en cada perturbación |
| `ACCEPT` | `improve` | Solo acepta si mejora (estándar) |
| `SEED` | entero | Semilla aleatoria para reproducibilidad |

### Ejemplos

**Prueba rápida en MEDIUM_test (20s):**
```bash
./build/emili instances/MEDIUM_test baoimrt 3 \
    ils best ifirstk tmaxiter 1 nangswap \
    tmaxiter 3 prangswap 1 improve \
    rnds 42
```

**Configuración ganadora irace en instancia submuestreada (~32 min):**
```bash
./build/emili irace_prostate/instances/PROSTATE_sampled_K4 baoimrt 4 \
    ils best ifirstk tmaxiter 1 nangswap \
    tmaxiter 3 prangswap 1 improve \
    rnds 42
```

**Validación final en instancia completa (~3.5h):**
```bash
./build/emili instances/PROSTATE_36ang baoimrt 4 \
    ils best ifirstk tmaxiter 1 nangswap \
    tmaxiter 3 prangswap 1 improve \
    rnds 42
```

**Ejecutar en background y guardar salida:**
```bash
./build/emili instances/PROSTATE_36ang baoimrt 4 \
    ils best ifirstk tmaxiter 1 nangswap \
    tmaxiter 3 prangswap 1 improve \
    rnds 42 > resultados/run_seed42.txt 2>/dev/null &
```

---

## 5. Interpretar resultados

### Salida estándar

```
time : 19.93                          ← tiempo de ejecución (segundos)
iteration counter : 0
Objective function value: 0.000000   ← f (minimizar; 0 = perfecto)
Found solution: angles=[0,90,180deg] f=0.00

================================================================
  IMRT FMO RESULT   K=3 beams
================================================================
Selected angles (deg) : [ 0, 90, 180 ]
FMO objective f*      : 0.00
Prescription dose Rx  : 68.00 Gy
```

### Tabla de estadísticas de dosis

```
Structure  Voxels   Dmin   Dmean   Dmax    D95     D5     D2
PTV_68      6770   54.33   67.63  77.13  60.61  73.55  74.40
Bladder    11596    0.15   36.38  68.58    —    62.97  64.34
Rectum      1764    0.24   40.87  66.79    —    61.37  63.71
```

| Métrica | Significado |
|---------|-------------|
| `D95` | Dosis recibida por el 95% del volumen. Para PTV debe ser ≥ 64.6 Gy (= 0.95 × 68) |
| `D2` | Dosis en el 2% más irradiado (hotspot). Para PTV debe ser ≤ 72.76 Gy (= 1.07 × 68) |
| `Dmax` | Dosis máxima puntual. Para OARs debe ser ≤ 50 Gy (aunque violaciones en 2% son comunes) |
| `V70` | Fracción del volumen que recibe ≥ 70 Gy. Para OARs debe ser ≤ 25% |

### Función objetivo f

```
f = w_under · Σ(max(0, 68 - dosis_PTV_i))²   ← penaliza subdosis en tumor
  + w_over  · Σ(max(0, dosis_OAR_j - Dmax))²  ← penaliza sobredosis en OARs
  + w_ptv_over · Σ(max(0, dosis_PTV_i - 72.8))² ← penaliza sobredosis en tumor
```

| Valor de f | Interpretación |
|------------|----------------|
| ≈ 0 | Plan perfecto (solo posible en instancias sintéticas) |
| 1,000 – 50,000 | Buen plan, violaciones menores |
| 50,000 – 500,000 | Plan aceptable, violaciones moderadas |
| > 1,000,000 | Plan deficiente |
| ≈ 31,000,000 | `max_intensity` mal configurado — dosis ≈ 0 Gy |

> **Nota**: f no es comparable entre instancias con distinto número de vóxeles. Usarlo solo para comparar runs sobre la misma instancia.

### Índices de calidad del plan

| Índice | Fórmula | Valor ideal |
|--------|---------|------------|
| CI (Conformidad) | V_Rx / V_PTV | 1.0 |
| HI (Homogeneidad) | (D2 - D98) / Dosis_Rx | 0.0 (cuanto menor mejor) |
| V95%Rx | % del PTV con dosis ≥ 0.95×Rx | 100% |

---

## 6. Ejecutar irace

irace calibra automáticamente los parámetros del ILS ejecutando muchas configuraciones y comparando estadísticamente sus resultados.

### Lanzar irace

```bash
cd /Users/marrojasr/Documents/code/emili_imrt

Rscript -e "
  scenario <- irace::readScenario(filename='irace_prostate/scenario.irace')
  irace::irace(scenario=scenario)
" > irace_prostate/results/irace_run.log 2>&1 &
```

### Monitorear progreso

```bash
# Ver progreso en tiempo real
tail -f irace_prostate/results/irace_run.log

# Ver cuántos procesos emili están corriendo
ps aux | grep emili | grep -v grep | wc -l
```

### Archivos de configuración

| Archivo | Propósito |
|---------|-----------|
| `irace_prostate/scenario.irace` | Presupuesto, timeouts, paralelismo, instancias |
| `irace_prostate/parameters.irace` | Espacio de parámetros a explorar |
| `irace_prostate/init_config.txt` | Configuración inicial (punto de partida) |
| `irace_prostate/target-runner` | Script que ejecuta un run de EMILI |
| `irace_prostate/instances.txt` | Lista de instancias de entrenamiento |
| `irace_prostate/results/irace.log` | Log binario R con todos los resultados |
| `irace_prostate/results/irace_run.log` | Log texto del progreso |

### Leer resultados de irace

```r
# En R:
load("irace_prostate/results/irace.log")
print(iraceResults$allElites)          # todas las configuraciones elite
print(iraceResults$iterationElites)    # ganadora por iteración
```

### Parámetros actuales de búsqueda

```
inner_explore  (best | first)        ← estrategia de exploración del vecindario
inner_init     (ifirstk | irandomk)  ← solución inicial
pert_swaps     (1 | 2)               ← ángulos intercambiados al perturbar
outer_iter     (1 | 3)               ← número de reinicios ILS
```

**Configuración inicial (punto de partida):** `best ifirstk pert=1 outer=2`

**Tiempos estimados por run (instancia submuestreada, max_intensity=15000):**
- Mínimo (outer=1): ~32 min
- Máximo (outer=3): ~64 min
- Budget total: 200,000s CPU → ~52 experimentos (~14h con 4 cores)

### Matar irace si es necesario

```bash
pkill -f "irace_prostate/target-runner"
pkill -f "PROSTATE_sampled"
pkill -f "Rscript"
```

---

## 7. Referencia rápida

### Instancias disponibles

| Instancia | Vóxeles | Ángulos | max_intensity | Uso |
|-----------|---------|---------|--------------|-----|
| `instances/MEDIUM_test` | 270 | 12 | 10.0 | Tests rápidos, verificación |
| `irace_prostate/instances/PROSTATE_sampled_K4` | 2,564 | 36 | 15000.0 | irace (seed=2024) |
| `irace_prostate/instances/PROSTATE_sampled2_K4` | 2,564 | 36 | 15000.0 | irace (seed=7777) |
| `instances/PROSTATE_36ang` | 20,133 | 36 | 15000.0 | Validación clínica final |

### Tiempos de ejecución por instancia

| Instancia | outer=1 | outer=3 |
|-----------|---------|---------|
| MEDIUM_test | ~20s | ~30s |
| PROSTATE_sampled_K4 | ~32 min | ~64 min |
| PROSTATE_36ang | ~2.5h | ~5.5h |

### Verificar que un run usa los parámetros correctos

Al iniciar, EMILI imprime la configuración completa:
```
BAO problem (OSQP-exact FMO + busqueda de angulos)
  K (angulos activos) : 4
  ILS
    BEST IMPROVEMENT          ← inner_explore=best
        BAO initial solution: first K angles   ← inner_init=ifirstk
        ...
      termination: max iterations
        max : 3               ← outer_iter=3
      BAO perturbation: random angle swap
        swaps : 1             ← pert_swaps=1
      improve acceptance      ← accept=improve
```

### Comandos de diagnóstico

```bash
# Ver procesos corriendo
ps aux | grep emili | grep -v grep

# Ver tiempos CPU de cada proceso
ps aux | grep emili | grep -v grep | awk '{print $2, $10, $11, $12}'

# Matar todos los procesos EMILI
pkill -f build/emili

# Verificar max_intensity de una instancia
grep "max_intensity" instances/PROSTATE_36ang/instance_config.txt

# Contar vóxeles por órgano
wc -l instances/PROSTATE_36ang/*VOILIST.txt
```

---

## 8. Estado actual del proyecto

### Configuración ganadora (irace con instancias submuestreadas)

| Parámetro | Valor |
|-----------|-------|
| `inner_explore` | `best` |
| `inner_init` | `ifirstk` |
| `pert_swaps` | 1 |
| `outer_iter` | 3 |

> **Nota**: Esta configuración fue obtenida con `max_intensity=10` (incorrecto). Pendiente re-calibrar irace con `max_intensity=15000`.

### Resultado validado en instancia submuestreada

```
Instancia : PROSTATE_sampled_K4 (2,564 vóxeles, 36 ángulos candidatos)
Ángulos   : [0°, 100°, 130°, 220°]
f         : 8,329
PTV D95   : 62.29 Gy  (meta: 64.60 Gy)
PTV V95%  : 88.6%
Bladder Dmax : 70.05 Gy  (límite: 50 Gy — violación en punto)
Rectum Dmax  : 60.40 Gy  (límite: 50 Gy — violación en punto)
Tiempo    : 31:44 min
```

### Correcciones aplicadas en esta sesión

1. `max_intensity`: corregido de 10.0 → 15000.0 en todas las instancias PROSTATE reales
2. `w_ptv_over 0.5`: añadido a instance_config.txt de PROSTATE_36ang y derivadas
3. Bug `BOUND` en target-runner: eliminado `BOUND=$1; shift` que consumía `--inner_explore`
4. Bug outer_iter en emilibase.cpp: corregido en sesión anterior (commit existente)
5. Instancias obsoletas eliminadas (PROSTATE_emili, TINY_test, irace_bao, K3/K5)

### Próximos pasos

1. **Re-ejecutar irace** con `max_intensity=15000` corregido (pendiente, ~14h)
2. **Validar configuración ganadora** en `instances/PROSTATE_36ang` (~5.5h)
3. **Explorar VNS/Tabu en nivel BAO** si se desea mejorar calidad de solución

### Flujo de trabajo recomendado

```
1. Desarrollo rápido   →  MEDIUM_test         (~20s/run)
2. Calibración irace   →  PROSTATE_sampled_K4 (~32-64 min/run)
3. Validación clínica  →  PROSTATE_36ang      (~2.5-5.5h/run)
```
