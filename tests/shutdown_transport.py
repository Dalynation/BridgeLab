"""Regression: native teardown must allow child cleanup before forced termination."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='bridgelab-shutdown-') as td:
 home=Path(td);runtime=home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64';runtime.parent.mkdir(parents=True);(home/'logs').mkdir()
 runtime.write_text('''#!/usr/bin/python3
import os,struct,time
from pathlib import Path
home=Path(os.environ['WINEPREFIX']).parents[1]
(home/'child-pid').write_text(str(os.getpid()))
os.write(1,struct.pack('<3I',0x424c3032,1,2))
while os.read(0,1048):
 if (home/'hang-mid-request').exists():
  (home/'request-received').touch();time.sleep(30)
if (home/'hang-after-eof').exists():time.sleep(30)
(home/'runner-exited').touch()
''')
 runtime.chmod(0o755)
 for arch in ['arm64','x86_64']:
  subprocess.run(['/usr/bin/arch','-'+arch,os.environ.get('BRIDGELAB_SHUTDOWN_TEST_HOST',str(root/'build/vst3/bridge-shutdown-transport')),str(home)],check=True,timeout=25)
