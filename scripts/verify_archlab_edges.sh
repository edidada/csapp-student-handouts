#!/usr/bin/env bash
set -euo pipefail
python3 - <<'PY'
from pathlib import Path
import subprocess,re,json
root=Path.cwd(); tool=root/'.tools/sim/misc'; work=root/'.tools/part-a-edges';work.mkdir()
mask=(1<<64)-1;reports=[]
cases=[[],[0],[7],[-1],[10,176,3072],[-5,0,7,-9],[0x7fffffffffffffff,1,-1]]
for name,fn in [('sum','sum_list'),('rsum','rsum_list'),('copy','copy_block')]:
    text=(root/f'archlab/solutions/{name}.ys').read_text();body=fn+':'+text.split(fn+':',1)[1].split('.pos',1)[0]
    for case,values in enumerate(cases):
        lengths=[len(values)]
        if name=='copy' and values: lengths += [0,-1]
        for length in lengths:
            src='src' if values else '$0';expected=0
            for v in (values if length>0 else []): expected=(expected ^ (v&mask)) if name=='copy' else (expected+v)&mask
            asm=['.pos 0','irmovq $0x1800, %rsp','irmovq $0x12345678, %rbx',f'irmovq {src}, %rdi']
            if name=='copy':asm += ['irmovq dest, %rsi',f'irmovq ${length}, %rdx']
            asm += [f'call {fn}','irmovq $0x800, %r10','rmmovq %rax, 0(%r10)','rmmovq %rsp, 8(%r10)','rmmovq %rbx, 16(%r10)']
            if name=='copy':
                asm += ['irmovq dest, %r11']
                for i in range(len(values)):
                    asm += [f'mrmovq {8*i}(%r11), %r8',f'rmmovq %r8, {32+8*i}(%r10)']
                asm += ['irmovq before, %r11','mrmovq 0(%r11), %r8','rmmovq %r8, 224(%r10)','irmovq after, %r11','mrmovq 0(%r11), %r8','rmmovq %r8, 232(%r10)']
            asm += ['halt','.pos 0x100',body,'.pos 0x400']
            if name=='copy':
                asm += ['src:']+[f'.quad 0x{v&mask:x}' for v in values]+['before: .quad 0x11223344','dest:']+['.quad 0x444' for v in values]+['after: .quad 0x55667788']
            elif values:
                for i,v in enumerate(values):asm += [('src:' if i==0 else f'node{i}:'),f'.quad 0x{v&mask:x}',f'.quad node{i+1}' if i+1<len(values) else '.quad 0']
            path=work/f'{name}-{case}-{length}.ys';path.write_text('\n'.join(asm)+'\n')
            subprocess.run([str(tool/'yas'),str(path)],check=True,capture_output=True)
            result=subprocess.run([str(tool/'yis'),str(path.with_suffix('.yo'))],check=True,capture_output=True,text=True).stdout
            (root/'test-results'/f'edge-{path.stem}.log').write_text(result)
            assert "Status 'HLT'" in result,result
            memory={int(a,16):int(v,16) for a,v in re.findall(r'^0x([0-9a-f]+):\s+0x[0-9a-f]+\s+0x([0-9a-f]+)',result,re.M)}
            assert memory.get(0x800,0)==expected,(path,result,expected)
            assert memory.get(0x808,0)==0x1800,(path,'stack',result)
            assert memory.get(0x810,0)==0x12345678,(path,'callee-saved',result)
            if name=='copy':
                assert memory[0x8e0]==0x11223344 and memory[0x8e8]==0x55667788
                for i,v in enumerate(values):assert memory.get(0x820+8*i,0)==((v&mask) if length>0 else 0x444),(path,i,result)
            reports.append({'program':name,'values':values,'length':length,'passed':True})
Path('test-results/part-a-edges.json').write_text(json.dumps(reports,indent=2)+'\n')
print('PART_A_EDGES_OK cases=',len(reports))
PY
