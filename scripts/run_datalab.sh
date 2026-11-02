#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../datalab"
mkdir -p results
chmod +x dlc driver.pl
./dlc -e bits.c 2>&1 | tee results/dlc.log
make clean
make btest
./btest 2>&1 | tee results/btest.log
./driver.pl 2>&1 | tee results/driver.log
python3 - <<'PY'
import re
from pathlib import Path
btest=Path('results/btest.log').read_text()
driver=Path('results/driver.log').read_text()
for text,pattern in [(btest,r'Total points:\s*(\d+)/(\d+)'),(driver,r'Score = (\d+)/(\d+)')]:
    match=re.search(pattern,text)
    assert match and match[1]==match[2] and int(match[1])>0,text
print('Official legality, correctness, and operator-budget score gates passed')
PY
