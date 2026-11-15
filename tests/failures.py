from pathlib import Path
import subprocess,os,json
root=Path(__file__).resolve().parents[3];b=root/'outputs/BridgeLab';env={**os.environ,'WINEPREFIX':str(b/'runtime/prefix'),'WINEDEBUG':'-all','MVK_CONFIG_LOG_LEVEL':'0'};results=[]
for name,expected in [('silent.dll',3),('invalid.dll',1),('missing.dll',1)]:
 r=subprocess.run([str(b/'runtime/Wine Devel.app/Contents/Resources/wine/bin/wine'),str(b/'build/runner-x64.exe'),'Z:'+str(root/'work/compatibility'/name),'Z:'+str(root/'work/compatibility'/(name+'.wav'))],env=env,capture_output=True,text=True,timeout=30)
 results.append(dict(test=name,expected_exit=expected,actual_exit=r.returncode,passed=r.returncode==expected,stdout=r.stdout.strip(),stderr=r.stderr[-1000:]))
(root/'work/compatibility/failure-results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
