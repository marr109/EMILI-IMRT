#!/usr/bin/env bash
set -euo pipefail
echo "=== IRACE TABU — BAO PROSTATE K=4 ==="
echo "Started: $(date)"
chmod +x /project/irace_tabu/target-runner
mkdir -p /project/irace_tabu/results
cd /project
apt-get update -qq && apt-get install -y -qq libuv1-dev
R -e 'install.packages(c("fs","irace"), repos="https://cloud.r-project.org")'
R -e 'library(irace); scenario <- readScenario("/project/irace_tabu/scenario.irace"); irace(scenario)' 2>&1 | tee /project/irace_tabu/results/irace_output.txt
echo "=== IRACE TABU DONE: $(date) ==="
