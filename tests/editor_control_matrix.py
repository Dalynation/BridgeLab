"""Audit original control classes in a disposable profile, saving state/pixel evidence.
A failed observation is an acceptance gap, not proof of a vendor defect. Menu
opening does not certify selection, dismissal, submenus, or external dialogs.
"""
from pathlib import Path
import os,sys,json,mmap,tempfile,struct,time,zlib
sys.path.insert(0,str(Path(__file__).resolve().parent))
import installed_editor as editor
from qa_runtime import stop_profiles
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file(),'Use a disposable QA home';out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
# Coordinates describe the original 1270x695 TAL-NoiseMaker v5.0.6 artwork.
cases=[
 ('volume-knob','drag',(1225,115),(1225,165),(1190,75,1260,155)),
 ('amp-attack-slider','drag',(309,440),(309,395),(285,380,335,470)),
 ('oscillator-waveform-click-response','probe',(530,190),None,(490,160,555,230)),
 ('oscillator-sync-toggle','click',(1088,193),None,(1060,175,1120,210)),
 ('chorus-I-toggle','click',(100,537),None,(78,520,125,555)),
 ('one-shot-toggle','click',(1206,374),None,(1150,355,1260,391)),
 ('lfo-trigger-toggle','click',(43,188),None,(15,173,75,204)),
 ('filter-dropdown','menu',(265,274),None,(145,289,350,460)),
 ('voices-dropdown','menu',(1177,193),None,(1110,210,1265,395)),
 ('lfo-destination-dropdown','menu',(120,225),None,(15,240,270,465)),
 ('amp-destination-dropdown','menu',(537,325),None,(460,340,700,550)),
 ('portamento-dropdown','menu',(1177,115),None,(1110,132,1265,300)),
 ('modulation-dropdown','menu',(1235,306),None,(1090,322,1265,510)),
 ('envelope-graph-point','drag',(576,373),(576,335),(560,280,650,410)),
 ('volume-wheel','wheel',(1225,115),None,(1190,75,1260,155)),
 ('volume-double-click-reset','reset',(1225,115),None,(1190,75,1260,155)),
 ('preset-selector','menu',(150,18),None,(5,40,500,450)),
 ('preset-next-button','click',(272,18),None,(10,0,288,39)),
 ('volume-context-menu','context',(1225,115),None,(990,135,1265,400)),
 ('volume-double-click-response','double',(1225,115),None,(1180,75,1265,155)),
 ('midi-learn-arm','nav',(482,18),None,(420,0,530,38)),
 ('parameter-lock-toggle','nav',(582,18),None,(531,0,630,38)),
]
fixtures={'a4def1870e48ca34':cases,
 'ffbb70884a6cd2af':[
 ('parameter-knob','drag',(234,326),(234,290),(215,300,255,350)),
 ('preset-next','click',(295,46),None,(20,35,280,77)),
 ('mono-toggle','click',(575,119),None,(540,105,595,135)),
 ('sync-toggle','click',(123,133),None,(90,112,145,149)),
 ('oscillator-page','nav',(180,120),None,(20,225,600,300)),
 ('lfo-page','nav',(71,379),None,(20,225,600,300)),
 ('envelope-page','nav',(278,379),None,(20,225,600,300)),
 ('matrix-page','nav',(463,379),None,(20,225,600,300)),
 ('bank-menu','menu',(340,62),None,(280,75,610,300)),
 ('file-menu','menu',(42,12),None,(0,25,280,300))],
 'bca6d84f7103c868':[
 ('master-level-slider','drag',(53,348),(53,310),(30,290,77,390)),
 ('reverb-amount-knob','drag',(1047,449),(1047,415),(1028,429,1067,472)),
 ('dco-wave-toggle','click',(229,157),None,(208,110,250,180)),
 ('chorus-I-toggle','click',(1098,157),None,(1078,110,1118,180)),
 ('arpeggiator-toggle','click',(970,331),None,(951,292,987,352)),
 ('lfo-wave-dropdown','menu',(410,325),None,(350,250,560,465)),
 ('maximum-polyphony-dropdown','menu',(443,425),None,(345,250,560,470)),
 ('voice-mode-dropdown','menu',(526,425),None,(465,230,680,470)),
 ('preset-next','click',(683,21),None,(432,1,700,40)),
 ('keyboard-panel','nav',(280,449),None,(0,0,1190,470)),
 ('midi-learn-arm','nav',(955,20),None,(911,1,1009,42)),
 ('filter-context-menu','context',(500,150),None,(470,170,750,450))]}
