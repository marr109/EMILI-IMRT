#!/usr/bin/env bash
apt-get update -qq && apt-get install -y -qq libuv1-dev
R -e 'install.packages(c("fs","irace"), repos="https://cloud.r-project.org")'
echo "IRACE OK"
