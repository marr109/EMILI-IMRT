#!/usr/bin/env bash
set -euo pipefail
echo "=== IRACE VNS — BAO PROSTATE K=4 ==="
echo "Started: $(date)"
chmod +x /project/irace_vns/target-runner
mkdir -p /project/irace_vns/results
cd /project
apt-get update -qq && apt-get install -y -qq libuv1-dev
R -e 'install.packages(c("fs","irace"), repos="https://cloud.r-project.org")'
R -e 'library(irace); scenario <- readScenario("/project/irace_vns/scenario.irace"); irace(scenario)' 2>&1 | tee /project/irace_vns/results/irace_output.txt
echo "=== IRACE VNS DONE: $(date) ==="
