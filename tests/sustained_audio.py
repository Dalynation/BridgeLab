"""Twenty-second exact live/offline comparisons at a small host buffer."""
from pathlib import Path
import os,json,subprocess
root=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']);out=Path(os.environ['BRIDGELAB_QA_OUTPUT'])/'sustained';out.mkdir(parents=True,exist_ok=True)
rows=json.loads((home/'plugins.json').read_text());results=[]
for arch in ['arm64','x86_64']:
 for row in rows:
  paths=[];runs=[]
  for live in [False,True]:
   path=out/(row['id']+'-'+arch+('-live' if live else '-offline')+'.f32');paths.append(path)
   env=dict(os.environ,BRIDGELAB_TEST_FRAMES='960000',BRIDGELAB_TEST_BLOCK='64')
   p=subprocess.run(['/usr/bin/arch','-'+arch,str(root/'build/vst3/bridge-smoke'),row['wrapper'],str(path)]+(['realtime'] if live else []),env=env,capture_output=True,text=True,timeout=120)
   runs.append(dict(exit=p.returncode,stdout=p.stdout.strip(),stderr=p.stderr))
  result=dict(arch=arch,plugin=row['name'],seconds=20,block=64,passed=all(r['exit']==0 for r in runs) and paths[0].read_bytes()==paths[1].read_bytes(),runs=runs);results.append(result);print(json.dumps(result),flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
