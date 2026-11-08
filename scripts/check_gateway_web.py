#!/usr/bin/env python3
"""Read-only live gateway verification. Never submits controls or configuration."""
import argparse, gzip, json, socket, time
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from concurrent.futures import ThreadPoolExecutor
p=argparse.ArgumentParser(description=__doc__);p.add_argument('host');p.add_argument('--output',type=Path,required=True);args=p.parse_args()
root=Path(__file__).resolve().parent.parent;base='http://'+args.host;report={'checks':[],'samples':[],'device_writes':0,'configuration_writes':0}
def request(path,headers=None):
    try:
        with urlopen(Request(base+path,headers=headers or {}),timeout=8) as response:return response.status,response.read(),dict(response.headers)
    except HTTPError as response:return response.code,response.read(),dict(response.headers)
def get(path):
    status,data,_=request(path);assert status==200,(path,status);return json.loads(data)
def passed(name):report['checks'].append(name);print('PASS:',name,flush=True)
args.output.parent.mkdir(parents=True,exist_ok=True)
try:
    initial=get('/api/v1/status');assert initial['gateway']['ready'];backup=get('/api/v1/backup');assert backup['boot']==initial['boot']
    for name,path in [('index.html','/'),('app.css','/app.css'),('app.js','/app.js'),('favicon.png','/favicon.png'),('apple-touch-icon.png','/apple-touch-icon.png'),('favicon.png','/favicon.ico')]:
        status,data,headers=request(path);assert status==200 and headers.get('Content-Encoding')=='gzip';assert gzip.decompress(data)==(root/'web'/name).read_bytes();assert headers.get('X-Content-Type-Options')=='nosniff' and 'Content-Security-Policy' in headers
    passed('deployed gzip assets match source and security headers')
    assert request('/api/v1/status',{'Host':'evil.invalid'})[0]==403
    assert request('/api/v1/plan')[0]==404
    passed('Host validation and removal of weekly API')
    for kind in ('sample','today'):
        meta=backup[kind]
        if not meta or not meta.get('records_url'):continue
        path=meta['records_url'];seen=0
        while path:
            page=get(path);assert page['boot']==backup['boot'] and page['revision']==meta['revision'] and len(page['records'])<=8
            seen+=len(page['records']);path=page['next']
        assert seen==meta['count']
    passed('daily metadata and bounded records pages')
    with ThreadPoolExecutor(max_workers=2) as pool:
        for _ in range(20):
            state,health=list(pool.map(get,('/api/v1/status','/api/v1/health')))
            assert state['boot']==initial['boot'] and health['wifi_connected'] and health['ble_initialized']
            report['samples'].append({k:health[k] for k in ('uptime_ms','free_heap','min_heap','largest_block','fridge_connected','state_stale')});report['samples'][-1]['sequence']=state['device']['sequence'];time.sleep(.5)
    assert any(x['fridge_connected'] and not x['state_stale'] for x in report['samples']);assert len({x['sequence'] for x in report['samples']})>=2
    passed('two HTTP clients coexist with fresh BLE and no observed restart')
finally:
    args.output.write_text(json.dumps(report,indent=2)+'\n')
