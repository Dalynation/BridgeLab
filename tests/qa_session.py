"""Run timing QA in one session and close only explicitly disposable Wine profiles.

Required: BRIDGELAB_HOME, BRIDGELAB_QA_OUTPUT, and a .bridgelab-qa marker in
that isolated home. Never point this at a user's installed BridgeLab profile.
"""
from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']).resolve();out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
if home==Path.home()/'Library/Application Support/BridgeLab' or not (home/'.bridgelab-qa').is_file():raise SystemExit('An explicitly disposable .bridgelab-qa profile is required')
results=[];cleanup=[]
try:
 for script in ['qa_matrix.py','high_rate_audio.py','sustained_audio.py','runtime_lifecycle.py','device_matrix.py','device_interaction.py']:
  with (out/(script+'.log')).open('w') as log:p=subprocess.run(['/usr/bin/python3',str(root/'tests'/script)],env=os.environ,stdout=log,stderr=subprocess.STDOUT)
  results.append(dict(test=script,exit=p.returncode));print(json.dumps(results[-1]),flush=True)
finally:
 from qa_runtime import stop_profiles
 cleanup=stop_profiles(home)
 remaining=[]
 for line in subprocess.check_output(['ps','-axo','pid=,command='],text=True).splitlines():
  fields=line.strip().split(None,1)
  if len(fields)==2 and fields[1].startswith(str(home/'engine/runtime')+'/'):remaining.append(int(fields[0]))
 (out/'session.json').write_text(json.dumps(dict(results=results,cleanup=cleanup,remaining_runtime_pids=remaining),indent=2)+'\n')
raise SystemExit(0 if all(r['exit']==0 for r in results) and all(r['exit']==0 if r['option']=='-w' else r['exit'] in [0,1] for r in cleanup) and not remaining else 1)
