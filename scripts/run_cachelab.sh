#!/usr/bin/env bash
set -euo pipefail
cd cachelab
chmod +x csim-ref test-csim
make clean
make
python3 driver.py -A | tee cachelab-grade.log
python3 - <<'PYCODE'
from pathlib import Path
import re
text=Path('cachelab-grade.log').read_text()
m=re.search(r'AUTORESULT_STRING=([0-9.]+):(\d+):(\d+):(\d+)',text)
assert m, text
assert float(m[1]) == 53, m.groups()
assert all(int(m[i]) <= limit for i,limit in [(2,300),(3,1300),(4,2000)]), m.groups()
print('Full official correctness and miss-budget score passed')
PYCODE
