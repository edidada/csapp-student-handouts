#!/usr/bin/env bash
set -euo pipefail
mkdir -p test-results .tools
exec > >(tee test-results/verification.log) 2>&1
baseline=806b895a35bed94853a69ae290f7dd024b5d4fd3
git diff --exit-code "$baseline" -- proxylab/driver.sh proxylab/tiny proxylab/csapp.c proxylab/csapp.h proxylab/Makefile
make -C proxylab clean
make -C proxylab
make -C proxylab/tiny clean
make -C proxylab/tiny
chmod +x proxylab/nop-server.py
# Ensure curl cannot silently bypass localhost via the runner's NO_PROXY.
(cd proxylab && env -u NO_PROXY -u no_proxy timeout 180 bash driver.sh) | tee test-results/original-driver.log
grep -q 'basicScore: 40/40' test-results/original-driver.log
grep -q 'concurrencyScore: 15/15' test-results/original-driver.log
grep -q 'cacheScore: 15/15' test-results/original-driver.log
grep -q 'totalScore: 70/70' test-results/original-driver.log
for mode in normal sanitized; do
  if [ "$mode" = sanitized ]; then
    cc -std=gnu11 -Wall -O1 -g -fsanitize=address,undefined \
      -fno-omit-frame-pointer -pthread -Iproxylab -c proxylab/csapp.c -o .tools/csapp-sanitized.o
    cc -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
      -fno-omit-frame-pointer -pthread -Iproxylab proxylab/proxy.c .tools/csapp-sanitized.o -o .tools/proxy-sanitized
    binary="$PWD/.tools/proxy-sanitized"
  else binary="$PWD/proxylab/proxy"; fi
  PROXY_BIN="$binary" PROXY_MODE="$mode" bash scripts/check_proxylab.sh | tee "test-results/extra-$mode.log"
done
