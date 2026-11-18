"""Regression matrix for the cold-load/MIDI reports. Uses an isolated install."""
from pathlib import Path
import concurrent.futures,hashlib,json,os,subprocess,time
source=Path(__file__).resolve().parents[1]
home=Path(os.environ['BRIDGELAB_HOME']);out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
rows=json.loads((home/'plugins.json').read_text());results=[]
def run(label,args,timeout=120):
 started=time.monotonic();r=subprocess.run(list(map(str,args)),env=os.environ,capture_output=True,text=True,timeout=timeout)
 row=dict(test=label,passed=r.returncode==0,exit_code=r.returncode,seconds=round(time.monotonic()-started,3),stdout=r.stdout.strip(),stderr=r.stderr[-1200:])
 print(json.dumps(row),flush=True);return row
for arch in ['arm64','x86_64']:
 for plugin in rows:
  for case in ['zero-sample-note','repeated-activation','prefetch','control-pressure','queue-recovery']:
   results.append(run(plugin['name']+' '+arch+' '+case,['/usr/bin/arch','-'+arch,source/'build/vst3/bridge-lifecycle-qa',plugin['wrapper'],case]))
# Cover tight and large host blocks in actual realtime, as well as offline PCM.
for arch in ['arm64','x86_64']:
 for plugin in rows:
  blocks=[64,256,1024,4096] if arch=='arm64' else [256]
  for block in blocks:
   os.environ['BRIDGELAB_TEST_BLOCK']=str(block)
   for live in [False,True]:
    name=plugin['id']+'-'+arch+'-'+str(block)+('-live' if live else '-offline')
    results.append(run(name,['/usr/bin/arch','-'+arch,source/'build/vst3/bridge-smoke',plugin['wrapper'],out/(name+'.f32')]+(['realtime'] if live else [])))
   offline=out/(plugin['id']+'-'+arch+'-'+str(block)+'-offline.f32');live=out/(plugin['id']+'-'+arch+'-'+str(block)+'-live.f32')
   results.append(dict(test=plugin['name']+' '+arch+' '+str(block)+' live/offline exact PCM',passed=offline.read_bytes()==live.read_bytes()))
os.environ.pop('BRIDGELAB_TEST_BLOCK',None)
# Four audio clients while Windows instances start concurrently.
jobs=[rows[0],rows[-1],rows[0],rows[-1]]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 futures=[pool.submit(run,'four simultaneous live clients '+str(i),[source/'build/vst3/bridge-smoke',p['wrapper'],out/('concurrent-'+str(i)+'.f32'),'realtime']) for i,p in enumerate(jobs)]
 results += [f.result() for f in futures]
for arch in ['arm64','x86_64']:
 results.append(run('three-instance state recall '+arch,['/usr/bin/arch','-'+arch,source/'build/vst3/bridge-multi',rows[0]['wrapper'],rows[-1]['wrapper']]))
for row in rows:results.append(run('Steinberg validation '+row['name'],[source/'build/vst3/bin/Release/validator',row['wrapper']],timeout=240))
report=dict(version='0.9.5',plugins=[r['name'] for r in rows],checks=results)
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
