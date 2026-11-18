"""Real instrument fixtures for the vendor-independent audio observer.
Patch edits below are test-only. Production diagnoses signal/events and never
changes parameters to make a quiet patch audible.
"""
from pathlib import Path
import os,sys,json,struct,re,time,mmap,tempfile,math
sys.path.insert(0,str(Path(__file__).resolve().parent))
from installed_editor import Session,MAP_BYTES
from qa_runtime import stop_profiles
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file()
out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
source=Path(os.environ['BRIDGELAB_STATE_FILE']).read_bytes();assert source[20:24]==b'VC2!'
def change(state,**fields):
 length=struct.unpack_from('<I',state,24)[0];text=state[28:28+length]
 for key,value in fields.items():
  text,count=re.subn(rb'\b'+key.encode()+rb'="[^"]*"',key.encode()+b'="'+str(value).encode()+b'"',text);assert count==1
 chunk=b'VC2!'+struct.pack('<I',len(text))+text+b'\0'
 return state[:16]+struct.pack('<I',len(chunk))+chunk
base=change(source,filtertype=0.0,cutoff=1.0,volume=.412)
cases=[('audible reference',base,True,False),('high-pass maximum cutoff',change(base,filtertype=4/11),True,True),('zero volume',change(base,volume=0.0),True,True),('low-pass minimum cutoff',change(base,cutoff=0.0),True,True),('oscillators off',change(base,osc1volume=0.0,osc2volume=0.0,osc3volume=0.0,ringmodulation=0.0),True,True),('envelope sustain zero',change(base,ampsustain=0.0,ampdecay=0.0),True,True),('no note input',base,False,False),('high output gain',change(base,filtertype=1.0,volume=1.0),True,False)]
row=next(r for r in json.loads((home/'plugins.json').read_text()) if r['id']=='a4def1870e48ca34');results=[]
def render(s,command=0,blocks=400):
 for i in range(blocks):
  midi=struct.pack('<I4B',0,command,60,100 if command==0x90 else 0,0) if command and i==0 else b''
  s.p.stdin.write(struct.pack('<6I',0x424c3032,256,int(bool(midi)),0,0,0)+midi+bytes(1024-len(midi)));s.p.stdin.flush();raw=s.read(2064);assert struct.unpack_from('<4I',raw)[2]==1

def snapshot(s):
 head,data=s.request(15);assert head[2]==1;v=struct.unpack('<10I',data)
 number=lambda b:struct.unpack('<f',struct.pack('<I',b))[0]
 return {'blocks':v[0],'note_ons':v[1],'held_keys':v[2],'rms':math.sqrt(number(v[3])),'peak':number(v[4]),'quiet':bool(v[5]),'invalid_samples':v[6],'above_full_scale_recent':v[7],'rate':v[9]}
try:
 for name,state,play,expected in cases:
  s=None;memory=None;path=None;r={'case':name,'passed':False}
  try:
   s=Session(row,sample_rate=44100);assert s.request(4,state)[0][2]==1;
   fd,path=tempfile.mkstemp(prefix='BridgeLab-editor-audio-health-');os.ftruncate(fd,MAP_BYTES);memory=mmap.mmap(fd,MAP_BYTES);os.close(fd);s.request(6,('Z:'+path).encode())
   deadline=time.monotonic()+30
   while time.monotonic()<deadline:
    s.pump(20)
    if struct.unpack('<4I',memory[:16])[2]:break
   assert struct.unpack('<4I',memory[:16])[2],'No original editor frame'
   before=s.request(3)[1]
   render(s,0x90 if play else 0);observed=snapshot(s);r['observed']=observed
   assert observed['quiet']==expected,'Unexpected quiet-note classification'
   assert observed['note_ons']==int(play) and observed['held_keys']==int(play)
   assert not observed['invalid_samples']
   if name=='high output gain':assert observed['above_full_scale_recent']>0 and observed['peak']>1
   assert s.request(3)[1]==before,'Observation changed plugin state'
   s.request(14);health=json.loads(Path(path+'.health.json').read_text());assert bool(health['quiet_while_notes'])==expected
   r['state_unchanged']=True;r['local_health_report']=True
   if name=='no note input':
    time.sleep(3.2);s.request(14);stale=json.loads(Path(path+'.health.json').read_text());assert stale['audio_observation_stale']==1;r['stale_audio_distinguished']=True
   render(s,0x80,200);assert not snapshot(s)['quiet'],'Released notes must clear quiet-held-note advice'
   assert s.request(4,base)[0][2]==1;render(s,0x90,250);recovered=snapshot(s)
   r['recovered']=recovered;assert not recovered['above_full_scale_recent'] and not recovered['quiet'] and recovered['rms']>.01,'Audible patch must clear quiet advice'
   s.request(14);assert json.loads(Path(path+'.health.json').read_text())['audio_observation_stale']==0
   render(s,0x80,250);r['passed']=True
  except Exception as error:r['error']=str(error)
  finally:
   if s:s.close()
   if memory:memory.close()
   if path:os.unlink(path)
  results.append(r);print(json.dumps(r),flush=True)
finally:
 stop_profiles(home);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if len(results)==len(cases) and all(r['passed'] for r in results) else 1)
