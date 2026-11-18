"""Force multiple requests in flight and fragment every protocol response."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='bridgelab-pipeline-') as td:
 home=Path(td);runtime=home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64';runtime.parent.mkdir(parents=True);(home/'logs').mkdir()
 runtime.write_text('''#!/usr/bin/python3
import os,struct,sys,time,atexit
from pathlib import Path
home=Path(os.environ['WINEPREFIX']).parents[1]
atexit.register(lambda:(home/'runner-exited').touch())
def read(n):
 data=b''
 while len(data)<n:
  part=os.read(0,n-len(data))
  if not part:sys.exit(0)
  data+=part
 return data
def send(data):
 for i in range(0,len(data),37):os.write(1,data[i:i+37])
send(struct.pack('<3I',0x424c3032,1,2))
request=read(1048);assert struct.unpack_from('<I',request,16)[0]==3
(home/'control-ready').touch()
while not (home/'control-release').exists():time.sleep(.001)
send(struct.pack('<4I',0x424c3032,0,1,0)+bytes(2048))
# A serial request/reply implementation cannot satisfy this barrier.
for i in range(8):read(1048)
for i in range(8):send(struct.pack('<4I',0x424c3032,256,1,0)+struct.pack('<512f',*([float(i+1)]*512)))
while True:read(1048)
''')
 runtime.chmod(0o755)
 for arch in ['arm64','x86_64']:
  subprocess.run(['/usr/bin/arch','-'+arch,os.environ.get('BRIDGELAB_PIPELINE_TEST_HOST',str(root/'build/vst3/bridge-pipeline-transport')),str(home)],check=True,timeout=20)
  (home/'control-ready').unlink();(home/'control-release').unlink();(home/'runner-exited').unlink()
