"""Compare actual vendor PCM before/after observation using a retained runner."""
from pathlib import Path
import os,sys,json,struct,re,hashlib
sys.path.insert(0,str(Path(__file__).resolve().parent))
from installed_editor import Session
from qa_runtime import stop_profiles
home=Path(os.environ['BRIDGELAB_HOME']);assert (home/'.bridgelab-qa').is_file()
out=Path(os.environ['BRIDGELAB_QA_OUTPUT']);out.mkdir(parents=True,exist_ok=True)
old=Path(os.environ['BRIDGELAB_BASELINE_RUNNER_X64']);new=Path(os.environ['BRIDGELAB_NEW_RUNNER_X64']);source=Path(os.environ['BRIDGELAB_STATE_FILE']).read_bytes()
row=next(r for r in json.loads((home/'plugins.json').read_text()) if r['id']=='a4def1870e48ca34')
def state(**changes):
 n=struct.unpack_from('<I',source,24)[0];text=source[28:28+n]
 for k,v in changes.items():text,count=re.subn(rb'\b'+k.encode()+rb'="[^"]*"',k.encode()+b'="'+str(v).encode()+b'"',text);assert count==1
 chunk=b'VC2!'+struct.pack('<I',len(text))+text+b'\0';return source[:16]+struct.pack('<I',len(chunk))+chunk
cases={'saved high-pass patch':source,'low-pass patch':state(filtertype=0.0),'zero volume':state(filtertype=0.0,volume=0.0)}
results=[]
try:
 for rate in [44100,48000]:
  for name,payload in cases.items():
   streams=[]
   for runner in [old,new]:
    os.environ['BRIDGELAB_TEST_RUNNER_X64']=str(runner);s=Session(row,sample_rate=rate)
    try:
     assert s.request(4,payload)[0][2]==1;pcm=bytearray()
     for i in range(200):
      midi=struct.pack('<I4B',0,0x90,60,100,0) if i==0 else b''
      s.p.stdin.write(struct.pack('<6I',0x424c3032,256,int(bool(midi)),0,0,0)+midi+bytes(1024-len(midi)));s.p.stdin.flush();raw=s.read(2064);assert struct.unpack_from('<4I',raw)[2]==1;pcm+=raw[16:]
     streams.append(bytes(pcm))
    finally:s.close()
   r={'case':name,'rate':rate,'passed':streams[0]==streams[1],'baseline_sha256':hashlib.sha256(streams[0]).hexdigest(),'observed_sha256':hashlib.sha256(streams[1]).hexdigest()};results.append(r);print(json.dumps(r),flush=True)
finally:
 stop_profiles(home);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if len(results)==6 and all(r['passed'] for r in results) else 1)
