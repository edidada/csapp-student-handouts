#!/usr/bin/env bash
set -euo pipefail
mkdir -p test-results
exec > >(tee test-results/verification.log) 2>&1
printf 'commit=%s\n' "$(git rev-parse HEAD)"
make -C shlab CFLAGS='-Wall -Wextra -O2 -g'
git diff --exit-code 806b895a35bed94853a69ae290f7dd024b5d4fd3 -- shlab/sdriver.pl shlab/tshref shlab/trace*.txt
python3 - <<'PY'
from pathlib import Path
import subprocess,re,json,os,signal,difflib
root=Path.cwd();results=[]
def normalize(output):
    return re.sub(r'\(\d+\)','(PID)',output).rstrip()
for n in range(1,17):
    outputs={}
    for shell in ['tshref','tsh']:
        result=subprocess.run(['perl','./sdriver.pl','-t',f'trace{n:02}.txt','-s','./'+shell,'-a','-p'],cwd='shlab',capture_output=True,text=True,timeout=60)
        path=root/'test-results'/f'trace{n:02}-{shell}.txt';path.write_text(result.stdout+result.stderr)
        assert result.returncode==0,(n,shell,result.returncode,result.stdout,result.stderr)
        outputs[shell]=normalize(result.stdout)
        # Terminate only process groups identified in this trace's job output.
        for pid in set(re.findall(r'^\[\d+\] \((\d+)\)|^Job \[\d+\] \((\d+)\)',result.stdout,re.M)):
            value=int(pid[0] or pid[1])
            try: os.killpg(value,signal.SIGKILL)
            except ProcessLookupError: pass
    difference='\n'.join(difflib.unified_diff(outputs['tshref'].splitlines(),outputs['tsh'].splitlines(),fromfile='reference',tofile='actual'))
    (root/'test-results'/f'trace{n:02}.diff').write_text(difference)
    results.append({'trace':n,'passed':not difference})
    print(f'TRACE {n:02} '+('PASS' if not difference else 'FAIL'),flush=True)
    if difference: print(difference,flush=True)
(root/'test-results/traces.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(r['passed'] for r in results),results
print('ORIGINAL_TRACES_OK count=16')
PY
