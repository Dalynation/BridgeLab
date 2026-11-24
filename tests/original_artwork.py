"""Original artwork, mouse input, audible volume and fresh-instance recall.

Imports are explicit SHA-based test fixtures for official vendor builds; the
production host does not branch on these IDs or plugin names. No vendor DLLs
are distributed. Uses BRIDGELAB_HOME and BRIDGELAB_QA_OUTPUT.
"""
from pathlib import Path
import os,json,sys,importlib.util,struct,time,mmap,tempfile,zlib,math
source=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']).resolve();out=Path(os.environ['BRIDGELAB_QA_OUTPUT'])/'original-artwork';out.mkdir(parents=True,exist_ok=True);assert (home/'.bridgelab-qa').is_file(), 'Use a disposable QA home';sys.path.insert(0,str(source/'tests'));from qa_runtime import stop_profiles
spec=importlib.util.spec_from_file_location('editor',source/'tests/installed_editor.py');e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e)
fixtures={'aa3d048dfa16e71d':((770,523),(1060,500),80),'9f901d6b8802bc8a':((684,479),(907,355),80),'bca6d84f7103c868':((803,388),(52,351),30)}
selected=os.environ.get('BRIDGELAB_ARTWORK_PLUGIN')
if selected:
 assert selected in fixtures,'Unknown artwork fixture'
 fixtures={selected:fixtures[selected]}
def audio(s,note=False,blocks=100):
 total=0;n=0;peak=0
 for i in range(blocks):
  midi=struct.pack('<I4B',0,0x90,60,100,0) if note and i==0 else b''
  s.p.stdin.write(struct.pack('<6I',0x424c3032,256,int(bool(midi)),0,0,0)+midi+bytes(1024-len(midi)));s.p.stdin.flush();raw=s.read(2064);v=struct.unpack_from('<512f',raw,16);peak=max(peak,max(abs(x) for x in v))
  if i>20:total+=sum(x*x for x in v);n+=len(v)
  time.sleep(.005)
 return dict(rms=math.sqrt(total/max(1,n)),peak=peak)
def png(frame,target):
 _,_,w,h=struct.unpack('<4I',frame[:16]);pixels=bytearray(frame[16:16+w*h*4]);pixels[0::4],pixels[2::4]=pixels[2::4],pixels[0::4];pixels[3::4]=bytes([255])*(w*h)
 def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
 raw=b''.join(b'\0'+pixels[y*w*4:(y+1)*w*4] for y in range(h));target.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
def region(frame,box):
 _,_,w,h=struct.unpack('<4I',frame[:16]);x1,y1,x2,y2=box;return b''.join(frame[16+(y*w+x1)*4:16+(y*w+x2)*4] for y in range(y1,y2))
regions={'aa3d048dfa16e71d':(16,34,199,61),'9f901d6b8802bc8a':(148,36,632,50),'bca6d84f7103c868':(18,7,338,34)}
results=[]
try:
 for row in json.loads((home/'plugins.json').read_text()):
  if row['id'] not in fixtures:continue
  result=dict(plugin=row['name']);s=None;frame=None;path=None
  try:
   s=e.Session(row);fd,path=tempfile.mkstemp(prefix='BridgeLab-GPU-knobs-');os.ftruncate(fd,e.MAP_BYTES);frame=mmap.mmap(fd,e.MAP_BYTES);os.close(fd);assert s.request(6,('Z:'+path).encode())[0][2]==1
   start=time.monotonic()
   while time.monotonic()-start<45:
    s.pump(8);_,_,w,h=struct.unpack('<4I',frame[:16])
    if w and h and len({frame[16+i:19+i] for i in range(0,w*h*4,max(4,(w*h//1000)*4))})>25:break
   else:raise AssertionError('No original editor pixels')
   result['first_frame_seconds']=time.monotonic()-start
   (cx,cy),(x,y),delta=fixtures[row['id']]
   s.input(0x201,cx,cy,1);s.input(0x202,cx,cy,0);s.pump(60);png(frame,out/(row['id']+'-before.png'));stable_before=region(frame,regions[row['id']]);assert max(stable_before[0::4]+stable_before[1::4]+stable_before[2::4])>100,'Original interface artwork missing';_,before=s.request(3);result['before']=audio(s,True)
   s.input(0x200,x,y,0);s.input(0x201,x,y,1)
   for dy in range(5,delta+1,5):s.input(0x200,x,y+dy,1)
   s.input(0x202,x,y+delta,0);s.pump(40);_,edited=s.request(3);png(frame,out/(row['id']+'-after.png'));stable_after=region(frame,regions[row['id']]);result['original_artwork_preserved']=stable_after==stable_before;assert stable_after==stable_before,'Unedited original artwork changed after knob input';result['after']=audio(s);result['mouse_changes_state']=before!=edited;assert before!=edited,'Original volume control did not change state';assert result['before']['rms']>1e-6,'Initial note silent';assert result['after']['rms']<result['before']['rms']*.85,'Volume drag did not reduce actual audio'
   result['graphics_open']=list(struct.unpack('<4I',s.request(13)[1]));assert result['graphics_open'][0:2]==[1,1];assert s.request(2)[0][2]==1;result['graphics_closed']=list(struct.unpack('<4I',s.request(13)[1]));assert result['graphics_closed'][0:3]==[0,0,0];assert struct.unpack('<4I',s.request(12)[1])[:3]==(0,0,0);assert s.request(4,edited)[0][2]==1;_,normalized=s.request(3);result['normalization_byte_delta']=len(normalized)-len(edited);s.close();s=e.Session(row);assert s.request(4,edited)[0][2]==1;_,recalled=s.request(3);assert recalled==normalized,'Edited state did not recreate after plugin normalization';result['recalled_audio']=audio(s,True);assert result['recalled_audio']['rms']<max(1e-5,result['after']['rms']*2),'Recalled volume was lost';result.update(normalized_edited_state_recreated_exactly=True,passed=True)
  except Exception as error:result.update(passed=False,error=str(error))
  finally:
   if s:s.close()
   if frame:frame.close()
   if path:os.unlink(path)
  results.append(result);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(result),flush=True)
finally:stop_profiles(home)
raise SystemExit(0 if len(results)==len(fixtures) and all(x['passed'] for x in results) else 1)
