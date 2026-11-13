#!/usr/bin/env bash
set -euo pipefail
python3 - <<'PY'
import os,signal,subprocess,time,select,re,json
from pathlib import Path
checks=[]
p=subprocess.Popen(['./tsh','-p'],cwd='shlab',stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True,bufsize=0)
log='';groups=set()
def send(s):p.stdin.write((s+'\n').encode());p.stdin.flush()
def until(pattern,timeout=8):
    global log
    start=len(log);deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        m=re.search(pattern,log[start:])
        if m:return m
        if select.select([p.stdout],[],[],.1)[0]:
            data=os.read(p.stdout.fileno(),65536)
            if not data:raise AssertionError(('shell exited',p.poll(),log))
            log+=data.decode()
    raise AssertionError(('timeout',pattern,log))
def children(pid):
    try:return [int(s) for s in Path(f'/proc/{pid}/task/{pid}/children').read_text().split()]
    except FileNotFoundError:return []
def state(pid):
    try:return Path(f'/proc/{pid}/stat').read_text().split(') ',1)[1].split()[0]
    except FileNotFoundError:return None
def waitfor(fn):
    deadline=time.monotonic()+5
    while not fn():
        assert time.monotonic()<deadline,('state timeout',log)
        time.sleep(.02)
try:
    for sig in [signal.SIGINT,signal.SIGTSTP]:os.kill(p.pid,sig)
    send('/bin/echo NO_FOREGROUND_OK');until('NO_FOREGROUND_OK');checks.append('no-foreground-signals')
    for i in range(150):send('/bin/true')
    send('/bin/echo FAST_FOREGROUND_OK');until('FAST_FOREGROUND_OK',15);checks.append('150-fast-foreground-exits')
    for batch in range(20):
        for i in range(5):send('/bin/true &')
        send(f'/bin/echo BATCH_{batch}_OK');until(f'BATCH_{batch}_OK');time.sleep(.03)
    waitfor(lambda:not children(p.pid));checks.append('100-fast-background-exits-no-zombies')
    send('/bin/sleep 30 &');m=until(r'\[(\d+)\] \((\d+)\) /bin/sleep 30 &');jid,bg=map(int,m.groups());groups.add(bg)
    os.killpg(bg,signal.SIGSTOP);until(rf'Job \[{jid}\] \({bg}\) stopped by signal {signal.SIGSTOP}')
    send('jobs');until(rf'\[{jid}\] \({bg}\) Stopped /bin/sleep 30 &')
    send(f'bg {bg}');until(rf'\[{jid}\] \({bg}\) /bin/sleep 30 &');waitfor(lambda:state(bg) not in ['T','t']);checks.append('bg-by-pid-and-stopped-list')
    send('./mysplit 30');waitfor(lambda:len(children(p.pid))==2)
    fg=next(x for x in children(p.pid) if x!=bg);groups.add(fg)
    waitfor(lambda:len(children(fg))==1);descendant=children(fg)[0]
    os.kill(p.pid,signal.SIGTSTP);until(rf'Job \[(\d+)\] \({fg}\) stopped by signal {signal.SIGTSTP}')
    waitfor(lambda:state(fg)=='T' and state(descendant)=='T');checks.append('stop-entire-foreground-group')
    send('jobs');m=until(rf'\[(\d+)\] \({fg}\) Stopped ./mysplit 30');fgjid=int(m[1])
    send(f'bg %{fgjid}');until(rf'\[{fgjid}\] \({fg}\) ./mysplit 30');waitfor(lambda:state(fg)!='T' and state(descendant)!='T');checks.append('bg-by-jid-resumes-descendant')
    send(f'fg {fg}');time.sleep(.1);os.kill(p.pid,signal.SIGINT);until(rf'Job \[{fgjid}\] \({fg}\) terminated by signal {signal.SIGINT}')
    waitfor(lambda:state(fg) in [None,'Z'] and state(descendant) in [None,'Z']);assert state(bg) not in [None,'T','Z'];checks.append('fg-by-pid-interrupts-group-isolates-background')
    send(f'fg %{jid}');time.sleep(.1);os.kill(p.pid,signal.SIGINT);until(rf'Job \[{jid}\] \({bg}\) terminated by signal {signal.SIGINT}')
    waitfor(lambda:not children(p.pid));checks.append('fg-by-jid-and-child-reaping')
    for command in ['bg','fg','bg abc','fg %9999','bg 9999999','fg -1','bg %1x']:
        send(command)
    send('/bin/echo ERRORS_OK');until('ERRORS_OK');checks.append('invalid-job-arguments-preserve-shell')
    send('quit');p.wait(timeout=5);assert p.returncode==0;checks.append('quit')
finally:
    if p.poll() is None:p.kill();p.wait()
    for pid in groups:
        try:os.killpg(pid,signal.SIGKILL)
        except ProcessLookupError:pass
    Path('test-results/job-control-extra.log').write_text(log)
Path('test-results/job-control-extra.json').write_text(json.dumps({'checks':checks,'passed':len(checks),'fast_foreground':150,'fast_background':100},indent=2)+'\n')
print('JOB_CONTROL_EXTRA_OK checks=',len(checks))
PY
