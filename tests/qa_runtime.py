"""Stop only explicitly disposable BridgeLab test profiles, including mixed-bitness QA."""
from pathlib import Path
import os,subprocess
def stop_profiles(home):
 home=Path(home).resolve()
 if not (home/'.bridgelab-qa').is_file() or home==Path.home()/'Library/Application Support/BridgeLab':raise RuntimeError('Disposable .bridgelab-qa profile required')
 server=home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wineserver';results=[]
 for suffix in ['x86','x64','shared']:
  prefix=home/'prefixes'/('crossover-'+suffix)
  if not prefix.is_dir():continue
  env=dict(os.environ,WINEPREFIX=str(prefix),WINEDEBUG='-all')
  for option in ['-k','-w']:
   try:
    p=subprocess.run([str(server),option],env=env,capture_output=True,text=True,timeout=15);results.append(dict(arch=suffix,option=option,exit=p.returncode,stderr=p.stderr))
   except subprocess.TimeoutExpired:results.append(dict(arch=suffix,option=option,exit=124))
 return results
