"""Cross-process shared transport regression; no Windows runtime required."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='bridgelab-shared-') as td:
 home=Path(td);runtime=home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64';runtime.parent.mkdir(parents=True);(home/'logs').mkdir()
 runtime.write_text('''#!/usr/bin/python3
import os,struct,sys,time,mmap,threading
from pathlib import Path
home=Path(os.environ['WINEPREFIX']).parents[1];path=sys.argv[-1][2:]
(home/'mapping-path').write_text(path)
f=open(path,'r+b');m=mmap.mmap(f.fileno(),0)
def get(o):return struct.unpack_from('<I',m,o)[0]
def put(o,v):struct.pack_into('<I',m,o,v)
def audio():
 put(40,1)
 while not get(36):
  if get(48):put(52,1);time.sleep(.001);continue
  put(52,0);r=get(16)
  if r==get(20):time.sleep(.0001);continue
  seq=struct.unpack_from('<Q',m,64+r%128*1056)[0];put(16,r+1);w=get(28)
  assert w-get(24)<128
  data=struct.pack('<Q4I',seq,0x424c3032,256,1,0)+struct.pack('<512f',*([float(seq+1)]*512))
  offset=64+128*1056+w%128*2072;m[offset:offset+2072]=data
  put(28,w+1);put(32,r+1)
threading.Thread(target=audio,daemon=True).start()
while not get(40):time.sleep(.001)
os.write(1,struct.pack('<3I',0x424c3032,1,2))
def read(n):
 data=b''
 while len(data)<n:
  p=os.read(0,n-len(data))
  if not p:sys.exit(0)
  data+=p
 return data
while True:
 req=read(1048);n=struct.unpack_from('<I',req,20)[0]
 if n:read(n)
 (home/'control-ready').touch()
 while not (home/'control-release').exists():time.sleep(.001)
 (home/'control-release').unlink();(home/'control-ready').unlink()
 os.write(1,struct.pack('<4I',0x424c3032,0,1,0)+bytes(2048))
''')
 runtime.chmod(0o755)
 for arch in ['arm64','x86_64']:
  subprocess.run(['/usr/bin/arch','-'+arch,str(root/'build/vst3/bridge-shared-transport'),str(home)],check=True,timeout=25)
