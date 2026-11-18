"""Verify original popup rendering, selection, dismissal, and live audio.
Uses a disposable original NoiseMaker copy; production code has no vendor names.
"""
from pathlib import Path
import os,sys,json,tempfile,mmap,struct,time
sys.path.insert(0,str(Path(__file__).resolve().parent))
import installed_editor as editor
from qa_runtime import stop_profiles
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file()
out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
cases={'filter':(265,274),'voices':(1177,193),'lfo-destination':(120,225),'amp-destination':(537,325),'portamento':(1177,115),'modulation':(1235,306)}
results=[];s=None;memory=None;path=None
try:
 row=next(r for r in json.loads((home/'plugins.json').read_text()) if r['id']=='a4def1870e48ca34');s=editor.Session(row)
 fd,path=tempfile.mkstemp(prefix='BridgeLab-popup-roundtrip-');os.ftruncate(fd,editor.MAP_BYTES);memory=mmap.mmap(fd,editor.MAP_BYTES);os.close(fd);s.request(6,('Z:'+path).encode())
 deadline=time.monotonic()+40
 while time.monotonic()<deadline:
  s.pump(20);_,seq,w,h=struct.unpack('<4I',memory[:16])
  if w and seq>20:break
 assert (w,h)==(1270,695);s.pump(80);initial=s.request(3)[1]
 def health():
  head,data=s.request(14);assert head[2]==1 and len(data)>=32
  values=struct.unpack('<'+'I'*(len(data)//4),data);assert values[0]==1 and len(values)==8+values[5]*4
  return values
 def click(x,y):
  for message,flags in [(0x200,0),(0x201,1),(0x202,0)]:s.input(message,x,y,flags)
 def audio(status=0,note=60,velocity=0):
  midi=struct.pack('<I4B',0,status,note,velocity,0) if status else b''
  s.p.stdin.write(struct.pack('<6I',0x424c3032,256,int(bool(status)),0,0,0)+midi+bytes(1024-len(midi)));s.p.stdin.flush()
  response=s.read(2064);header=struct.unpack_from('<4I',response);assert header[2]==1 and not header[3]
  return max(abs(x) for x in struct.unpack_from('<512f',response,16))
 def open_menu(x,y):
  previous_copies=health()[2];click(x,y);deadline=time.monotonic()+5
  while time.monotonic()<deadline:
   s.pump(10);status=health()
   if status[5]>0 and status[2]>previous_copies:break
  else:raise AssertionError('No new original popup capture: '+str(status[:8]))
  rect=status[8:12];left,top,right,bottom=rect
  pixels=b''.join(memory[16+(yy*w+left)*4:16+(yy*w+right)*4] for yy in range(top,bottom))
  colors=len({pixels[i:i+3] for i in range(0,len(pixels),4)})
  assert colors>10,'Popup has no original menu artwork'
  return rect
 for name,(x,y) in cases.items():
  r={'menu':name,'passed':False}
  try:
   s.request(4,initial);s.pump(35);state=s.request(3)[1];audio(0x90,60,100)
   rect=open_menu(x,y);r['original_menu_visible']=True;observed=json.loads(Path(path+'.health.json').read_text());assert observed['original_popup_pixels_forwarded']==1 and observed['unsupported_popup_modes']==0,str(observed);assert observed['input_queued']>=observed['input_delivered']>0 and observed['frames_published']>0 and observed['popup_backing_offscreen']==1,str(observed);r['automatic_health_report']=True;r['audio_peak_while_open']=max(audio() for _ in range(20));assert r['audio_peak_while_open']>1e-4
   left,top,right,bottom=rect
   click((left+right)//2,top+39);s.pump(45)
   selected=s.request(3)[1];r['selection_changed_state']=selected!=state;assert r['selection_changed_state'],'Original menu selection did not change state'
   assert health()[5]==0,'Menu remained visible after selection';r['selection_closed_menu']=True
   open_menu(x,y);s.input(0x100,x,y,27);s.input(0x101,x,y,27);s.pump(35)
   r['escape_closed_without_change']=health()[5]==0 and s.request(3)[1]==selected;assert r['escape_closed_without_change'],'Escape did not dismiss unchanged'
   open_menu(x,y);click(800,80);s.pump(35);r['outside_click_closed']=health()[5]==0;assert r['outside_click_closed'],'Outside click did not dismiss'
   r['health']=list(health()[:8]);assert not r['health'][4] and not r['health'][6]
   audio(0x80,60,0);s.pump(100);r['passed']=True
  except Exception as error:
   r['error']=str(error)
   try:s.input(0x100,0,0,27);s.input(0x101,0,0,27);s.pump(20);audio(0x80,60,0)
   except Exception:pass
  results.append(r);print(json.dumps(r),flush=True)
finally:
 if s:s.close()
 if memory:memory.close()
 if path:os.unlink(path)
 stop_profiles(home);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if len(results)==len(cases) and all(r['passed'] for r in results) else 1)
