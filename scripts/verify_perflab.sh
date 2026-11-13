#!/usr/bin/env bash
set -euo pipefail
mkdir -p test-results .tools
exec > >(tee test-results/verification.log) 2>&1
printf 'commit=%s\n' "$(git rev-parse HEAD)"
git diff --exit-code 806b895a35bed94853a69ae290f7dd024b5d4fd3 -- perflab/driver.c perflab/config.h perflab/defs.h perflab/fcyc.c perflab/clock.c perflab/Makefile
# Retain official IA32 cycle-counter path, including original -m32 build.
make -C perflab
cc -std=c11 -Wall -Wextra -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iperflab tests/perflab/edges.c perflab/kernels.c -o .tools/perflab-edges
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .tools/perflab-edges | tee test-results/edges.log
lscpu > test-results/cpu.txt
for seed in 17 42 99; do
    (cd perflab && timeout 180 ./driver -s "$seed") | tee "test-results/driver-$seed.log"
    ! grep -qi 'failed correctness\|Error:.*counter' "test-results/driver-$seed.log"
done
(cd perflab && timeout 180 ./driver -g -s 1234) | tee test-results/autograder.log
! grep -qi 'failed correctness' test-results/autograder.log
python3 - <<'PY'
from pathlib import Path
import re,math,json,statistics
runs=[]
assert re.search(r'bestscores:[0-9.]+:[0-9.]+:',Path('test-results/autograder.log').read_text())
for seed in [17,42,99]:
    text=Path(f'test-results/driver-{seed}.log').read_text()
    sections=re.split(r'(?=(?:Rotate|Smooth): Version = )',text)
    tables={}
    for section in sections:
        m=re.match(r'(Rotate|Smooth): Version = ([^:]+):',section)
        if not m:continue
        cpe=re.search(r'Your CPEs\s+([\d.\s]+)\n',section)
        assert cpe,section
        values=[float(v) for v in cpe[1].split()];assert len(values)==5 and min(values)>0
        tables[m[2]]=values
    result={'seed':seed,'cpe':tables,'same_runner_geometric_speedup':{}}
    for kernel in ['rotate','smooth']:
        baseline=tables['naive_'+kernel];optimized=tables[kernel]
        result['same_runner_geometric_speedup'][kernel]=math.prod(a/b for a,b in zip(baseline,optimized))**.2
    runs.append(result)
median={k:statistics.median(r['same_runner_geometric_speedup'][k] for r in runs) for k in ['rotate','smooth']}
Path('test-results/performance.json').write_text(json.dumps({'runs':runs,'median_same_runner_speedup':median,'historical_config_unchanged':True},indent=2)+'\n')
assert min(median.values())>1,median
assert 'PERFLAB_EDGES_OK' in Path('test-results/edges.log').read_text()
print('PERFLAB_VERIFIED median_same_runner_speedup=',median)
PY
