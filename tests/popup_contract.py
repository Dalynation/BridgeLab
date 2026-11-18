"""Original popup discovery, mapping, isolation and limits, without vendor DLLs."""
from pathlib import Path
import json,os,subprocess
from qa_runtime import stop_profiles
root=Path(__file__).resolve().parents[1]
home=Path(os.environ['BRIDGELAB_HOME']).resolve()
assert (home/'.bridgelab-qa').is_file(), 'Use a disposable QA home'
runtime=home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64'
results=[]
try:
 for architecture in ['i686','x86_64']:
  prefix=home/'prefixes/crossover-shared'
  if not prefix.exists():prefix=home/'prefixes'/('crossover-x86' if architecture=='i686' else 'crossover-x64')
  env=dict(os.environ,WINEPREFIX=str(prefix),WINEDEBUG='-all')
  p=subprocess.run([str(runtime),str(root/'build'/('popup-test-'+architecture+'.exe'))],env=env,capture_output=True,text=True,timeout=30)
  r=dict(architecture=architecture,passed=p.returncode==0,stdout=p.stdout,stderr=p.stderr);results.append(r);print(json.dumps(r),flush=True)
finally:
 cleanup=stop_profiles(home)
 path=Path(os.environ['BRIDGELAB_QA_OUTPUT'])/'popup-contract.json';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(dict(results=results,cleanup=cleanup),indent=2)+'\n')
raise SystemExit(0 if len(results)==2 and all(x['passed'] for x in results) else 1)
