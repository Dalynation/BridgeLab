"""Queue-overload recovery for both transports and native/Rosetta hosts.

Use a marked disposable home containing sounding instrument test fixtures.
"""
from pathlib import Path
import os,json,subprocess,shutil,sys
source=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']).resolve();assert (home/'.bridgelab-qa').is_file(), 'Use a disposable QA home';out=Path(os.environ['BRIDGELAB_QA_OUTPUT'])/'midi-recovery';out.mkdir(parents=True,exist_ok=True);rows=json.loads((home/'plugins.json').read_text());assert rows, 'Import sounding test instruments';results=[]
try:
 for mode in ['shared','pipe']:
  env=dict(os.environ,BRIDGELAB_HOME=str(home))
  if mode=='pipe':env['BRIDGELAB_PIPE_AUDIO']='1'
  for arch in ['arm64','x86_64']:
   for row in rows:
    for case in ['queue-recovery','held-key-recovery']:
     p=subprocess.run(['/usr/bin/arch','-'+arch,str(source/'build/vst3/bridge-lifecycle-qa'),row['wrapper'],case],env=env,capture_output=True,text=True,timeout=120);r=dict(plugin=row['name'],arch=arch,mode=mode,test=case,passed=p.returncode==0,stdout=p.stdout,stderr=p.stderr);results.append(r);print(json.dumps(r),flush=True)
finally:
 sys.path.insert(0,str(source/'tests'));from qa_runtime import stop_profiles
 cleanup=stop_profiles(home);(out/'results.json').write_text(json.dumps(dict(results=results,cleanup=cleanup),indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
