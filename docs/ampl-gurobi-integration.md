# Integración AMPL + Gurobi

Reemplaza la implementación exacta de OSQP que resolvía el FMO (`imrt/imrt_fmo.cpp`)
en `develop`. Esta rama (`feat/ampl-gurobi-solver`) resuelve el mismo problema QP
llamando a AMPL + Gurobi desde C++, y trabaja sobre una instancia real distinta
(`instances/CERR_Prostate`) en vez de los datos CORT usados hasta ahora.

## 1. Cómo se integró AMPL

### La API C++ real de AMPL, no un subprocess artesanal

AMPL provee una API C++ oficial. En vez de descargarla aparte, se encontró
**vendorizada dentro del paquete pip `amplpy`** (que la envuelve para exponerla en
Python): `amplpy/amplpython/cppinterface/` — headers completos más
`libampl.dylib`, un binario universal (x86_64 + arm64), 1.1MB.

Se copió a `third_party/ampl_cppapi/` (headers + lib), y `CMakeLists.txt` la
linkea directo al binario `emili`:

```cmake
set(EMILI_AMPL_CPPAPI_DIR ${CMAKE_SOURCE_DIR}/third_party/ampl_cppapi)
target_include_directories(${PROJECT_NAME} PRIVATE ${EMILI_AMPL_CPPAPI_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE ${EMILI_AMPL_CPPAPI_DIR}/lib/libampl.dylib)
target_compile_definitions(${PROJECT_NAME} PRIVATE EMILI_REPO_ROOT="${CMAKE_SOURCE_DIR}")
set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${EMILI_AMPL_CPPAPI_DIR}/lib"
    INSTALL_RPATH "${EMILI_AMPL_CPPAPI_DIR}/lib")
```

`libampl.dylib` internamente administra un subprocess `ampl` persistente (así
funciona la API de AMPL en cualquier lenguaje) — pero es la API oficial, con
protocolo propio, reutilizando la misma sesión entre llamadas. No es un
`popen()` armado a mano.

### Qué SÍ se vendoriza y qué NO

| Componente | Tamaño | ¿En git? | Por qué |
|---|---|---|---|
| `third_party/ampl_cppapi/` (headers + `libampl.dylib`) | 1.1MB | Sí | Estable, chico, hace falta para compilar en cualquier máquina |
| `ampl_gurobi/.venv/.../ampl_module_base/bin/` (binario `ampl`, licencia) | ~19MB | No (gitignored) | "Instalación" local, como antes se pedía `brew install osqp` |
| `ampl_gurobi/.venv/.../ampl_module_gurobi/bin/` (binario `gurobi`) | ~37MB | No (gitignored) | Ídem |

O sea: para *compilar* esta rama en otra máquina alcanza con clonar el repo. Para
*correr* el binario (que el FMO resuelva de verdad y no caiga al sentinel de
error) hace falta además el entorno `ampl_gurobi/.venv` (ver `ampl_gurobi/README`
o la sección de Prerequisites del `README.md` principal).

### Resolución de rutas en runtime

`ImrtFmoSolver::initAmpl()` (`imrt/imrt_fmo.cpp`) resuelve dos rutas vía
variables de entorno, con default relativo a la raíz del repo (inyectada en
compile-time como `EMILI_REPO_ROOT`):

```cpp
std::string bin_dir = envOr("EMILI_AMPL_BIN_DIR",
    repoPath("ampl_gurobi/.venv/lib/python3.9/site-packages/ampl_module_base/bin"));
std::string gurobi_bin = envOr("EMILI_GUROBI_BIN",
    repoPath("ampl_gurobi/.venv/lib/python3.9/site-packages/ampl_module_gurobi/bin/gurobi"));
```

Si el venv no existe, el binario igual compila y corre — pero cada llamada a
`ImrtFmoSolver::solve()` va a fallar (ver sección 5, comportamiento de error).

### Sesión persistente, no una por solve

`ImrtFmoSolver` guarda un `std::unique_ptr<ampl::AMPL>` como miembro, construido
**una sola vez** en el constructor (lee `ampl_gurobi/fmo.mod` una vez ahí). BAO
llama a `solve()` potencialmente miles de veces por corrida — reabrir sesión y
releer el modelo en cada llamada hubiera sido brutalmente caro. Entre llamadas
solo se actualizan los datos que cambian (ángulos activos, matriz de dosis
dispersa), no el modelo ni el entorno.

### La licencia es un "short-term lease" — hay que saberlo

La licencia AMPL Community Edition usada acá renueva cada **60–300 segundos**
contra el servidor de AMPL (`portal.ampl.com` y afines). Si en ese momento falla
la resolución DNS (cortó el hotspot, wifi inestable, lo que sea), la licencia
queda inválida hasta reactivarla a mano:

```bash
ampl_gurobi/.venv/bin/python -m amplpy.modules activate <uuid>
```

Esto pasó varias veces durante las corridas largas de esta rama (documentado en
memoria del proyecto). Para corridas de más de unos minutos conviene un wrapper
con reintento automático (ver `experiments/local_search/nangshift10/unrestricted/` — el
script de esa corrida ya lo tiene).

