#!/usr/bin/env bash
set -euo pipefail
python3 - <<'PY'
import socket,socketserver,threading,subprocess,time,os,json,concurrent.futures,struct
from pathlib import Path
from collections import Counter
counts=Counter(); seen={}; state_lock=threading.Lock(); slow=threading.Event(); checks=[]
def response(body,extra=b''):
 return b'HTTP/1.0 200 OK\r\nContent-Length: '+str(len(body)).encode()+b'\r\n'+extra+b'\r\n'+body
def sized(total):
 n=total-50
 for _ in range(5): n=total-len(response(b'x'*n))+n
 data=response(bytes((i*17)%256 for i in range(n)))
 assert len(data)==total
 return data
class Handler(socketserver.BaseRequestHandler):
 def handle(self):
  self.request.settimeout(8); raw=b''
  try:
   while b'\r\n\r\n' not in raw:
    chunk=self.request.recv(4096)
    if not chunk: return
    raw+=chunk
   path=raw.split(b' ')[1].decode()
   with state_lock: counts[path]+=1; seen[path]=raw
   if path.startswith('/slow'): slow.wait(4)
   if path.startswith('/size/'):
    data=sized(int(path.split('/')[2]))
   elif path.startswith('/lru/'):
    data=sized(100000)
   elif path.startswith('/truncated'):
    data=b'HTTP/1.0 200 OK\r\nContent-Length: 100\r\n\r\nabc'
   elif path.startswith('/private'):
    data=response(b'private',b'Cache-Control: private, no-store\r\n')
   elif path.startswith('/binary'):
    data=response(bytes(range(256))*200)
   elif path.startswith('/echo'):
    data=response(raw)
   else: data=response(path.encode())
   self.request.sendall(data)
  except (OSError,ValueError): pass
class Server(socketserver.ThreadingTCPServer):
 allow_reuse_address=True; daemon_threads=True
origin=Server(('127.0.0.1',0),Handler); port=origin.server_address[1]
threading.Thread(target=origin.serve_forever,daemon=True).start()
def freeport():
 with socket.socket() as s: s.bind(('127.0.0.1',0)); return s.getsockname()[1]
proxyport=freeport(); mode=os.environ['PROXY_MODE']
log=open('test-results/proxy-'+mode+'.log','wb')
process=subprocess.Popen([os.environ['PROXY_BIN'],str(proxyport)],stdout=log,stderr=log)
def raw_request(raw):
 with socket.create_connection(('127.0.0.1',proxyport),timeout=8) as s:
  s.sendall(raw); result=b''
  while True:
   data=s.recv(65536)
   if not data: break
   result+=data
  return result
def fetch(path,headers=b'',authority=None):
 host=authority or f'127.0.0.1:{port}'
 return raw_request(b'GET http://'+host.encode()+path.encode()+b' HTTP/1.1\r\n'+headers+b'\r\n')
def count(path):
 with state_lock: return counts[path]
