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

# Synthetic workloads are explicitly distinct from absent official full traces.
python3 - <<'PY'
from pathlib import Path
import random
out=Path('test-results/traces'); out.mkdir()
for kind in ['small','mixed','realloc']:
 for seed in [17,99]:
  r=random.Random(seed); live={}; ops=[]; slots=256
  def amount(): return r.randrange(1,129 if kind=='small' else 8193)
  for i in range(slots):
   live[i]=amount(); ops.append(f'a {i} {live[i]}')
  for k in range(6000):
   i=r.randrange(slots)
   if i not in live:
    live[i]=amount(); ops.append(f'a {i} {live[i]}')
   elif kind=='realloc' and r.random()<.8:
    live[i]=amount(); ops.append(f'r {i} {live[i]}')
   else:
    del live[i]; ops.append(f'f {i}')
  for i in live: ops.append(f'f {i}')
  (out/f'{kind}-{seed}.rep').write_text(f'0\n{slots}\n{len(ops)}\n1\n'+'\n'.join(ops)+'\n')
PY
for trace in test-results/traces/*.rep; do
  ASAN_OPTIONS=detect_leaks=1 timeout 300 .tools/stress-32 "$trace" | tee "${trace%.rep}-replay.log"
  (cd malloclab && timeout 180 ./mdriver -l -V -f "../$trace") | tee "${trace%.rep}.log"
  grep -q 'Perf index' "${trace%.rep}.log"
  if grep -Eq 'ERROR|Terminated with' "${trace%.rep}.log"; then exit 1; fi
done
python3 - <<'PY'
from pathlib import Path
import re,json
records=[]
for p in sorted(Path('test-results/traces').glob('*.log')):
 if p.stem.endswith('-replay'): continue
 text=p.read_text()
 sections=text.split('Results for mm malloc:')
 assert len(sections)==2,p
 match=re.search(r'^\s*0\s+yes\s+(\d+)%\s+(\d+)\s+([\d.]+)\s+([\d.]+)',sections[1],re.M)
 assert match,p
 records.append(dict(workload=p.stem,utilization_percent=int(match[1]),operations=int(match[2]),seconds=float(match[3]),kops=float(match[4])))
assert len(records)==6
Path('test-results/workloads.json').write_text(json.dumps(dict(scope='Six synthetic workloads, NOT the missing eleven official grading traces',results=records),indent=2)+'\n')
print('MALLOCLAB_WORKLOADS_OK count=6')
PY
