#!/usr/bin/env bash
set -euo pipefail
echo "=== IRACE ILS — BAO MEDIUM_test K=4 ==="
echo "Started: $(date)"
chmod +x /project/irace_prostate/target-runner
mkdir -p /project/irace_prostate/results
cd /project
apt-get update -qq && apt-get install -y -qq libuv1-dev
R -e 'install.packages(c("fs","irace"), repos="https://cloud.r-project.org")'
R -e 'library(irace); scenario <- readScenario("/project/irace_prostate/scenario.irace"); irace(scenario)' 2>&1 | tee /project/irace_prostate/results/irace_output.txt
echo "=== IRACE ILS DONE: $(date) ==="