plugin_id=os.environ.get('BRIDGELAB_CONTROL_PLUGIN','a4def1870e48ca34')
cases=fixtures[plugin_id]
results=[];s=None;mem=None;path=None;row={}
try:
 row=next(r for r in json.loads((home/'plugins.json').read_text()) if r['id']==plugin_id);s=editor.Session(row)
 fd,path=tempfile.mkstemp(prefix='BridgeLab-control-matrix-');os.ftruncate(fd,editor.MAP_BYTES);mem=mmap.mmap(fd,editor.MAP_BYTES);os.close(fd);s.request(6,('Z:'+path).encode())
 deadline=time.monotonic()+40
 while time.monotonic()<deadline:
  s.pump(20);_,seq,w,h=struct.unpack('<4I',mem[:16])
  if w and seq>20:break
 assert w and h;s.pump(80);initial=s.request(3)[1]
 def snap(name):
  global w,h
  _,seq,w,h=struct.unpack('<4I',mem[:16]);pix=bytes(mem[16:16+w*h*4]);rgba=bytearray(pix);rgba[0::4],rgba[2::4]=rgba[2::4],rgba[0::4];rgba[3::4]=bytes([255])*(w*h)
  def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
  scan=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
  (out/(name+'.png')).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b''));return pix
 def click(x,y,button=False,double=False):
  s.input(0x200,x,y,0);s.input(0x204 if button else 0x201,x,y,2 if button else 1)
  s.input(0x205 if button else 0x202,x,y,0)
  if double:s.input(0x203,x,y,1);s.input(0x202,x,y,0)
 for name,kind,point,end,box in cases:
  s.input(0x100,0,0,27);s.input(0x101,0,0,27)
  if plugin_id!='bca6d84f7103c868':s.request(4,initial)
  s.pump(35)
  _,_,w,h=struct.unpack('<4I',mem[:16])
  # Demo state restoration can show the vendor's license notice. Dismiss
  # its visible Cancel button normally; do not test through a modal overlay.
  if plugin_id=='bca6d84f7103c868':
   def trial_notice():
    colors=[mem[16+((h//2+12)*w+xx)*4:16+((h//2+12)*w+xx)*4+3] for xx in (300,900)]
    return all(max(c)-min(c)<3 and 45<c[0]<100 for c in colors)
   if trial_notice():
    snap(name+'-trial-notice');click(803,h//2+150);s.pump(45)
    assert not trial_notice(),'Vendor trial notice remained open after Cancel'
   if h!=477:
    assert (w,h)==(1198,650),'Unexpected U-NO-LX layout; coordinates not certified'
    click(280,449);s.pump(45);_,_,w,h=struct.unpack('<4I',mem[:16])
   assert (w,h)==(1198,477),'Could not restore the documented original control layout'
  x,y=point
  if kind=='reset':
   s.input(0x201,x,y,1);s.input(0x200,x,y+50,1);s.input(0x202,x,y+50,0);s.pump(30)
  before=snap(name+'-before');state=s.request(3)[1]
  if kind=='drag':
   s.input(0x200,x,y,0);s.input(0x201,x,y,1)
   for i in range(1,11):s.input(0x200,x+(end[0]-x)*i//10,y+(end[1]-y)*i//10,1)
   s.input(0x202,*end,0)
  elif kind=='wheel':s.input(0x200,x,y,0);s.input(0x20a,x,y,120)
  else:click(x,y,kind=='context',kind in ('double','reset'))
  s.pump(45);after=snap(name+'-after');stateChanged=s.request(3)[1]!=state
  x1,y1,x2,y2=box;x2=min(x2,w);y2=min(y2,h);changed=sum(before[(yy*w+xx)*4+c]!=after[(yy*w+xx)*4+c] for yy in range(y1,y2) for xx in range(x1,x2) for c in range(3))
  visual=changed>40
  passed=visual if kind in ('menu','context','nav') else visual and stateChanged
  if kind in ('double','probe'):passed=False # Repaint alone does not prove editable text input.
  r={'plugin':row['name'],'control':name,'kind':kind,'state_changed':stateChanged,'changed_region_bytes':changed,'passed':passed,'scope':'menu opening only; selection/closing not certified' if kind in ('menu','context') else 'visual response only; text entry not verified' if kind=='double' else 'unverified interaction semantics; not classified as a menu defect' if kind=='probe' else 'panel visual response only' if kind=='nav' else 'input, parameter state, and immediate repaint' }
  results.append(r);print(json.dumps(r),flush=True)
except Exception as error:
 results.append({'plugin':row.get('name',plugin_id),'control':'fixture setup or execution','passed':False,'blocked':True,'error':str(error)})
 raise
finally:
 if s:s.close()
 if mem:mem.close()
 if path:os.unlink(path)
 stop_profiles(home);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
