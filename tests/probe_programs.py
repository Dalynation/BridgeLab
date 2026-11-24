"""Probe disposable instances; never enumerate by changing a user's live patch."""
from pathlib import Path
import os,json,subprocess
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file()
out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
rows=json.loads((home/'plugins.json').read_text());results=[]
wine=home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64'
for row in rows:
 env=dict(os.environ,WINEPREFIX=str(home/'prefixes/crossover-shared'),WINEDEBUG='-all',MVK_CONFIG_LOG_LEVEL='0')
 result=subprocess.run([str(wine),str(home/'engine/bin'/('runner-'+row['architecture']+'.exe')),'Z:'+row['path'],'--probe'],env=env,capture_output=True,timeout=150)
 text=result.stdout.decode('utf-8',errors='replace');(out/(row['id']+'-probe.log')).write_text(text+result.stderr.decode('utf-8',errors='replace'))
 names=[line.split(' ',2)[2] for line in text.splitlines() if line.startswith('BridgeLabProgram ')]
 first=bytes.fromhex(names[0]).decode('utf-8',errors='replace') if names else ''
 entry=dict(plugin=row['name'],exit=result.returncode,count=len(names),first=first,passed=result.returncode==0 and bool(names))
 results.append(entry);print(json.dumps(entry),flush=True)
(out/'program-probes.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
