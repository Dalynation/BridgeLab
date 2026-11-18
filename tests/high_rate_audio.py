"""Apple Silicon QA: preserve scheduling time at 96/192 kHz, native and Rosetta."""
from pathlib import Path
import os,json,subprocess,math,re
root=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']);out=Path(os.environ['BRIDGELAB_QA_OUTPUT'])/'high-rate';out.mkdir(parents=True,exist_ok=True)
rows=json.loads((home/'plugins.json').read_text());results=[]
for arch in ['arm64','x86_64']:
 for row in rows:
  for rate in [96000,192000]:
   paths=[];runs=[];expected=max(8,math.ceil(rate*(2048/48000)/256))*256
   for live in [False,True]:
    path=out/(row['id']+'-'+arch+'-'+str(rate)+('-live' if live else '-offline')+'.f32');paths.append(path)
    env=dict(os.environ,BRIDGELAB_TEST_FRAMES=str(rate*4),BRIDGELAB_TEST_BLOCK='64',BRIDGELAB_TEST_RATE=str(rate))
    p=subprocess.run(['/usr/bin/arch','-'+arch,str(root/'build/vst3/bridge-smoke'),row['wrapper'],str(path)]+(['realtime'] if live else []),env=env,capture_output=True,text=True,timeout=120)
    latency=re.search(r'latency=(\d+)',p.stdout);runs.append(dict(exit=p.returncode,latency=int(latency[1]) if latency else None,stdout=p.stdout.strip(),stderr=p.stderr))
   result=dict(arch=arch,plugin=row['name'],rate=rate,expected_latency=expected,passed=all(r['exit']==0 and r['latency']==expected for r in runs) and paths[0].read_bytes()==paths[1].read_bytes(),runs=runs);results.append(result);print(json.dumps(result),flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
