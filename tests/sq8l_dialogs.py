"""Original WRITE controls, popup keyboard navigation, and text-focus ownership. QA only."""
from pathlib import Path
import sys,os,json,mmap,tempfile,struct,time,zlib,shutil,subprocess,configparser
sys.path.insert(0,str(Path(__file__).resolve().parent))
import installed_editor as ed
from qa_runtime import stop_profiles
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file()
out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
results=[]
(out/'results.json').write_text(json.dumps([{'control':'suite','passed':False,'status':'running'}])+'\n')
row=next(r for r in json.loads((home/'plugins.json').read_text()) if 'SQ8L' in r['name'])
folder=Path(row['path']).parent;assert folder.resolve().is_relative_to(home.resolve()),'WRITE tests require a QA-managed DLL copy'
backup=tempfile.TemporaryDirectory(prefix='BridgeLab-SQ8L-QA-backup-');shutil.copytree(folder,Path(backup.name)/'plugin')
s=ed.Session(row);fd,path=tempfile.mkstemp();os.ftruncate(fd,ed.MAP_BYTES);m=mmap.mmap(fd,ed.MAP_BYTES);os.close(fd)
def snap(name):
 _,seq,w,h=struct.unpack('<4I',m[:16]);pix=bytearray(m[16:16+w*h*4]);pix[0::4],pix[2::4]=pix[2::4],pix[0::4];pix[3::4]=bytes([255])*(w*h)
 def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
 scan=b''.join(b'\0'+pix[y*w*4:(y+1)*w*4] for y in range(h))
 (out/(name+'.png')).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b''))
 assert seq and (w,h)==(630,428)
 return bytes(m[16:16+w*h*4])
def click(x,y):
 for msg,f in [(0x200,0),(0x201,1),(0x202,0)]:s.input(msg,x,y,f)
def audio(status=0):
 midi=struct.pack('<I4B',0,status,60,100 if status==0x90 else 0,0) if status else b''
 s.p.stdin.write(struct.pack('<6I',0x424c3032,256,int(bool(status)),0,0,0)+midi+bytes(1024-len(midi)));s.p.stdin.flush();r=s.read(2064);assert struct.unpack_from('<4I',r)[2]==1
 return max(abs(x) for x in struct.unpack_from('<512f',r,16))
def check(name):
 audio(0x80);audio(0x90);peak=max(audio() for _ in range(60));audio(0x80);s.pump(30);snap(name)
 assert peak>1e-4,(name,'No audio after note input')
 flags=struct.unpack_from('<I',m,ed.MAP_BYTES-4)[0]
 if name in ('write-open','compare-toggle','write-list-select','write-list-arrow-down'):assert flags==2,(name,'Non-text dialog control stole piano keys',flags)
 results.append({'control':name,'audio_peak':peak,'keyboard_flags':flags,'passed':True});print(json.dumps(results[-1]),flush=True)
