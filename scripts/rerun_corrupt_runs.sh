#!/usr/bin/env bash
set -uo pipefail

# Re-ejecuta las corridas cuyo FMO devolvio valores invalidos.
#
# Que paso
# --------
# ImrtFmoSolver::solve() llamaba a ampl_->solve() sin verificar el resultado y
# leia el objetivo inmediatamente despues. Cuando el solve no se completaba
# --Gurobi sin licencia, modelo infactible, solver interrumpido-- AMPL conservaba
# el objetivo de la instancia ANTERIOR y lo devolvia sin senalar nada. Con el
# solver caido por completo el valor emitido era 0, que en un problema de
# minimizacion es el mejor posible: la busqueda lo habria tomado como optimo.
#
# Sintoma en los datos: decenas de configuraciones de angulos distintas con el
# mismo objetivo hasta el decimal, y busquedas que cortaban por falso minimo
# local porque ningun vecino parecia mejorar. La corrida de referencia
# (local_search/nangshift10/unrestricted/best/seed02) evaluo 29 soluciones en
# vez de 101 y reporto 97584,1 donde la misma configuracion vale 97284,6.
#
# Corregido en imrt_fmo.cpp verificando solve_result: un solve incompleto ahora
# lanza excepcion y el vecino se descarta con 1e30 en vez de contaminar la
# trayectoria.
#
# Alcance
# -------
# Afectadas 10 corridas, TODAS anteriores a la matriz de presupuesto de tiempo.
# Los datos de experiments/ils/*/*budget1080s* y de experiments/sa estan limpios:
# 0 de 300 y 0 de 15 respectivamente.
#
# Este script cubre las que sustentan tablas o figuras del informe. Quedan fuera
# tabu/ (fuera del alcance actual) y _legacy/ (historico, no citado).
#
# Requiere el binario corregido en ./build/emili.

BACKUP="experiments/_corrupt_backup_$(date +%Y%m%d)"
DRY="${DRY:-0}"

say() { echo "[$(date '+%F %H:%M:%S')] $*"; }

# Verifica que el binario tenga el fix antes de regenerar nada: re-correr con el
# binario viejo reproduciria exactamente el mismo dato invalido.
# grep -a en vez de strings: strings vive en /usr/bin y no siempre esta en el
# PATH que hereda un screen desatendido, y entonces la tuberia sale vacia y el
# guard rechaza un binario que si tiene el fix.
if ! grep -aq "solve_result=" ./build/emili 2>/dev/null; then
  echo "ERROR: ./build/emili no contiene la verificacion de solve_result." >&2
  echo "       Compilar primero, o instalar build-fix/emili en build/emili." >&2
  exit 1
fi

CORRUPT=(
  "experiments/local_search/nangshift10/unrestricted/data/best/seed02"
  "experiments/ils/nangshift5/restricted/data/first/seed12"
  "experiments/ils/nangshift5/restricted-adaptive/data/first/seed03"
  "experiments/ils/nangshift5/unrestricted-adaptive/data/first/seed03"
  "experiments/ils/nangshift10/restricted-adaptive/data/first/seed03"
  "experiments/ils/nangshift10/unrestricted-adaptive/data/first/seed03"
)

say "respaldando ${#CORRUPT[@]} corridas en ${BACKUP}/"
for d in "${CORRUPT[@]}"; do
  [ -d "$d" ] || { say "  AUSENTE (se omite): $d"; continue; }
  dest="${BACKUP}/${d#experiments/}"
  if [ "$DRY" = "1" ]; then say "  [dry] $d -> $dest"; continue; fi
  mkdir -p "$(dirname "$dest")" && cp -r "$d" "$dest" && rm -rf "$d"
  say "  respaldada y removida: ${d#experiments/}"
done
[ "$DRY" = "1" ] && { say "DRY RUN: no se regenero nada"; exit 0; }

# Los scripts de lote saltean toda corrida que ya tenga su marca de completa, asi
# que al haber removido solo las invalidas regeneran unicamente esas.
say "regenerando ils/nangshift5/restricted/first"
scripts/run_ils_batch.sh 5 first restricted

for step in 5 10; do
  for cat in restricted unrestricted; do
    say "regenerando ils/nangshift${step}/${cat}-adaptive/first"
    scripts/run_ils_adaptive_batch.sh "$step" first "$cat"
  done
done

# local_search/nangshift10/unrestricted/best no tiene script de lote: esa
# condicion se corrio a mano. El comando se reconstruye desde su run.log, que
# registra BEST IMPROVEMENT, irandomk sin catalogo restringido, terminacion por
# minimo local y nangshift 10.
say "regenerando local_search/nangshift10/unrestricted/best/seed02"
D="experiments/local_search/nangshift10/unrestricted/data/best/seed02"
mkdir -p "$D"
./build/emili instances/CERR_Prostate baoimrt 4 csv "$D/trajectory.csv" \
  best irandomk unrestricted locmin nangshift 10 rnds 2 > "$D/run.log" 2>&1

say "COMPLETO. Verificar con la deteccion de objetivos repetidos antes de reanalizar."