## 2. Cómo funciona Gurobi acá

- **Versión confirmada en uso: Gurobi 13.0.2.**
- No se llama directo a la librería C de Gurobi — AMPL lo invoca como *solver
  driver* externo (`ampl.setOption("solver", "<ruta absoluta al binario gurobi>")`),
  el patrón estándar de AMPL para cualquier solver.
- El binario driver (`ampl_module_gurobi/bin/gurobi`) resuelve el problema NL que
  AMPL le pasa y devuelve la solución por el mismo canal.
- La ruta se pasa **absoluta**, no por `PATH` — se verificó explícitamente que
  funciona incluso con el entorno del proceso completamente vacío (`env -i`).
- Otros solvers quedaron instalados en el mismo venv (`highs`, `xpress`,
  `cplex`) por si se quiere comparar — no se usan por default, solo Gurobi.

## 3. La instancia nueva: `instances/CERR_Prostate`

### No es CORT — es un export crudo de CERR

A diferencia de las instancias `PROSTATE_*` (formato CORT, con
`instance_config.txt` y VOILists, ya no soportado en esta rama — ver más abajo),
`CERR_Prostate` es la exportación *cruda* del sistema CERR, sin ningún paso de
conversión hacia el formato que usaba el pipeline viejo:

| | |
|---|---|
| Tamaño en disco | ~11GB |
| Archivos | 2173 |
| Ángulos candidatos | 360 (0°–359°, resolución de 1°) |
| Beamlets totales | 23971 |
| Beamlets por ángulo | **variable** (69–72 típico, no fijo) |
| Órganos | `PTVHD` (2518 voxeles), `PTVLD` (2595), `BLADDER` (3639), `RECTUM` (1894) |

Formato de archivos (documentado también en `imrt/cerr_instance.h`):

```
<ORGANO>.txt              lista de ids de vóxel global; orden de línea = índice local del boxet
<ORGANO>_<angle_idx>.txt  filas: id_vóxel_global  id_beamlet_local(1-based)  dose_rate
beamletIndex.txt          filas: angle_idx  global_start  global_end (1-based, inclusive)
```

### Por qué no se pudo reusar el loader viejo (`ImrtInstance`)

`ImrtInstance` asumía una cantidad **fija** de beamlets por ángulo
(`n_dimlets_per_angle`, usado en `angle_idx * n_dimlets_per_angle + local` para
el índice global). CERR_Prostate no cumple eso — confirmado con datos reales
(`beamsInfo.txt`: ángulo 0→70, ángulo 4→71, ángulo 8→69, ...). Forzarlo hubiera
significado misrepresentar la geometría real de beamlets.

Se optó por una estructura nueva en paralelo (`CerrFmoSource`,
`imrt/cerr_instance.h/.cpp`) detrás de una interfaz común (`IFmoDataSource`,
`imrt/imrt_fmo_source.h`) que tanto `ImrtFmoSolver` como el problema `imrt`
clásico consumen — sin tocar `ImrtInstance`. Una vez que nada más dependía de
`ImrtInstance`, se borró junto con todo el soporte CORT (decisión explícita del
usuario: esta rama es 100% CERR).

### Carga perezosa (lazy), no todo en memoria

11GB no entran cómodos de una. `CerrFmoSource` parsea cada ángulo **la primera
vez que se pide** (`ensureAngleLoaded`), y cachea el resultado en memoria sin
límite ni expiración — BAO revisita ángulos entre iteraciones, así que cachear
evita reparsear disco. Una corrida que explora amplio (búsqueda local completa,
sin restringir el catálogo de 360 ángulos — requisito explícito para esta tesis)
puede terminar cacheando buena parte del dataset.

## 4. Función objetivo y `ampl_gurobi/fmo.mod`

### La formulación (idéntica a la que usaba OSQP — solo cambió el solver)

Dado un conjunto fijo de K ángulos activos:

```
min   w_under · Σ u_b²  +  w_over · Σ v_b²  +  w_ptv_over · Σ w_b²

s.t.  D_ptv · x + u  ≥  Dmin          (piso de dosis PTV, u absorbe el déficit)
      D_oar · x − v  ≤  Dmax          (techo de dosis OAR, v absorbe el exceso)
      D_ptv · x − w  ≤  1.07·Dmin     (techo de sobredosis PTV, si w_ptv_over > 0)
      u, v, w  ≥  0
      0  ≤  x_j  ≤  max_intensity
```

- `x` = intensidad de cada beamlet activo (K × beamlets_por_ángulo variables).
- `u` = slack de subdosis por boxet PTV (uno por vóxel de PTVHD/PTVLD).
- `v` = slack de sobredosis por boxet OAR (uno por vóxel de BLADDER/RECTUM).
- `w` = slack de sobredosis PTV (hot-spot), solo penalizado si `w_ptv_over > 0`.

### El `.mod` (`ampl_gurobi/fmo.mod`) — única fuente de verdad

