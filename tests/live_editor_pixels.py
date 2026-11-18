"""Require legacy original knob pixels to change without closing the editor."""
from pathlib import Path
import os,sys,json,mmap,tempfile,struct,traceback,time
sys.path.insert(0,str(Path(__file__).resolve().parent))
import installed_editor as editor
from qa_runtime import stop_profiles
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file()
out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
fixtures={'ffbb70884a6cd2af':(234,326,-36,(215,300,255,350)),
          'a4def1870e48ca34':(1225,115,60,(1190,75,1260,160))}
results=[]
try:
 for row in json.loads((home/'plugins.json').read_text()):
  if row['id'] not in fixtures:continue
  s=None;memory=None;path=None;r=dict(plugin=row['name'],passed=False)
  try:
   s=editor.Session(row);fd,path=tempfile.mkstemp(prefix='BridgeLab-live-pixels-');os.ftruncate(fd,editor.MAP_BYTES);memory=mmap.mmap(fd,editor.MAP_BYTES);os.close(fd)
   assert s.request(6,('Z:'+path).encode())[0][2]==1,'Open-editor request rejected'
   deadline=time.monotonic()+30
   while time.monotonic()<deadline:
    s.pump(20)
    if struct.unpack('<4I',memory[:16])[2]:break
   _,_,w,h=struct.unpack('<4I',memory[:16]);assert w and h,'No original editor frame'
   x,y,delta,box=fixtures[row['id']]
   def region(rect=box):
    x1,y1,x2,y2=rect
    return b''.join(memory[16+(yy*w+x1)*4:16+(yy*w+x2)*4] for yy in range(y1,y2))
   stable_box=(25,305,100,320) if row['id']=='ffbb70884a6cd2af' else (0,0,760,38)
   # A published frame can precede the plugin's first complete paint. Require
   # actual knob artwork and three stable snapshots before applying input.
   last=None;settled=0;deadline=time.monotonic()+15
   while time.monotonic()<deadline:
    s.pump(20);current=region()
    colors=len({current[i:i+3] for i in range(0,len(current),4)})
    settled=settled+1 if colors>10 and current==last else 0;last=current
    if settled>=3:break
   assert settled>=3,'Initial knob artwork never settled'
   before=region();stable=region(stable_box);state=s.request(3)[1]
   s.input(0x200,x,y,0);s.input(0x201,x,y,1)
   for step in range(1,13):s.input(0x200,x,y+delta*step//12,1)
   s.input(0x202,x,y+delta,0);s.pump(80)
   r['state_changed']=s.request(3)[1]!=state;r['changed_knob_bytes']=sum(a!=b for a,b in zip(before,region()))
   r['unchanged_artwork_preserved']=stable==region(stable_box)
   r['passed']=r['state_changed'] and r['changed_knob_bytes']>30 and r['unchanged_artwork_preserved']
  except Exception as error:r['error']=str(error);r['traceback']=traceback.format_exc()
  finally:
   if s:s.close()
   if memory:memory.close()
   if path:os.unlink(path)
  results.append(r);print(json.dumps(r),flush=True)
finally:
 stop_profiles(home);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if len(results)==2 and all(r['passed'] for r in results) else 1)
