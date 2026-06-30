#!/usr/bin/env bash
set -euo pipefail
echo "=== IRACE BAO PROSTATE K=4 ==="
echo "Started: $(date)"
apt-get update -qq && apt-get install -y -qq libuv1-dev
R -e 'install.packages(c("fs","irace"), repos="https://cloud.r-project.org")'
mkdir -p /project/irace_prostate/results
chmod +x /project/irace_prostate/target-runner-docker
cd /project
R -e 'library(irace); scenario <- readScenario("/project/irace_prostate/scenario-docker.irace"); irace(scenario)' 2>&1 | tee /project/irace_prostate/results/irace_output.txt
echo "=== IRACE DONE: $(date) ==="
