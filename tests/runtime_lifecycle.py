"""Repeated actual Wine sessions: exact audio and bounded per-profile services.
Run after qa_matrix.py with the same BRIDGELAB_HOME and BRIDGELAB_QA_OUTPUT.
"""
from pathlib import Path
import subprocess,json,os
root=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']);reference=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out=reference/'lifecycle';out.mkdir(parents=True,exist_ok=True);rows=json.loads((home/'plugins.json').read_text());results=[]
def helpers():
 result=[]
 for line in subprocess.check_output(['ps','-axo','pid=,command='],text=True).splitlines():
  p=line.strip().split(None,1)
  if len(p)==2 and p[1].startswith(str(home/'engine/runtime')+'/') and 'wine64-preloader C:\\windows\\system32\\' in p[1]:result.append(int(p[0]))
 return result
before=helpers()
for arch in ['arm64','x86_64']:
 for row in rows:
  for i in range(4):
   pcm=out/('lifetime-'+arch+'-'+row['id']+'-'+str(i)+'.f32');env=dict(os.environ,BRIDGELAB_TEST_BLOCK='64')
   p=subprocess.run(['/usr/bin/arch','-'+arch,str(root/'build/vst3/bridge-smoke'),row['wrapper'],str(pcm),'realtime'],capture_output=True,text=True,env=env,timeout=90)
   # Reference latency is identical for 64 and 256 host callbacks.
   ref=reference/(row['id']+'-'+arch+'-'+('64' if arch=='arm64' else '256')+'-offline.f32')
   count=len(helpers());r=dict(arch=arch,plugin=row['name'],trial=i,exact=p.returncode==0 and pcm.read_bytes()==ref.read_bytes(),helper_count=count,stdout=p.stdout,stderr=p.stderr);results.append(r);print(json.dumps(r),flush=True)
report=dict(before=len(before),after=len(helpers()),trials=results,passed=all(x['exact'] and x['helper_count']<=16 for x in results))
(out/'runtime-lifetime-results.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if report['passed'] else 1)