El modelo AMPL se lee **una sola vez** desde archivo (no está duplicado como
string embebido en C++) — tanto el driver Python de referencia como
`ImrtFmoSolver::initAmpl()` apuntan al mismo `ampl_gurobi/fmo.mod`.

```ampl
param n_ptv integer >= 0;
param n_oar integer >= 0;

set DIMLETS;
set PTV_B := 0 .. n_ptv - 1;
set OAR_B := 0 .. n_oar - 1;

param max_intensity > 0;
param w_under >= 0;
param w_over  >= 0;
param w_ptv_over >= 0 default 0;

param dmin {PTV_B} >= 0;
param dmax {OAR_B} >= 0;
param dmax_ptv {PTV_B} >= 0 default Infinity;

set PTV_DOSE within {PTV_B, DIMLETS};
set OAR_DOSE within {OAR_B, DIMLETS};
param d_ptv {PTV_DOSE} >= 0;
param d_oar {OAR_DOSE} >= 0;

var x {DIMLETS} >= 0, <= max_intensity;
var u {PTV_B} >= 0;
var v {OAR_B} >= 0;
var w {PTV_B} >= 0;

minimize fmo_objective:
    w_under    * sum {b in PTV_B} u[b]^2
  + w_over     * sum {b in OAR_B} v[b]^2
  + w_ptv_over * sum {b in PTV_B} w[b]^2;

subject to ptv_floor {b in PTV_B}:
    sum {(b,j) in PTV_DOSE} d_ptv[b,j] * x[j] + u[b] >= dmin[b];

subject to oar_ceiling {b in OAR_B}:
    sum {(b,j) in OAR_DOSE} d_oar[b,j] * x[j] - v[b] <= dmax[b];

subject to ptv_ceiling {b in PTV_B}:
    sum {(b,j) in PTV_DOSE} d_ptv[b,j] * x[j] - w[b] <= dmax_ptv[b];
```

**Truco de simplificación deliberado:** en vez de activar/desactivar la
restricción `ptv_ceiling` condicionalmente según `w_ptv_over > 0` (como hacía la
versión OSQP, que directamente omitía las filas), acá `w` y su restricción
**siempre** están presentes. Cuando `w_ptv_over = 0` y `dmax_ptv = Infinity`
(el caso actual, ver más abajo), la restricción queda no-binding y `w` no cuesta
nada en el objetivo, así que Gurobi lo lleva a 0 sin esfuerzo — mismo
comportamiento que "desactivado", sin lógica condicional en el `.mod`.

### Sets dispersos, no matrices densas

`PTV_DOSE`/`OAR_DOSE` son sets de tuplas `(boxet, dimlet)` — solo los pares con
dosis no-nula. Con miles de boxets y solo K×beamlets_por_ángulo dimlets activos
a la vez, una matriz densa sería inmanejable; el `.mod` está armado para
sparsity desde el diseño.

### Por qué el objetivo NO cambió (y no se va a cambiar)

Existe una decisión de investigación explícita y previa: la penalización
cuadrática (no gEUD, no IRLS) es la formulación definitiva de este proyecto.
Esta integración cambia el **solver** (OSQP → AMPL/Gurobi), no la
**formulación** — el `.mod` es matemáticamente la misma función objetivo y las
mismas restricciones que ya usaba `imrt_fmo.cpp` con OSQP.

### ⚠️ Valores clínicos: placeholder, no validados

`CerrFmoSource` (constructor, `imrt/cerr_instance.cpp`) hardcodea:

| Parámetro | Valor | Fuente |
|---|---|---|
| Dmin PTVHD | 65.0 Gy | placeholder |
| Dmin PTVLD | 65.0 Gy | placeholder |
| Dmax BLADDER | 50.0 Gy | placeholder |
| Dmax RECTUM | 50.0 Gy | placeholder |
| w_under | 1.0 | placeholder |
| w_over | 0.5 | placeholder |
| w_ptv_over | 0.0 (desactivado) | placeholder |
| max_intensity | 15000.0 | placeholder |

No existe en el repo ningún `instance_config.txt` ni plan de tratamiento CERR
real que documente la prescripción de este paciente — `MANUAL.md` (rama vieja)
solo documentaba el dataset CORT (`PROSTATE_36ang`, un único PTV a 68.0 Gy), que
es un paciente distinto. El usuario autorizó explícitamente estos valores para
validar el pipeline mecánicamente. **Reemplazar antes de sacar cualquier
conclusión clínica.**

## 5. Comportamiento cuando algo falla

`ImrtFmoSolver::solve()` nunca deja una excepción sin capturar — ante cualquier
falla (venv no configurado, licencia vencida, lo que sea), loguea a `stderr` con
prefijo `[FMO] ERROR:` y devuelve el sentinel `{x = vector de ceros, f = 1e30}`
(el mismo valor centinela "peor caso" que usaba el código OSQP para sus propios
fallos). BAO sigue corriendo sin crashear, pero cualquier resultado con
`f = 1e30` es una señal inequívoca de que el FMO no resolvió nada real — no es
un objetivo válido.
