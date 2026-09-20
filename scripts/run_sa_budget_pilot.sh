#!/usr/bin/env bash
set -uo pipefail

# Uso: scripts/run_sa_budget_pilot.sh [segundos] [paralelismo] [temperatura]
#   por defecto: 1080 s (18 min), 4 simultaneas, T = 1000
#
# MUESTRA PILOTO de recocido simulado sobre BAO con evaluacion exacta del FMO.
#
# Que responde y que NO responde
# ------------------------------
# Responde: si SA es funcional sobre este problema y donde queda su objetivo
# final frente a ILS bajo el mismo presupuesto de 18 minutos (15 semillas, la
# misma cantidad que las condiciones de ILS, para que las distribuciones sean
# comparables).
#
# NO responde cual es la mejor temperatura. Barrer temperaturas a mano seria
# hacer a mano lo que irace hace sistematicamente, y el informe ya declara a
# irace como el mecanismo de calibracion. La calibracion de T, del paso del
# vecindario y del resto de los hiperparametros queda explicitamente fuera.
#
# De donde sale T = 1000
# ----------------------
# Metropolis acepta un empeoramiento con probabilidad exp(-delta/T), asi que T
# solo tiene sentido en la escala de los delta del problema. Midiendo los 7387
# empeoramientos observados en las 120 trayectorias de la matriz de presupuesto
# (experiments/ils/*/**-budget1080s/):
#
#     p25 = 360    mediana = 901    p75 = 1919    media = 1427
#
# lo que da estas tasas de aceptacion:
#
#     T =   100  ->  6,8 %      T =  2500  ->  64,8 %
#     T =   500  -> 27,7 %      T =  5000  ->  78,3 %
#     T =  1000  -> 43,0 %      T = 25000  ->  94,6 %
#
# T = 1000 deja la aceptacion cerca del equilibrio entre aceptar y rechazar, que
# es el regimen razonable para una temperatura CONSTANTE. Nota: esta Metropolis
# no enfria, asi que la tasa se mantiene durante toda la corrida; no es SA
# clasico con esquema de enfriamiento, y conviene reportarlo con ese nombre.

BUDGET="${1:-1080}"
PAR="${2:-4}"
TEMP="${3:-1000}"
STEP="${STEP:-5}"
CATALOG="${CATALOG:-restricted}"
SEEDS="${SEEDS:-15}"

if [ -z "${EMILI_AMPL_BIN_DIR:-}" ] || [ -z "${EMILI_GUROBI_BIN:-}" ]; then
  SP="$(echo ampl_gurobi/.venv/lib/python*/site-packages)"
  [ -d "$SP/ampl_module_base" ] || { echo "No se encontro ampl_module_base bajo $SP" >&2; exit 1; }
  export EMILI_AMPL_BIN_DIR="$PWD/$SP/ampl_module_base/bin"
  export EMILI_GUROBI_BIN="$PWD/$SP/ampl_module_gurobi/bin/gurobi"
fi
export EMILI_GUROBI_OPTIONS="${EMILI_GUROBI_OPTIONS:-threads=4}"

PY="ampl_gurobi/.venv/bin/python3"
CAT_TOKEN=""; [ "$CATALOG" = "unrestricted" ] && CAT_TOKEN="unrestricted"
OUT="experiments/sa/nangshift${STEP}/${CATALOG}-budget${BUDGET}s-T${TEMP}/data"
MASTER="experiments/sa/pilot_T${TEMP}_budget${BUDGET}s.log"
mkdir -p "$OUT" "$(dirname "$MASTER")"

log() { echo "[$(date '+%F %H:%M:%S')] $*" | tee -a "$MASTER"; }

run_one() {
  local seed="$1"
  local dir="${OUT}/seed${seed}"
  local traj="${dir}/trajectory.csv" rlog="${dir}/run.log"
  mkdir -p "$dir"

  # El corte por presupuesto sale por finalise()/exit(0) y saltea el bloque de
  # main() que imprime "Found solution"; el reporte clinico (atexit) es lo unico
  # presente en los dos caminos.
  if [ -f "$rlog" ] && grep -q "Conformity Index" "$rlog" 2>/dev/null; then
    return 0
  fi

  # tmaxiter inalcanzable: el unico criterio de corte es -it, igual que en la
  # matriz de ILS, para que ambos compitan por el mismo recurso.
  # -it va ANTES de rnds: getTime() usa checkToken() sobre la posicion actual
  # del cursor mientras rnds usa seek(), asi que al final se ignora en silencio.
  ./build/emili instances/CERR_Prostate baoimrt 4 csv "$traj" \
    sa irandomk $CAT_TOKEN \
    tmaxiter 1000000 \
    nangshift "$STEP" \
    metropolis "$TEMP" \
    -it "$BUDGET" \
    rnds "$seed" \
    > "$rlog" 2>&1

  if grep -q "Conformity Index" "$rlog" 2>/dev/null; then
    "$PY" scripts/plot_trajectory.py --csv "$traj" --skip-trajectory \
      --improvements-csv "${dir}/trajectory_improvements.csv" \
      --convergence-output "${dir}/convergence.png" \
      --iterations-output "${dir}/iterations.png" >> "$rlog" 2>&1 || true
  else
    echo "WARNING seed${seed}: sin marca de corrida completa" >> "$MASTER"
  fi
}
export -f run_one
export BUDGET TEMP STEP CATALOG CAT_TOKEN OUT PY MASTER \
       EMILI_AMPL_BIN_DIR EMILI_GUROBI_BIN EMILI_GUROBI_OPTIONS

log "PILOTO SA START  T=${TEMP}  paso=${STEP}  catalogo=${CATALOG}  presupuesto=${BUDGET}s  semillas=${SEEDS}  paralelismo=${PAR}"

DONE=0
for seed in $(seq -w 1 "$SEEDS"); do
  run_one "$seed" &
  while [ "$(jobs -rp | wc -l)" -ge "$PAR" ]; do sleep 5; done
  DONE=$((DONE + 1))
  [ $((DONE % PAR)) -eq 0 ] && log "lanzadas ${DONE}/${SEEDS}"
done
wait

COMP=$(find "$OUT" -name run.log -exec grep -l "Conformity Index" {} \; 2>/dev/null | wc -l)
log "PILOTO SA COMPLETE  corridas completas: ${COMP}/${SEEDS}"
