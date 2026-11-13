#!/usr/bin/env bash
set -euo pipefail
mkdir -p test-results .tools
exec > >(tee test-results/verification.log) 2>&1
baseline=806b895a35bed94853a69ae290f7dd024b5d4fd3
git diff --exit-code "$baseline" -- malloclab/mdriver.c malloclab/memlib.c malloclab/config.h malloclab/Makefile malloclab/mm.h malloclab/short1-bal.rep malloclab/short2-bal.rep
make -C malloclab clean
make -C malloclab
for trace in short1 short2; do
  (cd malloclab && timeout 180 ./mdriver -V -f "$trace-bal.rep") | tee "test-results/$trace.log"
  grep -q 'Perf index' "test-results/$trace.log"
  if grep -Eq 'ERROR|Terminated with' "test-results/$trace.log"; then exit 1; fi
done
for bits in 32 64; do
  cc -m"$bits" -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -O2 -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Imalloclab \
    tests/malloclab/stress.c malloclab/mm.c malloclab/memlib.c -o ".tools/stress-$bits"
  ASAN_OPTIONS=detect_leaks=1 timeout 300 ".tools/stress-$bits" | tee "test-results/stress-$bits.log"
  grep -q MALLOCLAB_STRESS_OK "test-results/stress-$bits.log"
done
lscpu > test-results/cpu.txt