def ok(name): checks.append(name); print('CHECK_OK',name,flush=True)
try:
 for _ in range(100):
  try:
   with socket.create_connection(('127.0.0.1',proxyport),timeout=.1): pass
   break
  except OSError:
   assert process.poll() is None; time.sleep(.05)
 else: raise AssertionError('proxy not listening')
 assert fetch('/binary')==response(bytes(range(256))*200); ok('binary-nul-byte-exact')
 headers=b'Host: virtual.example\r\nUser-Agent: ignored\r\nConnection: Keep-Alive, X-Hop\r\nProxy-Connection: keep-alive\r\nX-Hop: secret\r\nX-End: keep  spaces\r\n'
 reply=fetch('/echo?x=1',headers)
 forwarded=reply.split(b'\r\n\r\n',1)[1]
 assert forwarded.startswith(b'GET /echo?x=1 HTTP/1.0\r\n')
 assert b'Host: virtual.example\r\n' in forwarded and b'User-Agent: Mozilla/' in forwarded
 assert b'Connection: close\r\n' in forwarded and b'Proxy-Connection: close\r\n' in forwarded
 assert b'X-End: keep  spaces\r\n' in forwarded and b'X-Hop:' not in forwarded and b'ignored' not in forwarded
 ok('host-override-user-agent-http10-hop-fields-extra-header')
 assert fetch('')==response(b'/'); assert fetch('?q=yes')==response(b'/?q=yes'); ok('missing-path-query-default')
 assert raw_request(f'GET /origin-form HTTP/1.0\r\nHost: 127.0.0.1:{port}\r\n\r\n'.encode())==response(b'/origin-form'); ok('origin-form-explicit-port')
 # Required default HTTP port, served by a separate fixture on port 80.
 # A narrowly scoped runner-only helper binds privileged port 80; proxy stays
 # unprivileged and no account/capability/kernel settings are changed.
 default_code=r"""import socket
s=socket.socket(); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
s.bind(('127.0.0.1',80)); s.listen(1); s.settimeout(10)
print('READY',flush=True)
c,_=s.accept(); c.settimeout(5); data=b''
while b'\r\n\r\n' not in data: data+=c.recv(4096)
body=b'/default-port'
c.sendall(b'HTTP/1.0 200 OK\r\nContent-Length: '+str(len(body)).encode()+b'\r\n\r\n'+body)
c.close(); s.close()
"""
 default=subprocess.Popen(['sudo','-n','python3','-u','-c',default_code],stdout=subprocess.PIPE,text=True)
 try:
  assert default.stdout.readline().strip()=='READY'
  assert fetch('/default-port',authority='127.0.0.1')==response(b'/default-port')
 finally: assert default.wait(timeout=15)==0
 ok('default-port-80')
 invalid=[(b'POST http://localhost/ HTTP/1.0\r\n\r\n',501),
  (b'GET https://localhost/ HTTP/1.0\r\n\r\n',400),
  (b'GET http://localhost:99999/ HTTP/1.0\r\n\r\n',400),
  (b'GET http://localhost/ HTTP/1.0\r\nBad Header\r\n\r\n',400),
  (b'GET http://localhost/ HTTP/1.0\r\nHost: a\r\nHost: b\r\n\r\n',400),
  (b'GET http://localhost/ HTTP/1.0\r\nContent-Length: 5\r\n\r\n',400),
  (b'GET http://localhost/ HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n',400),
  (b'GET http://localhost/ HTTP/1.0\r\nX:'+b'x'*9000+b'\r\n\r\n',400)]
 for request,code in invalid:
  try: data=raw_request(request)
  except ConnectionResetError: raise AssertionError('error status lost to reset')
  assert data.startswith(f'HTTP/1.0 {code} '.encode()),(code,data[:100])
 assert fetch('/after-malformed')==response(b'/after-malformed'); ok('malformed-method-port-framing-overlong-survival')
 dead=freeport(); assert fetch('/failed-connect',authority=f'127.0.0.1:{dead}').startswith(b'HTTP/1.0 502 '); ok('connect-failure-502')
 # One stalled origin must not hold up independent requests.
 with concurrent.futures.ThreadPoolExecutor(max_workers=40) as pool:
  stalled=pool.submit(fetch,'/slow-blocked')
  time.sleep(.1); start=time.monotonic()
  tasks=[pool.submit(fetch,f'/concurrent/{i}') for i in range(32)]
  for i,f in enumerate(tasks): assert f.result(timeout=3)==response(f'/concurrent/{i}'.encode())
  assert time.monotonic()-start<3; slow.set(); assert stalled.result()==response(b'/slow-blocked')
 ok('32-concurrent-requests-while-origin-stalled')
 # Cache hits, independent cache readers, request-header and origin-port keys.
 assert fetch('/cached')==response(b'/cached')
 with concurrent.futures.ThreadPoolExecutor(max_workers=32) as pool:
  assert all(x==response(b'/cached') for x in pool.map(lambda _:fetch('/cached'),range(96)))
 assert count('/cached')==1; ok('96-concurrent-cache-hits')
 a=fetch('/echo-variant',b'X-Variant: a\r\n'); b=fetch('/echo-variant',b'X-Variant: b\r\n')
 assert a!=b and count('/echo-variant')==2; assert fetch('/echo-variant',b'X-Variant: a\r\n')==a; ok('request-header-cache-key-isolation')
 other=Server(('127.0.0.1',0),Handler); threading.Thread(target=other.serve_forever,daemon=True).start()
 try:
  before=count('/origin-key'); fetch('/origin-key'); fetch('/origin-key',authority=f'127.0.0.1:{other.server_address[1]}'); assert count('/origin-key')==before+2
 finally: other.shutdown(); other.server_close()
 ok('origin-port-cache-key-isolation')
 for total in [102400,102401]:
  path=f'/size/{total}'; a=fetch(path); b=fetch(path); assert a==b==sized(total)
  assert count(path)==(1 if total==102400 else 2)
 ok('exact-object-limit-102400-and-oversize-bypass')
 for path in ['/private','/truncated']:
  a=fetch(path); b=fetch(path); assert a==b and count(path)==2
 ok('private-and-truncated-not-cached')
 for path,header in [('/auth',b'Authorization: secret\r\n'),('/cookie',b'Cookie: session=x\r\n')]:
  fetch(path,header); fetch(path,header); assert count(path)==2
 ok('authenticated-cookie-cache-bypass')
 # Fill 10 objects; a hit refreshes 0, insertion 10 must evict 1 first.
 for i in range(10): assert fetch(f'/lru/{i}')==sized(100000)
 assert fetch('/lru/0')==sized(100000); assert count('/lru/0')==1
 fetch('/lru/10'); assert fetch('/lru/0')==sized(100000); assert count('/lru/0')==1
 fetch('/lru/1'); assert count('/lru/1')==2; ok('bounded-cache-and-lru-hit-refresh')
 # Aborted clients and reset origin must not terminate the long-lived process.
 for i in range(40):
  with socket.create_connection(('127.0.0.1',proxyport),timeout=3) as s:
   s.sendall(f'GET http://127.0.0.1:{port}/binary?abort={i} HTTP/1.0\r\n\r\n'.encode())
   s.setsockopt(socket.SOL_SOCKET,socket.SO_LINGER,struct.pack('ii',1,0))
 time.sleep(.3); assert process.poll() is None; assert fetch('/survived-reset')==response(b'/survived-reset'); ok('40-reset-clients-sigpipe-survival')
 # Repeated completed requests must not accumulate descriptors or joinable threads.
 fetch('/fd-probe')
 with concurrent.futures.ThreadPoolExecutor(max_workers=32) as pool:
  assert all(x==response(b'/fd-probe') for x in pool.map(lambda _:fetch('/fd-probe'),range(500)))
 recovery_start=time.monotonic()
 for _ in range(750):
  fds=len(list(Path(f'/proc/{process.pid}/fd').iterdir()))
  threads=int(next(x.split(':')[1] for x in Path(f'/proc/{process.pid}/status').read_text().splitlines() if x.startswith('Threads:')))
  if fds<=6 and threads<=3: break
  time.sleep(.02)
 assert fds<=6 and threads<=3,(fds,threads)
 print('RESOURCE_RECOVERY seconds=%.3f fds=%d threads=%d'%(time.monotonic()-recovery_start,fds,threads),flush=True)
 assert count('/fd-probe')==1; ok('500-cache-requests-no-fd-thread-accumulation')
 # Warmed cache must remain usable after origin disappears.
 fetch('/offline'); origin.shutdown(); origin.server_close()
 assert fetch('/offline')==response(b'/offline'); ok('offline-cache-with-origin-down')
 result=dict(mode=mode,checks=checks,passed=len(checks),origin_counts=dict(counts),object_limit=102400,cache_limit=1049000)
 Path('test-results/extra-'+mode+'.json').write_text(json.dumps(result,indent=2)+'\n')
 print('PROXY_EXTRA_OK mode='+mode+' checks='+str(len(checks)))
finally:
 slow.set(); process.terminate()
 try: process.wait(timeout=5)
 except subprocess.TimeoutExpired: process.kill(); process.wait()
 log.close(); origin.shutdown(); origin.server_close()
 diagnostic=Path('test-results/proxy-'+mode+'.log').read_text(errors='replace')
 assert 'AddressSanitizer' not in diagnostic and 'runtime error:' not in diagnostic,diagnostic
PY
