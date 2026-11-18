"""Core Audio render-callback PCM checks; output is silent and device settings unchanged."""
from pathlib import Path
import os,json,subprocess
root=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']).resolve();out=Path(os.environ['BRIDGELAB_QA_OUTPUT'])/'device';out.mkdir(parents=True,exist_ok=True)
if not (home/'.bridgelab-qa').is_file():raise SystemExit('Disposable .bridgelab-qa profile required')
rows=json.loads((home/'plugins.json').read_text());results=[];cleanup=[]
cases=[(44100,b,176400) for b in [64,256,4096]]+[(48000,b,153600) for b in [64,256,1024,4096]]+[(r,64,r*4) for r in [96000,192000]]+[(48000,64,960000),(192000,64,3840000)]
try:
 for arch in ['arm64','x86_64']:
  for row in rows:
   for rate,block,frames in cases:
    paths=[];runs=[];env=dict(os.environ,BRIDGELAB_TEST_FRAMES=str(frames),BRIDGELAB_TEST_BLOCK=str(block),BRIDGELAB_TEST_RATE=str(rate))
    for live in [False,True]:
     path=out/(row['id']+'-'+arch+'-'+str(rate)+'-'+str(block)+'-'+str(frames)+('-device' if live else '-offline')+'.f32');paths.append(path)
     host=root/'build/vst3'/('bridge-device' if live else 'bridge-smoke');p=subprocess.Popen(['/usr/bin/arch','-'+arch,str(host),row['wrapper'],str(path)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
     try:stdout,stderr=p.communicate(timeout=120)
     except subprocess.TimeoutExpired:p.kill();p.communicate();raise
     log=(home/'logs/host.log').read_text();mode='shared' if ('transport=shared pid='+str(p.pid)) in log else 'pipe';expected=os.environ.get('BRIDGELAB_EXPECT_TRANSPORT') if live else None;rt=('transport=shared pid='+str(p.pid)+' scheduler=1') in log;requireRt=live and os.environ.get('BRIDGELAB_EXPECT_RT')=='1'
     runs.append(dict(exit=p.returncode,stdout=stdout.strip(),stderr=stderr,transport=mode,native_realtime=rt,transport_matches=(not expected or expected==mode) and (not requireRt or rt)))
    result=dict(arch=arch,plugin=row['name'],rate=rate,block=block,frames=frames,passed=all(r['exit']==0 and r['transport_matches'] for r in runs) and paths[0].read_bytes()==paths[1].read_bytes(),runs=runs);results.append(result);print(json.dumps(result),flush=True)
    (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    if len(results)==1 and any(r['exit'] or not r['transport_matches'] for r in runs):raise RuntimeError('First Core Audio case failed')
finally:
 from qa_runtime import stop_profiles
 cleanup=stop_profiles(home)
 (out/'cleanup.json').write_text(json.dumps(cleanup,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
