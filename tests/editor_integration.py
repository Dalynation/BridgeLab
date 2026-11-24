from pathlib import Path
import subprocess,os,struct,json,hashlib
root=Path(__file__).resolve().parents[3];bridge=root/'outputs/BridgeLab'
class Session:
 def __init__(self,path,arch):
  env={**os.environ,'WINEPREFIX':str(bridge/'runtime'/('legacy-prefix' if arch=='x86' else 'prefix')),'WINEDEBUG':'-all','MVK_CONFIG_LOG_LEVEL':'0'}
  self.p=subprocess.Popen([str(bridge/('runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64' if arch=='x86' else 'runtime/Wine Devel.app/Contents/Resources/wine/bin/wine')),str(bridge/'build'/('runner-'+arch+'-v9.exe')),'Z:'+str(path),'--serve','48000','realtime'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=open('work/editor-state-stderr.log','ab'),env=env)
  assert struct.unpack('<3I',self.read(12))[0]==0x424c3032
 def read(self,n):
  result=b''
  while len(result)<n:
   part=self.p.stdout.read(n-len(result))
   if not part:raise RuntimeError('Runner closed')
   result+=part
  return result
 def request(self,cmd=0,payload=b'',program=0,note=False):
  midi=struct.pack('<I4B',0,0x90,60,100,0)+bytes(1016) if note else bytes(1024)
  self.p.stdin.write(struct.pack('<6I',0x424c3032,256 if cmd==0 else 0,int(note),program,cmd,len(payload))+midi+payload);self.p.stdin.flush()
  response=self.read(2064);head=struct.unpack_from('<4I',response);data=self.read(head[3]) if head[3] else b'';return head,data,response[16:]
 def close(self):self.p.stdin.close();self.p.wait(timeout=10)
def audio(session,program=0):
 result=b''
 for i in range(32):
  h,_,block=session.request(program=program,note=i==0);assert h[2]==1;result+=block
 return result

import mmap,time
frame=Path('work/embedded-frame.bin').resolve();frame.write_bytes(bytes(16+2048*2048*4+4));m=mmap.mmap(os.open(frame,os.O_RDWR),0)
s=Session(root/'outputs/SQ8L/SQ8L.dll','x86')
try:
 h,_,_=s.request(6,('Z:'+str(frame)).encode());print('embed status',h,flush=True);assert h[2]==1

 for i in range(200):s.request();time.sleep(.005)
 time.sleep(.5)
 magic,seq,w,h=struct.unpack('<4I',m[:16]);print('frame',hex(magic),seq,w,h,flush=True);assert magic==0x424c4631 and w>0 and h>0
 print('colors',len(set(m[16+i:16+i+3] for i in range(0,w*h*4,4))),flush=True)
 _,before,_=s.request(3)
 # Original SQ8L preset-down button, in editor client coordinates.
 for msg,value in [(0x201,1),(0x202,0)]:
  response,_,_=s.request(7,struct.pack('<IiiI',msg,295,46,value));assert response[2]==1

 for i in range(100):s.request();time.sleep(.005)
 time.sleep(.1)
 _,after,_=s.request(3)
 assert before!=after
 print('preset click changes state: PASS',flush=True)
 # Drag a rotary control and verify a second state edit.
 for msg,x,y,value in [(0x200,234,326,0),(0x201,234,326,1),(0x200,234,320,1),(0x200,234,310,1),(0x200,234,300,1),(0x200,234,270,1),(0x202,234,270,0)]:
  response,_,_=s.request(7,struct.pack('<IiiI',msg,x,y,value));assert response[2]==1;time.sleep(.06)

 for i in range(100):s.request();time.sleep(.005)
 time.sleep(.1)
 _,dragged,_=s.request(3)
 assert dragged!=after
 print('enabled level knob drag changes state: PASS',flush=True)

 # Restore the actual edited GUI state in a new Windows process.
 s.close();s=Session(root/'outputs/SQ8L/SQ8L.dll','x86')
 head,_,_=s.request(4,dragged);assert head[2]==1
 _,restored,_=s.request(3)
 assert len(restored)==len(dragged)
 assert sum(a!=b for a,b in zip(restored,dragged))<=1
 print('GUI-edited state fresh-process restore: PASS',flush=True)
 Path('outputs/BridgeLab/embedded-editor-results.json').write_text(json.dumps({'plugin':'SQ8L 0.91b x86','frame_size':[w,h],'preset_click_changes_state':True,'enabled_level_drag_changes_state':True,'gui_edited_state_restores_in_fresh_process':True,'popup_menus_and_dialogs_verified':False},indent=2)+'\n')

 
 if os.environ.get('BRIDGE_EMBED_HOLD'):time.sleep(90)
finally:s.close();m.close()