def extended_checks():
 global s
 s=ed.Session(row);previous=struct.unpack('<4I',m[:16])[1];s.request(6,('Z:'+path).encode());deadline=time.monotonic()+40
 while time.monotonic()<deadline:
  s.pump(20)
  if struct.unpack('<4I',m[:16])[1]!=previous:break
 s.pump(50);check('extended-initial');initial=s.request(3)[1]
 def status(allow_unsupported=False):
  head,data=s.request(14);assert head[2]==1
  value=struct.unpack('<'+'I'*(len(data)//4),data);assert not value[4] and (allow_unsupported or not value[6]),value
  return value
 def wait_popups(count):
  deadline=time.monotonic()+5
  while time.monotonic()<deadline:
   s.pump(8);value=status()
   if value[5]==count:return [value[i:i+4] for i in range(8,len(value),4)]
  raise AssertionError(('Popup count',count,value))
 def escape():
  s.input(0x100,0,0,27);s.input(0x102,0,0,27);s.input(0x101,0,0,27)
 def double(x,y):
  click(x,y);s.input(0x203,x,y,1);s.input(0x202,x,y,0)
 def options_submenu(y):
  click(103,12);wait_popups(1);s.input(0x200,180,y,0);return wait_popups(2)[0]
 def emulation_leaf(index):
  left,top,right,bottom=options_submenu(41);s.input(0x200,left+50,top+9+17*index,0);return wait_popups(3)[0]
 def file_dialog(y):
  click(42,12);wait_popups(1);click(100,y);wait_popups(1);s.pump(25)
  assert struct.unpack_from('<I',m,ed.MAP_BYTES-4)[0]&1,'Filename field did not capture text'
 def type_path(value):
  for c in ('Z:'+str(value)).replace('/',chr(92)):s.input(0x102,0,0,ord(c))
 def confirm_load(required=True):
  s.pump(40)
  if not required and status()[5]==0:return
  left,top,right,bottom=wait_popups(1)[0];assert right-left<300 and bottom-top<100,'Unexpected library prompt'
  s.input(0x100,0,0,13);s.input(0x102,0,0,13);s.input(0x101,0,0,13);wait_popups(0)
 # Cancelling a value menu must release the knob, not drag it on the next click.
 click(50,148);s.pump(25);before=s.request(3)[1];double(234,326);wait_popups(1);snap('value-menu');escape();wait_popups(0)
 assert s.request(3)[1]==before,'Escape changed the parameter'
 click(42,12);wait_popups(1);snap('file-after-value-escape');assert s.request(3)[1]==before,'Next click dragged the previous knob'
 escape();wait_popups(0);check('value-menu-cancel-next-click')
 double(234,326);left,top,right,bottom=wait_popups(1)[0];click(left+22,top+108);wait_popups(0)
 assert s.request(3)[1]!=before;check('value-menu-selection')
 s.request(4,initial);click(453,62);s.pump(35);assert s.request(3)[1]!=initial;check('init')
 s.request(4,initial);audio(0x90);assert max(audio() for _ in range(30))>1e-4
 click(219,12);s.pump(5);residual=max(audio() for _ in range(50));assert residual<1e-5,residual
 audio(0x80);check('panic-and-new-note');results[-1]['peak_after_panic']=residual
 click(166,12);wait_popups(1);click(220,48);left,top,right,bottom=wait_popups(1)[0];snap('about')
 click((left+right)//2,(top+bottom)//2);wait_popups(0);check('about-click-dismissal')
 # Use original common dialogs for an actual library round trip. Windows file
 # dialogs require Windows paths; the default library is a disposable copy.
 file_dialog(24);type_path(folder/'SQ8L/SQ8L_backup.dat');click(410,201);confirm_load();check('library-open')
 target=out/('library-roundtrip-'+str(time.time_ns())+'.8XL')
 file_dialog(41);type_path(target);snap('save-library-filename');click(410,201);wait_popups(0)
 assert target.stat().st_size>1000;check('library-save');results[-1]['bytes_saved']=target.stat().st_size
 file_dialog(24);type_path(target);click(410,201);confirm_load(False);check('saved-library-reload')
 # Select real nested settings, then verify the vendor's saved configuration.
 left,top,right,bottom=options_submenu(24);click((left+right)//2,top+40);wait_popups(0);check('voice-stealing-hard')
 for index in range(4):
  left,top,right,bottom=emulation_leaf(index);snap('emulation-'+str(index));click((left+right)//2,bottom-8);wait_popups(0);check('emulation-setting-'+str(index))
 for name,first in [('menu-mouse-restore',True),('knob-mouse-restore',False)]:
  left,top,right,bottom=options_submenu(67);click((left+right)//2,top+10 if first else bottom-8);wait_popups(0);check(name)
 click(103,12);wait_popups(1);click(180,85);wait_popups(0);check('right-click-scroll-toggle')
 s.input(0x204,300,265,2);s.input(0x205,300,265,0);s.pump(25);value=status(True);snap('page-context-menu')
 if value[6]:
  results.append({'control':'page-context-menu','passed':False,'known_limitation':'Original context menu exceeds the fixed editor canvas; diagnosed, not fully displayed'})
 else:assert value[5]==1,'Context menu did not open'
 escape();wait_popups(0);check('page-context-menu-escape-recovery')
 # Teardown must exit the vendor menu loop before effEditClose destroys its GUI.
 click(42,12);wait_popups(1);assert s.request(2)[0][2]==1
 resources=struct.unpack('<4I',s.request(12)[1]);assert resources[:3]==(0,0,0),resources
 results.append({'control':'close-with-native-menu-open','passed':True,'resources':list(resources)})
 s.close();s=None;stop_profiles(home)
 config=configparser.ConfigParser();config.read(folder/'SQ8L/SQ8L.ini')
 expected={'synth':{'voiceStealMode':1,'oscDcaMode':2,'dca4Mode':2,'muffleMode':2,'dcbMode':3},'gui':{'restMouseMenu':0,'restMouseKnob':0,'rmbScrollDisplay':0}}
 for section,values in expected.items():
  for key,value in values.items():assert config.getint(section,key)==value,(section,key,config.get(section,key))
 results.append({'control':'menu-settings-persisted','passed':True,'settings':expected});print(json.dumps(results[-1]),flush=True)

try:
 if os.environ.get('BRIDGELAB_SQ8L_EXTENDED_ONLY'):
  s.close();s=None;stop_profiles(home);extended_checks()
 else:
  s.request(6,('Z:'+path).encode());
  deadline=time.monotonic()+40
  while time.monotonic()<deadline:
   s.pump(20)
   if struct.unpack('<4I',m[:16])[1]>20:break
  check('initial');initial=s.request(3)[1]
  click(409,62);s.pump(60);check('write-open')
  click(423,77);s.pump(60);check('compare-toggle')
  click(460,140);s.pump(60);check('write-list-select');before=snap('before-arrow')
  s.input(0x100,0,0,40);s.input(0x101,0,0,40);s.pump(60);check('write-list-arrow-down');after=snap('after-arrow')
  changed=sum(before[(yy*630+xx)*4+c]!=after[(yy*630+xx)*4+c] for yy in range(135,165) for xx in range(417,577) for c in range(3));assert changed>100,'List arrow did not move selection'
  results[-1]['changed_selection_bytes']=changed
  click(555,393);s.pump(60);check('cancel')
  assert not (results[-1]['keyboard_flags']&2),'WRITE did not close'
  click(175,54);s.pump(40);check('sound-name-text-focus');assert results[-1]['keyboard_flags']&1,'Text input did not request keyboard'
  s.input(0x100,0,0,13);s.input(0x102,0,0,13);s.input(0x101,0,0,13);s.pump(40);snap('sound-name-committed')
  assert struct.unpack_from('<I',m,ed.MAP_BYTES-4)[0]==0,'Enter left text capture enabled'
  s.request(4,initial);click(234,326);s.pump(40);check('knob-restores-piano-focus');assert not(results[-1]['keyboard_flags']&1),'Knob still captured text keys'
  # Exercise every page button in the original SQ8L surface. Page selection
  # changes the display, not the stored synth patch; do not require a fake
  # parameter mutation for navigation controls.
  pages=[('mix',(50,148)),('osc-1',(180,120)),('osc-2',(180,147)),('osc-3',(180,175)),('dca-1',(299,120)),('dca-2',(299,147)),('dca-3',(299,175)),('filter',(399,147)),('dca-4',(467,147)),('modes-emu',(574,171))]
  pages += [('lfo-'+str(i+1),(71+47*i,379)) for i in range(4)]
  pages += [('env-'+str(i+1),(278+47*i,379)) for i in range(4)]
  pages += [('matrix-'+str(i+1),(463+47*i,379)) for i in range(3)]
  for name,point in pages:
   s.request(4,initial);click(*( (180,120) if name=='mix' else (50,148)));s.pump(25);before=snap(name+'-before')
   click(*point);s.pump(35);after=snap(name+'-after')
   changed=sum(before[(yy*630+xx)*4+c]!=after[(yy*630+xx)*4+c] for yy in range(245,300) for xx in range(24,590) for c in range(3))
   assert changed>40,(name,'Page display did not update')
   check(name);assert results[-1]['keyboard_flags']==0,(name,'Page stole piano keys');results[-1]['display_changed_bytes']=changed
  # A displayed dialog is insufficient proof of WRITE. Save to a disposable
  # library slot and verify its name from a fresh runner, then restore the QA
  # library in finally. No user library is used or modified.
  s.request(4,initial);click(175,54);s.pump(25)
  for c in 'XY':s.input(0x102,0,0,ord(c))
  s.input(0x100,0,0,13);s.input(0x102,0,0,13);s.input(0x101,0,0,13);s.pump(30);check('sound-name-edited')
  assert results[-1]['keyboard_flags']==0,'Enter left text capture enabled'
  click(409,62);s.pump(40);click(460,154);s.pump(25);click(455,393);s.pump(50);check('write-commit')
  s.close();s=None;stop_profiles(home)
  env={**os.environ,'WINEPREFIX':str(home/'prefixes/crossover-shared'),'WINEDEBUG':'-all'}
  probe=subprocess.run([str(home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64'),str(home/'engine/bin/runner-x86.exe'),'Z:'+row['path'],'--probe'],env=env,capture_output=True,text=True,timeout=90)
  assert probe.returncode==0,probe.stderr
  record=next(line for line in probe.stdout.splitlines() if line.startswith('BridgeLabProgram 3 '));saved_name=bytes.fromhex(record.split(' ',2)[2]).decode();assert saved_name=='VOYAGERXY',saved_name
  results.append({'control':'write-reload','slot':3,'name_after_runner_recreation':saved_name,'passed':True});print(json.dumps(results[-1]),flush=True)
  extended_checks()
 results.append({'control':'suite-complete','passed':True})
except Exception as error:
 results.append({'control':'suite-execution','passed':False,'error':str(error)});raise
finally:
 if s:s.close()
 m.close();Path(path).unlink(missing_ok=True);stop_profiles(home)
 shutil.rmtree(folder);shutil.copytree(Path(backup.name)/'plugin',folder);backup.cleanup()
 (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
