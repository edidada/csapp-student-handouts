#!/usr/bin/env bash
set -euo pipefail
root="$PWD"
mkdir -p .tools test-results
exec > >(tee test-results/verification.log) 2>&1
printf 'commit=%s\n' "$(git rev-parse HEAD)"
tar -xf archlab/sim.tar -C .tools
sim="$root/.tools/sim"
cp archlab/solutions/seq-full.hcl "$sim/seq/"
cp archlab/solutions/pipe-full.hcl archlab/solutions/ncopy.ys "$sim/pipe/"
cp archlab/solutions/{sum,rsum,copy}.ys "$sim/misc/"
# Modern GCC defaults to -fno-common; these old tools have tentative globals.
make -C "$sim/misc" CFLAGS='-Wall -O2 -fcommon' LCFLAGS='-O2 -fcommon'
for kind in seq pipe; do
    make -C "$sim/$kind" VERSION=full GUIMODE= TKLIBS= TKINC= CFLAGS='-Wall -O2 -fcommon'
done
make -C "$sim/y86-code" all
for name in sum rsum copy; do
    "$sim/misc/yas" "$sim/misc/$name.ys"
    "$sim/misc/yis" "$sim/misc/$name.yo" | tee "test-results/part-a-$name.log"
    grep -Eq '%rax:.*0x0000000000000cba' "test-results/part-a-$name.log"
done
for kind in seq pipe; do
    if [[ "$kind" == seq ]]; then target=testssim; binary=ssim; else target=testpsim; binary=psim; fi
    make -C "$sim/y86-code" "$target" | tee "test-results/$kind-benchmarks.log"
    ! grep -qi 'ISA Check.*Fail' "test-results/$kind-benchmarks.log"
    for flag in base immediate; do
        options=(); [[ "$flag" == immediate ]] && options+=(TFLAGS=-i)
        (cd "$sim/ptest" && make "SIM=../$kind/$binary" "${options[@]}") | tee "test-results/$kind-ptest-$flag.log"
        ! grep -qi 'Checks Failed\|test .* failed' "test-results/$kind-ptest-$flag.log"
        [[ $(grep -c 'ISA Checks Succeed' "test-results/$kind-ptest-$flag.log") == 4 ]]
    done
done
(cd "$sim/pipe" && ./check-len.pl < sdriver.yo) | tee test-results/ncopy-length.log
for mode in isa pipe; do
    options=(); [[ "$mode" == pipe ]] && options+=(-p)
    (cd "$sim/pipe" && ./correctness.pl "${options[@]}") | tee "test-results/ncopy-$mode.log"
done
(cd "$sim/pipe" && ./benchmark.pl) | tee test-results/ncopy-performance.log
python3 - <<'PY'
from pathlib import Path
import re,json
r=Path('test-results')
for mode in ['isa','pipe']:
    m=re.search(r'(\d+)/(\d+) pass correctness test',(r/f'ncopy-{mode}.log').read_text());assert m and m[1]==m[2],mode
m=re.search(r'ncopy length = (\d+) bytes',(r/'ncopy-length.log').read_text());assert m and int(m[1])<=1000
cpe=float(re.search(r'Average CPE\s+([\d.]+)',(r/'ncopy-performance.log').read_text())[1])
(r/'summary.json').write_text(json.dumps({'ncopy_bytes':int(m[1]),'average_cpe':cpe,'performance_goal_below_9':cpe<9},indent=2)+'\n')
print('ARCHLAB_CORRECTNESS_OK average_cpe=',cpe)
PY
