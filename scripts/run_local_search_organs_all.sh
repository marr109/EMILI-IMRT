#!/usr/bin/env bash
set -euo pipefail

# Runs the 24 local-search conditions (step x catalog x order x strategy) with the
# per-organ model. Usage: scripts/run_local_search_organs_all.sh [parallel_jobs]
# Each job is one condition (15 seeds, sequential). Each AMPL/Gurobi session needs a
# license seat: check how many concurrent sessions your license allows.
JOBS="${1:-1}"
for step in 5 10; do for catalog in restricted unrestricted; do
for order in fixed circular rand; do for strategy in first best; do
  echo "$step $strategy $catalog $order"
done; done; done; done | xargs -P "$JOBS" -L 1 scripts/run_local_search_organs.sh
