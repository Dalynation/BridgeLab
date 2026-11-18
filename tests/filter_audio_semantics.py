"""NoiseMaker fixture: real filter choices may legitimately attenuate audio.
Verify each GUI selection changes only its intended field, handles fresh notes,
and recovers audibly with LP 24dB without reloading the plugin. No production
code branches on vendor names, these attributes, or control coordinates.
"""
from pathlib import Path
import json,os,sys,struct,tempfile,mmap,time,re,math,xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(__file__).resolve().parent))
from installed_editor import Session,MAP_BYTES
from qa_runtime import stop_profiles
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file()
out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
labels=['LP 24dB','LP 18dB','LP 12dB','LP 6dB','HP 12dB','BP 12dB','Notch','LP SV 24dB','HP SV 24dB','BP SV 24dB','LP Zero 24dB','HP Zero 24dB']
results=[];s=None;memory=None;path=None

def attributes(state):
 assert state[20:24]==b'VC2!';length=struct.unpack_from('<I',state,24)[0]
 return ET.fromstring(state[28:28+length]).find('.//program').attrib

def with_cutoff(state,value):
 length=struct.unpack_from('<I',state,24)[0];text,n=re.subn(rb'\bcutoff="[^"]*"',b'cutoff="'+str(value).encode()+b'"',state[28:28+length]);assert n==1
 chunk=b'VC2!'+struct.pack('<I',len(text))+text+b'\0'
 return state[:16]+struct.pack('<I',len(chunk))+chunk

try:
 row=next(r for r in json.loads((home/'plugins.json').read_text()) if r['id']=='a4def1870e48ca34');s=Session(row,sample_rate=44100)
 source=os.environ.get('BRIDGELAB_STATE_FILE');initial=Path(source).read_bytes() if source else s.request(3)[1]
 assert s.request(4,with_cutoff(initial,1.0))[0][2]==1
 fd,path=tempfile.mkstemp(prefix='BridgeLab-filter-semantics-');os.ftruncate(fd,MAP_BYTES);memory=mmap.mmap(fd,MAP_BYTES);os.close(fd);s.request(6,('Z:'+path).encode())
 deadline=time.monotonic()+40
 while time.monotonic()<deadline:
  s.pump(10);_,seq,w,h=struct.unpack('<4I',memory[:16])
  if (w,h)==(1270,695) and seq>20:break
 assert (w,h)==(1270,695)
 def click(x,y):
  for message,flags in [(0x200,0),(0x201,1),(0x202,0)]:s.input(message,x,y,flags)
 def status():
  head,data=s.request(14);assert head[2]==1
  return struct.unpack('<'+'I'*(len(data)//4),data)
 def select(index):
  previous=status()[2];click(265,274);deadline=time.monotonic()+5
  while time.monotonic()<deadline:
   s.pump(10);info=status()
   if info[5] and info[2]>previous:break
  else:raise AssertionError('Original filter menu did not render')
  left,top,right,bottom=info[8:12]
  click((left+right)//2,top+int((index+.5)*(bottom-top)/12));s.pump(25)
  assert not status()[5],'Menu did not close'
  params=attributes(s.request(3)[1]);assert round(float(params['filtertype'])*11)==index,'Wrong filter item selected'
  return params
 def audio(command=0,blocks=100):
  total=0;count=0;peak=0
  for i in range(blocks):
   midi=struct.pack('<I4B',0,command,60,100 if command==0x90 else 0,0) if command and i==0 else b''
   s.p.stdin.write(struct.pack('<6I',0x424c3032,256,int(bool(midi)),0,0,0)+midi+bytes(1024-len(midi)));s.p.stdin.flush();raw=s.read(2064);head=struct.unpack_from('<4I',raw);assert head[2]==1 and not head[3],'Audio transport failed'
   values=struct.unpack_from('<512f',raw,16);assert all(math.isfinite(v) for v in values),'Nonfinite audio'
   if i>25:total+=sum(v*v for v in values);count+=len(values);peak=max(peak,max(map(abs,values)))
  return {'rms':math.sqrt(total/count),'peak':peak}
 select(0);reference=audio(0x90);assert reference['rms']>1e-3
 for index,label in enumerate(labels):
  before=attributes(s.request(3)[1]);after=select(index)
  changed=sorted(k for k in set(before)|set(after) if before.get(k)!=after.get(k))
  assert set(changed)<= {'filtertype'},'Menu altered other controls: '+str(changed)
  held=audio();audio(0x80,40);fresh=audio(0x90)
  select(0);recovered=audio();assert recovered['rms']>reference['rms']*.1,'Audio did not recover after returning to LP 24dB'
  result={'filter':label,'changed_fields':changed,'held_note':held,'fresh_note':fresh,'LP24_recovered_without_reload':recovered,'passed':True}
  results.append(result);print(json.dumps(result),flush=True)
 released=audio(0x80,200);tail=audio(blocks=200);print(json.dumps({'release':released,'tail':tail}),flush=True)
 assert tail['rms']<1e-6,'Settled note-release tail exceeds this dry patch noise limit'
 (out/'summary.json').write_text(json.dumps({'sample_rate':44100,'reference_LP24':reference,'released_note':released,'released_tail':tail,'all_filter_selections':len(results),'passed':len(results)==12},indent=2)+'\n')
finally:
 if s:s.close()
 if memory:memory.close()
 if path:os.unlink(path)
 stop_profiles(home);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
