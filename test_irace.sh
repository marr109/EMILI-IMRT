#!/usr/bin/env bash
# Quick test: list irace functions
apt-get update -qq && apt-get install -y -qq libuv1-dev
R -e 'install.packages(c("fs","irace"), repos="https://cloud.r-project.org")'
R -e 'library(irace); cat(ls("package:irace"), sep="\n")'
